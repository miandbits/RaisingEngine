import {readFile,realpath} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import path from 'node:path';
import type {Trace} from './model.js';
import type {StoredRun} from './archive.js';
import type {ContentSnapshot} from '../core/model.js';

type Row={run:number;path:Uint32Array};
type Descriptor={label:string;kind:string;periodIndex:number;passed:boolean|null};
/** A prefix tree of actual ordered observations. Repeated nodes never merge across prefixes. */
export class RouteTreeIndex {
  constructor(private labels:Record<string,string>={}){}
  private ids:string[]=[];
  private symbols=new Map<string,number>();
  private descriptors:Descriptor[]=[];
  private rows:Row[]=[];
  add(trace:Pick<Trace,'run'|'route'|'outcome'|'state'>){
    const points=[...(trace.route??[]),{nodeId:JSON.stringify(['route-end',trace.outcome,trace.state.endingId]),label:trace.outcome==='finished'?`종료 · ${trace.state.endingId??'엔딩'}`:`종료 · ${trace.outcome}`,kind:'terminal',periodIndex:trace.state.periodIndex,passed:null}];
    const encoded=points.map(p=>{let id=this.symbols.get(p.nodeId);if(id===undefined){id=this.ids.length;this.ids.push(p.nodeId);this.symbols.set(p.nodeId,id);this.descriptors.push({label:this.labels[`${p.kind}:${p.label}`]??p.label,kind:p.kind,periodIndex:p.periodIndex,passed:p.passed});}return id;});
    this.rows.push({run:trace.run,path:Uint32Array.from(encoded)});
  }
  private matching(prefix:unknown):Row[]{
    if(!Array.isArray(prefix)||prefix.length>50000||prefix.some(p=>typeof p!=='string'||p.length>4096))throw new Error('경로 접두사가 올바르지 않습니다.');
    const codes=prefix.map(id=>this.symbols.get(id));if(codes.some(id=>id===undefined))return [];
    return this.rows.filter(row=>row.path.length>=codes.length&&codes.every((id,i)=>row.path[i]===id));
  }
  runs(prefix:unknown){return new Set(this.matching(prefix).map(row=>row.run));}
  page(prefix:string[]=[],offset=0,limit=40){
    if(!Number.isSafeInteger(offset)||offset<0||!Number.isSafeInteger(limit)||limit<1||limit>100)throw new Error('트리 목록 범위가 올바르지 않습니다.');
    const rows=this.matching(prefix),groups=new Map<number,Row[]>();
    for(const row of rows){const next=row.path[prefix.length];if(next!==undefined){const group=groups.get(next)??[];group.push(row);groups.set(next,group);}}
    const sorted=[...groups.entries()].sort((a,b)=>b[1].length-a[1].length||a[0]-b[0]);
    const children=sorted.slice(offset,offset+limit).map(([code,group])=>{
      let end=prefix.length+1;
      // Compress unary spans, but bound each card so long runs remain navigable.
      while(end<prefix.length+32){const next=group[0]!.path[end];if(next===undefined||!group.every(row=>row.path[end]===next))break;end++;}
      const codes=Array.from(group[0]!.path.slice(prefix.length,end));
      const display=(id:number)=>{const d=this.descriptors[id]!;return d.label+(d.passed===null?'':d.passed?' · 성공':' · 실패');};
      const trail=codes.map(display),first=this.descriptors[code]!;
      return {prefix:Array.from(group[0]!.path.slice(0,end)).map(id=>this.ids[id]!),label:display(code),kind:first.kind,periodIndex:first.periodIndex,passed:first.passed,trail,count:group.length,parentCount:rows.length,observedPercent:rows.length?group.length/rows.length*100:0,terminal:group.every(row=>row.path.length===end),sampleRuns:group.slice(0,5).map(row=>row.run)};
    });
    return {count:rows.length,runCount:this.rows.length,totalBranches:groups.size,offset,children};
  }
}

const cache=new Map<string,{rows:StoredRun[];value:Promise<RouteTreeIndex>}>();
export function loadRouteTree(prefix:string,rows:StoredRun[],fingerprint:string,snapshotHash?:string):Promise<RouteTreeIndex>{
  if(rows.length===0)return Promise.resolve(new RouteTreeIndex());
  const hit=cache.get(prefix);if(hit?.rows===rows)return hit.value;
  const value=(async()=>{
    const dir=`${prefix}.runs`;if((await realpath(dir)).toLowerCase()!==dir.toLowerCase())throw new Error('분석 기록 경로의 링크는 허용하지 않습니다.');
    const raw=await readFile(`${prefix}.snapshot.json`,'utf8');
    if(snapshotHash&&createHash('sha256').update(raw).digest('hex')!==snapshotHash)throw new Error('분석 스냅샷 파일이 변경되었습니다.');
    const content=JSON.parse(raw) as ContentSnapshot;if(content.contentFingerprint!==fingerprint)throw new Error('분석 스냅샷이 기록과 다릅니다.');
    const labels:Record<string,string>={};
    for(const action of content.project.gameplay?.actions??[])labels[`action:${action.id}`]=action.name;
    for(const policy of content.project.gameplay?.policyGroups??[])for(const option of policy.options)labels[`policy:${policy.id}/${option.id}`]=`${policy.name} · ${option.name}`;
    const tree=new RouteTreeIndex(labels);
    for(let start=0;start<rows.length;start+=16){
      const traces=await Promise.all(rows.slice(start,start+16).map(async row=>{
        if(!Number.isSafeInteger(row.run)||row.run<0)throw new Error('실행 회차가 올바르지 않습니다.');
        const text=await readFile(path.join(dir,`${row.run}.json`),'utf8');
        if(row.traceHash&&createHash('sha256').update(text).digest('hex')!==row.traceHash)throw new Error('분석 실행 기록 파일이 변경되었습니다.');
        const trace=JSON.parse(text) as Trace;
        if(trace.run!==row.run||trace.seed!==row.seed||trace.state.contentFingerprint!==fingerprint||!Array.isArray(trace.route))throw new Error('경로 기록과 분석 작업이 일치하지 않습니다.');
        return trace;
      }));
      traces.forEach(trace=>tree.add(trace));
    }
    return tree;
  })();
  if(cache.size>=2)cache.delete(cache.keys().next().value!);cache.set(prefix,{rows,value});
  void value.catch(()=>{if(cache.get(prefix)?.value===value)cache.delete(prefix);});return value;
}

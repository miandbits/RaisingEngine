import {createHash} from 'node:crypto';
import {canonicalJson} from '../../dist/packages/project-service/snapshot.js';
import {historicalStep} from '../../dist/packages/analysis-service/goal-history.js';

const hash=v=>createHash('sha256').update(canonicalJson(v)).digest('hex');
function leaves(path,before,after){
  if(canonicalJson(before??null)===canonicalJson(after??null))return [];
  if(before&&after&&typeof before==='object'&&typeof after==='object'&&Array.isArray(before)===Array.isArray(after)&&(!Array.isArray(before)||before.length===after.length)){
    return [...new Set([...Object.keys(before),...Object.keys(after)])].flatMap(key=>leaves(path+'/'+key.replaceAll('~','~0').replaceAll('/','~1'),before[key],after[key]));
  }
  return [{path,before,after}];
}
export function goalChangeItems(pair){
  const groups=new Map(),labels=new Map((Array.isArray(pair.content.attributes)?pair.content.attributes:pair.content.attributes?.attributes??[]).map(a=>[a.id,a.label]));
  const context=hash({baseline:pair.baseline.checksum,route:pair.right.route,runtime:pair.runtime});
  function add(kind,path,before,after,step,label){
    const key=kind+':'+path;
    if(!groups.has(key))groups.set(key,{kind,path,label:label??path,firstStep:step,observations:[]});
    groups.get(key).observations.push({step,before:before??null,after:after??null});
    if(groups.size>2000)throw Error('부분 승인 변경 항목이 2000개를 넘습니다. 경로를 나누어 검사하세요.');
  }
  for(let i=0;i<Math.min(pair.left.steps.length,pair.right.steps.length);i++){
    const d=historicalStep(pair,i,i);
    for(const c of d.differences)add(/\/attributes\//.test(c.path)?'attribute':'state',c.path,c.before,c.after,i,labels.get(c.path.split('/').at(-1)));
    for(const c of d.history.relatedChanges)add(c.category==='비용'?'cost-rule':c.category==='효과'?'effect-rule':'rule',c.path,c.before,c.after,i);
    for(const c of d.history.observationChanges.flatMap(c=>leaves(c.path,c.before,c.after))){
      const match=c.path.match(/^\/checks\/(\d+)(?:\/|$)/),index=match?Number(match[1]):-1;
      const a=d.left.checks[index],b=d.right.checks[index];
      // Structural/kind changes are not accepted as ordinary numeric cost/effect changes.
      const kind=a?.kind===b?.kind&&a?.path===b?.path&&['cost','effect'].includes(a?.kind)?a.kind+'-check':'observation';
      const path=kind==='observation'?c.path:a.path+c.path.replace(/^\/checks\/\d+/,'');
      add(kind,path,c.before,c.after,i,path);
    }
  }
  return [...groups.values()].map(item=>({...item,context,id:hash({context,...item}),approvable:['attribute','cost-rule','effect-rule','cost-check','effect-check'].includes(item.kind)}));
}
export function reviewGoalItems(pair,accepted=[]){
  const items=goalChangeItems(pair),context=hash({baseline:pair.baseline.checksum,route:pair.right.route,runtime:pair.runtime});
  const key=i=>i.kind+':'+i.path,groups=new Map(items.map(i=>[key(i),{...i,approved:false,expectation:i.observations}]));
  const indexed=observations=>{const counts=new Map();return new Map(observations.map(o=>{const n=counts.get(o.step)??0;counts.set(o.step,n+1);return [o.step+':'+n,o];}));};
  for(const previous of accepted.filter(i=>i.context===context)){
    const current=groups.get(key(previous)),expected=indexed(previous.expectation??previous.observations),actual=indexed(current?.observations??[]),differences=[];
    for(const k of new Set([...expected.keys(),...actual.keys()])){
      const a=expected.get(k),b=actual.get(k),before=a?a.after:b.before,after=b?b.after:a.before;
      if(canonicalJson(before)!==canonicalJson(after))differences.push({step:(a??b).step,before,after});
    }
    differences.sort((a,b)=>a.step-b.step);
    if(!differences.length)groups.set(key(previous),{...previous,approved:true});
    else{const item={...previous,observations:differences,expectation:current?.observations??[],firstStep:differences[0].step,approved:false};item.id=hash({context,kind:item.kind,path:item.path,expected:[...expected],differences});groups.set(key(previous),item);}
  }
  return [...groups.values()];
}
export function applyGoalPartial(summary,changes){
  const remaining=changes.filter(i=>!i.approved);
  const blocked=summary.flags.goalFailed||summary.flags.executionFailed;
  const structural=summary.flags.routeChanged||summary.flags.runtimeChanged;
  // Success/route/runtime changes remain independent guardrails; accepting data never hides them.
  const status=blocked?'failed':structural||remaining.length?'changed':'unchanged';
  const points=remaining.map(i=>i.firstStep);
  if(structural||blocked)points.push(summary.firstDifference??0);
  const firstDifference=points.length?Math.min(...points):null;
  return {...summary,status,firstDifference,changeItems:changes,approvedChangeCount:changes.length-remaining.length,pendingChangeCount:remaining.length,
    flags:{...summary.flags,stateChanged:remaining.some(i=>['attribute','state'].includes(i.kind)),costChanged:remaining.some(i=>i.kind.startsWith('cost-')),effectsChanged:remaining.some(i=>i.kind.startsWith('effect-'))},
    valueChanges:remaining.filter(i=>['attribute','state'].includes(i.kind)&&/\/attributes\/|\/inventory\/|\/relationships\/|\/actionPoints(?:\/|$)/.test(i.path)).slice(0,40).map(i=>({...i.observations[0],path:i.path,label:i.label})),
    valueChangeCount:remaining.filter(i=>['attribute','state'].includes(i.kind)&&/\/attributes\/|\/inventory\/|\/relationships\/|\/actionPoints(?:\/|$)/.test(i.path)).length};
}

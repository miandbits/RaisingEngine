import {readFile,stat,appendFile} from 'node:fs/promises';
import type {RunSummary} from './routes.js';
export type StoredRun=Omit<RunSummary,'nodeIds'|'edgeIds'>&{encoding?:'dictionary-v1';nodeIds:(string|number)[];edgeIds:(string|number)[]};
type SymbolRow={kind:'node'|'edge';index:number;id:string};
/** Symbols are durable before any run index references them. Both logs only append. */
export class ArchiveWriter {
  private nodes=new Map<string,number>();private edges=new Map<string,number>();
  constructor(private prefix:string){}
  async append(summary:RunSummary){
    const symbols:SymbolRow[]=[];
    const encode=(kind:'node'|'edge',id:string)=>{const map=kind==='node'?this.nodes:this.edges;let index=map.get(id);if(index===undefined){index=map.size;map.set(id,index);symbols.push({kind,index,id});}return index;};
    const row:StoredRun={...summary,encoding:'dictionary-v1',nodeIds:summary.nodeIds.map(id=>encode('node',id)),edgeIds:summary.edgeIds.map(id=>encode('edge',id))};
    if(symbols.length)await appendFile(`${this.prefix}.symbols.jsonl`,symbols.map(row=>JSON.stringify(row)).join('\n')+'\n');
    await appendFile(`${this.prefix}.runs.jsonl`,JSON.stringify(row)+'\n');
  }
}
const cache=new Map<string,{size:number;mtime:number;rows:unknown[]}>();
async function lines(file:string):Promise<unknown[]>{
  let info;try{info=await stat(file);}catch(e){if((e as NodeJS.ErrnoException).code==='ENOENT')return [];throw e;}
  const hit=cache.get(file);if(hit?.size===info.size&&hit.mtime===info.mtimeMs)return hit.rows;
  const text=await readFile(file,'utf8'),rows:unknown[]=[];for(const line of text.split('\n'))if(line.trim())try{rows.push(JSON.parse(line));}catch{/* An in-progress final append is read on the next request. */}
  if(cache.size>24)cache.delete(cache.keys().next().value!);cache.set(file,{size:info.size,mtime:info.mtimeMs,rows});return rows;
}
export async function readArchive(prefix:string){
  const rows=await lines(`${prefix}.runs.jsonl`) as StoredRun[],symbols=await lines(`${prefix}.symbols.jsonl`) as SymbolRow[];
  const nodes:string[]=[],edges:string[]=[];for(const row of symbols)(row.kind==='node'?nodes:edges)[row.index]=row.id;
  return {rows,nodes,edges,decode:(row:StoredRun):RunSummary=>{
    const {encoding,...rest}=row;return {...rest,nodeIds:row.nodeIds.map(id=>typeof id==='number'?nodes[id]!:id),edgeIds:row.edgeIds.map(id=>typeof id==='number'?edges[id]!:id)};
  }};
}

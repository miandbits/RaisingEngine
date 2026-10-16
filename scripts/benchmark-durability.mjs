import assert from 'node:assert/strict';
import { mkdir, cp, mkdtemp, writeFile } from 'node:fs/promises';
import { resolve, join } from 'node:path';
import os from 'node:os';
import { StudioEditor } from '../dist/packages/studio-service/editor.js';
import { buildCatalog, searchCatalog, buildRelations } from '../dist/packages/analysis-service/catalog.js';
import { loadProject, buildSnapshot } from '../dist/packages/project-service/index.js';
import { searchGoalRoute } from '../dist/packages/analysis-service/goal-search.js';

const count = Number(process.env.DURABILITY_COUNT || 10000);
assert.ok(Number.isInteger(count) && count >= 100 && count <= 50000);
const output = resolve('.cache/durability'); await mkdir(output,{recursive:true});
const root = await mkdtemp(join(output,'project-'));
await cp(resolve('examples/academy-journey'),root,{recursive:true});
const editor = new StudioEditor(); await editor.open(root);
const docs=editor.view().documents, project=structuredClone(docs.find(d=>d.kind==='project').data);
const group=Math.floor(count/10), pad=i=>String(i).padStart(5,'0');
project.gameplay.items=Array.from({length:count-3*group},(_,i)=>({id:`durable-item-${pad(i)}`,name:`내구성 상품 ${pad(i)}`,maxStack:99})).concat(project.gameplay.items);
await writeFile(join(root,'project.json'),JSON.stringify(project));
// Synthetic load fixture, never represented as GUI-authored content.
for(const [kind,folder] of [['event','events'],['ending','endings'],['character','characters']]){
  const base=docs.find(d=>d.kind===kind).data;
  for(let i=0;i<group;i++){
    const data=structuredClone(base);data.id=`durable-${kind}-${pad(i)}`;
    if(kind==='character')data.name=`내구성 인물 ${pad(i)}`;
    else {data.label=`내구성 ${kind} ${pad(i)}`;data.requirements={op:'compare',comparator:'gte',left:{op:'read',target:{kind:'attribute',attributeId:'skill'}},right:{op:'literal',value:9999}};if(kind==='ending')data.priority=i%100;}
    await writeFile(join(root,folder,`${data.id}.json`),JSON.stringify(data));
  }
}
const report={version:1,at:new Date().toISOString(),node:process.version,platform:process.platform,cpu:os.cpus()[0]?.model,syntheticEntities:count,root,measurements:{},checks:{}};
const measure=async(name,runs,fn)=>{
  const times=[];let result;
  for(let i=0;i<runs;i++){const start=performance.now();result=await fn(i);times.push(performance.now()-start);}
  const sorted=[...times].sort((a,b)=>a-b);report.measurements[name]={runs,firstMs:times[0],medianMs:sorted[Math.floor(sorted.length/2)],p95Ms:sorted[Math.min(sorted.length-1,Math.ceil(sorted.length*.95)-1)],maxMs:sorted.at(-1)};return result;
};
await measure('open',3,()=>editor.open(root));
const validation=await measure('validate',3,()=>editor.validate(editor.view().revision));
assert.equal(validation.error,null);assert.deepEqual(editor.view().diagnostics.filter(d=>d.severity==='error').slice(0,3),[]);
let catalog=await measure('catalog',5,()=>buildCatalog(editor.view().documents,editor.view().diagnostics));
report.catalogEntries=catalog.length;
await measure('search',30,()=>{const rows=searchCatalog(catalog,{text:`내구성 상품 ${pad(count-3*group-1)}`});assert.equal(rows.length,1);});
await measure('sort',10,()=>searchCatalog(catalog,{sort:'label'}));
await measure('references',5,()=>buildRelations(catalog));
await measure('editUndo',5,i=>{const data=structuredClone(project);data.gameplay.items[0].name=`수정 ${i}`;const reply=editor.edit({type:'replace',file:'project.json',data},editor.view().revision);assert.equal(reply.error,null);assert.equal(editor.edit({type:'undo'},editor.view().revision).error,null);});
const broken=structuredClone(project);broken.gameplay.interactions[0].itemId='missing-durability-item';const invalid=editor.edit({type:'replace',file:'project.json',data:broken},editor.view().revision);assert.equal(invalid.error,null);
await editor.validate(editor.view().revision);
assert.ok(editor.view().diagnostics.some(d=>d.severity==='error'&&d.pointer==='/gameplay/interactions/0/itemId'));report.checks.invalidReference=true;
editor.edit({type:'undo'},editor.view().revision);
await measure('save',1,async()=>{const reply=await editor.save(editor.view().revision);assert.equal(reply.error,null);});
await measure('saveChanged',1,async()=>{
  const data=structuredClone(editor.view().documents.find(d=>d.kind==='project').data);data.gameplay.items[0].name='저장 후 재개방 확인';
  assert.equal(editor.edit({type:'replace',file:'project.json',data},editor.view().revision).error,null);
  assert.equal((await editor.save(editor.view().revision)).error,null);
});
await measure('reopenChanged',1,()=>editor.open(root));
assert.equal(editor.view().documents.find(d=>d.kind==='project').data.gameplay.items[0].name,'저장 후 재개방 확인');report.checks.changedSaveReopened=true;
const built=await measure('snapshot',1,async()=>buildSnapshot(await loadProject(root)));assert.ok(built.ok,JSON.stringify(built.diagnostics?.slice(0,3)));
const target={kind:'ending',id:built.snapshot.endings.find(e=>e.id==='ordinary')?.id??built.snapshot.endings.find(e=>!e.id.startsWith('durable-')).id};
const search=await measure('goalSearch',1,()=>searchGoalRoute(built.snapshot,{target,runs:1,maxStates:40,maxCommands:120,beamWidth:4,timeoutMs:2000}));
report.checks.goalSearch={status:search.status,exploredStates:search.exploredStates,message:search.message??null};
const cancelled=await measure('cancelSearch',1,()=>searchGoalRoute(built.snapshot,{target,timeoutMs:2000},{cancelled:()=>true}));assert.equal(cancelled.status,'cancelled');
report.memory=process.memoryUsage();
report.budgets={searchP95Ms:100,sortP95Ms:250,editUndoP95Ms:1500,validateP95Ms:10000,cancelSearchMs:250};
report.violations=Object.entries(report.budgets).filter(([name,max])=>report.measurements[name.replace(/P95Ms$|Ms$/,'')].p95Ms>max).map(([name,max])=>({name,max}));
const file=resolve(process.env.DURABILITY_REPORT||join(output,`report-${count}.json`));await writeFile(file,JSON.stringify(report,null,2));
console.log(JSON.stringify({report:file,entities:report.catalogEntries,measurements:report.measurements,violations:report.violations}));
if(report.violations.length)process.exitCode=1;

import test from 'node:test';
import assert from 'node:assert/strict';
import { cp, mkdir, mkdtemp, readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { StudioEditor } from '../../dist/packages/studio-service/editor.js';
import { buildCatalog, searchCatalog, buildRelations } from '../../dist/packages/analysis-service/catalog.js';
import { runAnalysis, replayTrace, impossibilityProof } from '../../dist/packages/analysis-service/runner.js';
import { AnalysisJobs } from '../../dist/packages/analysis-service/jobs.js';
import { traceScenario } from '../../dist/packages/analysis-service/scenario.js';
import { McpWorkspace } from '../../dist/packages/mcp-service/workspace.js';
const defaults={runs:4,seed:42,strategy:'random',maxCommands:2000,timeoutMs:60000};
async function fixture(year=false){const e=new StudioEditor();await e.open(resolve(`examples/academy-${year?'year':'demo'}`),true);const s=await e.snapshot(e.revision);assert.ok(s.ok);return {e,content:s.snapshot};}
async function temporary(){await mkdir('.cache/analysis-tests',{recursive:true});const root=await mkdtemp(resolve('.cache/analysis-tests/project-'));await cp('examples/academy-demo',root,{recursive:true});return root;}
test('individual catalog items, literal search, filter and 10000 items remain deterministic',async()=>{
 const {e}=await fixture();const catalog=buildCatalog(e.documents);assert.ok(catalog.some(i=>i.id==='skill'&&i.pointer==='/attributes/0'));
 const docs=Array.from({length:10000},(_,i)=>({kind:'ending',file:`endings/e-${i}.json`,data:{id:`ending-${i}`,label:`결말 ${i}`,description:'설명',tags:i%2?['졸업']:[]}}));
 const rows=buildCatalog(docs);assert.equal(searchCatalog(rows,{tag:'졸업'}).length,5000);assert.equal(searchCatalog(rows,{text:'ending-9999'})[0].id,'ending-9999');
});
test('relations distinguish effect writes from expression and condition reads',()=>{
 const items=buildCatalog([{kind:'attributes',file:'attributes.json',data:{attributes:[{id:'skill'}]}},{kind:'activity',file:'activities/a.json',data:{id:'a',baseEffects:[{op:'add',target:{kind:'attribute',attributeId:'skill'},value:{op:'read',target:{kind:'attribute',attributeId:'skill'}}}],requirements:{op:'not',condition:{op:'compare',left:{op:'read',target:{kind:'attribute',attributeId:'skill'}},right:{op:'literal',value:5}}}}}]);
 const relations=buildRelations(items);assert.ok(relations.some(r=>r.kind==='writes'&&r.pointer.includes('/target/attributeId')));assert.ok(relations.some(r=>r.kind==='reads'&&r.context.includes('/not')));
});
test('same seed commands outcomes coverage and replay are identical',async()=>{
 const {content}=await fixture();const a=await runAnalysis(content,defaults),b=await runAnalysis(content,defaults);assert.deepEqual(a,b);assert.equal(a.completed,4);assert.equal(a.outcomes.finished,4);
 for(const trace of a.traces)assert.deepEqual(replayTrace(content,trace).state,trace.state);
 const corrupted=structuredClone(a.traces[0]);corrupted.state.attributes.skill++;assert.throws(()=>replayTrace(content,corrupted),/최종 상태/);
});
test('12 month strategies complete without editing values',async()=>{
 const {content}=await fixture(true);for(const strategy of ['focus','conserve','explore']){const r=await runAnalysis(content,{...defaults,runs:1,strategy,attributeId:'skill'});assert.equal(r.completed,1);assert.equal(r.outcomes.finished,1,JSON.stringify(r.outcomes));assert.deepEqual(replayTrace(content,r.traces[0]).state,r.traces[0].state);}
});
test('target search proves reachability using real commands',async()=>{
 const {content}=await fixture();const r=await runAnalysis(content,{...defaults,runs:1,target:{kind:'ending',id:'certified'},beamWidth:20,maxStates:5000});assert.equal(r.targetStatus,'found');assert.equal(r.traces[0].state.endingId,'certified');replayTrace(content,r.traces[0]);
});
test('priority shadowed ending is not labeled impossible',async()=>{
 const {content}=await fixture();const c=structuredClone(content);c.endings.forEach(e=>{e.requirements={op:'always'};});const loser=[...c.endings].sort((a,b)=>b.priority-a.priority||a.id.localeCompare(b.id))[1];
 const r=await runAnalysis(c,{...defaults,runs:1,target:{kind:'ending',id:loser.id},maxStates:500,beamWidth:3});assert.equal(r.targetStatus,'not-found');assert.equal(r.proof,null);
});
test('only supported contradictory range has an impossibility proof, any/not remain sound',async()=>{
 const {content}=await fixture();const c=structuredClone(content);const ending=c.endings[0];const impossible={op:'compare',comparator:'gt',left:{op:'read',target:{kind:'attribute',attributeId:'skill'}},right:{op:'literal',value:100000}};
 ending.requirements=impossible;assert.ok(impossibilityProof(c,{kind:'ending',id:ending.id}));ending.requirements={op:'any',conditions:[impossible,{op:'always'}]};assert.equal(impossibilityProof(c,{kind:'ending',id:ending.id}),null);ending.requirements={op:'not',condition:impossible};assert.equal(impossibilityProof(c,{kind:'ending',id:ending.id}),null);
});
test('cancellation limits and malformed settings are explicit',async()=>{
 const {content}=await fixture();let cancel=false;const r=await runAnalysis(content,{...defaults,runs:100},{cancelled:()=>cancel,progress:x=>{if(x.completed)cancel=true;}});assert.equal(r.stopReason,'cancelled');assert.ok(r.completed>=1&&r.completed<100);
 const limited=await runAnalysis(content,{...defaults,runs:1,maxCommands:1});assert.equal(limited.completed,1);assert.notEqual(limited.outcomes.finished,1);
 await assert.rejects(()=>runAnalysis(content,{...defaults,runs:0}));
});
test('worker persists results shared between managers, cancellation and MCP conflict protection',async()=>{
 const root=await temporary();const editor=new StudioEditor();await editor.open(root);const snapshot=await editor.snapshot(editor.revision);const first=new AnalysisJobs(),second=new AnalysisJobs();
 let job=await first.start(root,snapshot.snapshot,editor.diskVersion(),editor.revision,{...defaults,runs:1000});assert.equal((await second.get(root,job.id)).status,'running');
 await second.cancel(root,job.id);for(let i=0;i<100&&job.status==='running';i++){await new Promise(r=>setTimeout(r,50));job=await second.get(root,job.id);}assert.equal(job.status,'cancelled');
 const mcp=new McpWorkspace(root);const version=(await mcp.readProject()).version;const doc=editor.documents.find(d=>d.kind==='activity');doc.data.label='changed';editor.edit({type:'replace',file:doc.file,data:doc.data},editor.revision);await editor.save(editor.revision);
 await assert.rejects(()=>mcp.startAnalysis(version,defaults),e=>e.code==='VERSION_CONFLICT');assert.equal((await mcp.analysisStatus(job.id)).stale,true);
 await assert.rejects(()=>new McpWorkspace(root,true).startAnalysis(editor.diskVersion(),defaults),e=>e.code==='READ_ONLY');
});
test('finished trace exports through scenario contract and protects existing documents',async()=>{
 const {content}=await fixture();const r=await runAnalysis(content,{...defaults,runs:1});const scenario=await traceScenario(content,r.traces[0],'auto-route');const root=await temporary();const e=new StudioEditor();await e.open(root);e.edit({type:'importScenario',id:'auto-route',data:scenario},e.revision);await e.save(e.revision);assert.equal(JSON.parse(await readFile(resolve(root,'scenarios/auto-route.json'),'utf8')).id,'auto-route');assert.throws(()=>e.edit({type:'importScenario',id:'auto-route',data:scenario},e.revision),/이미 존재/);
});
test('concurrent workers produce identical results and snapshot remains fixed after edits',async()=>{
 const root=await temporary();const e=new StudioEditor();await e.open(root);const snapshot=await e.snapshot(e.revision);const jobs=new AnalysisJobs();
 let a=await jobs.start(root,snapshot.snapshot,e.draftVersion(),e.revision,defaults),b=await jobs.start(root,snapshot.snapshot,e.draftVersion(),e.revision,defaults);
 const activity=structuredClone(e.documents.find(d=>d.kind==='activity'));activity.data.label='수정된 활동';e.edit({type:'replace',file:activity.file,data:activity.data},e.revision);assert.notEqual(e.draftVersion(),a.sourceVersion);
 for(let i=0;i<100&&(a.status==='running'||b.status==='running');i++){await new Promise(r=>setTimeout(r,30));a=await jobs.get(root,a.id);b=await jobs.get(root,b.id);}
 assert.equal(a.status,'completed');assert.equal(b.status,'completed');assert.deepEqual(a.result,b.result);assert.equal((await jobs.stored(root,a.id)).content.contentFingerprint,snapshot.snapshot.contentFingerprint);
});

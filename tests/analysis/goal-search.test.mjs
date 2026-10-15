import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdtemp,readFile,mkdir,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {listGoalRoutes} from '../../apps/qt-studio/goal-routes.mjs';
import {searchGoalRoute} from '../../dist/packages/analysis-service/goal-search.js';
import {runGoalRoute} from '../../dist/packages/analysis-service/goal-routes.js';

const project=path.resolve('examples/starlight-three-months'),evidence=path.resolve('docs/evidence/goal-search');
async function setup(){const root=await mkdtemp(path.resolve('.cache/goal-search-'));await cp(project,root,{recursive:true,filter:s=>!s.split(path.sep).includes('.studio')});const service=new QtStudioService();await service.handle({method:'open',params:{root}});const call=(method,params={})=>service.handle({method,params:{root:service.editor.root,revision:service.editor.revision,...params}});return {service,call,root};}
async function wait(call,id){for(let i=0;i<1300;i++){const r=await call('goalSearchStatus',{jobId:id});if(r.goalSearch.status!=='running')return r.goalSearch;await new Promise(r=>setTimeout(r,100));}throw Error('search timeout');}
test('target ending/event search, registration, save replay and bounded miss',{timeout:180000},async()=>{
 const {service,call,root}=await setup();const results=[],initialCount=(await listGoalRoutes(root)).length;
 for(const target of [{kind:'event',id:'month-01'},{kind:'ending',id:'wanderer'}]){
  const started=await call('goalSearchStart',{target,useSavedRoutes:false,options:{timeoutMs:30000}});assert.equal(started.error,null);
  const job=await wait(call,started.goalSearch.id);assert.equal(job.status,'found',JSON.stringify(job));assert.ok(job.result.report.saves>=2);
  const saved=await call('goalSearchRegister',{jobId:job.id,id:`auto-${target.id}`,name:`자동 ${target.id}`});assert.equal(saved.error,null);assert.equal(saved.goalSearchRegistered.report.ok,true);results.push(job);
 }
 const checked=await call('goalRoutesRun');assert.equal(checked.goalReport.ok,true);assert.equal(checked.goalReport.reports.length,initialCount+2);
 const guided=await call('goalSearchStart',{target:{kind:'ending',id:'knight'}});const known=await wait(call,guided.goalSearch.id);assert.equal(known.status,'found');assert.equal(known.result.report.target.id,'knight');assert.equal(known.progress.exploredStates,0);
 // A tiny command budget must not be reported as proof of impossibility.
 const limited=await call('goalSearchStart',{target:{kind:'ending',id:'knight'},options:{maxCommands:1,maxStates:1,timeoutMs:1000}});
 const miss=await wait(call,limited.goalSearch.id);assert.equal(miss.status,'not-found');assert.match(miss.result.message,/불가능 판정이 아닙니다/);
 const reopened=new QtStudioService();await reopened.handle({method:'open',params:{root}});assert.equal((await reopened.handle({method:'goalRoutesList',params:{revision:reopened.editor.revision}})).goalRoutes.length,initialCount+2);
 await mkdir(evidence,{recursive:true});await writeFile(path.join(evidence,'search-and-registration.json'),JSON.stringify({results,checked:checked.goalReport,miss,reopen:true},null,2));
});
test('balance regression repair preserves original; stale result, collision and cancellation rejected',{timeout:180000},async()=>{
 const {service,call,root}=await setup();const original=(await listGoalRoutes(root)).find(r=>r.route.id==='success');const originalBytes=await readFile(original.file,'utf8');
 const ending=service.editor.documents.find(d=>d.kind==='ending'&&d.data.id==='knight');const harder=structuredClone(ending.data);
 harder.requirements={op:'compare',comparator:'gte',left:{op:'read',target:{kind:'attribute',attributeId:'skill'}},right:{op:'literal',value:60}};
 await call('edit',{edit:{type:'replace',file:ending.file,data:harder}});
 const content=await service.debugSnapshot(service.editor.revision);const broken=await runGoalRoute(content,original.route);assert.equal(broken.ok,false);
 const started=await call('goalSearchStart',{sourceId:'success',sourceRevision:original.revision,options:{timeoutMs:45000}});assert.equal(started.error,null);
 const job=await wait(call,started.goalSearch.id);assert.equal(job.status,'found',JSON.stringify(job));
 await assert.rejects(call('goalSearchRegister',{jobId:job.id,id:'success',name:'overwrite'}),/다른 ID/);
 const saved=await call('goalSearchRegister',{jobId:job.id,id:'success-alternative',name:'균형 수정 후 대체 경로'});assert.equal(saved.goalSearchRegistered.report.ok,true);
 assert.equal(await readFile(original.file,'utf8'),originalBytes);assert.equal(saved.goalSearchRegistered.entry.route.sourceRouteId,'success');
 await assert.rejects(call('goalSearchRegister',{jobId:job.id,id:'success-alternative',name:'collision'}),/충돌/);
 await call('edit',{edit:{type:'replace',file:ending.file,data:ending.data}});
 await assert.rejects(call('goalSearchRegister',{jobId:job.id,id:'stale',name:'stale'}),/변경/);
 const cancel=await call('goalSearchStart',{target:{kind:'ending',id:'knight'},options:{timeoutMs:45000}});const stopped=await call('goalSearchStop',{jobId:cancel.goalSearch.id});assert.equal(stopped.goalSearch.status,'cancelled');
 await assert.rejects(call('goalSearchRegister',{jobId:cancel.goalSearch.id,id:'cancelled',name:'cancelled'}),/결과가 없습니다/);
 await mkdir(evidence,{recursive:true});await writeFile(path.join(evidence,'alternative-regression.json'),JSON.stringify({broken,job,saved:saved.goalSearchRegistered,originalPreserved:true,staleRejected:true,cancelled:true},null,2));
});
test('event target is observed at entry and invalid inputs do not become passing witnesses',async()=>{
 const {service}=await setup(),content=await service.debugSnapshot(service.editor.revision);
 await assert.rejects(searchGoalRoute(content,{target:{kind:'event',id:'missing'}}),/없습니다/);
 await assert.rejects(searchGoalRoute(content,{target:{kind:'choice',id:'month-01'}}),/선택/);
 const cancelled=await searchGoalRoute(content,{target:{kind:'event',id:'month-01'}},{cancelled:()=>true});assert.equal(cancelled.status,'cancelled');
});

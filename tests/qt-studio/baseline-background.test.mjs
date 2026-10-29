import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,mkdir,cp,readdir,readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {readGoalBaseline} from '../../apps/qt-studio/goal-baselines.mjs';
import {captureGoalBaseline} from '../../dist/packages/analysis-service/goal-history.js';
import {stageGoalBaseline,publishStagedGoalBaseline} from '../../apps/qt-studio/goal-baseline-publication.mjs';

async function fixture(t){
  await mkdir('.cache/baseline-background',{recursive:true});const root=await mkdtemp(path.resolve('.cache/baseline-background/run-'));
  await cp('examples/starlight-three-months',root,{recursive:true,filter:p=>!p.split(path.sep).some(x=>['.studio','baselines'].includes(x))});
  const service=new QtStudioService();t.after(async()=>{await service.goalRouteBatches.close();await service.editor.close();});
  await service.handle({method:'open',params:{root}});
  const call=(method,params={})=>service.handle({method,params:{root,revision:service.editor.revision,...params}});
  const entries=(await call('goalRoutesList')).goalRoutes;
  const options={ids:entries.map(e=>e.route.id),names:Object.fromEntries(entries.map(e=>[e.route.id,'Before change · '+e.route.name])),routeRevisions:Object.fromEntries(entries.map(e=>[e.route.id,e.revision]))};
  return {service,root,call,entries,options};
}
async function wait(call,id,predicate=j=>!['running','cancelling'].includes(j.status)){
  const until=Date.now()+30000;while(Date.now()<until){const job=(await call('goalBaselineBatchStatus',{jobId:id})).goalBaselineBatch;if(predicate(job))return job;await delay(20);}throw Error('Baseline batch did not finish');
}

test('registered routes create immutable named baselines in the background with bounded progress and reports',{timeout:45000},async t=>{
  const {service,root,call,entries,options}=await fixture(t),before=performance.now();
  const job=(await call('goalBaselineBatchStart',options)).goalBaselineBatch;
  assert.equal(job.mode,'baseline');assert.equal(job.status,'running');assert.ok(performance.now()-before<5000);
  assert.ok(service.goalRouteBatches.get(root,job.id).worker.spawnargs.includes('--max-old-space-size=1024'));
  assert.equal(job.sourceVersion,service.editor.draftVersion());
  const final=await wait(call,job.id);assert.equal(final.ok,true,JSON.stringify(final));assert.equal(final.status,'completed');assert.equal(final.stale,false);
  assert.equal(final.reports.length,entries.length);
  for(const row of final.reports){assert.equal(row.status,'passed');assert.equal(row.baseline.name,options.names[row.id]);const baseline=await readGoalBaseline(root,row.baseline.id);assert.equal(baseline.trace.reached,true);assert.equal(baseline.trace.unexecuted,0);assert.equal(baseline.content.contentFingerprint,job.contentFingerprint);assert.equal(baseline.trace.steps.length,baseline.trace.recordedDetails.length);}
  assert.equal((await readdir(path.join(root,'.studio/route-batches'))).filter(x=>x.endsWith('.pending')).length,0);
  assert.equal(JSON.parse(await readFile(final.file,'utf8')).reports.filter(r=>r.baseline).length,entries.length);
});

test('editing during capture rejects unfinished publication and cannot silently register old content',{timeout:30000},async t=>{
  const {service,root,call,options}=await fixture(t);
  const job=(await call('goalBaselineBatchStart',options)).goalBaselineBatch;
  const doc=service.editor.documents.find(d=>d.kind==='project');await call('edit',{edit:{type:'replace',file:doc.file,data:{...doc.data,description:'Edited while capturing'}}});
  const final=await wait(call,job.id);assert.equal(final.ok,false);assert.equal(final.stale,true);assert.equal(final.reports.some(row=>row.failure?.code==='STALE_INPUT'),true,JSON.stringify(final));
  const files=await readdir(path.join(root,'playtests/baselines')).catch(()=>[]);assert.equal(files.filter(x=>x.endsWith('.json')).length,0);
});

test('cancellation and project changes retain completed records and retire unfinished worker output',{timeout:30000},async t=>{
  const {service,root,call,entries}=await fixture(t),ordinary=entries.find(e=>e.route.id==='ordinary').route;
  const long={...ordinary,id:'z-long',name:'Long unfinished route',commands:Array.from({length:10000},()=>({type:'setSchedule',activityIds:['study','study','rest']})),checkpoints:[]};
  await call('goalRouteSave',{route:long,expectedRevision:null});
  const job=(await call('goalBaselineBatchStart',{ids:['ordinary','z-long'],names:{ordinary:'Completed first', 'z-long':'Unfinished second'}})).goalBaselineBatch;
  const partial=await wait(call,job.id,j=>j.reports[0].status==='passed');assert.ok(partial.reports[0].baseline);
  await call('goalBaselineBatchStop',{jobId:job.id});const final=await wait(call,job.id);assert.equal(final.ok,false);assert.equal(final.status,'cancelled');assert.equal(final.reports[0].status,'passed');assert.equal((await readGoalBaseline(root,final.reports[0].baseline.id)).name,'Completed first');
  assert.equal((await readdir(path.join(root,'.studio/route-batches'))).filter(x=>x.endsWith('.pending')).length,0);
  const next=(await call('goalBaselineBatchStart',{ids:['z-long'],names:{'z-long':'Switch witness'}})).goalBaselineBatch;
  await call('open',{root});assert.equal(service.goalRouteBatches.get(root,next.id).status,'cancelled');
});

test('publication rejects route changes, foreign IDs, names and baseline count overflow',{timeout:30000},async t=>{
  const {service,root,call,entries,options}=await fixture(t);
  await assert.rejects(call('goalBaselineBatchStart',{...options,names:{}}),/name/);
  await assert.rejects(call('goalBaselineBatchStart',{...options,ids:['missing']}));
  const job=(await call('goalBaselineBatchStart',options)).goalBaselineBatch;
  await writeFile(entries[0].file,(await readFile(entries[0].file,'utf8'))+'\n');
  const final=await wait(call,job.id);assert.equal(final.ok,false);assert.equal(final.routesChanged,true);assert.ok(final.reports.some(r=>r.failure?.code==='STALE_INPUT'));
  await assert.rejects(call('goalBaselineBatchStop',{root:path.dirname(root),jobId:job.id}),/current project/);
  const dir=path.join(root,'playtests/baselines');await mkdir(dir,{recursive:true});for(let i=0;i<128;i++)await writeFile(path.join(dir,'count-'+i+'.json'),'{}');
  await assert.rejects(call('goalBaselineBatchStart',options),/128/);
});

test('staged publication validates bytes, size, final guards and no-overwrite semantics',{timeout:15000},async t=>{
  const {service,root,entries}=await fixture(t),content=await service.debugSnapshot(service.editor.revision);
  const baseline=await captureGoalBaseline(content,entries[0].route,'Staged witness','test-runtime');
  const dir=path.join(root,'.studio/route-batches');await mkdir(dir,{recursive:true});const file=path.join(dir,'staged.pending');
  const metadata=await stageGoalBaseline(file,baseline);
  await assert.rejects(publishStagedGoalBaseline(root,file,{...metadata,bytes:64*1024*1024+1},async()=>{}),/metadata/);
  await assert.rejects(publishStagedGoalBaseline(root,file,metadata,async()=>{throw Error('stale guard');}),/stale guard/);
  const saved=await publishStagedGoalBaseline(root,file,metadata,async()=>{}),original=await readFile(saved.file,'utf8');
  await stageGoalBaseline(file,baseline);await assert.rejects(publishStagedGoalBaseline(root,file,metadata,async()=>{}),error=>error.code==='EEXIST');assert.equal(await readFile(saved.file,'utf8'),original);
  await writeFile(file,'tampered');await assert.rejects(publishStagedGoalBaseline(root,file,metadata,async()=>{}),/changed/);
  await assert.rejects(captureGoalBaseline(content,entries[0].route,'Cancelled','test-runtime',{cancelled:()=>true}),error=>error.code==='CANCELLED');
});

test('runtime mismatch and unreached routes cannot produce misleading baseline records',{timeout:15000},async t=>{
  const {service,root,call}=await fixture(t),content=await service.debugSnapshot(service.editor.revision);
  const prepared=await service.goalRouteBatches.prepare(root,{ids:['ordinary']});
  prepared.baseline={names:{ordinary:'Wrong runtime'},runtime:'cpp-sha256:other-runtime'};
  prepared.publishBaseline=()=>{throw Error('A mismatched runtime must never reach publication.');};
  const job=await service.goalRouteBatches.start(root,content,service.editor.draftVersion(),service.editor.revision,{ids:['ordinary']},async()=>{},prepared);
  const final=await wait(call,job.id);assert.equal(final.ok,false);assert.equal(final.reports[0].failure.code,'STALE_RUNTIME');
  const route={...prepared.entries[0].route,id:'unreached',name:'Unreached target',commands:[{type:'setSchedule',activityIds:['study','study','rest']}],checkpoints:[]};
  await call('goalRouteSave',{route,expectedRevision:null});
  const failed=(await call('goalBaselineBatchStart',{ids:['unreached'],names:{unreached:'Do not register'}})).goalBaselineBatch;
  const result=await wait(call,failed.id);assert.equal(result.ok,false);assert.equal(result.reports[0].failure.code,'TARGET_NOT_REACHED');
  assert.equal((await readdir(path.join(root,'playtests/baselines')).catch(()=>[])).filter(x=>x.endsWith('.json')).length,0);
});

test('expired batch history never removes registered baselines',{timeout:30000},async t=>{
  const {service,root,call}=await fixture(t);
  const first=(await call('goalBaselineBatchStart',{ids:['ordinary'],names:{ordinary:'Permanent comparison record'}})).goalBaselineBatch;
  const final=await wait(call,first.id);assert.equal(final.ok,true);const baseline=final.reports[0].baseline;
  const content=await service.debugSnapshot(service.editor.revision);
  for(let i=0;i<8;i++){const job=await service.goalRouteBatches.start(root,content,service.editor.draftVersion(),service.editor.revision,{ids:['ordinary']});await service.goalRouteBatches.get(root,job.id).completion;}
  assert.throws(()=>service.goalRouteBatches.get(root,first.id),/current project/);
  assert.equal((await readGoalBaseline(root,baseline.id)).name,'Permanent comparison record');
  assert.equal((await readdir(path.join(root,'.studio/route-batches'))).filter(x=>/^\d+-[a-f0-9-]{36}\.json$/.test(x)).length,8);
});

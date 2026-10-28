import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,cp,mkdir,readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {saveAndWait} from '../helpers/save-check.mjs';

async function fixture(t){
  await mkdir('.cache/save-background',{recursive:true});
  const root=await mkdtemp(path.resolve('.cache/save-background/run-'));
  await cp('examples/starlight-three-months',root,{recursive:true,filter:p=>!p.split(path.sep).some(x=>['.studio','baselines'].includes(x))});
  const service=new QtStudioService();
  t.after(async()=>{await service.goalRouteBatches.close();await service.editor.close();});
  await service.handle({method:'open',params:{root}});
  const call=(method,params={})=>service.handle({method,params:{root,revision:service.editor.revision,...params}});
  const ordinary=(await call('goalRoutesList')).goalRoutes.find(x=>x.route.id==='ordinary');
  const long={...ordinary.route,id:'a-long',name:'Long background witness',checkpoints:[],checkpointEvery:1,commands:Array.from({length:10000},()=>({type:'setSchedule',activityIds:['study','study','rest']}))};
  return {service,root,call,ordinary,long};
}
async function wait(call,predicate){const end=Date.now()+30000;while(Date.now()<end){const check=(await call('goalSaveCheckStatus')).goalSaveCheck;if(predicate(check))return check;await delay(20);}throw Error('Saved check wait expired');}

test('save returns while long work runs; edits stay responsive, repeated saves coalesce, captured inputs remain immutable',{timeout:60000},async t=>{
  const {service,root,call,ordinary,long}=await fixture(t);
  await call('goalRouteSave',{route:long,expectedRevision:null});
  const started=performance.now(),first=(await call('save')).goalSaveCheck;
  assert.ok(performance.now()-started<5000,'Save must return without replaying the long route.');
  assert.equal(service.editor.view().dirty,false);
  assert.ok(['queued','running'].includes(first.status));
  await wait(call,j=>j.status==='running');
  const repeat=(await call('save')).goalSaveCheck;assert.equal(repeat.id,first.id,'Same saved content keeps the useful active job.');
  const doc=service.editor.documents.find(d=>d.kind==='project'),data={...doc.data,description:'New saved content'};
  const editStart=performance.now();await call('edit',{edit:{type:'replace',file:doc.file,data}});assert.ok(performance.now()-editStart<5000);
  const second=(await call('save')).goalSaveCheck;assert.notEqual(second.id,first.id);
  const entry=(await call('goalRoutesList')).goalRoutes.find(x=>x.route.id==='a-long');
  await call('goalRouteSave',{route:{...ordinary.route,id:'a-long',name:'Replacement short route'},expectedRevision:entry.revision});
  const third=(await call('save')).goalSaveCheck;assert.notEqual(third.id,second.id);
  const captured=service.savedGoalChecks.latest;
  await call('edit',{edit:{type:'replace',file:doc.file,data:{...data,description:'Unsaved newer content'}}});
  const file=path.join(root,'playtests/routes/a-long.json');await writeFile(file,(await readFile(file,'utf8'))+'\n');
  const final=await wait(call,j=>!['queued','running','cancelling'].includes(j.status));
  assert.equal(final.id,third.id);assert.equal(final.status,'completed',JSON.stringify(final));assert.equal(final.ok,true);assert.equal(final.stale,true);assert.equal(final.routesChanged,true);
  assert.ok(final.goalImpact.counts['missing-baseline']>0);assert.equal(final.goalImpact.complete,false,'Missing baselines are not a comparison pass.');
  const input=JSON.parse(await readFile(final.file.replace(/\.json$/,'.inputs.json'),'utf8'));
  assert.equal(input.content.project.description,'New saved content');assert.equal(input.entries.find(e=>e.route.id==='a-long').route.name,'Replacement short route');
  assert.equal(captured.content,undefined,'Large snapshots are released after launch.');
  assert.ok([...service.goalRouteBatches.jobs.values()].some(j=>j.status==='cancelled'));
});

test('automatic save checks wait behind manual work; cancellation and close retire queued work',{timeout:30000},async t=>{
  const {service,call,long}=await fixture(t);await call('goalRouteSave',{route:long,expectedRevision:null});
  const manual=(await call('goalRouteBatchStart',{ids:['a-long'],limits:{routeTimeoutMs:10000,batchTimeoutMs:15000}})).goalRouteBatch;
  const saved=(await call('save')).goalSaveCheck;assert.equal(saved.status,'queued');
  assert.equal(service.goalRouteBatches.get(service.editor.root,manual.id).status,'running','Saving never cancels a manual batch.');
  await call('goalSaveCheckStop');assert.equal((await call('goalSaveCheckStatus')).goalSaveCheck.status,'cancelled');
  await call('save');await service.goalRouteBatches.close();
  assert.equal(service.savedGoalChecks.pending,null);assert.equal(service.savedGoalChecks.draining,null);
  assert.ok([...service.goalRouteBatches.jobs.values()].every(j=>!['running','cancelling'].includes(j.status)));
  assert.equal((await call('goalSaveCheckStatus')).goalSaveCheck.status,'cancelled');
});

test('baseline changes after queued save prevent a current comparison; saved project survives queue startup errors',{timeout:30000},async t=>{
  const {service,root,call,long}=await fixture(t);await call('goalRouteSave',{route:long,expectedRevision:null});
  const manual=(await call('goalRouteBatchStart',{ids:['a-long'],limits:{routeTimeoutMs:1000,batchTimeoutMs:1500}})).goalRouteBatch;
  // Keep only short routes in the automatic capture while the manual input stays immutable.
  const entry=(await call('goalRoutesList')).goalRoutes.find(x=>x.route.id==='a-long');
  const ordinary=(await call('goalRoutesList')).goalRoutes.find(x=>x.route.id==='ordinary');
  await call('goalRouteSave',{route:{...ordinary.route,id:'a-long'},expectedRevision:entry.revision});
  const saved=(await call('save')).goalSaveCheck;assert.equal(saved.status,'queued');
  await mkdir(path.join(root,'playtests/baselines'),{recursive:true});await writeFile(path.join(root,'playtests/baselines/.approvals'),JSON.stringify({version:1,active:{},history:[]}));
  await call('goalRouteBatchStop',{jobId:manual.id});
  const result=await wait(call,j=>!['queued','running','cancelling'].includes(j.status));
  assert.equal(result.stale,true);assert.equal(result.goalImpact.complete,false);assert.match(result.goalImpact.error,/Baseline records changed/);
  const original=service.goalRouteBatches.prepare;service.goalRouteBatches.prepare=async()=>{throw Error('Capture test failure');};
  const failure=await call('save');assert.equal(failure.error,null);assert.equal(failure.view.dirty,false);assert.equal(failure.goalSaveCheck.status,'failed');assert.match(failure.goalSaveCheck.error,/Project saved/);
  service.goalRouteBatches.prepare=original;
  await assert.rejects(call('goalSaveCheckStatus',{root:path.dirname(root)}),/current project/);
});

test('projects without registered routes save without starting a background worker',async t=>{
  const {service,call}=await fixture(t);service.goalRouteBatches.prepare=async root=>({root,entries:[]});
  const result=await call('save');assert.equal(result.error,null);assert.equal(result.goalSaveCheck,null);assert.equal(service.goalRouteBatches.jobs.size,0);assert.equal((await call('goalSaveCheckStatus')).goalSaveCheck,null);
});

test('cancellation during worker handoff retires the newly created automatic worker',{timeout:15000},async t=>{
  const {service,call,long}=await fixture(t);await call('goalRouteSave',{route:long,expectedRevision:null});
  const manager=service.goalRouteBatches,start=manager.start.bind(manager);
  let release,announced;const handoff=new Promise(resolve=>{announced=resolve;}),gate=new Promise(resolve=>{release=resolve;});
  manager.start=async(...args)=>{const job=await start(...args);announced(job);await gate;return job;};
  const saved=(await call('save')).goalSaveCheck;const worker=await handoff;
  assert.equal((await call('goalSaveCheckStop')).goalSaveCheck.status,'cancelled');
  release();await service.savedGoalChecks.draining;
  assert.equal(manager.get(service.editor.root,worker.id).status,'cancelled');
  assert.equal((await call('goalSaveCheckStatus')).goalSaveCheck.id,saved.id);
  assert.equal((await call('goalSaveCheckStatus')).goalSaveCheck.status,'cancelled');
});

test('expired retained history stays readable as stale and the next save creates a fresh check',{timeout:15000},async t=>{
  const {service,call}=await fixture(t);
  const first=(await saveAndWait(service)).goalSaveCheck;
  // Other manual batches can evict the older automatic entry from the bounded map.
  service.goalRouteBatches.jobs.delete(first.batchId);
  const expired=(await call('goalSaveCheckStatus')).goalSaveCheck;
  assert.equal(expired.id,first.id);assert.equal(expired.historyExpired,true);assert.equal(expired.stale,true);assert.match(expired.error,/retained batch history/);
  const next=(await saveAndWait(service)).goalSaveCheck;assert.notEqual(next.id,first.id);assert.equal(next.status,'completed');assert.equal(next.stale,false);
});

test('a delayed cancellation token cannot stop the automatic check from a newer save',{timeout:15000},async t=>{
  const {service,call,long}=await fixture(t);
  await call('goalRouteSave',{route:long,expectedRevision:null});
  await call('goalRouteBatchStart',{ids:['a-long'],limits:{routeTimeoutMs:10000,batchTimeoutMs:15000}});
  const old=(await call('save')).goalSaveCheck;
  const doc=service.editor.documents.find(d=>d.kind==='project');
  await call('edit',{edit:{type:'replace',file:doc.file,data:{...doc.data,description:'A newer saved revision'}}});
  const current=(await call('save')).goalSaveCheck;
  assert.notEqual(current.id,old.id);assert.equal(current.status,'queued');
  const delayed=(await call('goalSaveCheckStop',{checkId:old.id})).goalSaveCheck;
  assert.equal(delayed.id,current.id);assert.equal(delayed.status,'queued','Old UI cancellation must not own a replacement check.');
  assert.equal(service.savedGoalChecks.pending.id,current.id);
  const cancelled=(await call('goalSaveCheckStop',{checkId:current.id})).goalSaveCheck;
  assert.equal(cancelled.id,current.id);assert.equal(cancelled.status,'cancelled');
});

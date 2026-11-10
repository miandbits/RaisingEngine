import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,cp,mkdir,readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {saveAndWait} from '../helpers/save-check.mjs';
import {mkdirSync,rmdirSync} from 'node:fs';
import {randomUUID} from 'node:crypto';

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
async function wait(call,predicate){const end=Date.now()+30000;let check;while(Date.now()<end){check=(await call('goalSaveCheckStatus')).goalSaveCheck;if(predicate(check))return check;await delay(20);}throw Error('Saved check wait expired: '+JSON.stringify({id:check?.id,batchId:check?.batchId,status:check?.status,stage:check?.stage,stopReason:check?.stopReason,error:String(check?.error??'').slice(0,4096),completed:check?.completed}));}
async function assertStatus(check,expectedStatus,phase){
  if(check.status===expectedStatus)return;
  // Keep diagnostics in the already-uploaded public-fixture smoke directory.
  // Do not copy content snapshots or unbounded command/observation histories.
  const clip=value=>typeof value==='string'?value.slice(0,4096):value;
  const failure=value=>value?{code:value.code,step:value.step,message:clip(value.message),messageTruncated:typeof value.message==='string'&&value.message.length>4096}:undefined;
  const summary=value=>({id:value.id,batchId:value.batchId,status:value.status,ok:value.ok,stale:value.stale,error:clip(value.error),errorTruncated:typeof value.error==='string'&&value.error.length>4096,stopReason:value.stopReason,completed:value.completed,reports:value.reports?.slice(0,128).map(r=>({id:r.id,status:r.status,ok:r.ok,executed:r.executed,commands:r.commands,failure:failure(r.failure)})),comparison:value.goalImpact?{id:value.goalImpact.id,ok:value.goalImpact.ok,complete:value.goalImpact.complete,stale:value.goalImpact.stale,archived:value.goalImpact.archived,error:clip(value.goalImpact.error),archiveError:clip(value.goalImpact.archiveError),counts:value.goalImpact.counts}:null});
  const diagnostic={phase,expectedStatus,platform:process.platform,node:process.version,check:summary(check)};
  if(check.file)try{diagnostic.persisted=summary(JSON.parse(await readFile(check.file,'utf8')));}catch(error){diagnostic.readError=clip(error.message);}
  await mkdir('.cache/creator-smoke',{recursive:true});
  const file=path.resolve('.cache/creator-smoke/background-save-failure-'+randomUUID()+'.json');await writeFile(file,JSON.stringify(diagnostic,null,2));
  assert.equal(check.status,expectedStatus,JSON.stringify({...diagnostic,file}));
}
const assertCompleted=(check,phase)=>assertStatus(check,'completed',phase);

test('save returns while long work runs; edits stay responsive, repeated saves coalesce, captured inputs remain immutable',{timeout:60000},async t=>{
  const {service,root,call,ordinary,long}=await fixture(t);
  await call('goalRouteSave',{route:long,expectedRevision:null});
  const started=performance.now(),first=(await call('save')).goalSaveCheck;
  assert.ok(performance.now()-started<5000,'Save must return without replaying the long route.');
  assert.equal(service.editor.view().dirty,false);
  assert.ok(['queued','running'].includes(first.status));
  const running=await wait(call,j=>j.status==='running');
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
  await assertStatus(service.goalRouteBatches.public(service.goalRouteBatches.get(root,running.batchId)),'cancelled','A newer save retires the exact previously running automatic worker');
});

test('explicit cancellation retires the observed running automatic saved-check worker',{timeout:30000},async t=>{
  const {service,root,call,long}=await fixture(t);await call('goalRouteSave',{route:long,expectedRevision:null});
  await call('save');const running=await wait(call,j=>j.status==='running');
  await call('goalSaveCheckStop',{checkId:running.id});
  const final=await wait(call,j=>!['queued','running','cancelling'].includes(j.status));
  assert.equal(final.id,running.id);await assertStatus(final,'cancelled','Explicit stop of the currently running saved check');
  await assertStatus(service.goalRouteBatches.public(service.goalRouteBatches.get(root,running.batchId)),'cancelled','Exact owned worker after explicit automatic cancellation');
  assert.equal(final.ok,false);
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
  await assertStatus(manager.public(manager.get(service.editor.root,worker.id)),'cancelled','Automatic cancellation during worker handoff');
  assert.equal((await call('goalSaveCheckStatus')).goalSaveCheck.id,saved.id);
  assert.equal((await call('goalSaveCheckStatus')).goalSaveCheck.status,'cancelled');
});

test('expired retained history stays readable through twenty fresh saved-check replacements',{timeout:120000},async t=>{
  const {service,call}=await fixture(t);
  let previous=(await saveAndWait(service)).goalSaveCheck;
  await assertCompleted(previous,'initial saved check');
  // Each cycle must pass on its first attempt. Repeated owned-worker retirement
  // exercises the intermittent Windows failure without retrying failed checks.
  for(let cycle=1;cycle<=20;cycle++){
    // Other manual batches can evict the older automatic entry from the bounded map.
    service.goalRouteBatches.jobs.delete(previous.batchId);
    const expired=(await call('goalSaveCheckStatus')).goalSaveCheck;
    assert.equal(expired.id,previous.id);assert.equal(expired.historyExpired,true);assert.equal(expired.stale,true);assert.match(expired.error,/retained batch history/);
    const next=(await saveAndWait(service)).goalSaveCheck;assert.notEqual(next.id,previous.id);await assertCompleted(next,'replacement cycle '+cycle+' after retained-history expiry');assert.equal(next.stale,false);previous=next;
  }
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

test('archive failure remains visible after completion and identical save retries after recovery',{timeout:30000},async t=>{
  const {service,root,call}=await fixture(t);
  for(const entry of (await call('goalRoutesList')).goalRoutes)await call('goalBaselineSave',{routeId:entry.route.id,name:'Before editing · '+entry.route.name});
  const first=(await saveAndWait(service)).goalSaveCheck;assert.equal(first.goalImpact.ok,true);assert.equal(first.goalImpact.archived,true);
  const manager=service.goalRouteBatches,start=manager.start.bind(manager);manager.jobs.delete(first.batchId);
  let blockedFile;
  manager.start=async(...args)=>{
    const job=await start(...args);manager.start=start;
    manager.get(root,job.id).worker.prependListener('message',message=>{
      if(message.goalImpact?.id){blockedFile=path.join(root,'.studio','goal-impact-'+message.goalImpact.id+'.json');mkdirSync(blockedFile);}
    });return job;
  };
  const failed=(await saveAndWait(service)).goalSaveCheck;
  assert.equal(failed.status,'completed','Route completion remains separate from comparison archival.');
  assert.equal(failed.goalImpact.archived,false);assert.equal(failed.goalImpact.ok,false);assert.equal(failed.goalImpact.complete,false);assert.equal(failed.goalImpact.stale,true);assert.ok(failed.goalImpact.archiveError);
  const persisted=JSON.parse(await readFile(failed.file,'utf8'));assert.equal(persisted.goalImpact.archiveError,failed.goalImpact.archiveError);
  assert.equal((await readFile(failed.file,'utf8')).includes('\n  '),false,'Large enclosing batch reports use compact lossless JSON.');
  rmdirSync(blockedFile);
  const latest=(await call('goalImpactLatest')).goalImpact;assert.equal(latest.id,failed.goalImpact.id,'An older successful disk report must not hide the latest archive failure.');assert.equal(latest.ok,false);assert.ok(latest.archiveError);
  const retry=(await saveAndWait(service)).goalSaveCheck;assert.notEqual(retry.id,failed.id);assert.equal(retry.goalImpact.archived,true);assert.equal(retry.goalImpact.ok,true);
});

test('explicit bounded retry preserves the timed-out report and binds fresh saved routes and baselines',{timeout:60000},async t=>{
  const {service,root,call,ordinary,long}=await fixture(t);
  const baselines=[];for(const entry of (await call('goalRoutesList')).goalRoutes)baselines.push((await call('goalBaselineSave',{routeId:entry.route.id,name:'Original baseline · '+entry.route.name})).goalBaseline);
  await call('goalRouteSave',{route:long,expectedRevision:null});
  const started=(await call('goalSaveCheckRetry',{limits:{routeTimeoutMs:1000,batchTimeoutMs:30000}})).goalSaveCheck;
  assert.equal(started.origin,'retry');assert.deepEqual(started.limits,{routeTimeoutMs:1000,batchTimeoutMs:30000});
  const old=await wait(call,j=>!['queued','running','cancelling'].includes(j.status));assert.equal(old.status,'timed-out');
  const oldBytes=await readFile(old.file),oldInput=await readFile(old.file.replace(/\.json$/,'.inputs.json')),baselineBytes=await Promise.all(baselines.map(b=>readFile(b.file)));
  const entry=(await call('goalRoutesList')).goalRoutes.find(e=>e.route.id==='a-long');
  await call('goalRouteSave',{route:{...ordinary.route,id:'a-long',name:'Repaired short route'},expectedRevision:entry.revision});
  await call('goalBaselineSave',{routeId:'a-long',name:'New explicit fixture baseline'});
  const next=(await call('goalSaveCheckRetry',{checkId:old.id,limits:{routeTimeoutMs:10000,batchTimeoutMs:30000}})).goalSaveCheck;
  assert.notEqual(next.id,old.id);assert.equal(next.retryOf,old.id);assert.equal(next.origin,'retry');assert.equal(next.testedRevision,service.editor.revision);
  const final=await wait(call,j=>!['queued','running','cancelling'].includes(j.status));await assertCompleted(final,'Explicit retry after timed-out source');assert.equal(final.stale,false);assert.equal(final.goalImpact.complete,true);assert.equal(final.goalImpact.ok,true);assert.equal(final.goalImpact.archived,true);assert.equal(final.goalImpact.limits.routeTimeoutMs,10000);
  const captured=JSON.parse(await readFile(final.file.replace(/\.json$/,'.inputs.json'),'utf8'));assert.equal(captured.content.contentFingerprint,final.contentFingerprint);assert.equal(captured.entries.find(e=>e.route.id==='a-long').route.name,'Repaired short route');assert.notEqual(captured.impact.baselineStamp,JSON.parse(oldInput).impact.baselineStamp);
  assert.deepEqual(await readFile(old.file),oldBytes);assert.deepEqual(await readFile(old.file.replace(/\.json$/,'.inputs.json')),oldInput);for(let i=0;i<baselines.length;i++)assert.deepEqual(await readFile(baselines[i].file),baselineBytes[i]);
});

test('retry refuses dirty drafts, stale caller identity, bad limits and never silently saves',{timeout:30000},async t=>{
  const {service,root,call}=await fixture(t),doc=service.editor.documents.find(d=>d.kind==='project'),original=await readFile(path.join(root,doc.file));
  for(const limits of [{routeTimeoutMs:0},{routeTimeoutMs:300001},{batchTimeoutMs:3600001},{routeTimeoutMs:1001,batchTimeoutMs:1000},{routeTimeoutMs:1.5},{unknown:1}])await assert.rejects(call('goalSaveCheckRetry',{limits}));
  await assert.rejects(call('goalSaveCheckRetry',{checkId:'old-check'}),/older check/);
  await assert.rejects(call('goalSaveCheckRetry',{root:path.dirname(root)}),/current project/);
  await assert.rejects(call('goalSaveCheckRetry',{revision:service.editor.revision+1}));
  await call('edit',{edit:{type:'replace',file:doc.file,data:{...doc.data,description:'Not saved'}}});
  await assert.rejects(call('goalSaveCheckRetry'),/does not save automatically/);assert.equal(service.editor.view().dirty,true);assert.deepEqual(await readFile(path.join(root,doc.file)),original);assert.equal(service.goalRouteBatches.jobs.size,0);assert.equal(service.savedGoalChecks.latest,null);
});

test('retry works after a fresh editor process and observed cancellation retires its worker',{timeout:30000},async t=>{
  const {service,root,call,long}=await fixture(t);await call('goalRouteSave',{route:long,expectedRevision:null});
  await service.goalRouteBatches.close();await service.editor.close();
  const reopened=new QtStudioService();t.after(async()=>{await reopened.goalRouteBatches.close();await reopened.editor.close();});await reopened.handle({method:'open',params:{root}});
  const invoke=(method,params={})=>reopened.handle({method,params:{root,revision:reopened.editor.revision,...params}});
  assert.equal((await invoke('goalSaveCheckStatus')).goalSaveCheck,null);
  const retry=(await invoke('goalSaveCheckRetry',{limits:{routeTimeoutMs:10000,batchTimeoutMs:30000}})).goalSaveCheck;
  const running=await wait(invoke,j=>j.status==='running');assert.equal(running.id,retry.id);await assert.rejects(invoke('goalSaveCheckRetry'),/still running/);
  await invoke('goalSaveCheckStop',{checkId:retry.id});const final=await wait(invoke,j=>!['queued','running','cancelling'].includes(j.status));assert.equal(final.status,'cancelled');assert.equal(final.origin,'retry');assert.equal(final.ok,false);assert.equal(reopened.goalRouteBatches.get(root,final.batchId).worker,null);
});

test('two simultaneous retry admissions cannot replace each other after asynchronous guards',{timeout:30000},async t=>{
  const {service,root}=await fixture(t),content=await service.debugSnapshot(service.editor.revision);
  let entered=0,announce,release;const both=new Promise(r=>announce=r),gate=new Promise(r=>release=r),guard=async()=>{if(++entered===2)announce();await gate;};
  const retry=()=>service.savedGoalChecks.retry(root,content,service.editor.draftVersion(),service.editor.revision,{limits:{routeTimeoutMs:10000,batchTimeoutMs:30000}},guard);
  const a=retry(),b=retry();await both;release();const results=await Promise.allSettled([a,b]);assert.equal(results.filter(r=>r.status==='fulfilled').length,1);const rejected=results.find(r=>r.status==='rejected');assert.match(rejected.reason.message,/changed while preparing/);const accepted=results.find(r=>r.status==='fulfilled').value;assert.equal(service.savedGoalChecks.latest.id,accepted.id);await service.savedGoalChecks.draining;assert.equal(service.goalRouteBatches.jobs.size,1);
});

test('queued explicit retry keeps exact inputs and rejects changed baseline selection as stale',{timeout:30000},async t=>{
  const {service,root,call,ordinary,long}=await fixture(t);await call('goalRouteSave',{route:long,expectedRevision:null});
  const manual=(await call('goalRouteBatchStart',{ids:['a-long'],limits:{routeTimeoutMs:10000,batchTimeoutMs:15000}})).goalRouteBatch;
  const entry=(await call('goalRoutesList')).goalRoutes.find(e=>e.route.id==='a-long');await call('goalRouteSave',{route:{...ordinary.route,id:'a-long'},expectedRevision:entry.revision});
  const retry=(await call('goalSaveCheckRetry',{limits:{routeTimeoutMs:10000,batchTimeoutMs:30000}})).goalSaveCheck;assert.equal(retry.status,'queued');
  await mkdir(path.join(root,'playtests/baselines'),{recursive:true});await writeFile(path.join(root,'playtests/baselines/.approvals'),JSON.stringify({version:1,active:{},history:[]}));
  const doc=service.editor.documents.find(d=>d.kind==='project');await call('edit',{edit:{type:'replace',file:doc.file,data:{...doc.data,description:'Newer unsaved draft'}}});
  await call('goalRouteBatchStop',{jobId:manual.id});const final=await wait(call,j=>!['queued','running','cancelling'].includes(j.status));assert.equal(final.id,retry.id);assert.equal(final.stale,true);assert.equal(final.goalImpact.complete,false);assert.match(final.goalImpact.error,/Baseline records changed/);assert.notEqual(final.sourceVersion,service.editor.draftVersion());
  const captured=JSON.parse(await readFile(final.file.replace(/\.json$/,'.inputs.json'),'utf8'));assert.equal(captured.content.project.description,doc.data.description);assert.equal(service.editor.view().dirty,true);
});

test('project close during retry admission rejects the old epoch before any worker starts',{timeout:30000},async t=>{
  const {service,root}=await fixture(t),content=await service.debugSnapshot(service.editor.revision);
  let announce,release;const entered=new Promise(r=>announce=r),gate=new Promise(r=>release=r);
  const retry=service.savedGoalChecks.retry(root,content,service.editor.draftVersion(),service.editor.revision,{},async()=>{announce();await gate;});
  await entered;await service.goalRouteBatches.close();release();
  await assert.rejects(retry,/project changed while preparing/);assert.equal(service.savedGoalChecks.latest,null);assert.equal(service.savedGoalChecks.pending,null);assert.equal(service.goalRouteBatches.jobs.size,0);
});

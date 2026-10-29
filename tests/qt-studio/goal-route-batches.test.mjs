import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,cp,readFile,writeFile,readdir,symlink,mkdir} from 'node:fs/promises';
import {setTimeout as delay} from 'node:timers/promises';
import path from 'node:path';
import {spawnSync,fork} from 'node:child_process';
import {existsSync} from 'node:fs';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {readGoalRouteBytes} from '../../apps/qt-studio/goal-routes.mjs';
import {GoalRouteBatches,batchLimits} from '../../apps/qt-studio/goal-route-batches.mjs';
const fixturePath=path.resolve('examples/starlight-three-months');
async function fixture(t){await mkdir('.cache/route-batches',{recursive:true});const root=await mkdtemp(path.resolve('.cache/route-batches/run-'));await cp(fixturePath,root,{recursive:true,filter:s=>!s.split(path.sep).includes('.studio')});const s=new QtStudioService();t.after(async()=>{await s.goalRouteBatches.close();await s.editor.close();});await s.handle({method:'open',params:{root}});const call=(method,params={})=>s.handle({method,params:{revision:s.editor.revision,...params}});return {s,root,call};}
async function finished(call,root,id){const end=Date.now()+60000;while(Date.now()<end){const j=(await call('goalRouteBatchStatus',{root,jobId:id})).goalRouteBatch;if(!['running','cancelling'].includes(j.status))return j;await delay(30);}throw Error('Batch did not finish');}
test('manual background batch captures immutable draft/routes, completes saves, stays responsive and reports stale edits',{timeout:90000},async t=>{
 const {s,root,call}=await fixture(t),entries=(await call('goalRoutesList')).goalRoutes,routeRevisions=Object.fromEntries(entries.map(e=>[e.route.id,e.revision]));
 const start=performance.now(),job=(await call('goalRouteBatchStart',{routeRevisions})).goalRouteBatch;assert.equal(job.status,'running');assert.ok(performance.now()-start<5000);assert.equal(job.reports.length,entries.length);
 await assert.rejects(call('goalRouteBatchStart'),/already running/);
 const doc=s.editor.documents.find(d=>d.kind==='project'),data=structuredClone(doc.data);data.description+=' Changed while checking';await call('edit',{edit:{type:'replace',file:doc.file,data}});
 const changed={...entries[0].route,name:'Edited after capture'};await call('goalRouteSave',{route:changed,expectedRevision:entries[0].revision});
 const result=await finished(call,root,job.id);assert.equal(result.ok,true,JSON.stringify(result));assert.equal(result.status,'completed');assert.equal(result.stale,true);assert.equal(result.routesChanged,true);assert.ok(result.reports.every(r=>r.status==='passed'&&r.saves>=2));assert.equal(result.reports[0].name,entries[0].route.name);
 const input=JSON.parse(await readFile(result.file.replace(/\.json$/,'.inputs.json'),'utf8'));assert.equal(input.content.project.description,doc.data.description);assert.equal(input.entries[0].route.name,entries[0].route.name);
 await s.goalRouteBatches.get(root,job.id).persistence;const archived=JSON.parse(await readFile(result.file,'utf8'));assert.equal(archived.contentFingerprint,job.contentFingerprint);assert.deepEqual(archived.reports,JSON.parse(JSON.stringify(result.reports)));
 // Compatibility: the old synchronous route method still returns the original report envelope.
 const legacy=(await call('goalRoutesRun',{ids:[entries[1].route.id]})).goalReport;assert.equal(legacy.ok,true);assert.equal(legacy.version,1);
});
test('cancellation and project replacement preserve completed/not-run distinctions without a false pass',{timeout:60000},async t=>{
 const {s,root,call}=await fixture(t),job=(await call('goalRouteBatchStart')).goalRouteBatch;
 const stopped=(await call('goalRouteBatchStop',{root,jobId:job.id})).goalRouteBatch;assert.ok(['cancelling','cancelled'].includes(stopped.status));const final=await finished(call,root,job.id);assert.equal(final.status,'cancelled');assert.equal(final.ok,false);assert.ok(final.reports.some(r=>r.status==='not-run'));assert.ok(final.reports.every(r=>['passed','cancelled','not-run'].includes(r.status)));
 const next=(await call('goalRouteBatchStart')).goalRouteBatch;await call('open',{root});const old=s.goalRouteBatches.get(root,next.id);assert.equal(old.status,'cancelled');assert.equal(old.ok,false);
});
test('IDs, expected revisions, limits and project roots fail closed; external route changes mark stale',{timeout:60000},async t=>{
 const {s,root,call}=await fixture(t),entries=(await call('goalRoutesList')).goalRoutes;
 for(const ids of [['../outside'],['ordinary','ordinary'],['missing'],'ordinary'])await assert.rejects(call('goalRouteBatchStart',{ids}));
 await assert.rejects(call('goalRouteBatchStart',{ids:['ordinary'],routeRevisions:{ordinary:'stale'}}),/Routes changed/);
 for(const limits of [{routeTimeoutMs:0},{batchTimeoutMs:3600001},{routeTimeoutMs:1.5},{unknown:1}])assert.throws(()=>batchLimits(limits));
 const job=(await call('goalRouteBatchStart',{ids:['ordinary']})).goalRouteBatch;
 await assert.rejects(call('goalRouteBatchStatus',{root:path.dirname(root),jobId:job.id}),/current project/);await assert.rejects(call('goalRouteBatchStop',{root,jobId:'../invalid'}),/current project/);
 const original=await readFile(entries.find(e=>e.route.id==='ordinary').file,'utf8');await writeFile(entries.find(e=>e.route.id==='ordinary').file,original+'\n');const result=await finished(call,root,job.id);assert.equal(result.ok,true);assert.equal(result.stale,true);assert.equal(result.routesChanged,true);
});
test('a real per-route timeout is distinct from failure and later routes can still run',{timeout:60000},async t=>{
 const {s,root,call}=await fixture(t);const entries=(await call('goalRoutesList')).goalRoutes,route=structuredClone(entries.find(e=>e.route.id==='success').route);
 // Repeated legal draft commands create a long canonical route; each checkpoint replays the growing history.
 route.id='a-long';route.name='Timeout witness';route.checkpointEvery=1;route.checkpoints=[];
 // Replacing a draft schedule is an accepted command in this fixture's starting planning phase.
 route.commands=Array.from({length:10000},()=>({type:'setSchedule',activityIds:['study','study','rest']}));
 await call('goalRouteSave',{route,expectedRevision:null});
 const result=await finished(call,root,(await call('goalRouteBatchStart',{ids:['a-long','ordinary'],limits:{routeTimeoutMs:1000,batchTimeoutMs:30000}})).goalRouteBatch.id);
 assert.equal(result.reports[0].status,'timed-out',JSON.stringify(result));assert.equal(result.reports[0].failure.code,'TIMEOUT');assert.equal(result.reports[1].status,'passed');assert.equal(result.status,'timed-out');assert.equal(result.ok,false);
});
test('batch-wide deadline leaves unstarted routes not-run and linked storage is refused',{timeout:60000},async t=>{
 const {s,root,call}=await fixture(t);const entries=(await call('goalRoutesList')).goalRoutes;const long={...entries.find(e=>e.route.id==='success').route,id:'a-long',name:'Deadline witness',commands:Array.from({length:10000},()=>({type:'setSchedule',activityIds:['study','study','rest']})),checkpointEvery:1,checkpoints:[]};await call('goalRouteSave',{route:long,expectedRevision:null});
 const result=await finished(call,root,(await call('goalRouteBatchStart',{limits:{routeTimeoutMs:60000,batchTimeoutMs:1000}})).goalRouteBatch.id);assert.equal(result.status,'timed-out');assert.equal(result.ok,false);assert.ok(result.reports.some(r=>r.status==='not-run'));assert.ok(result.reports.some(r=>['passed','timed-out'].includes(r.status)));
 const external=await mkdtemp(path.resolve('.cache/route-batches/external-'));const linked=await mkdtemp(path.resolve('.cache/route-batches/linked-'));await cp(fixturePath,linked,{recursive:true,filter:p=>!p.split(path.sep).includes('.studio')});await symlink(external,path.join(linked,'.studio'),'dir');const content=await s.debugSnapshot(s.editor.revision);await assert.rejects(new GoalRouteBatches().start(linked,content,'test',1),/inside the project/);assert.deepEqual(await readdir(external),[]);
});

test('invalid and wrong-target routes are failures; prior successful routes stay passed',{timeout:60000},async t=>{
 const {s,root,call}=await fixture(t),entries=(await call('goalRoutesList')).goalRoutes,ordinary=entries.find(e=>e.route.id==='ordinary');await call('goalRouteSave',{route:{...ordinary.route,id:'wrong-target',endingId:'knight'},expectedRevision:null});await writeFile(path.join(root,'playtests/routes/broken.json'),'{ broken');
 const result=await finished(call,root,(await call('goalRouteBatchStart',{ids:['broken','ordinary','wrong-target']})).goalRouteBatch.id);assert.equal(result.status,'failed');assert.equal(result.ok,false);assert.deepEqual(result.reports.map(r=>r.status),['failed','passed','failed']);assert.equal(result.reports[0].failure.code,'INVALID_ROUTE');assert.equal(result.reports[2].failure.code,'TARGET_NOT_REACHED');
});
test('capture rejects a concurrently changed route and cleans its unstarted input; bounded history retains eight batches',{timeout:60000},async t=>{
 const {s,root}=await fixture(t),content=await s.debugSnapshot(s.editor.revision),manager=s.goalRouteBatches;let guards=0;
 await assert.rejects(manager.start(root,content,s.editor.draftVersion(),s.editor.revision,{ids:['ordinary']},async()=>{if(++guards===2){const file=path.join(root,'playtests/routes/ordinary.json');await writeFile(file,(await readFile(file,'utf8'))+'\n');}}),/Routes changed while preparing/);
 assert.deepEqual(await readdir(path.join(root,'.studio/route-batches')),[]);
 const ordinary=JSON.parse(await readFile(path.join(root,'playtests/routes/ordinary.json'),'utf8'));
 for(const id of ['large-a','large-b'])await writeFile(path.join(root,'playtests/routes',id+'.json'),JSON.stringify({...ordinary,id,commands:[{type:'unknown-command',note:'x'.repeat(2200000)}],checkpointEvery:1,checkpoints:[]}));
 for(let n=0;n<9;n++){const job=await manager.start(root,content,'same',1,{ids:['large-a','large-b']});await manager.get(root,job.id).completion;const result=manager.get(root,job.id);assert.equal(result.ok,false);assert.ok(result.reports.every(r=>r.failure.code==='INVALID_COMMAND'));const report=await readFile(result.file);assert.ok(report.length>4*1024*1024,'Large valid failure reports must participate in the eight-batch retention limit.');assert.equal(JSON.parse(report).reports[0].failure.command.note.length,2200000,'Failure evidence remains complete.');}
 const files=await readdir(path.join(root,'.studio/route-batches'));assert.equal(files.filter(f=>f.endsWith('.inputs.json')).length,8);assert.equal(files.filter(f=>!f.endsWith('.inputs.json')).length,8);assert.equal(manager.jobs.size,8);
});


test('hard deadlines and cancellation retire only the owned stalled native process tree',{timeout:60000},async t=>{
 const {s,root}=await fixture(t),content=await s.debugSnapshot(s.editor.revision),out=path.dirname(root),source=path.join(root,'stalled-native.cpp'),exe=path.join(root,process.platform==='win32'?'stalled-native.exe':'stalled-native');
 await writeFile(source,String.raw`#include <fstream>
#include <cstdlib>
#include <thread>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
int main(){
#ifdef _WIN32
 const auto pid=GetCurrentProcessId();
#else
 const auto pid=getpid();
#endif
 std::ofstream out(std::getenv("RAISING_BATCH_TEST_PID"));out<<pid;out.close();
 // Deliberately ignore stdin closure and never return a native protocol reply.
 for(;;)std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
`);
 const mingw=path.join(process.env.QT_COMPILER??'C:/Program Files/mingw64/bin','g++.exe'),compiler=process.env.CXX??(process.platform==='win32'?(existsSync(mingw)?mingw:'g++'):'c++');
 const built=spawnSync(compiler,['-std=c++17','-O1',source,'-o',exe],{encoding:'utf8',timeout:30000});assert.equal(built.status,0,built.stderr||String(built.error));
 const keys=['RAISING_NATIVE_EXECUTABLE','RAISING_NATIVE_TRANSPORT','RAISING_BATCH_TEST_PID'],original=Object.fromEntries(keys.map(k=>[k,process.env[k]])),pids=[],results=[];
 const alive=pid=>{try{process.kill(pid,0);return true;}catch(error){if(error.code==='ESRCH')return false;throw error;}};
 t.after(()=>{for(const key of keys)if(original[key]===undefined)delete process.env[key];else process.env[key]=original[key];for(const pid of pids)if(alive(pid))process.kill(pid,'SIGKILL');});
 for(const mode of ['deadline','cancel','disconnect','deadline']){
  process.env.RAISING_NATIVE_EXECUTABLE=exe;process.env.RAISING_NATIVE_TRANSPORT='process';process.env.RAISING_BATCH_TEST_PID=path.join(root,'native-'+results.length+'.pid');
  const manager=new GoalRouteBatches();t.after(()=>manager.close());const start=Date.now(),job=await manager.start(root,content,'fixture',1,{ids:['ordinary'],limits:{routeTimeoutMs:1000,batchTimeoutMs:10000}});
  const born=Date.now()+5000;while(!existsSync(process.env.RAISING_BATCH_TEST_PID)&&Date.now()<born)await delay(20);
  const pid=Number(await readFile(process.env.RAISING_BATCH_TEST_PID,'utf8'));assert.ok(Number.isSafeInteger(pid)&&pid>0);pids.push(pid);assert.equal(alive(pid),true);
  if(mode==='cancel')manager.stop(root,job.id);if(mode==='disconnect')manager.get(root,job.id).worker.disconnect();
  await manager.get(root,job.id).completion;const final=await manager.status(root,job.id,'fixture');
  const reaped=Date.now()+3000;while(alive(pid)&&Date.now()<reaped)await delay(20);
  assert.equal(alive(pid),false,'The stalled native descendant must be gone before the batch can be reused.');assert.equal(final.ok,false);assert.equal(final.status,mode==='cancel'?'cancelled':mode==='disconnect'?'failed':'timed-out');assert.ok(Date.now()-start<10000);
  const saved=JSON.parse(await readFile(final.file,'utf8'));assert.equal(saved.status,final.status);assert.equal(saved.ok,false);results.push({mode,pid,status:final.status,elapsedMs:Date.now()-start});
 }
 await writeFile(path.join(root,'owned-process-cleanup.json'),JSON.stringify({platform:process.platform,results},null,2));
});

test('supervisor remains owned and alive after replay-worker failure until the parent retires it',{timeout:15000},async t=>{
 const child=fork(new URL('../../apps/qt-studio/goal-route-batch-worker.mjs',import.meta.url),[],{execArgv:[],stdio:['ignore','ignore','ignore','ipc'],serialization:'advanced',windowsHide:true});
 t.after(()=>{if(child.exitCode===null&&child.signalCode===null)child.kill('SIGKILL');});
 // Invalid internal input makes the replay worker throw outside its route catch.
 // The supervising process must not exit and orphan its process tree on Windows.
 const fatal=new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(Error('No worker failure reported')),5000);child.on('message',m=>{if(m.fatal){clearTimeout(timer);resolve(m.fatal);}});child.on('error',reject);});
 child.send({content:{},limits:{routeTimeoutMs:1000,batchTimeoutMs:1000}});assert.match(await fatal,/length/);await delay(100);assert.equal(child.exitCode,null);assert.equal(child.signalCode,null);assert.equal(child.connected,true);child.kill('SIGKILL');await new Promise(resolve=>child.once('exit',resolve));
});

test('batch reads enforce their byte bound and reject directories without changing legacy list behavior',async()=>{
 const root=await mkdtemp(path.resolve('.cache/route-batches/bounded-')),file=path.join(root,'input.json');await writeFile(file,'12345678');assert.equal((await readGoalRouteBytes(file,8)).toString(),'12345678');await assert.rejects(readGoalRouteBytes(file,7),/byte limit/);await assert.rejects(readGoalRouteBytes(root,8),error=>['EISDIR','EPERM'].includes(error.code)||/regular file/.test(error.message));
});

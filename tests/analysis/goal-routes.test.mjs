import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdtemp,cp,mkdir} from 'node:fs/promises';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {listGoalRoutes,saveGoalRoute} from '../../apps/qt-studio/goal-routes.mjs';
import {routeFromRecording,runGoalRoute,validateGoalRoute} from '../../dist/packages/analysis-service/goal-routes.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {loadProject} from '../../dist/packages/project-service/loader.js';

const project=path.resolve('examples/starlight-three-months'),evidence=path.resolve('.cache/goal-routes-service');
const names={success:'별빛 길드의 동료',relationship:'마을에 남은 약속',ordinary:'다시 시작하는 계절'};
// Synthetic recording envelopes from committed route commands: no private GUI evidence required.
async function recording(kind){const route=JSON.parse(await readFile(path.join(project,'playtests/routes',kind+'.json'),'utf8'));return {recordingVersion:1,gameId:route.gameId,contentFingerprint:route.recordedFingerprint,origin:route.origin,commands:route.commands,checkpoints:route.checkpoints,state:{endingId:route.endingId}};}

test('committed pilot routes: recording import, latest content replay, every save boundary and autosave regression',{timeout:180000},async()=>{
 const root=await mkdtemp(path.resolve('.cache/goal-regression-'));await cp(project,root,{recursive:true,filter:s=>!s.split(path.sep).includes('.studio')&&!s.split(path.sep).includes('playtests')});
 const service=new QtStudioService();await service.handle({method:'open',params:{root}});
 const call=(method,params={})=>service.handle({method,params:{revision:service.editor.revision,...params}});
 for(const kind of Object.keys(names)){
   await mkdir(evidence,{recursive:true});await writeFile(path.join(evidence,(kind==='success'?'play':kind)+'-recording.json'),JSON.stringify(await recording(kind)));
   const imported=await call('goalRouteImport',{file:path.join(evidence,(kind==='success'?'play':kind)+'-recording.json'),metadata:{id:kind,name:names[kind],checkpointEvery:7}});
   assert.ok(imported.goalRoute.commands.length>40);await call('goalRouteSave',{route:imported.goalRoute,expectedRevision:null});
 }
 const first=(await call('goalRoutesRun')).goalReport;assert.equal(first.ok,true,JSON.stringify(first));assert.equal(first.reports.length,3);assert.ok(first.reports.every(r=>r.saves>7));
 // A harmless content edit changes the fingerprint; stored commands are re-run, not invalidated.
 const manifest=service.editor.documents.find(d=>d.file==='project.json');const data=structuredClone(manifest.data);data.description+=' 회귀 검사 문구 변경';
 await call('edit',{edit:{type:'replace',file:'project.json',data}});
 const afterEdit=await call('save');assert.equal(afterEdit.goalReport.ok,true);assert.ok(afterEdit.goalReport.reports.every(r=>r.contentChanged));
 // A legal balance edit keeps commands valid but changes the achieved ending.
 const ending=service.editor.documents.find(d=>d.kind==='ending'&&d.data.id==='knight');const harder=structuredClone(ending.data);
 harder.requirements={op:'compare',comparator:'gte',left:{op:'read',target:{kind:'attribute',attributeId:'skill'}},right:{op:'literal',value:80}};
 await call('edit',{edit:{type:'replace',file:ending.file,data:harder}});
 const changed=await call('save');assert.equal(changed.error,null);assert.equal(changed.goalReport.ok,false);const failed=changed.goalReport.reports.find(r=>r.id==='success');assert.equal(failed.failure.code,'TARGET_NOT_REACHED');assert.equal(failed.failure.step,failed.commands);assert.equal(failed.actualEnding,'neighbor');
 assert.equal(changed.goalReport.reports.filter(r=>r.ok).length,2);
 await call('edit',{edit:{type:'replace',file:ending.file,data:ending.data}});assert.equal((await call('save')).goalReport.ok,true);
 // A removed referenced command fails at its exact index, and the stored witness survives.
 const routes=await listGoalRoutes(root),original=routes.find(e=>e.route.id==='success');const invalid=structuredClone(original.route);invalid.commands[4]={type:'executeAction',actionId:'deleted-action',quantity:1};
 const snapshot=(await buildSnapshot(await loadProject(root))).snapshot;const failure=await runGoalRoute(snapshot,invalid);assert.equal(failure.ok,false);assert.equal(failure.failure.step,5);assert.equal(failure.executed,4);
 const changedRoute={...original.route,name:'새 이름'};await saveGoalRoute(root,changedRoute,original.revision);await assert.rejects(saveGoalRoute(root,{...original.route,name:'덮어쓰기'},original.revision),/버전 충돌/);assert.equal((await listGoalRoutes(root)).find(e=>e.route.id==='success').route.name,'새 이름');
 const restarted=new QtStudioService();await restarted.handle({method:'open',params:{root}});assert.equal((await restarted.handle({method:'goalRoutesList',params:{revision:restarted.editor.revision}})).goalRoutes.length,3);
 const cancelled=await runGoalRoute(snapshot,original.route,{cancelled:()=>true});assert.equal(cancelled.status,'cancelled');assert.equal(cancelled.ok,false);
 await mkdir(evidence,{recursive:true});await writeFile(path.join(evidence,'service-regression.json'),JSON.stringify({ok:true,initial:first,harmlessEdit:afterEdit.goalReport,balanceRegression:changed.goalReport,commandRegression:failure,conflictPreserved:true,reopen:true,cancelled},null,2));
});
test('route import rejects malformed origin, traversal IDs, oversized/invalid checkpoints and foreign game',async()=>{
 const record=await recording('ordinary');const route=routeFromRecording(record,{id:'ordinary',name:names.ordinary});
 assert.throws(()=>validateGoalRoute({...route,id:'../escape'}));assert.throws(()=>validateGoalRoute({...route,checkpoints:[route.commands.length+1]}));assert.throws(()=>validateGoalRoute({...route,origin:{...route.origin,seed:-1}}));assert.throws(()=>validateGoalRoute({...route,checkpointEvery:0}));assert.throws(()=>routeFromRecording({},{id:'bad',name:'bad'}));
 const built=await buildSnapshot(await loadProject(project));assert.equal((await runGoalRoute(built.snapshot,{...route,gameId:'foreign'})).failure.code,'GAME_MISMATCH');assert.equal((await runGoalRoute(built.snapshot,{...route,endingId:'missing'})).failure.code,'UNKNOWN_ENDING');
});
test('recording capability is explicit; older runtimes fail before launching',async()=>{
 await mkdir('.cache/goal-routes-service',{recursive:true});const runtimes=await mkdtemp(path.resolve('.cache/goal-routes-service/runtime-'));
 for(const [name,toolFeatures] of [['supported',['goal-route-recording-v1']],['unsupported',[]]]){await mkdir(path.join(runtimes,name));await writeFile(path.join(runtimes,name,'capabilities.json'),JSON.stringify({toolFeatures}));}
 const supported=new QtStudioService({nativeTarget:'windows',runtimes:{windows:path.join(runtimes,'supported')}});
 const unsupported=new QtStudioService({nativeTarget:'windows',runtimes:{windows:path.join(runtimes,'unsupported')}});
 assert.equal((await supported.handle({method:'goalRecordPrepare'})).recordingSupported,true);
 await assert.rejects(unsupported.handle({method:'goalRecordPrepare'}),/기록을 지원하지/);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir,mkdtemp,cp} from 'node:fs/promises';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {saveGoalRoute} from '../../apps/qt-studio/goal-routes.mjs';
import {readGoalBaseline,saveGoalBaseline} from '../../apps/qt-studio/goal-baselines.mjs';
import {captureGoalBaseline,compareGoalBaseline,historicalStep} from '../../dist/packages/analysis-service/goal-history.js';

async function setup(){const root=await mkdtemp(path.resolve('.cache/goal-history-'));await cp(path.resolve('examples/starlight-three-months'),root,{recursive:true,filter:s=>!s.split(path.sep).includes('.studio')});const service=new QtStudioService();await service.handle({method:'open',params:{root}});const call=(method,params={})=>service.handle({method,params:{revision:service.editor.revision,root,...params}});return {root,service,call};}
test('persisted historical states survive restart; same commands pinpoint costs, effects and conditions',{timeout:120000},async()=>{
 const {root,service,call}=await setup();const ordinary=JSON.parse(await readFile(path.join(root,'playtests/routes/ordinary.json'),'utf8'));
 const route={...ordinary,id:'history-meal',name:'생활과 세 달의 기록',checkpoints:[],commands:[{type:'navigate',screenId:'home'},{type:'executeAction',actionId:'action-1',quantity:1},...ordinary.commands]};await saveGoalRoute(root,route);
 const saved=(await call('goalBaselineSave',{routeId:route.id,name:'생활 규칙 수정 전'})).goalBaseline;assert.ok(saved?.id);
 const frozen=await readGoalBaseline(root,saved.id),originalBytes=await readFile(saved.file,'utf8');assert.equal(frozen.trace.states[2].attributes.money,88);assert.equal(frozen.trace.states[2].attributes.stamina,92);assert.equal(frozen.trace.reached,true);
 const same=(await call('goalCompareOpen',{baselineId:saved.id})).goalComparison;assert.equal(same.firstStateDifference,null);assert.equal(same.firstCheckDifference,null);assert.equal(same.sourceChangeCount,0);assert.equal(same.firstCommandDifference,null);
 const doc=service.editor.documents.find(d=>d.kind==='project'),changed=structuredClone(doc.data),action=changed.gameplay.actions.find(a=>a.id==='action-1');action.costs[0].amount=7;action.effects[0].value.value=3;action.requirements={op:'compare',comparator:'gte',left:{op:'read',target:{kind:'attribute',attributeId:'money'}},right:{op:'literal',value:0}};
 await call('edit',{edit:{type:'replace',file:doc.file,data:changed}});await call('save');
 const reopened=new QtStudioService();await reopened.handle({method:'open',params:{root}});const invoke=(method,params={})=>reopened.handle({method,params:{root,revision:reopened.editor.revision,...params}});
 const list=(await invoke('goalBaselineList')).goalBaselines;assert.equal(list.find(b=>b.id===saved.id).name,'생활 규칙 수정 전');
 const view=(await invoke('goalCompareOpen',{baselineId:saved.id})).goalComparison;assert.equal(view.mode,'historical');assert.equal(view.firstCommandDifference,null);assert.equal(view.firstStateDifference,2);assert.equal(view.firstCheckDifference,2);assert.equal(view.sharedThrough,1);assert.equal(view.runtimeChanged,false);
 const detail=(await invoke('goalCompareStep',{comparisonId:view.id,leftStep:2,rightStep:2})).goalComparisonStep;
 assert.equal(detail.left.after.attributes.money,88);assert.equal(detail.right.after.attributes.money,83);assert.equal(detail.left.after.attributes.stamina,92);assert.equal(detail.right.after.attributes.stamina,83);
 assert.deepEqual(detail.history.beforeDifferences,[]);assert.ok(detail.history.relatedChanges.some(c=>c.category==='비용'&&c.path.endsWith('/costs/0/amount')&&c.before===2&&c.after===7));assert.ok(detail.history.relatedChanges.some(c=>c.category==='효과'&&c.before===12&&c.after===3));assert.ok(detail.history.relatedChanges.some(c=>c.category==='조건'));
 assert.ok(detail.left.checks.some(c=>c.kind==='cost'&&c.amount===2));assert.ok(detail.right.checks.some(c=>c.kind==='cost'&&c.amount===7));assert.ok(detail.history.observationChanges.length);
 const propagated=(await invoke('goalCompareStep',{comparisonId:view.id,leftStep:3,rightStep:3})).goalComparisonStep;assert.ok(propagated.history.beforeDifferences.length);assert.match(propagated.history.explanation,/누적/);
 assert.deepEqual((await invoke('goalCompareStep',{comparisonId:view.id,leftStep:2,rightStep:2})).goalComparisonStep,detail);assert.equal(await readFile(saved.file,'utf8'),originalBytes);
 const currentDoc=reopened.editor.documents.find(d=>d.kind==='project'),blocked=structuredClone(currentDoc.data);blocked.gameplay.actions.find(a=>a.id==='action-1').requirements.right.value=999;
 await invoke('edit',{edit:{type:'replace',file:currentDoc.file,data:blocked}});assert.equal((await invoke('goalCompareStep',{comparisonId:view.id,leftStep:2,rightStep:2})).goalComparisonStep.stale,true);
 const failed=(await invoke('goalCompareOpen',{baselineId:saved.id})).goalComparison;assert.equal(failed.right.steps.length,3);assert.equal(failed.right.steps[2].ok,false);assert.ok(failed.right.unexecuted>0);assert.equal(failed.firstCheckDifference,2);
 const failedDetail=(await invoke('goalCompareStep',{comparisonId:failed.id,leftStep:2,rightStep:2})).goalComparisonStep;assert.equal(failedDetail.left.availability.ok,true);assert.equal(failedDetail.right.availability.ok,false);assert.ok(failedDetail.history.relatedChanges.some(c=>c.category==='조건'));
 await assert.rejects(invoke('goalCompareStep',{comparisonId:failed.id,leftStep:2,rightStep:3}),/범위/);
 const evidence=path.resolve('docs/evidence/goal-history');await mkdir(evidence,{recursive:true});await writeFile(path.join(evidence,'service-verification.json'),JSON.stringify({ok:true,root,baselineId:saved.id,summary:view,step:detail,failedSummary:failed,failedStep:failedDetail,originalPreserved:true,reopened:true},null,2));await writeFile(path.resolve('.cache/goal-history-fixture.txt'),root);
});
test('baselines are immutable, integrity checked, bounded by valid IDs; unrelated edits do not create state divergence',{timeout:60000},async()=>{
 const {root,service,call}=await setup(),content=await service.debugSnapshot(service.editor.revision),route=JSON.parse(await readFile(path.join(root,'playtests/routes/ordinary.json'),'utf8'));
 const b=await captureGoalBaseline(content,route,'immutable','test-runtime');await saveGoalBaseline(root,b);await assert.rejects(saveGoalBaseline(root,b),/덮어쓰지/);await assert.rejects(readGoalBaseline(root,'../outside'),/ID/);
 const doc=service.editor.documents.find(d=>d.kind==='project');await call('edit',{edit:{type:'replace',file:doc.file,data:{...doc.data,title:doc.data.title+' 표시 변경'}}});const edited=await service.debugSnapshot(service.editor.revision);
 const compared=await compareGoalBaseline(edited,b,'test-runtime');assert.equal(compared.firstStateDifference,null);assert.equal(compared.firstCheckDifference,null);assert.ok(compared.sourceChanges.length);
 const drift=await compareGoalBaseline(content,b,'other-runtime');assert.equal(historicalStep(drift,1,1).history.runtimeChanged,true);
 const file=path.join(root,'playtests/baselines',b.id+'.json'),bad=structuredClone(b);bad.trace.states[0].attributes.money=999;await writeFile(file,JSON.stringify(bad));await assert.rejects(readGoalBaseline(root,b.id),/무결성/);const list=(await call('goalBaselineList')).goalBaselines;assert.match(list.find(x=>x.id===b.id).error,/무결성/);
 await assert.rejects(call('goalBaselineSave',{routeId:route.id,name:''}),/이름/);await assert.rejects(call('goalCompareOpen',{baselineId:b.id}),/무결성/);
});
test('clamped equal states still expose changed effect checks; active activity cost definition is related',{timeout:60000},async()=>{
 const {root,service,call}=await setup(),base=JSON.parse(await readFile(path.join(root,'playtests/routes/ordinary.json'),'utf8'));
 const doc=service.editor.documents.find(d=>d.kind==='project'),initial=structuredClone(doc.data);initial.gameplay.actions.find(a=>a.id==='action-1').effects[0].value.value=20;
 await call('edit',{edit:{type:'replace',file:doc.file,data:initial}});let content=await service.debugSnapshot(service.editor.revision);
 const route={...base,checkpoints:[],commands:[{type:'navigate',screenId:'home'},{type:'executeAction',actionId:'action-1',quantity:1}]};const baseline=await captureGoalBaseline(content,route,'clamp','same');
 const changed=structuredClone(initial);changed.gameplay.actions.find(a=>a.id==='action-1').effects[0].value.value=30;await call('edit',{edit:{type:'replace',file:doc.file,data:changed}});content=await service.debugSnapshot(service.editor.revision);
 const pair=await compareGoalBaseline(content,baseline,'same');assert.equal(pair.firstStateDifference,null);assert.equal(pair.firstCheckDifference,2);assert.equal(pair.sharedThrough,1);assert.ok(historicalStep(pair,2,2).history.relatedChanges.some(c=>c.category==='효과'&&c.before===20&&c.after===30));
 const study={...base,checkpoints:[],commands:[{type:'navigate',screenId:'schedule'},{type:'setSchedule',activityIds:['study','rest','rest']},{type:'executeActivity'}]};const activityBaseline=await captureGoalBaseline(content,study,'study','same');
 const activity=service.editor.documents.find(d=>d.kind==='activity'&&d.data.id==='study'),costChanged=structuredClone(activity.data);costChanged.costs[0].amount+=1;await call('edit',{edit:{type:'replace',file:activity.file,data:costChanged}});content=await service.debugSnapshot(service.editor.revision);
 const costPair=await compareGoalBaseline(content,activityBaseline,'same');assert.equal(costPair.firstStateDifference,3);assert.ok(historicalStep(costPair,3,3).history.relatedChanges.some(c=>c.category==='비용'&&c.path.startsWith('/activities/study/costs')));
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdtemp,cp} from 'node:fs/promises';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {saveAndWait} from '../helpers/save-check.mjs';
import {saveGoalRoute} from '../../apps/qt-studio/goal-routes.mjs';

test('graph distinguishes approved and pending changes with original/expected/current values and stale protection',{timeout:180000},async()=>{
 const root=await mkdtemp(path.resolve('.cache/goal-review-graph-'));
 await cp(path.resolve('examples/starlight-three-months'),root,{recursive:true,filter:s=>!s.split(path.sep).includes('.studio')});
 const service=new QtStudioService();await service.handle({method:'open',params:{root}});const call=(method,params={})=>service.handle({method,params:{revision:service.editor.revision,...params}});
 const original=JSON.parse(await readFile(path.join(root,'playtests/routes/ordinary.json'),'utf8'));
 await saveGoalRoute(root,{...original,id:'graph-meal',name:'그래프 승인 · 생활',checkpoints:[],commands:[{type:'navigate',screenId:'home'},{type:'executeAction',actionId:'action-1',quantity:1},...original.commands]});
 const baseline=(await call('goalBaselineSave',{routeId:'graph-meal',name:'생활 원본'})).goalBaseline;
 const doc=service.editor.documents.find(d=>d.kind==='project'),data=structuredClone(doc.data),action=data.gameplay.actions.find(a=>a.id==='action-1');action.costs[0].amount=7;action.effects[0].value.value=3;
 await call('edit',{edit:{type:'replace',file:doc.file,data}});let report=(await saveAndWait(service)).goalImpact;
 const params=()=>({impactReportId:report.id,impactRow:report.rows.findIndex(r=>r.routeId==='graph-meal')});
 const open=async()=>(await call('goalCompareOpen',params())).goalComparison;
 const step=async(view,at=2)=>(await call('goalCompareStep',{comparisonId:view.id,leftStep:at,rightStep:at})).goalComparisonStep;
 let view=await open(),detail=await step(view);const money=detail.review.rows.find(r=>r.path==='/attributes/money'),cost=detail.review.rows.find(r=>r.kind==='cost-rule');
 assert.deepEqual([money.original,money.expected,money.current],[88,88,83]);assert.equal(money.status,'regression');assert.ok(view.review.markers.some(m=>m.step===2&&m.pending>0&&!m.approved));
 const oldParams=params();report=(await call('goalBaselineApprovePartial',{...params(),itemIds:[money.id,cost.id],note:'그래프에서 선택 승인'})).goalImpact;
 assert.equal((await step(view)).stale,true);await assert.rejects(call('goalBaselineApprovePartial',{...oldParams,itemIds:[money.id]}),/활성 기준이 변경/);
 view=await open();detail=await step(view);const approved=detail.review.rows.find(r=>r.path==='/attributes/money');assert.deepEqual([approved.original,approved.expected,approved.current],[88,83,83]);assert.equal(approved.status,'approved');assert.ok(view.review.markers.some(m=>m.step===2&&m.pending>0&&m.approved>0));assert.ok(view.review.markers.some(m=>m.approved>0&&m.pending===0));
 const unaligned=(await call('goalCompareStep',{comparisonId:view.id,leftStep:1,rightStep:2})).goalComparisonStep;assert.equal(unaligned.review.aligned,false);
 const changed=structuredClone(data);changed.gameplay.actions.find(a=>a.id==='action-1').costs[0].amount=9;await call('edit',{edit:{type:'replace',file:doc.file,data:changed}});report=(await saveAndWait(service)).goalImpact;
 view=await open();detail=await step(view);const cost9=detail.review.rows.find(r=>r.kind==='cost-rule');assert.deepEqual([cost9.original,cost9.expected,cost9.current],[2,7,9]);assert.equal(cost9.status,'regression');
 changed.gameplay.actions.find(a=>a.id==='action-1').costs[0].amount=2;await call('edit',{edit:{type:'replace',file:doc.file,data:changed}});report=(await saveAndWait(service)).goalImpact;view=await open();detail=await step(view);const cost2=detail.review.rows.find(r=>r.kind==='cost-rule');assert.deepEqual([cost2.original,cost2.expected,cost2.current],[2,7,2]);assert.equal(cost2.status,'regression');
 report=(await call('goalBaselineApprovePartial',{...params(),itemIds:[cost2.id]})).goalImpact;view=await open();detail=await step(view);const reset=detail.review.rows.find(r=>r.kind==='cost-rule');assert.deepEqual([reset.original,reset.expected,reset.current],[2,2,2]);assert.equal(reset.status,'approved');
 report=(await call('goalBaselineRestore',{...params(),baselineId:baseline.id,note:'그래프 기준 복원'})).goalImpact;view=await open();assert.ok(view.review.markers.every(m=>!m.approved));
 // Leave a pristine pending-change project for the actual Qt graph interaction test.
 await call('edit',{edit:{type:'replace',file:doc.file,data}});await saveAndWait(service);await writeFile(path.resolve('.cache/goal-review-graph-fixture.txt'),root);
});

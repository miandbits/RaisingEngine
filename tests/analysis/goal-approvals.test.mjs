import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdtemp,cp} from 'node:fs/promises';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {saveGoalRoute} from '../../apps/qt-studio/goal-routes.mjs';
import {goalBaselineSelection} from '../../apps/qt-studio/goal-approvals.mjs';

test('review one route, preserve other regressions, reject stale approvals, restore and persist history',{timeout:180000},async()=>{
 const root=await mkdtemp(path.resolve('.cache/goal-approvals-'));
 await cp(path.resolve('projects/starlight-three-months'),root,{recursive:true,filter:s=>!s.split(path.sep).includes('.studio')});
 const service=new QtStudioService();await service.handle({method:'open',params:{root}});
 const call=(method,params={})=>service.handle({method,params:{revision:service.editor.revision,...params}});
 const ordinary=JSON.parse(await readFile(path.join(root,'playtests/routes/ordinary.json'),'utf8'));
 for(const id of ['review-a','review-b'])await saveGoalRoute(root,{...ordinary,id,name:id,checkpoints:[],commands:[{type:'navigate',screenId:'home'},{type:'executeAction',actionId:'action-1',quantity:1},...ordinary.commands]});
 const original=(await call('goalBaselineSave',{routeId:'review-a',name:'초기 비용 기준'})).goalBaseline;
 await call('goalBaselineSave',{routeId:'review-b',name:'다른 경로 기준'});
 const bytes=await readFile(original.file,'utf8');
 const doc=service.editor.documents.find(d=>d.kind==='project'),data=structuredClone(doc.data);data.gameplay.actions.find(a=>a.id==='action-1').costs[0].amount=7;
 await call('edit',{edit:{type:'replace',file:doc.file,data}});
 const report=(await call('save')).goalImpact,index=report.rows.findIndex(r=>r.routeId==='review-a');
 assert.equal(report.rows[index].status,'changed');
 const params={impactReportId:report.id,impactRow:index,note:'비용 7로 의도한 조정',name:'승인된 비용'};
 const other=new QtStudioService();await other.handle({method:'open',params:{root}});
 const approved=await call('goalBaselineApprove',params),newId=approved.goalApproval.baselineId;
 assert.equal(approved.goalImpact.rows.find(r=>r.routeId==='review-a').status,'unchanged');
 assert.equal(approved.goalImpact.rows.find(r=>r.routeId==='review-b').status,'changed');
 assert.equal(approved.goalImpact.rows.find(r=>r.routeId==='review-a').baselineId,newId);
 assert.equal(await readFile(original.file,'utf8'),bytes);
 await assert.rejects(call('goalBaselineApprove',params),/활성 기준이 변경/);
 await assert.rejects(other.handle({method:'goalBaselineApprove',params:{...params,revision:other.editor.revision}}),/활성 기준이 변경/);
 await assert.rejects(call('goalCompareOpen',params),/활성 기준이 변경/);
 const beforeRestore=approved.goalImpact,restoreParams={impactReportId:beforeRestore.id,impactRow:beforeRestore.rows.findIndex(r=>r.routeId==='review-a'),baselineId:original.id,note:'조정 취소'};
 const restored=await call('goalBaselineRestore',restoreParams),row=restored.goalImpact.rows.find(r=>r.routeId==='review-a');
 assert.equal(row.status,'changed');assert.equal(row.baselineId,original.id);assert.equal(row.approvalHistory.length,2);assert.equal(row.approvalHistory[1].note,'조정 취소');
 assert.equal(row.firstDifference,2);
 const reopened=new QtStudioService();await reopened.handle({method:'open',params:{root}});
 const latest=(await reopened.handle({method:'goalImpactLatest',params:{revision:reopened.editor.revision}})).goalImpact;
 assert.equal(latest.stale,false);assert.equal(latest.rows.find(r=>r.routeId==='review-a').baselineId,original.id);
 // A newly captured record cannot silently replace an explicitly selected/rolled-back baseline.
 await call('goalBaselineSave',{routeId:'review-a',name:'별도 참고 기록'});
 assert.equal((await call('save')).goalImpact.rows.find(r=>r.routeId==='review-a').baselineId,original.id);
 // Failed goals remain failures and cannot be hidden by approval.
 const ending=service.editor.documents.find(d=>d.kind==='ending'&&d.data.id==='knight');await call('edit',{edit:{type:'replace',file:ending.file,data:{...ending.data,requirements:{op:'always'}}}});
 const failed=(await call('save')).goalImpact;assert.equal(failed.rows.find(r=>r.routeId==='review-a').status,'failed');
 await assert.rejects(call('goalBaselineApprove',{impactReportId:failed.id,impactRow:failed.rows.findIndex(r=>r.routeId==='review-a')}),/목표에 도달/);
 const ledger=path.join(root,'playtests/baselines/.approvals');await writeFile(ledger,'corrupt');
 await assert.rejects(goalBaselineSelection(root),/승인 기록을 읽을 수/);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir,mkdtemp,cp} from 'node:fs/promises';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {compareGoalRoutes,comparisonSummary,compareGoalStep} from '../../dist/packages/analysis-service/goal-compare.js';
import {saveGoalRoute} from '../../apps/qt-studio/goal-routes.mjs';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/session.js';

const project=path.resolve('examples/starlight-three-months'),evidence=path.resolve('docs/evidence/route-compare');
async function setup(){const root=await mkdtemp(path.resolve('.cache/route-compare-'));await cp(project,root,{recursive:true,filter:s=>!s.split(path.sep).includes('.studio')});const service=new QtStudioService();await service.handle({method:'open',params:{root}});const call=(method,params={})=>service.handle({method,params:{revision:service.editor.revision,root:service.editor.root,...params}});return {root,service,call};}
test('failed original vs changed activity: exact branch, state deltas, authoritative costs, immutable replay',{timeout:60000},async()=>{
 const {service,call,root}=await setup();const original=JSON.parse(await readFile(path.join(root,'playtests/routes/ordinary.json'),'utf8'));
 const left={...original,id:'compare-original',name:'수정 전 일정',target:{kind:'ending',id:'knight'}};delete left.endingId;
 const right={...left,id:'compare-alternative',name:'대체 일정',sourceRouteId:left.id,commands:left.commands.map(c=>c.type==='setSchedule'?{...c,activityIds:c.activityIds.map((_,i)=>i===0?'study':'rest')}:c)};
 const ending=service.editor.documents.find(d=>d.kind==='ending'&&d.data.id==='knight'),harder=structuredClone(ending.data);harder.requirements={op:'compare',comparator:'gte',left:{op:'read',target:{kind:'attribute',attributeId:'skill'}},right:{op:'literal',value:8}};
 await call('edit',{edit:{type:'replace',file:ending.file,data:harder}});await call('save');
 const prepared=await service.debugSnapshot(service.editor.revision);let cursor=createSession(prepared,right.origin.seed,right.origin).session;
 for(const command of right.commands){const r=dispatchSession(prepared,cursor,command,cursor.state.stateRevision);assert.equal(r.ok,true,JSON.stringify({command,error:r.error,state:cursor.state.phase,period:cursor.state.periodIndex}));cursor=r.session;}
 for(let i=0;i<10&&cursor.state.phase==='narrative';i++){const command={type:'advanceNarrative'},r=dispatchSession(prepared,cursor,command,cursor.state.stateRevision);assert.equal(r.ok,true);right.commands.push(command);cursor=r.session;}
 await saveGoalRoute(root,left);await saveGoalRoute(root,right);
 const bytes=await readFile(path.join(root,'playtests/routes/compare-original.json'),'utf8');
 const opened=await call('goalCompareOpen',{leftId:left.id,rightId:right.id});const view=opened.goalComparison;
 assert.equal(view.firstCommandDifference,2);assert.equal(view.sharedThrough,1);assert.equal(view.firstStateDifference,2);assert.equal(view.left.failure.code,'TARGET_NOT_REACHED');assert.equal(view.right.reached,true);
 const step=(await call('goalCompareStep',{comparisonId:view.id,leftStep:3,rightStep:3})).goalComparisonStep;
 assert.equal(step.left.before.attributes.skill,0);assert.equal(step.left.after.attributes.skill,0);assert.equal(step.right.after.attributes.skill,8);
 assert.ok(step.differences.some(d=>d.path==='/attributes/skill'&&d.before===0&&d.after===8));assert.ok(step.right.changes.some(d=>d.path==='/attributes/skill'));
 assert.equal(step.right.conditions[0].left,0);assert.equal(step.right.targetAfter[0].left,8);assert.equal(step.right.conditions[0].right,8);
 const final=(await call('goalCompareStep',{comparisonId:view.id,leftStep:view.left.steps.length-1,rightStep:view.right.steps.length-1})).goalComparisonStep;assert.equal(final.left.targetAfter[0].value,false);assert.equal(final.right.targetAfter[0].value,true);
 assert.deepEqual((await call('goalCompareStep',{comparisonId:view.id,leftStep:3,rightStep:3})).goalComparisonStep,step);
 const content=await service.debugSnapshot(service.editor.revision);let replay=createSession(content,right.origin.seed,right.origin).session;for(const command of right.commands){const r=dispatchSession(content,replay,command,replay.state.stateRevision);assert.equal(r.ok,true);replay=r.session;}assert.deepEqual(final.right.after,replay.state);
 await call('edit',{edit:{type:'replace',file:ending.file,data:ending.data}});const stale=(await call('goalCompareStep',{comparisonId:view.id,leftStep:3,rightStep:3})).goalComparisonStep;assert.equal(stale.stale,true);assert.deepEqual(stale.right.after,step.right.after);
 assert.equal(await readFile(path.join(root,'playtests/routes/compare-original.json'),'utf8'),bytes);
 await mkdir(evidence,{recursive:true});await writeFile(path.join(evidence,'service-verification.json'),JSON.stringify({ok:true,root,summary:view,activityStep:step,finalStep:final,immutableAfterEdit:true,originalPreserved:true},null,2));
 // This saved copy is the GUI fixture; its disk still has the changed ending rule.
 await writeFile(path.resolve('.cache/route-compare-fixture.txt'),root);
});
test('execution failure stops at failing command; different origins, single paths and invalid indices are explicit',async()=>{
 const {service,call,root}=await setup(),content=await service.debugSnapshot(service.editor.revision);const base=JSON.parse(await readFile(path.join(root,'playtests/routes/ordinary.json'),'utf8'));
 const broken={...base,commands:[{type:'executeAction',actionId:'deleted-action',quantity:1},...base.commands]};
 const pair=await compareGoalRoutes(content,broken,base);assert.equal(pair.left.steps.length,2);assert.equal(pair.left.steps[1].ok,false);assert.equal(pair.left.unexecuted,base.commands.length);assert.deepEqual(pair.left.states[0],pair.left.states[1]);assert.throws(()=>compareGoalStep(pair,2,0),/범위/);
 const difference=await compareGoalRoutes(content,base,{...base,origin:{...base.origin,seed:43}});assert.equal(difference.sharedThrough,-1);assert.equal(difference.firstStateDifference,0);assert.ok(difference.originDiff.length);
 const same=await compareGoalRoutes(content,base,base);assert.equal(same.firstCommandDifference,null);assert.equal(same.firstStateDifference,null);assert.equal(same.sharedThrough,base.commands.length);
 const meal={...base,checkpoints:[],commands:[{type:'executeAction',actionId:'action-1',quantity:1}]};const single=await compareGoalRoutes(content,null,meal);const detail=compareGoalStep(single,0,1);assert.equal(detail.left,null);assert.ok(detail.right.checks.some(x=>x.kind==='cost'&&x.before===90&&x.after===88));assert.equal(detail.right.after.attributes.money,88);
 const opened=(await call('goalCompareOpen',{rightId:'ordinary'})).goalComparison;await call('goalCompareOpen',{rightId:'success'});await assert.rejects(call('goalCompareStep',{comparisonId:opened.id,rightStep:0}),/만료/);
 await assert.rejects(call('goalCompareOpen',{rightId:'missing'}),/없습니다/);
});
test('unregistered discovered route is viewable; stale search results cannot be presented as current',async()=>{
 const {service,call}=await setup();const started=await call('goalSearchStart',{target:{kind:'event',id:'flowers-bring-friendship'}});let job;
 for(let i=0;i<200;i++){job=(await call('goalSearchStatus',{jobId:started.goalSearch.id})).goalSearch;if(job.status!=='running')break;await new Promise(r=>setTimeout(r,50));}assert.equal(job.status,'found');
 const opened=(await call('goalCompareOpen',{jobId:job.id,leftId:job.sourceRouteId})).goalComparison;assert.equal(opened.right.reached,true);assert.equal(opened.right.id,'found-route');
 const event=service.editor.documents.find(d=>d.kind==='event');await call('edit',{edit:{type:'replace',file:event.file,data:{...event.data,label:event.data.label+' 수정'}}});
 await assert.rejects(call('goalCompareOpen',{jobId:job.id}),/초안이 변경/);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import { fixture } from './fixture.mjs';
import { createSession, dispatchSession } from '../../dist/packages/runtime-service/session.js';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';
import { getGameplayView } from '../../dist/packages/gameplay/service.js';
import { readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
const add=(id,value)=>({op:'add',target:{kind:'attribute',attributeId:id},value:{op:'literal',value}});
export function genericRules(p) {
  p.gameplay.actions=[{id:'meal',name:'Meals',costs:[{attributeId:'money',amount:2}],effects:[add('stamina',1)]},{id:'study',name:'Study',costs:[],effects:[add('stamina',2)]},{id:'sleep',name:'Sleep',costs:[],effects:[add('stamina',3)]},{id:'heal',name:'Treatment',locationIds:['market'],costs:[{attributeId:'money',amount:4}],effects:[add('stamina',3)],maxQuantity:3,rejectNoEffect:true,confirmation:{title:'Confirm treatment'}}];
  p.gameplay.policyBudgets=[{id:'hours',name:'Hours',capacity:24,unit:'h'}];
  p.gameplay.policyGroups=['meal','study','sleep'].map((id,i)=>({id,name:id,trigger:{type:i===1?'afterActivity':'beforeActivity',intervalPeriods:i===0?3:1},options:[{id:'normal',name:'Normal',actionId:id,budgetUses:[{budgetId:'hours',amount:[2,8,8][i]}]},{id:'intense',name:'Intense',actionId:id,budgetUses:[{budgetId:'hours',amount:16}]}]}));
}
async function setup(t,mutate=()=>{}) {
  const f=await fixture(t,p=>{genericRules(p);mutate(p)});assert.equal(f.built.ok,true,JSON.stringify(f.built.diagnostics));const content=f.built.snapshot;let session=createSession(content,42).session;
  return {content,get session(){return session},send(command){const r=dispatchSession(content,session,command,session.state.stateRevision);assert.equal(r.ok,true,JSON.stringify(r.error));session=r.session;return session.state},fail(command,code,revision=session.state.stateRevision){const before=session,r=dispatchSession(content,session,command,revision);assert.equal(r.ok,false);assert.equal(r.error.code,code);assert.equal(r.session,before)},async restore(){const e=await encodeSave(content,session);assert.equal(e.ok,true,JSON.stringify(e));const d=await decodeSave(content,e.text);assert.equal(d.ok,true,JSON.stringify(d));assert.deepEqual(d.session,session);session=d.session}};
}
test('independent policies share budgets and before/after triggers, preserve cadence across clearing and save',async t=>{
 const g=await setup(t);const money=g.session.state.attributes.money;
 for(const groupId of ['meal','study','sleep'])g.send({type:'selectPolicy',groupId,optionId:'normal'});
 assert.equal(getGameplayView(g.content,g.session.state).policyBudgets[0].used,18);
 g.fail({type:'selectPolicy',groupId:'sleep',optionId:'intense'},'POLICY_BUDGET_EXCEEDED');
 g.send({type:'setSchedule',activityIds:['rest','rest','rest']});g.send({type:'executeActivity'});
 assert.equal(g.session.state.attributes.money,money-2);assert.deepEqual(g.session.state.gameplay.policyNextPeriods,{meal:3,sleep:1,study:1});await g.restore();
 const changes=g.session.state.lastActivityResult.changes;assert.ok(changes.findIndex(x=>x.source.includes('/actions/meal/'))<changes.findIndex(x=>x.source.includes('/actions/study/')));
 g.send({type:'acknowledgeResult'});g.send({type:'resolvePeriodEnd'});g.send({type:'selectPolicy',groupId:'meal',optionId:null});g.send({type:'selectPolicy',groupId:'meal',optionId:'normal'});g.send({type:'executeActivity'});assert.equal(g.session.state.attributes.money,money-2);await g.restore();
});
test('after trigger failure rolls back activity effects, RNG and earlier policy charges',async t=>{
 const g=await setup(t,p=>p.gameplay.actions[1].costs=[{attributeId:'money',amount:10000}]);for(const groupId of ['meal','study'])g.send({type:'selectPolicy',groupId,optionId:'normal'});g.send({type:'setSchedule',activityIds:['rest','rest','rest']});g.fail({type:'executeActivity'},'INSUFFICIENT_FUNDS');assert.equal(g.session.state.gameplay.policyAppliedPeriods,undefined);
});
test('same action contract supports treatment quantities, conditions, no-effect and stale confirmations',async t=>{
 const g=await setup(t);g.fail({type:'executeAction',actionId:'heal',quantity:1},'WRONG_LOCATION');g.send({type:'visit',locationId:'city'});g.send({type:'visit',locationId:'market'});const revision=g.session.state.stateRevision,before=g.session.state.attributes.money;g.send({type:'executeAction',actionId:'heal',quantity:2});assert.equal(g.session.state.attributes.money,before-8);g.fail({type:'executeAction',actionId:'heal',quantity:2},'REVISION_CONFLICT',revision);await g.restore();
 const capped=await setup(t,p=>{p.initialState.attributes.stamina=100;p.gameplay.actions[3].locationIds=[]});capped.fail({type:'executeAction',actionId:'heal',quantity:1},'ACTION_NO_EFFECT');
 const locked=await setup(t,p=>{p.gameplay.actions[3].locationIds=[];p.gameplay.actions[3].requirements={op:'compare',left:{op:'read',target:{kind:'attribute',attributeId:'money'}},comparator:'gte',right:{op:'literal',value:99999}}});locked.fail({type:'executeAction',actionId:'heal',quantity:1},'ACTION_LOCKED');
});
test('semantic validation identifies policy references, duplicate allocation and scheduled event reentry',async t=>{
 const f=await fixture(t,p=>{genericRules(p);p.gameplay.actions[0].eventId='gift-event';p.gameplay.policyGroups[0].options[0].budgetUses.push({budgetId:'hours',amount:1});p.gameplay.policyGroups[1].options[0].actionId='missing';p.gameplay.policyGroups[2].options[0].budgetUses[0].budgetId='missing'});assert.equal(f.built.ok,false);for(const code of ['POLICY_EVENT_ACTION','POLICY_BUDGET_DUPLICATE','REFERENCE_MISSING'])assert.ok(f.built.diagnostics.some(x=>x.code===code),code);
});
test('before policies feed activity eligibility and probability before costs; after policies do not',async t=>{
 const f=await fixture(t,p=>{genericRules(p);p.initialState.attributes.stamina=0;p.gameplay.actions[0].effects=[add('stamina',100)];},async root=>{const file=path.join(root,'activities/rest.json'),a=JSON.parse(await readFile(file,'utf8'));a.requirements={op:'compare',comparator:'gte',left:{op:'read',target:{kind:'attribute',attributeId:'stamina'}},right:{op:'literal',value:100}};a.successCheck={chance:{op:'multiply',left:{op:'read',target:{kind:'attribute',attributeId:'stamina'}},right:{op:'literal',value:100}}};a.successEffects=[];a.failureEffects=[];await writeFile(file,JSON.stringify(a));});
 assert.equal(f.built.ok,true,JSON.stringify(f.built.diagnostics));const content=f.built.snapshot;let session=createSession(content,42).session;
 for(const command of [{type:'selectPolicy',groupId:'meal',optionId:'normal'},{type:'setSchedule',activityIds:['rest','rest','rest']},{type:'executeActivity'}]){const r=dispatchSession(content,session,command,session.state.stateRevision);assert.equal(r.ok,true,JSON.stringify(r.error));session=r.session;}
 assert.equal(session.state.lastActivityResult.success.requestedChance,10000);assert.equal(session.state.lastActivityResult.success.passed,true);
});

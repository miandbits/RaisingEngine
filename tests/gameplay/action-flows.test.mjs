import test from 'node:test';
import assert from 'node:assert/strict';
import { fixture } from './fixture.mjs';
import { createSession, dispatchSession } from '../../dist/packages/runtime-service/session.js';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';
const literal=value=>({op:'literal',value});
const add=(attributeId,value)=>({op:'add',target:{kind:'attribute',attributeId},value:literal(value)});
const stock=(op,value)=>({op,target:{kind:'inventory',itemId:'flower'},value:literal(value)});
const action=(id,effects,costs=[])=>({id,name:id,costs,effects});
async function setup(t,mutate,mode='full') {
 const f=await fixture(t,p=>{p.gameplay.actions=[];p.gameplay.rules=[];mutate(p)});assert.equal(f.built.ok,true,JSON.stringify(f.built.diagnostics));const content=f.built.snapshot;let session=createSession(content,42,{mode}).session;
 return {content,get session(){return session},send(command){const r=dispatchSession(content,session,command,session.state.stateRevision);assert.equal(r.ok,true,JSON.stringify(r.error));session=r.session;return session.state},fail(command,code){const before=session,r=dispatchSession(content,session,command,session.state.stateRevision);assert.equal(r.ok,false);assert.equal(r.error.code,code);assert.equal(r.session,before)},async restore(){const e=await encodeSave(content,session);assert.equal(e.ok,true,JSON.stringify(e));const d=await decodeSave(content,e.text);assert.equal(d.ok,true,JSON.stringify(d));assert.deepEqual(d.session,session);session=d.session}};
}
test('inventory expressions select action branches and chained costs/effects replay exactly',async t=>{
 const g=await setup(t,p=>{p.gameplay.items[0].initialQuantity=1;p.gameplay.actions=[action('consume',[stock('add',-1),add('money',7)]),action('fallback',[add('money',1)]),{...action('choose',[]),flow:[{id:'owns-item',condition:{op:'compare',comparator:'gte',left:{op:'read',target:{kind:'inventory',itemId:'flower'}},right:literal(1)},thenActionId:'consume',elseActionId:'fallback'}]}]});
 const money=g.session.state.attributes.money;g.send({type:'executeAction',actionId:'choose',quantity:1});assert.equal(g.session.state.gameplay.inventory.flower,0);assert.equal(g.session.state.attributes.money,money+7);g.send({type:'executeAction',actionId:'choose',quantity:1});assert.equal(g.session.state.attributes.money,money+8);await g.restore();
});
test('a later flow failure rolls back earlier payment, inventory and prior child effects',async t=>{
 const g=await setup(t,p=>{p.gameplay.items[0].initialQuantity=1;p.gameplay.actions=[action('consume',[stock('add',-1)]),action('overflow',[stock('set',4)]),{...action('transaction',[add('stamina',5)],[{attributeId:'money',amount:3}]),flow:[{id:'first',thenActionId:'consume'},{id:'last',thenActionId:'overflow'}]}]});
 g.fail({type:'executeAction',actionId:'transaction',quantity:1},'INVENTORY_RANGE');assert.equal(g.session.state.gameplay.inventory.flower,1);await g.restore();
});
test('location and purchase hooks filter sources; relationship recursion fires each rule once',async t=>{
 const g=await setup(t,p=>{p.gameplay.actions=[action('welcome',[add('money',1)]),action('reward',[add('money',2)]),action('bond',[{op:'add',target:{kind:'relationship',characterId:'mentor',relationshipId:'trust'},value:literal(1)}])];p.gameplay.rules=[{id:'arrival',name:'Arrival',trigger:{type:'locationEnter',locationId:'city'},actionId:'welcome'},{id:'purchase',name:'Purchase',trigger:{type:'purchaseComplete',shopId:'florist',itemId:'flower'},actionId:'reward'},{id:'relationship',name:'Relationship',trigger:{type:'relationshipChanged',characterId:'mentor',relationshipId:'trust'},actionId:'bond'}]});
 const money=g.session.state.attributes.money,trust=g.session.state.relationships.mentor.trust;
 g.send({type:'visit',locationId:'city'});assert.equal(g.session.state.attributes.money,money+1);g.send({type:'visit',locationId:'market'});assert.equal(g.session.state.attributes.money,money+1);g.send({type:'buy',shopId:'florist',itemId:'flower',quantity:1});assert.equal(g.session.state.attributes.money,money-7);g.send({type:'executeAction',actionId:'bond',quantity:1});assert.equal(g.session.state.relationships.mentor.trust,trust+2);await g.restore();
});
test('explicit seven-day accounting splits 2/2/3, hooks precede month close and survive restore',async t=>{
 const g=await setup(t,p=>{p.gameplay.ruleCalendar={daysPerMonth:7};const amounts={monthStart:100,periodStart:10,dayStart:1,dayEnd:2,periodEnd:20,monthEnd:200};p.gameplay.actions=Object.entries(amounts).map(([id,n])=>action(id.toLowerCase(),[add('money',n)]));p.gameplay.rules=Object.keys(amounts).map(type=>({id:type.toLowerCase(),name:type,trigger:{type},actionId:type.toLowerCase()}))},'activities-only');
 const money=g.session.state.attributes.money;g.send({type:'setSchedule',activityIds:['rest','rest','rest']});
 g.send({type:'executeActivity'});assert.equal(g.session.state.attributes.money,money+116);await g.restore();g.send({type:'acknowledgeResult'});g.send({type:'completePeriodWithoutEvents'});assert.equal(g.session.state.attributes.money,money+136);
 for(let i=0;i<2;i++){g.send({type:'executeActivity'});g.send({type:'acknowledgeResult'});g.send({type:'completePeriodWithoutEvents'});}assert.equal(g.session.state.attributes.money,money+411);assert.equal(g.session.state.periodIndex,3);assert.deepEqual(g.session.state.gameplay.rulePeriods,{periodStart:2,periodEnd:2});await g.restore();
});
test('flow cycles, missing references, invalid filters and implicit day clocks are rejected with locations',async t=>{
 const f=await fixture(t,p=>{p.gameplay.actions=[{...action('first',[]),flow:[{id:'loop',thenActionId:'second'}]},{...action('second',[]),flow:[{id:'loop',thenActionId:'first'}]},action('bad-item',[{op:'add',target:{kind:'inventory',itemId:'absent'},value:literal(1)}])];p.gameplay.rules=[{id:'daily',name:'Daily',trigger:{type:'dayStart',shopId:'florist'},actionId:'first'}]});assert.equal(f.built.ok,false);for(const code of ['FLOW_CYCLE','REFERENCE_MISSING','RULE_CALENDAR_REQUIRED','RULE_TRIGGER_FILTER'])assert.ok(f.built.diagnostics.some(x=>x.code===code&&x.pointer),code);
});
test('runtime cycle guard protects unvalidated snapshots and preserves the full command state',async t=>{
 const g=await setup(t,p=>{p.gameplay.actions=[action('loop',[add('money',1)])]});const content=structuredClone(g.content);content.project.gameplay.actions[0].flow=[{id:'again',thenActionId:'loop'}];const r=dispatchSession(content,g.session,{type:'executeAction',actionId:'loop',quantity:1},g.session.state.stateRevision);assert.equal(r.ok,false);assert.equal(r.error.code,'FLOW_CYCLE');assert.equal(r.session,g.session);
});

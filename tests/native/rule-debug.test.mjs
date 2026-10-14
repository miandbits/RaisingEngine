import test from 'node:test';
import assert from 'node:assert/strict';
import {RuleDebugSession,ruleImpact} from '../../dist/packages/rule-debug/index.js';
import {fixture} from '../gameplay/fixture.mjs';
import {writeFile} from 'node:fs/promises';
import path from 'node:path';

const compare=(id,value)=>({op:'compare',comparator:'gte',left:{op:'read',target:{kind:'attribute',attributeId:id}},right:{op:'literal',value}});
const add=(id,value)=>({op:'add',target:{kind:'attribute',attributeId:id},value:{op:'literal',value}});
async function content(t){
  const {built}=await fixture(t,p=>{
    p.initialState.attributes.money=100;p.initialState.attributes.stamina=40;
    p.gameplay.actions=[
      {id:'blocked',name:'Blocked rite',requirements:{op:'all',conditions:[compare('stamina',50),{op:'any',conditions:[compare('money',10),compare('stamina',90)]}]},costs:[{attributeId:'money',amount:1}],effects:[add('stamina',5)]},
      {id:'recover',name:'Recovery',costs:[{attributeId:'money',amount:4}],effects:[add('stamina',10)],maxQuantity:3},
      {id:'follow',name:'Follow-up',costs:[],effects:[add('money',1)],flow:[{id:'branch',condition:compare('stamina',50),thenActionId:'recover'}]}
    ];
    p.gameplay.policyGroups=[{id:'sleep',name:'Sleep',trigger:{type:'afterActivity'},options:[{id:'normal',name:'Normal',actionId:'recover'}]}];
  });assert.equal(built.ok,true,JSON.stringify(built.diagnostics));return built.snapshot;
}
test('native debug explains nested actual operands without mutation and rolls back blocked/stale commands',async t=>{
  const c=await content(t),debug=new RuleDebugSession(c,7,42),before=debug.view();
  const inspected=debug.inspect({kind:'action',id:'blocked'});assert.equal(inspected.inspection.eligible,false);assert.deepEqual(debug.view().state,before.state);
  const tree=inspected.inspection.condition;assert.equal(tree.value,false);assert.equal(tree.children[0].leftValue,40);assert.equal(tree.children[0].rightValue,50);assert.equal(tree.children[1].value,true);assert.match(tree.children[0].path,/actions\/0\/requirements\/conditions\/0$/);
  const failed=debug.command({type:'executeAction',actionId:'blocked',quantity:1},before.stateRevision);assert.equal(failed.ok,false);assert.deepEqual(failed.state,before.state);assert.deepEqual(failed.diff,[]);assert.ok(failed.trace.length);
  const recovered=debug.command({type:'executeAction',actionId:'recover',quantity:2},before.stateRevision);assert.equal(recovered.ok,true);assert.equal(recovered.state.attributes.money,92);assert.equal(recovered.state.attributes.stamina,60);assert.ok(recovered.diff.some(row=>row.path==='/attributes/stamina'&&row.before===40&&row.after===60));assert.ok(recovered.trace.length);
  const stale=debug.command({type:'executeAction',actionId:'recover',quantity:1},before.stateRevision);assert.equal(stale.ok,false);assert.deepEqual(stale.state,recovered.state);
  const inspection=debug.inspect({kind:'action',id:'follow'});assert.equal(inspection.inspection.eligible,true);assert.ok(inspection.inspection.trace.length);assert.deepEqual(debug.view().state,recovered.state);
  const fork=new RuleDebugSession(c,7,42);assert.deepEqual(fork.view().state,before.state);assert.notEqual(fork.sessionId,debug.sessionId);
});
test('rule impact links policy options and action branches to shared action and includes cost/effect dependencies',async t=>{
  const c=await content(t),impact=ruleImpact(c,{kind:'action',id:'recover'});
  assert.ok(impact.edges.some(e=>e.from==='policy:sleep'&&e.to==='action:recover'));
  assert.ok(impact.edges.some(e=>e.from==='action:follow'&&e.to==='action:recover'&&e.relation==='branch'));
  assert.ok(impact.edges.some(e=>e.from==='action:recover'&&e.to==='attribute:money'&&e.relation==='cost'));
  assert.ok(impact.edges.some(e=>e.from==='action:recover'&&e.to==='attribute:stamina'&&e.relation==='effect'));
});
test('event inspection reports canonical timing and priority suppression without selecting an event',async t=>{
  const {built}=await fixture(t,undefined,async root=>{
    for(const [id,priority] of [['debug-high',100],['debug-low',99]])await writeFile(path.join(root,`events/${id}.json`),JSON.stringify({formatVersion:'0.1.0',id,label:id,priority,window:{fromPeriod:0,throughPeriod:0},requirements:{op:'always'},entryNodeId:'line',nodes:[{id:'line',type:'dialogue',speakerId:'mentor',text:id,next:'end'},{id:'end',type:'end'}]}));
  });assert.equal(built.ok,true,JSON.stringify(built.diagnostics));const debug=new RuleDebugSession(built.snapshot,1);
  assert.equal(debug.inspect({kind:'event',id:'debug-high'}).inspection.reason,'wrong-phase');
  for(const command of [{type:'setSchedule',activityIds:['rest','rest','rest']},{type:'executeActivity'},{type:'acknowledgeResult'}])assert.equal(debug.command(command,debug.view().stateRevision).ok,true);
  const before=debug.view().state;assert.equal(before.phase,'period-end');
  const low=debug.inspect({kind:'event',id:'debug-low'}),high=debug.inspect({kind:'event',id:'debug-high'});
  assert.equal(low.inspection.eligible,false);assert.equal(low.inspection.reason,'priority');assert.equal(high.inspection.eligible,true);assert.equal(high.inspection.reason,'selected');assert.deepEqual(debug.view().state,before);
  assert.ok(low.inspection.checks.length);assert.ok(low.inspection.eventDecisions.length);
});

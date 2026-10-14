import test from 'node:test';
import assert from 'node:assert/strict';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import { WasmRuntime } from '../../dist/packages/native-bridge/wasm.js';
import { WebPlayerController } from '../../dist/packages/web-player/controller.js';
import { fixture } from '../gameplay/fixture.mjs';

test('web controller uses shared actions and independent policy groups with Wasm, stale protection and exact save replay',async t=>{
  const add=value=>({op:'add',target:{kind:'attribute',attributeId:'stamina'},value:{op:'literal',value}});
  const {built}=await fixture(t,p=>{
    p.initialState.attributes.money=100;p.initialState.attributes.stamina=40;
    p.gameplay.actions=[
      {id:'blessing',name:'Blessing',costs:[{attributeId:'money',amount:4}],effects:[add(5)],maxQuantity:3},
      {id:'sleep-effect',name:'Sleep recovery',costs:[],effects:[add(2)]},
      {id:'study-effect',name:'Study exertion',costs:[{attributeId:'money',amount:1}],effects:[add(-1)]}
    ];
    p.gameplay.policyBudgets=[{id:'hours',name:'Daily allocation',capacity:24,unit:'hours'}];
    p.gameplay.policyGroups=[
      {id:'sleep',name:'Sleep',trigger:{type:'afterActivity'},options:[{id:'eight',name:'Eight hours',actionId:'sleep-effect',budgetUses:[{budgetId:'hours',amount:8}]}]},
      {id:'study',name:'Study',trigger:{type:'beforeActivity'},options:[{id:'six',name:'Six hours',actionId:'study-effect',budgetUses:[{budgetId:'hours',amount:6}]},{id:'eighteen',name:'Eighteen hours',actionId:'study-effect',budgetUses:[{budgetId:'hours',amount:18}]}]}
    ];
  });
  assert.equal(built.ok,true,JSON.stringify(built.diagnostics));let saved=null;
  const controller=new WebPlayerController(built.snapshot,new WasmRuntime(await createModule()),{read:async()=>saved,write:async text=>{saved=text;}},()=>true);
  assert.equal((await controller.newRun(null)).error,null);
  const version=()=>({generation:controller.generation,revision:controller.session.state.stateRevision});
  const send=async action=>{const reply=await controller.act(action,version());assert.equal(reply.error,null);return reply.view;};
  await send({type:'selectPolicy',groupId:'sleep',optionId:'eight'});
  const selected=await send({type:'selectPolicy',groupId:'study',optionId:'six'});
  assert.equal(selected.state.attributes.money,100);assert.equal(selected.state.attributes.stamina,40);
  assert.deepEqual(selected.state.gameplay.policySelections,{sleep:'eight',study:'six'});
  assert.equal(selected.gameplay.policyBudgets[0].used,14);
  const before=structuredClone(controller.session.state);
  assert.ok((await controller.act({type:'selectPolicy',groupId:'study',optionId:'eighteen'},version())).error);
  assert.deepEqual(controller.session.state,before);
  const stale=version();await send({type:'executeAction',actionId:'blessing',quantity:2});
  const expected=structuredClone(controller.session.state);assert.equal(expected.attributes.money,92);assert.equal(expected.attributes.stamina,50);
  assert.match((await controller.act({type:'executeAction',actionId:'blessing',quantity:2},stale)).error,/갱신/);assert.deepEqual(controller.session.state,expected);
  assert.equal((await controller.save(version())).error,null);
  await send({type:'selectPolicy',groupId:'study',optionId:null});
  assert.equal(controller.session.state.gameplay.policySelections.sleep,'eight');
  assert.equal((await controller.load(version())).error,null);assert.deepEqual(controller.session.state,expected);
});

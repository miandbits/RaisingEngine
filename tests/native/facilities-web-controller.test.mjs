import test from 'node:test';
import assert from 'node:assert/strict';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import { WasmRuntime } from '../../dist/packages/native-bridge/wasm.js';
import { WebPlayerController } from '../../dist/packages/web-player/controller.js';
import { fixture } from '../gameplay/fixture.mjs';

test('web bridge forwards diet/service/sale commands to real Wasm, rejects stale repeats and restores exact state',async t=>{
  const add=value=>({op:'add',target:{kind:'attribute',attributeId:'stamina'},value:{op:'literal',value}});
  const {built}=await fixture(t,p=>{
    p.initialState.attributes.money=100;p.initialState.attributes.stamina=40;
    p.gameplay.diets=[{id:'balanced',name:'Balanced',locationId:'home',intervalPeriods:2,costs:[{attributeId:'money',amount:2}],effects:[add(1)]}];
    p.gameplay.services=[{id:'clinic',name:'Clinic',locationId:'market',maxQuantity:3,costs:[{attributeId:'money',amount:4}],effects:[add(10)]}];
    p.gameplay.shops[0].sellOffers=[{itemId:'flower',price:3,maxQuantity:3}];
  });
  assert.equal(built.ok,true,JSON.stringify(built.diagnostics));let saved=null;
  const controller=new WebPlayerController(built.snapshot,new WasmRuntime(await createModule()),{read:async()=>saved,write:async text=>{saved=text;}},()=>true);
  assert.equal((await controller.newRun(null)).error,null);
  const version=()=>({generation:controller.generation,revision:controller.session.state.stateRevision});
  const send=async action=>{const reply=await controller.act(action,version());assert.equal(reply.error,null);return reply.view.state;};
  await send({type:'setDiet',dietId:'balanced'});assert.equal(controller.session.state.attributes.money,100);
  await send({type:'visit',locationId:'city'});await send({type:'visit',locationId:'market'});
  await send({type:'buy',shopId:'florist',itemId:'flower',quantity:2});
  await send({type:'sell',shopId:'florist',itemId:'flower',quantity:1});
  const stale=version();await send({type:'useService',serviceId:'clinic',quantity:2});
  const expected=structuredClone(controller.session.state);assert.equal(expected.attributes.money,75);assert.equal(expected.attributes.stamina,60);assert.equal(expected.gameplay.inventory.flower,1);
  assert.match((await controller.act({type:'useService',serviceId:'clinic',quantity:2},stale)).error,/갱신/);assert.deepEqual(controller.session.state,expected);
  assert.equal((await controller.save(version())).error,null);await send({type:'useService',serviceId:'clinic',quantity:1});assert.equal((await controller.load(version())).error,null);assert.deepEqual(controller.session.state,expected);assert.equal(controller.session.state.gameplay.dietId,'balanced');
});

import test from 'node:test';
import assert from 'node:assert/strict';
import { fixture } from '../gameplay/fixture.mjs';
import { nativeCreateRun, nativeDispatchCommand, nativeReplay, nativeRequest } from '../../dist/packages/native-bridge/index.js';
import { createRun, dispatchCommand } from '../../dist/packages/core/run.js';
import { getGameplayView } from '../../dist/packages/gameplay/service.js';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';

const effect = (attributeId, value) => ({op:'add',target:{kind:'attribute',attributeId},value:{op:'literal',value}});
async function setup(t) {
  const {built} = await fixture(t); assert.equal(built.ok,true);
  const content=structuredClone(built.snapshot), g=content.project.gameplay;
  content.project.initialState.attributes.money=200;
  content.project.initialState.attributes.stamina=10;
  g.diets=[{id:'balanced',name:'Balanced',locationId:'home',intervalPeriods:3,costs:[{attributeId:'money',amount:7}],effects:[effect('stamina',2)]}];
  g.services=[{id:'clinic',name:'Clinic',locationId:'market',maxQuantity:3,costs:[{attributeId:'money',amount:5}],effects:[effect('stamina',10)]}];
  g.shops[0].offers[0].maxQuantity=2;
  g.shops[0].sellOffers=[{itemId:'flower',price:4,maxQuantity:2}];
  return content;
}
function run(content) {
  const origin={seed:42,mode:'full',initialOverrides:{attributes:{},flags:{},relationships:{}}}, commands=[];
  let state=nativeCreateRun(content,42,origin).state;
  assert.deepEqual(state,createRun(content,42,origin).state);
  return {
    get state(){return state;}, commands, origin,
    command(command, code) {
      const before=structuredClone(state), out=nativeDispatchCommand(content,state,command,state.stateRevision);
      const reference=dispatchCommand(content,state,command,state.stateRevision);
      if(out.ok)assert.deepEqual(out,reference);
      else {assert.equal(reference.ok,false);assert.equal(out.error.code,reference.error.code);assert.equal(out.error.path,reference.error.path);}
      if(code){assert.equal(out.ok,false,JSON.stringify(out));assert.equal(out.error.code,code);assert.deepEqual(out.state,before);}
      else {assert.equal(out.ok,true,JSON.stringify(out));state=out.state;commands.push(command);}
      return out;
    }
  };
}
test('facilities native: buy, sell, quantity treatment, limits, stale revision and deterministic replay',async t=>{
  const content=await setup(t), r=run(content);
  r.command({type:'useService',serviceId:'clinic',quantity:1},'WRONG_LOCATION');
  r.command({type:'visit',locationId:'city'});r.command({type:'visit',locationId:'market'});
  r.command({type:'buy',shopId:'florist',itemId:'flower',quantity:3},'QUANTITY_LIMIT');
  r.command({type:'buy',shopId:'florist',itemId:'flower',quantity:2});
  assert.equal(r.state.attributes.money,180);
  r.command({type:'sell',shopId:'florist',itemId:'flower',quantity:1});
  assert.equal(r.state.attributes.money,184);assert.equal(r.state.gameplay.inventory.flower,1);
  r.command({type:'sell',shopId:'florist',itemId:'flower',quantity:2},'INSUFFICIENT_ITEMS');
  r.command({type:'useService',serviceId:'clinic',quantity:2});
  assert.equal(r.state.attributes.money,174);assert.equal(r.state.attributes.stamina,30);
  r.command({type:'useService',serviceId:'clinic',quantity:4},'QUANTITY_LIMIT');
  r.command({type:'useService',serviceId:'clinic',quantity:0},'INVALID_COMMAND');
  const stale=nativeDispatchCommand(content,r.state,{type:'useService',serviceId:'clinic',quantity:1},r.state.stateRevision-1);
  assert.equal(stale.error.code,'REVISION_CONFLICT');assert.deepEqual(stale.state,r.state);
  assert.deepEqual(nativeReplay(content,r.origin,r.commands),{ok:true,state:r.state});
  const projection=nativeRequest({operation:'view',content,state:r.state});
  assert.equal(projection.gameplay.shops[0].sellOffers[0].price,4);
  assert.equal(projection.gameplay.services[0].maxQuantity,3);
  assert.deepEqual(projection.gameplay,getGameplayView(content,r.state));
});
test('facilities native: treatment no effect, duplicate costs and sale currency ceiling roll back',async t=>{
  const content=await setup(t);content.project.initialState.attributes.stamina=content.attributes.find(x=>x.id==='stamina').max;
  const r=run(content);r.command({type:'visit',locationId:'city'});r.command({type:'visit',locationId:'market'});
  r.command({type:'useService',serviceId:'clinic',quantity:1},'SERVICE_NO_EFFECT');
  const duplicate=structuredClone(content);duplicate.project.gameplay.services[0].costs=[{attributeId:'money',amount:120},{attributeId:'money',amount:120}];
  duplicate.project.initialState.attributes.stamina=10;
  const d=run(duplicate);d.command({type:'visit',locationId:'city'});d.command({type:'visit',locationId:'market'});
  d.command({type:'useService',serviceId:'clinic',quantity:1},'INSUFFICIENT_FUNDS');
  const ceiling=structuredClone(content);ceiling.project.initialState.attributes.money=ceiling.attributes.find(x=>x.id==='money').max;ceiling.project.gameplay.items[0].initialQuantity=1;
  const c=run(ceiling);c.command({type:'visit',locationId:'city'});c.command({type:'visit',locationId:'market'});
  c.command({type:'sell',shopId:'florist',itemId:'flower',quantity:1},'CURRENCY_FULL');
});
test('facilities native: persistent three-period diet charges exactly once and replays after save boundary',async t=>{
  const content=await setup(t);content.events=[];const r=run(content);
  r.command({type:'setDiet',dietId:'balanced'});assert.equal(r.state.attributes.money,200);
  r.command({type:'setDiet',dietId:'balanced'},'DIET_UNCHANGED');
  r.command({type:'setSchedule',activityIds:['rest','rest','rest']});
  r.command({type:'executeActivity'});assert.equal(r.state.gameplay.dietAppliedPeriod,0);assert.equal(r.state.gameplay.dietNextPeriod,3);
  assert.equal(r.state.lastActivityResult.changes.filter(x=>x.source.startsWith('/project/gameplay/diets')).length,2);
  const boundary=nativeReplay(content,r.origin,r.commands);assert.deepEqual(boundary,{ok:true,state:r.state});
  const encoded=await encodeSave(content,{origin:r.origin,commands:r.commands,state:r.state});assert.equal(encoded.ok,true,JSON.stringify(encoded));
  const loaded=await decodeSave(content,encoded.text);assert.equal(loaded.ok,true,JSON.stringify(loaded));assert.deepEqual(loaded.session.state,r.state);
  r.command({type:'acknowledgeResult'});r.command({type:'resolvePeriodEnd'});
  while(r.state.phase==='narrative') r.command({type:'advanceNarrative'});
  r.command({type:'setDiet',dietId:null});r.command({type:'setDiet',dietId:'balanced'});
  r.command({type:'executeActivity'});assert.equal(r.state.gameplay.dietAppliedPeriod,0);
  assert.equal(r.state.lastActivityResult.changes.filter(x=>x.source.startsWith('/project/gameplay/diets')).length,0);
  while(r.state.periodIndex<3){
    r.command({type:'acknowledgeResult'});r.command({type:'resolvePeriodEnd'});
    while(r.state.phase==='narrative') r.command({type:'advanceNarrative'});
    if(r.state.periodIndex<3)r.command({type:'executeActivity'});
  }
  r.command({type:'setSchedule',activityIds:['rest','rest','rest']});
  r.command({type:'executeActivity'});
  assert.equal(r.state.gameplay.dietAppliedPeriod,3);assert.equal(r.state.gameplay.dietNextPeriod,6);
  assert.equal(r.state.lastActivityResult.changes.filter(x=>x.source.startsWith('/project/gameplay/diets')).length,2);
  assert.deepEqual(nativeReplay(content,r.origin,r.commands),{ok:true,state:r.state});
});
test('facilities native: diet plus activity insufficient costs rejects whole activity including diet effects',async t=>{
  const content=await setup(t);content.project.initialState.attributes.money=10;
  content.activities.find(x=>x.id==='rest').costs=[{attributeId:'money',amount:5}];
  const r=run(content);r.command({type:'setDiet',dietId:'balanced'});r.command({type:'setSchedule',activityIds:['rest','rest','rest']});
  r.command({type:'executeActivity'},'INSUFFICIENT_COST');
  assert.equal(r.state.attributes.money,10);assert.equal(r.state.attributes.stamina,10);assert.equal(r.state.gameplay.dietAppliedPeriod,undefined);
});

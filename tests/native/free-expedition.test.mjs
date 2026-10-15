import test from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {createRun,dispatchCommand} from '../../dist/packages/core/run.js';
import {nativeCreateRun,nativeDispatchCommand,nativeReplay} from '../../dist/packages/native-bridge/index.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';
import {fixture} from '../systems/fixture.mjs';
const built=await buildSnapshot(await loadProject(path.resolve('tests/fixtures/projects/free-expedition')));
assert.equal(built.ok,true,JSON.stringify(built.diagnostics));const content=built.snapshot;
function session(overrides={}){
 const origin={seed:42,mode:'full',initialOverrides:{attributes:{'day-stage':1,...overrides},flags:{},relationships:{}}};
 const first=nativeCreateRun(content,origin.seed,origin);assert.deepEqual(first,createRun(content,origin.seed,origin));assert.equal(first.ok,true);
 let state=first.state;const commands=[];
 const send=(command,ok=true)=>{const result=nativeDispatchCommand(content,state,command,state.stateRevision);assert.deepEqual(result,dispatchCommand(content,state,command,state.stateRevision));assert.equal(result.ok,ok,JSON.stringify(result));if(ok){state=result.state;commands.push(command);}else assert.deepEqual(result.state,state);return result;};
 return {origin,commands,send,get state(){return state;},act:(id,ok=true)=>send({type:'executeAction',actionId:id,quantity:1},ok),
 async roundtrip(){const e=await encodeSave(content,{origin,state,commands});assert.equal(e.ok,true,JSON.stringify(e));const d=await decodeSave(content,e.text);assert.equal(d.ok,true);assert.deepEqual(d.session.state,state);assert.deepEqual(nativeReplay(content,origin,commands),{ok:true,state});return e.text;}};
}
test('free expedition restores an existing schedule; movement, combat, rewards and save journal agree in both cores',async()=>{
 const s=session();s.send({type:'setSchedule',activityIds:['rest']});const before=structuredClone(s.state);
 s.act('depart-forest');assert.equal(s.state.phase,'expedition');assert.equal(s.state.attributes.energy,7);await s.roundtrip();
 s.send({type:'moveExpedition',nodeId:'exit'},false);
 s.send({type:'moveExpedition',nodeId:'clearing'});assert.equal(s.state.phase,'battle');
 s.send({type:'battleAction',action:'useItem',itemId:'potion'},false);
 s.send({type:'battleAction',action:'guard'});s.send({type:'battleAction',action:'attack'});await s.roundtrip();
 s.send({type:'battleAction',action:'useItem',itemId:'potion'});
 for(let i=0;s.state.phase==='battle'&&i<20;i++)s.send({type:'battleAction',action:'attack'});
 assert.equal(s.state.systems.lastBattle.outcome,'victory');assert.equal(s.state.attributes.money,550);await s.roundtrip();
 const inventory=structuredClone(s.state.gameplay.inventory);s.send({type:'moveExpedition',nodeId:'entrance'});s.send({type:'moveExpedition',nodeId:'clearing'});
 assert.equal(s.state.phase,'expedition');assert.equal(s.state.attributes.money,550);assert.deepEqual(s.state.gameplay.inventory,inventory);
 s.send({type:'moveExpedition',nodeId:'exit'});s.send({type:'returnFromExpedition'});
 assert.equal(s.state.phase,before.phase);assert.equal(s.state.periodIndex,before.periodIndex);assert.deepEqual(s.state.schedule,before.schedule);assert.deepEqual(s.state.lastActivity,before.lastActivity);await s.roundtrip();
 s.send({type:'acknowledgeResult'},false);
});
test('free expedition retreat, flee, defeat and rejected departure preserve time and atomic costs',async()=>{
 for(const route of ['retreat','flee','defeat']){
  const s=session(route==='defeat'?{hp:1}:{});s.act('depart-forest');
  s.send({type:'moveExpedition',nodeId:route==='retreat'?'safe':'clearing'});
  if(route==='retreat')s.send({type:'returnFromExpedition'});
  else s.send({type:'battleAction',action:route==='flee'?'flee':'guard'});
  assert.equal(s.state.phase,'planning');assert.equal(s.state.periodIndex,0);assert.equal(s.state.attributes.energy,7);assert.equal(s.state.systems.lastExpedition.outcome,route==='defeat'?'defeated':'retreated');await s.roundtrip();
 }
 const s=session({energy:2});s.act('depart-forest',false);assert.equal(s.state.attributes.energy,2);assert.equal(s.state.systems.expedition,null);
});
test('authoring reports missing, conflicting and nested expedition references at their data location',async()=>{
 const f=await fixture(p=>{
  const action=(id,extra={})=>({id,name:id,costs:[],effects:[],...extra});
  p.gameplay.actions=[action('missing',{expeditionId:'missing'}),action('bad-quantity',{expeditionId:'forest',maxQuantity:2}),action('conflict',{expeditionId:'forest',eventId:'friendship-unlock'}),action('trip',{expeditionId:'forest'}),action('nested',{flow:[{id:'go',thenActionId:'trip'}]})];
  p.gameplay.rules=[{id:'on-arrival',name:'Arrival',trigger:{type:'locationEnter'},actionId:'trip'}];
 });
 assert.equal(f.built.ok,false);
 for(const code of ['ACTION_EXPEDITION_REFERENCE','ACTION_EXPEDITION_CONTEXT','FLOW_EXPEDITION_ACTION','RULE_EVENT_ACTION'])assert.ok(f.built.diagnostics.some(d=>d.code===code&&d.pointer),code);
});

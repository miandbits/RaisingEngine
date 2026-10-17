import test from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {createRun,dispatchCommand} from '../../dist/packages/core/run.js';
import {nativeCreateRun,nativeDispatchCommand,nativeReplay} from '../../dist/packages/native-bridge/index.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';
import {fixture} from '../systems/fixture.mjs';

const loaded=await buildSnapshot(await loadProject(path.resolve('tests/fixtures/projects/free-expedition')));
assert.equal(loaded.ok,true,JSON.stringify(loaded.diagnostics));
const effect=(id,value,op='add')=>({op,target:{kind:'attribute',attributeId:id},value:{op:'literal',value}});
const action=(id,effects=[],costs=[])=>({id,name:id,costs,effects});
function setup(repeat=false){
 const c=structuredClone(loaded.snapshot),d=c.project.systems,g=c.project.gameplay;
 const trip=d.expeditions.find(e=>e.id===g.actions.find(a=>a.id==='depart-forest').expeditionId);
 Object.assign(trip,{entryNodeId:'entrance',entryActionId:'prepare',moveActionId:'step',completionActionId:'complete',retreatActionId:'retreat',defeatActionId:'defeat',nodes:[
  {id:'entrance',name:'Entrance',connections:['safe','locked']},
  {id:'safe',name:'Stream',connections:['entrance','exit'],entryActionId:'gather',repeatEntryAction:repeat,actionIds:['inspect']},
  {id:'locked',name:'Locked',connections:['exit'],requirements:{op:'compare',left:{op:'read',target:{kind:'attribute',attributeId:'money'}},comparator:'gte',right:{op:'literal',value:1000}}},
  {id:'exit',name:'Exit',connections:[],exit:true},
 ]});
 g.actions.push(action('prepare',[effect('energy',10,'set')]),action('step',[],[{attributeId:'energy',amount:2}]),action('gather',[effect('money',5)]),action('inspect',[effect('money',3)],[{attributeId:'energy',amount:1}]),action('complete',[effect('money',7)]),action('retreat',[effect('money',1)]),action('defeat',[effect('money',-2)]));
 const origin={seed:42,mode:'full',initialOverrides:{attributes:{'day-stage':1},flags:{},relationships:{}}},commands=[];
 let state=nativeCreateRun(c,42,origin).state;assert.deepEqual(state,createRun(c,42,origin).state);
 return {c,origin,commands,get state(){return state;},send(command,error){const before=structuredClone(state),n=nativeDispatchCommand(c,state,command,state.stateRevision),ts=dispatchCommand(c,state,command,state.stateRevision);assert.deepEqual(n,ts);if(error){assert.equal(n.ok,false,JSON.stringify(n));assert.equal(n.error.code,error);assert.deepEqual(n.state,before);}else{assert.equal(n.ok,true,JSON.stringify(n));state=n.state;commands.push(command);}return n;},async save(){const e=await encodeSave(c,{origin,state,commands});assert.equal(e.ok,true,JSON.stringify(e));const d=await decodeSave(c,e.text);assert.equal(d.ok,true,JSON.stringify(d));assert.deepEqual(d.session.state,state);assert.deepEqual(nativeReplay(c,origin,commands),{ok:true,state});}};
}
test('node actions, movement cost, once-per-trip entry and save replay agree across cores',async()=>{
 const r=setup(),act=id=>r.send({type:'executeAction',actionId:id,quantity:1}),move=id=>r.send({type:'moveExpedition',nodeId:id});
 act('depart-forest');assert.equal(r.state.attributes.energy,10);
 r.send({type:'executeAction',actionId:'inspect',quantity:1},'WRONG_PHASE');
 r.send({type:'moveExpedition',nodeId:'locked'},'NODE_LOCKED');assert.equal(r.state.attributes.energy,10);
 move('safe');assert.equal(r.state.attributes.money,505);assert.equal(r.state.attributes.energy,8);await r.save();
 act('inspect');assert.equal(r.state.attributes.money,508);assert.equal(r.state.attributes.energy,7);
 move('entrance');move('safe');assert.equal(r.state.attributes.money,508);assert.deepEqual(r.state.systems.expedition.visitedNodes,['safe']);await r.save();
 move('exit');r.send({type:'returnFromExpedition'});assert.equal(r.state.attributes.money,515);assert.equal(r.state.phase,'planning');await r.save();
 act('prepare');act('depart-forest');move('safe');assert.equal(r.state.attributes.money,520,'new trip resets entry history');r.send({type:'returnFromExpedition'});assert.equal(r.state.attributes.money,521);await r.save();
});
test('repeated entries opt in and insufficient movement budget leaves the whole state unchanged',async()=>{
 const r=setup(true),move=id=>r.send({type:'moveExpedition',nodeId:id});r.send({type:'executeAction',actionId:'depart-forest',quantity:1});
 move('safe');move('entrance');move('safe');assert.equal(r.state.attributes.money,510);
 move('entrance');move('safe');assert.equal(r.state.attributes.energy,0);
 r.send({type:'moveExpedition',nodeId:'exit'},'INSUFFICIENT_FUNDS');await r.save();r.send({type:'returnFromExpedition'});assert.equal(r.state.attributes.money,516);await r.save();
});
test('exploration references reject event recursion and return fees',async()=>{
 const f=await fixture(p=>{p.gameplay.actions=[action('fee',[],[{attributeId:'money',amount:1}]),{...action('story'),eventId:'friendship-unlock'}];const e=p.systems.expeditions[0];e.completionActionId='fee';e.moveActionId='missing';e.nodes[0].actionIds=['story'];});
 assert.equal(f.built.ok,false);for(const code of ['REFERENCE_MISSING','EXPEDITION_ACTION_CONTEXT','EXPEDITION_RETURN_COST'])assert.ok(f.built.diagnostics.some(d=>d.code===code),code);
});

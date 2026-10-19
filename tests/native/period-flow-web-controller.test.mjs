import test from 'node:test';
import assert from 'node:assert/strict';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import {WasmRuntime} from '../../dist/packages/native-bridge/wasm.js';
import {WebPlayerController} from '../../dist/packages/web-player/controller.js';
import {fixture} from '../gameplay/fixture.mjs';
import validateSave from '../../apps/web/generated/save-validator.mjs';

const add=(id,value)=>({op:'add',target:{kind:'attribute',attributeId:id},value:{op:'literal',value}});
test('Web period flow pauses for free actions and choices, restores boundaries, and charges each activity once',async t=>{
 const {built}=await fixture(t);assert.equal(built.ok,true);const content=structuredClone(built.snapshot);
 content.flags.push({id:'free-done',label:'Done',initial:false});
 content.project.initialState.attributes.money=10;content.project.initialState.attributes.stamina=30;
 content.project.schedulePlayback='continuous';
 content.project.periodFlow={intermission:{screenId:'city',completionFlagId:'free-done',entryEffects:[add('stamina',-1)]}};
 content.project.gameplay.actions=[{id:'visit',name:'Visit',costs:[{attributeId:'money',amount:1}],effects:[],eventId:'visit'}];
 content.project.gameplay.policyGroups=[];
 content.activities=[{id:'rest',label:'Rest',requirements:{op:'always'},costs:[{attributeId:'money',amount:2}],baseEffects:[add('stamina',4)]}];
 content.events=[{id:'visit',label:'Visit',trigger:'interaction',priority:10,window:{fromPeriod:0,throughPeriod:35},requirements:{op:'always'},entryNodeId:'hello',nodes:[
  {id:'hello',type:'dialogue',text:'Welcome',next:'choice'},
  {id:'choice',type:'choice',prompt:'Return?',choices:[{id:'yes',text:'Return',requirements:{op:'always'},effects:[{op:'setFlag',flagId:'free-done',value:true}],next:'end'}]},
  {id:'end',type:'end'},
 ]}];
 const runtime=new WasmRuntime(await createModule());let saved;
 const player=new WebPlayerController(content,runtime,{read:async()=>saved,write:async text=>{saved=text;}},validateSave);
 const version=()=>({generation:player.generation,revision:player.session.state.stateRevision});
 const act=async action=>{const reply=await player.act(action,version());assert.equal(reply.error,null,reply.error);return reply;};
 const checkpoint=async()=>{const state=structuredClone(player.session.state);assert.equal((await player.save(version())).error,null);assert.equal((await player.load(version())).error,null);assert.deepEqual(player.session.state,state);};
 assert.equal((await player.newRun(null)).error,null);
 await act({type:'schedule',activityIds:['rest','rest','rest']});await act({type:'execute'});
 assert.equal(player.session.state.attributes.money,8);await checkpoint();
 await act({type:'continue'});assert.equal(player.session.state.activityIntermission,true);assert.equal(player.session.state.attributes.stamina,33);
 const pending=structuredClone(player.session.state);await act({type:'scheduleStep'});assert.deepEqual(player.session.state,pending);
 const rejected=await player.act({type:'execute'},version());assert.match(rejected.error,/INTERMISSION_PENDING/);assert.deepEqual(player.session.state,pending);await checkpoint();
 await act({type:'executeAction',actionId:'visit',quantity:1});assert.equal(player.session.state.phase,'narrative');assert.equal(player.session.state.attributes.money,7);await checkpoint();
 await act({type:'continue'});assert.equal(player.session.state.activeNarrative.nodeId,'choice');await act({type:'scheduleStep'});assert.equal(player.session.state.activeNarrative.nodeId,'choice');await checkpoint();
 await act({type:'choose',choiceId:'yes'});assert.equal(player.session.state.phase,'ready');assert.equal(player.session.state.flags['free-done'],true);await checkpoint();
 for(const action of ['continue','scheduleStep']){
  const boundary=saved;await act({type:action});assert.equal(player.session.state.periodIndex,1);assert.equal(player.session.state.activityIntermission,undefined);assert.equal(player.session.state.attributes.money,7);
  await act({type:'execute'});assert.equal(player.session.state.attributes.money,5);assert.equal(player.session.state.attributes.stamina,37);
  saved=boundary;assert.equal((await player.load(version())).error,null);
 }
});

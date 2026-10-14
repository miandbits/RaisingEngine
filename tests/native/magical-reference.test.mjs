import test from 'node:test';
import assert from 'node:assert/strict';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {createRun,dispatchCommand} from '../../dist/packages/core/run.js';
import {nativeDispatchCommand} from '../../dist/packages/native-bridge/index.js';
import {WasmRuntime} from '../../dist/packages/native-bridge/wasm.js';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';

test('observed childhood slice: four months, three result beats, C++/Wasm parity and save replay',async()=>{
 const loaded=await loadProject('projects/magical-childhood-reference'),built=await buildSnapshot(loaded);
 assert.equal(built.ok,true,JSON.stringify(built)); const content=built.snapshot;
 const wasm=new WasmRuntime(await createModule());
 const origin={seed:572,mode:'full',initialOverrides:{attributes:{},flags:{},relationships:{}}};
 let state=createRun(content,origin.seed,origin).state;const commands=[];
 const send=command=>{
  const out=dispatchCommand(content,state,command,state.stateRevision);assert.equal(out.ok,true,JSON.stringify(out));
  assert.deepEqual(nativeDispatchCommand(content,state,command,state.stateRevision),out);
  const response=wasm.request({operation:'command',content,state,command,expectedRevision:state.stateRevision});
  assert.deepEqual(response,out);state=out.state;commands.push(command);
 };
 const act=actionId=>send({type:'executeAction',actionId,quantity:1});
 for(let month=0;month<4;month++){
  assert.equal(state.attributes.energy,3);act('open-play');act('read-book');
  assert.equal(state.attributes.energy,0);assert.equal(state.attributes.int,22+month*6);assert.equal(state.attributes.beat,1);
  const save=await encodeSave(content,{origin,commands,state});assert.equal(save.ok,true);
  const restored=await decodeSave(content,save.text);assert.equal(restored.ok,true,JSON.stringify(restored));assert.deepEqual(restored.session.state,state);
  act('next-result');act('next-result');assert.equal(state.attributes.int,26+month*6);assert.equal(state.attributes.stress,(month+1)*2);
  assert.equal(state.attributes.beat,3);act('finish-reading');
  const before=structuredClone(state);const rejected=dispatchCommand(content,state,{type:'executeAction',actionId:'read-book',quantity:1},state.stateRevision);assert.equal(rejected.ok,false);assert.deepEqual(state,before);
  send({type:'setSchedule',activityIds:['rest']});send({type:'executeActivity'});send({type:'acknowledgeResult'});
  let guard=0;while(!['planning','finished'].includes(state.phase)&&guard++<10){send({type:state.phase==='period-end'?'resolvePeriodEnd':'advanceNarrative'});}
  assert.ok(guard<10);assert.equal(state.attributes.energy,3);
 }
 assert.equal(state.phase,'finished');assert.equal(state.attributes.int,44);assert.equal(state.attributes.money,500);
 const save=await encodeSave(content,{origin,commands,state});assert.equal(save.ok,true);const restored=await decodeSave(content,save.text);assert.equal(restored.ok,true,JSON.stringify(restored));assert.deepEqual(restored.session.state,state);
});

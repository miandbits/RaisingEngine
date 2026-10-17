import test from 'node:test';
import assert from 'node:assert/strict';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import {WasmRuntime} from '../../dist/packages/native-bridge/wasm.js';
import {WebPlayerController} from '../../dist/packages/web-player/controller.js';
import {content,doc,start,send} from '../runtime/helpers.mjs';
import {encodeSave} from '../../dist/packages/runtime-service/index.js';
import {resolveScreen} from '../../dist/packages/visual/routing.js';
import validateSave from '../../apps/web/generated/save-validator.mjs';

test('WASM web controller loads native navigation, validates new optional save field and preserves old saves',async()=>{
  const c=await content(p=>doc(p,'project').visual={width:1280,height:720,routing:{defaultScreenId:'canvas',phases:[]},screens:[{id:'canvas',label:'Main',elements:[]},{id:'details',label:'Details',elements:[]}]});
  const runtime=new WasmRuntime(await createModule());
  let session=start(c);session=send(c,session,{type:'navigate',screenId:'details'});
  const encoded=await encodeSave(c,session);assert.equal(encoded.ok,true);let saved=encoded.text;
  assert.equal(validateSave(JSON.parse(saved)),true,JSON.stringify(validateSave.errors));
  const player=new WebPlayerController(c,runtime,{read:async()=>saved,write:async text=>{saved=text;}},validateSave);
  let reply=await player.load(null);assert.equal(reply.error,null,JSON.stringify(reply.error));assert.deepEqual(reply.view.state,session.state);assert.equal(resolveScreen(c.project.visual,reply.view.state),'details');
  const version=()=>({generation:player.generation,revision:player.session.state.stateRevision});
  reply=await player.save(version());assert.equal(reply.error,null);reply=await player.newRun(version());assert.equal(reply.error,null);assert.equal(reply.view.state.uiScreenId,undefined);
  reply=await player.load(version());assert.equal(reply.error,null);assert.equal(reply.view.state.uiScreenId,'details');
  const legacy=await encodeSave(c,start(c));saved=legacy.text;assert.equal(validateSave(JSON.parse(saved)),true);reply=await player.load(version());assert.equal(reply.error,null);assert.equal(reply.view.state.uiScreenId,undefined);
});

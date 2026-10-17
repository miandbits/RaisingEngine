import test from 'node:test';
import assert from 'node:assert/strict';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import {WasmRuntime} from '../../dist/packages/native-bridge/wasm.js';
import {WebPlayerController} from '../../dist/packages/web-player/controller.js';
import {content,scenarios} from '../runtime/helpers.mjs';
import {runScenario,encodeSave} from '../../dist/packages/runtime-service/index.js';

test('Wasm web player reconstructs relationship display history from existing ending saves',async()=>{
  const c=await content(),runtime=new WasmRuntime(await createModule());
  for(const scenario of scenarios){
    const ended=await runScenario(c,scenario);assert.equal(ended.ok,true);
    const encoded=await encodeSave(c,ended.session);assert.equal(encoded.ok,true);let saved=encoded.text;
    const player=new WebPlayerController(c,runtime,{read:async()=>saved,write:async text=>{saved=text;}},()=>true);
    let reply=await player.load(null);assert.equal(reply.error,null);
    assert.equal(reply.view.state.phase,'finished');assert.deepEqual(reply.view.relationshipChanges,ended.session.relationshipChanges);
    const version=()=>({generation:player.generation,revision:player.session.state.stateRevision});
    reply=await player.save(version());assert.equal(reply.error,null);assert.equal(Object.hasOwn(JSON.parse(saved),'relationshipChanges'),false);
    reply=await player.newRun(version());assert.equal(reply.error,null);assert.deepEqual(reply.view.relationshipChanges??{},{});
    reply=await player.load(version());assert.equal(reply.error,null);assert.deepEqual(reply.view.state,ended.session.state);assert.deepEqual(reply.view.relationshipChanges,ended.session.relationshipChanges);
  }
});

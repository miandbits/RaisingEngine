import test from 'node:test';
import assert from 'node:assert/strict';
import {resolve} from 'node:path';
import {PlayerController} from '../../dist/packages/player-service/controller.js';
import {WebPlayerController} from '../../dist/packages/web-player/controller.js';
import {WasmRuntime} from '../../dist/packages/native-bridge/wasm.js';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import {createSession} from '../../dist/packages/runtime-service/session.js';

test('commitPlan executes the same canonical command pair with version guards in desktop and Wasm',async()=>{
  const desktop=new PlayerController();assert.equal((await desktop.openProject(resolve('examples/academy-demo'))).error,null);desktop.newRun(null);
  let saved=null;const web=new WebPlayerController(desktop.content,new WasmRuntime(await createModule()),{read:async()=>saved,write:async text=>{saved=text}},()=>true);await web.newRun(null);
  const version=c=>({generation:c.generation,revision:c.session.state.stateRevision});const plan={type:'commitPlan',activityIds:['study','work','rest']};
  for(const controller of [desktop,web]){
    const old=version(controller);assert.equal((await controller.act(plan,old)).error,null);assert.equal(controller.session.state.phase,'activity-result');
    assert.deepEqual(controller.session.commands.map(c=>c.type),['setSchedule','executeActivity']);
    const before=controller.session;assert.ok((await controller.act(plan,old)).error);assert.equal(controller.session,before);
  }
  assert.deepEqual(web.session.state,desktop.session.state);assert.deepEqual(web.session.commands,desktop.session.commands);
  const expected=structuredClone(web.session);assert.equal((await web.save(version(web))).error,null);await web.act({type:'continue'},version(web));assert.equal((await web.load(version(web))).error,null);assert.deepEqual(web.session,expected);
  for(const controller of [desktop,web]){
    controller.session=createSession(desktop.content,42,{initialOverrides:{attributes:{money:0},flags:{},relationships:{}}}).session;
    assert.ok((await controller.act(plan,version(controller))).error);assert.equal(controller.session.state.phase,'ready');assert.equal(controller.session.commands.length,1);
    assert.equal((await controller.act({type:'commitPlan',activityIds:['rest','rest','rest']},version(controller))).error,null);assert.equal(controller.session.state.phase,'activity-result');
  }
});

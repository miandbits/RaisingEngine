import test from 'node:test';
import assert from 'node:assert/strict';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import { WasmRuntime } from '../../dist/packages/native-bridge/wasm.js';
import { nativeRequest } from '../../dist/packages/native-bridge/index.js';
import { createRun, dispatchCommand } from '../../dist/packages/core/run.js';
import { WebPlayerController } from '../../dist/packages/web-player/controller.js';
import validateSave from '../../apps/web/generated/save-validator.mjs';
import { content, doc } from '../runtime/helpers.mjs';

const wasm = new WasmRuntime(await createModule());
const named = () => content(p => { doc(p, 'project').userFields = [
  { id: 'player-name', type: 'text', label: '이름', initial: '올리브', minLength: 1, maxLength: 4 },
  { id: 'family-name', type: 'text', label: '가족', initial: '가족', minLength: 0, maxLength: 40 },
]; });

test('TS, native executable/addon backend and actual Wasm keep identical name state and transitions', async () => {
  const c = await named();
  for (const userValues of [undefined, {}, { 'player-name': '미😀나', 'family-name': '  가문  ' }, { 'family-name': '' }]) {
    const options = { mode: 'full', ...(userValues === undefined ? {} : { userValues }) };
    const request = { operation: 'create', content: c, seed: 123, options };
    let expected = createRun(c, 123, options); assert.equal(expected.ok, true);
    assert.deepEqual(nativeRequest(request), expected); assert.deepEqual(wasm.request(request), expected);
    for (const command of [{ type: 'setSchedule', activityIds: ['study', 'work', 'rest'] }, { type: 'executeActivity' }, { type: 'acknowledgeResult' }]) {
      const previous = expected.state; expected = dispatchCommand(c, previous, command, previous.stateRevision);
      assert.equal(expected.ok, true, JSON.stringify(expected));
      const r = { operation: 'command', content: c, state: previous, command, expectedRevision: previous.stateRevision };
      assert.deepEqual(nativeRequest(r), expected); assert.deepEqual(wasm.request(r), expected);
    }
  }
  const old = await content(), expected = createRun(old, 42, { mode: 'full' });
  assert.equal(Object.hasOwn(expected.state, 'userValues'), false);
  for (const execute of [nativeRequest, r => wasm.request(r)]) assert.deepEqual(execute({ operation: 'create', content: old, seed: 42, options: { mode: 'full' } }), expected);
});

test('raw native and Wasm JSON-representable input errors match TS field diagnostics', async () => {
  const c = await named();
  for (const userValues of [null, [], 'wrong', { 'unknown/field~': 'x' }, { 'player-name': 1 }, { 'player-name': '' }, { 'player-name': '12345' }, { 'player-name': 'x\n' }, { 'player-name': '\u0085' }, { 'player-name': '\u2029' }]) {
    const options = { mode: 'full', userValues }, expected = createRun(c, 42, options);
    assert.equal(expected.ok, false);
    const r = { operation: 'create', content: c, seed: 42, options };
    assert.deepEqual(nativeRequest(r), expected); assert.deepEqual(wasm.request(r), expected);
  }
  // Strict JSON/UTF-8 transport rejects unpaired surrogates before reaching C++ field validation.
  assert.equal(wasm.request({ operation: 'create', content: c, seed: 42, options: { mode: 'full', userValues: { 'player-name': '\ud800' } } }).ok, false);
});

test('actual Wasm web creation rejects lossy JS inputs and saves/reloads Unicode values atomically', async () => {
  const c = await named(); let saved;
  const web = new WebPlayerController(c, wasm, { read: async () => saved, write: async text => { saved = text; } }, validateSave);
  const version = () => ({ generation: web.generation, revision: web.session.state.stateRevision });
  assert.equal((await web.newRun(null, { 'player-name': '미😀나' })).error, null);
  const session = web.session;
  for (const bad of [{ 'player-name': undefined }, { unknown: undefined }, { 'player-name': '\ud800' }, { 'player-name': '\u0000' }, null]) {
    const reply = await web.newRun(version(), bad); assert.ok(reply.error); assert.equal(web.session, session);
  }
  assert.equal((await web.save(version())).error, null);
  assert.equal((await web.newRun(version(), { 'player-name': '다른' })).error, null);
  assert.equal((await web.load(version())).error, null); assert.deepEqual(web.session, session);
  const bytes = saved, tamper = JSON.parse(bytes); tamper.origin.userValues['player-name'] = '가짜'; saved = JSON.stringify(tamper);
  const before = web.session; assert.ok((await web.load(version())).error); assert.equal(web.session, before);
  saved = bytes; assert.equal((await web.load(version())).error, null);
});

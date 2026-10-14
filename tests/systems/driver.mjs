import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { createRun, dispatchCommand } from '../../dist/packages/core/run.js';
import { getSystemsView } from '../../dist/packages/systems/service.js';
import { nativeCreateRun, nativeDispatchCommand, nativeRequest } from '../../dist/packages/native-bridge/index.js';
import { WasmRuntime } from '../../dist/packages/native-bridge/wasm.js';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';

export async function driver(content, wasm = false) {
  const seed = 129, origin = { seed, mode: 'full', initialOverrides: { attributes: {}, flags: {}, relationships: {} } };
  let state = createRun(content, seed, origin).state;
  assert.ok(state); assert.deepEqual(nativeCreateRun(content, seed, origin).state, state);
  const createWasm = wasm ? (await import('../../dist/wasm/raising-wasm.mjs')).default : null;
  const web = createWasm ? new WasmRuntime(await createWasm({ wasmBinary: await readFile('dist/wasm/raising-wasm.wasm') })) : null;
  if (web) assert.deepEqual(web.request({ operation: 'create', content, seed, options: origin }).state, state);
  const commands = [];
  return { get state() { return state; }, async send(command, error) {
    const before = state, result = dispatchCommand(content, state, command, state.stateRevision);
    const native = nativeDispatchCommand(content, state, command, state.stateRevision);
    assert.equal(result.ok, !error, JSON.stringify(result.error)); assert.equal(native.ok, result.ok, JSON.stringify(native.error));
    if (error) { assert.equal(result.error.code, error); assert.equal(native.error.code, error); assert.strictEqual(result.state, before); return result; }
    assert.deepEqual(native.state, result.state, JSON.stringify(command)); assert.deepEqual(native.transition, result.transition);
    if (web) { const output = web.request({ operation: 'command', content, state, command, expectedRevision: state.stateRevision }); assert.ok(output.ok, JSON.stringify(output)); assert.deepEqual(output.state, result.state); assert.deepEqual(web.request({ operation: 'view', content, state: result.state }).systems, getSystemsView(content, result.state)); }
    state = result.state; commands.push(command);
    assert.deepEqual(nativeRequest({ operation: 'view', content, state }).systems, getSystemsView(content, state));
    const saved = await encodeSave(content, { origin, commands, state }); assert.ok(saved.ok, JSON.stringify(saved));
    const restored = await decodeSave(content, saved.text); assert.ok(restored.ok, JSON.stringify(restored)); assert.deepEqual(restored.session.state, state);
    return result;
  } };
}
export async function start(d, accept = true) {
  if (accept) await d.send({ type: 'acceptQuest', questId: 'forest-help' });
  await d.send({ type: 'equip', equipmentId: 'iron-sword' });
  await d.send({ type: 'setSchedule', activityIds: ['forest-trip', 'rest', 'rest'] }); await d.send({ type: 'executeActivity' });
}
export async function win(d) { for (let i = 0; d.state.phase === 'battle' && i < 50; i++) await d.send({ type: 'battleAction', action: 'attack' }); assert.equal(d.state.phase, 'expedition'); }
export async function settle(d, content) {
  for (let i = 0; i < 100; i++) {
    if (d.state.phase === 'activity-result') await d.send({ type: 'acknowledgeResult' });
    else if (d.state.phase === 'period-end') await d.send({ type: 'resolvePeriodEnd' });
    else if (d.state.phase === 'narrative') {
      const cursor = d.state.activeNarrative, story = (cursor.kind === 'event' ? content.events : content.endings).find(e => e.id === cursor.id), node = story.nodes.find(n => n.id === cursor.nodeId);
      await d.send(node.type === 'choice' ? { type: 'choose', choiceId: node.choices[0].id } : { type: 'advanceNarrative' });
    } else return;
  }
  assert.fail('narrative did not settle');
}

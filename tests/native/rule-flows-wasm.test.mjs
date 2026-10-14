import test from 'node:test';
import assert from 'node:assert/strict';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import { WasmRuntime } from '../../dist/packages/native-bridge/wasm.js';
import { nativeRequest } from '../../dist/packages/native-bridge/index.js';
import { createRun, dispatchCommand } from '../../dist/packages/core/run.js';
import { fixture } from '../gameplay/fixture.mjs';
const wasm = new WasmRuntime(await createModule());
test('Wasm/native/TS agree on flow inventory, authored day hooks, save replay, inspect and rollback', async t => {
  const { built } = await fixture(t); assert.equal(built.ok, true);
  const content = structuredClone(built.snapshot), g = content.project.gameplay; content.events = [];
  content.project.initialState.attributes.money = 200;
  g.actions = [
    { id: 'income', name: 'Income', costs: [], effects: [{ op: 'add', target: { kind: 'attribute', attributeId: 'money' }, value: { op: 'literal', value: 1 } }] },
    { id: 'flower', name: 'Flower', costs: [], effects: [{ op: 'add', target: { kind: 'inventory', itemId: 'flower' }, value: { op: 'literal', value: 1 } }] },
    { id: 'bundle', name: 'Bundle', costs: [{ attributeId: 'money', amount: 5 }], effects: [], flow: [{ id: 'item', thenActionId: 'flower' }, { id: 'bonus', thenActionId: 'income' }] },
  ];
  g.rules = [{ id: 'daily', name: 'Daily', trigger: { type: 'dayStart' }, actionId: 'income' }]; g.ruleCalendar = { daysPerMonth: 9 };
  const origin = { seed: 42, mode: 'full', initialOverrides: { attributes: {}, flags: {}, relationships: {} } }, commands = [];
  let state = createRun(content, 42, origin).state;
  assert.deepEqual(wasm.request({ operation: 'create', content, seed: 42, options: origin }).state, state);
  const send = command => {
    const expected = dispatchCommand(content, state, command, state.stateRevision), request = { operation: 'command', content, state, command, expectedRevision: state.stateRevision };
    assert.deepEqual(wasm.request(request), expected); assert.deepEqual(nativeRequest(request), expected);
    if (expected.ok) { state = expected.state; commands.push(command); }
    return expected;
  };
  send({ type: 'executeAction', actionId: 'bundle', quantity: 1 }); assert.equal(state.attributes.money, 196);
  send({ type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] }); send({ type: 'executeActivity' }); assert.equal(state.attributes.money, 199);
  send({ type: 'acknowledgeResult' }); send({ type: 'resolvePeriodEnd' });
  send({ type: 'executeAction', actionId: 'bundle', quantity: 1 }); send({ type: 'executeAction', actionId: 'bundle', quantity: 1 });
  const before = structuredClone(state), fail = send({ type: 'executeAction', actionId: 'bundle', quantity: 1 });
  assert.equal(fail.error.code, 'INVENTORY_RANGE'); assert.deepEqual(state, before);
  const inspection = { operation: 'inspect', content, state, command: { type: 'executeAction', actionId: 'income', quantity: 1 }, expectedRevision: state.stateRevision };
  assert.deepEqual(wasm.request(inspection), nativeRequest(inspection)); assert.deepEqual(state, before);
  assert.deepEqual(wasm.request({ operation: 'replay', content, origin, commands }), { ok: true, state });
});

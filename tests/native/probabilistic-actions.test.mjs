import test from 'node:test';
import assert from 'node:assert/strict';
import { fixture } from '../gameplay/fixture.mjs';
import { createRun, dispatchCommand } from '../../dist/packages/core/run.js';
import { nativeCreateRun, nativeDispatchCommand } from '../../dist/packages/native-bridge/index.js';
import { getGameplayView } from '../../dist/packages/gameplay/service.js';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';

const literal = value => ({ op: 'literal', value });
const effect = (id, value) => ({ op: 'add', target: { kind: 'attribute', attributeId: id }, value: literal(value) });
const action = (id, effects = [], extra = {}) => ({ id, name: id, costs: [], effects, ...extra });
async function content(t, chance = literal(5000), condition, failureCost = 0) {
  const f = await fixture(t, p => {
    p.initialState.attributes.money = 200;
    p.gameplay.actions = [
      action('practice', [], { costs: [{ attributeId: 'money', amount: 1 }], flow: [
        { id: 'result', thenActionId: 'good', elseActionId: 'bad', chance, ...(condition ? { condition } : {}) },
      ] }),
      action('good', [effect('knowledge', 3)]),
      action('bad', [effect('knowledge', 1)], { costs: [{ attributeId: 'money', amount: failureCost }] }),
    ];
  });
  assert.equal(f.built.ok, true, JSON.stringify(f.built));
  return f.built.snapshot;
}
const command = { type: 'executeAction', actionId: 'practice', quantity: 1 };
test('probabilistic actions: seeded TS/native parity, both outcomes, view purity and exact save replay', async t => {
  const c = await content(t); const gains = new Set();
  for (let seed = 0; seed < 24; seed++) {
    const origin = { seed, mode: 'full', initialOverrides: { attributes: {}, flags: {}, relationships: {} } };
    let state = createRun(c, seed, origin).state;
    assert.deepEqual(nativeCreateRun(c, seed, origin).state, state);
    const before = structuredClone(state);
    getGameplayView(c, state); getGameplayView(c, state);
    assert.deepEqual(state, before);
    const out = dispatchCommand(c, state, command, state.stateRevision);
    assert.equal(out.ok, true, JSON.stringify(out));
    assert.deepEqual(nativeDispatchCommand(c, state, command, state.stateRevision), out);
    gains.add(out.state.attributes.knowledge - state.attributes.knowledge);
    assert.equal(out.state.rng.draws, state.rng.draws + 1);
    state = out.state;
    const encoded = await encodeSave(c, { origin, commands: [command], state });
    assert.equal(encoded.ok, true, JSON.stringify(encoded));
    const loaded = await decodeSave(c, encoded.text);
    assert.equal(loaded.ok, true, JSON.stringify(loaded));
    assert.deepEqual(loaded.session.state, state);
    assert.deepEqual(dispatchCommand(c, state, command, state.stateRevision), dispatchCommand(c, loaded.session.state, command, loaded.session.state.stateRevision));
  }
  assert.deepEqual([...gains].sort(), [1, 3]);
});
test('probabilistic actions: condition short circuit, boundary clamps, state expression after costs', async t => {
  for (const [chance, condition, gain, draws] of [
    [literal(-1), undefined, 1, 1], [literal(10001), undefined, 3, 1],
    [literal(10000), { op: 'not', condition: { op: 'always' } }, 1, 0],
    [{ op: 'multiply', left: { op: 'read', target: { kind: 'attribute', attributeId: 'money' } }, right: literal(10000) }, undefined, 3, 1],
  ]) {
    const c = await content(t, chance, condition); const s = createRun(c, 7, { seed: 7, mode: 'full', initialOverrides: { attributes: {}, flags: {}, relationships: {} } }).state;
    const out = dispatchCommand(c, s, command, s.stateRevision);
    assert.equal(out.ok, true, JSON.stringify(out));
    assert.deepEqual(nativeDispatchCommand(c, s, command, s.stateRevision), out);
    assert.equal(out.state.attributes.knowledge - s.attributes.knowledge, gain);
    assert.equal(out.state.rng.draws - s.rng.draws, draws);
  }
});
test('probabilistic actions: failed child rolls back cost, random state and history; stale revisions reject', async t => {
  const c = await content(t, literal(0), undefined, 1000); const s = createRun(c, 19, { seed: 19, mode: 'full', initialOverrides: { attributes: {}, flags: {}, relationships: {} } }).state;
  const before = structuredClone(s);
  for (const dispatch of [dispatchCommand, nativeDispatchCommand]) {
    const out = dispatch(c, s, command, s.stateRevision);
    assert.equal(out.ok, false); assert.equal(out.error.code, 'INSUFFICIENT_FUNDS');
    assert.deepEqual(out.state, before); assert.deepEqual(s, before);
    assert.equal(dispatch(c, s, command, s.stateRevision + 1).ok, false);
  }
});

test('probabilistic actions: invalid chance references are rejected at authoring time', async t => {
  const f = await fixture(t, p => { p.gameplay.actions = [action('bad-reference', [], { flow: [{ id: 'step', thenActionId: 'target', chance: { op: 'read', target: { kind: 'attribute', attributeId: 'missing-stat' } } }] }), action('target')]; });
  assert.equal(f.built.ok, false);
  assert.ok(JSON.stringify(f.built).includes('/chance/target/attributeId'), JSON.stringify(f.built));
});

test('probabilistic actions: nested no-effect rejection warns about deterministic retry stalls', async t => {
  const f = await fixture(t, p => { p.gameplay.actions = [action('outer', [], { rejectNoEffect: true, flow: [{ id: 'nested', thenActionId: 'inner' }] }), action('inner', [], { flow: [{ id: 'roll', chance: literal(0), thenActionId: 'target' }] }), action('target')]; });
  assert.ok(JSON.stringify(f.built).includes('CHANCE_NO_EFFECT_RETRY'), JSON.stringify(f.built));
});

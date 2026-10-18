import test from 'node:test';
import assert from 'node:assert/strict';
import { fixture } from '../gameplay/fixture.mjs';
import { nativeCreateRun, nativeDispatchCommand, nativeReplay, nativeRequest } from '../../dist/packages/native-bridge/index.js';
import { createRun, dispatchCommand } from '../../dist/packages/core/run.js';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';
const literal = value => ({ op: 'literal', value });
const numeric = (target, value) => ({ op: 'add', target, value: literal(value) });
const attribute = (id, value) => numeric({ kind: 'attribute', attributeId: id }, value);
const inventory = (id, value) => numeric({ kind: 'inventory', itemId: id }, value);
const action = (id, effects, extra = {}) => ({ id, name: id, costs: [], effects, ...extra });
const rule = (id, type, actionId, extra = {}) => ({ id, name: id, trigger: { type }, actionId, ...extra });
async function setup(t) {
  const { built } = await fixture(t); assert.equal(built.ok, true, JSON.stringify(built));
  const c = structuredClone(built.snapshot); c.project.initialState.attributes.money = 200; c.project.initialState.attributes.stamina = 10;
  return c;
}
function run(c) {
  const origin = { seed: 123, mode: 'full', initialOverrides: { attributes: {}, flags: {}, relationships: {} } }, commands = [];
  let state = nativeCreateRun(c, origin.seed, origin).state;
  assert.deepEqual(state, createRun(c, origin.seed, origin).state);
  return {
    get state() { return state; }, origin, commands,
    send(command, code) {
      const before = structuredClone(state), out = nativeDispatchCommand(c, state, command, state.stateRevision), reference = dispatchCommand(c, state, command, state.stateRevision);
      if (out.ok) assert.deepEqual(out, reference);
      else { assert.equal(reference.ok, false, JSON.stringify(reference)); assert.equal(out.error.code, reference.error.code); assert.equal(out.error.path, reference.error.path); }
      if (code) { assert.equal(out.ok, false, JSON.stringify(out)); assert.equal(out.error.code, code); assert.deepEqual(out.state, before); }
      else { assert.equal(out.ok, true, JSON.stringify(out)); state = out.state; commands.push(command); }
      return out;
    },
  };
}
test('native rules: branches reuse actions and inventory formulas, later failure rolls back the entire flow', async t => {
  const c = await setup(t), g = c.project.gameplay;
  const hasFlower = { op: 'compare', comparator: 'gt', left: { op: 'read', target: { kind: 'inventory', itemId: 'flower' } }, right: literal(0) };
  g.actions = [action('take', [inventory('flower', 1)]), action('use', [inventory('flower', -1), attribute('stamina', 5)]),
    action('root', [], { flow: [{ id: 'branch', condition: hasFlower, thenActionId: 'use', elseActionId: 'take' }] }),
    action('fail', [], { costs: [{ attributeId: 'money', amount: 1000 }] }),
    action('atomic', [attribute('money', 10)], { flow: [{ id: 'one', thenActionId: 'take' }, { id: 'two', thenActionId: 'fail' }] })];
  const r = run(c);
  r.send({ type: 'executeAction', actionId: 'root', quantity: 1 }); assert.equal(r.state.gameplay.inventory.flower, 1);
  r.send({ type: 'executeAction', actionId: 'root', quantity: 1 }); assert.equal(r.state.gameplay.inventory.flower, 0); assert.equal(r.state.attributes.stamina, 15);
  r.send({ type: 'executeAction', actionId: 'atomic', quantity: 1 }, 'INSUFFICIENT_FUNDS');
  r.send({ type: 'executeAction', actionId: 'use', quantity: 1 }, 'INVENTORY_RANGE');
  assert.deepEqual(nativeReplay(c, r.origin, r.commands), { ok: true, state: r.state });
});
test('native rules: location, purchase and relationship hooks run in declaration order with duplicate guards', async t => {
  const c = await setup(t), g = c.project.gameplay;
  g.actions = [action('reward', [attribute('money', 3)]), action('trust', [numeric({ kind: 'relationship', characterId: 'mentor', relationshipId: 'trust' }, 1)]),
    action('twice', [], { flow: [{ id: 'first', thenActionId: 'trust' }, { id: 'second', thenActionId: 'trust' }] })];
  g.rules = [rule('city', 'locationEnter', 'reward', { trigger: { type: 'locationEnter', locationId: 'city' } }),
    rule('purchase', 'purchaseComplete', 'reward', { trigger: { type: 'purchaseComplete', shopId: 'florist', itemId: 'flower' } }),
    rule('relation', 'relationshipChanged', 'reward', { trigger: { type: 'relationshipChanged', characterId: 'mentor', relationshipId: 'trust' } })];
  const r = run(c); r.send({ type: 'visit', locationId: 'city' }); assert.equal(r.state.attributes.money, 203);
  r.send({ type: 'visit', locationId: 'market' }); assert.equal(r.state.attributes.money, 203);
  r.send({ type: 'buy', shopId: 'florist', itemId: 'flower', quantity: 1 }); assert.equal(r.state.attributes.money, 196);
  r.send({ type: 'executeAction', actionId: 'twice', quantity: 1 }); assert.equal(r.state.attributes.money, 199);
  assert.deepEqual(nativeReplay(c, r.origin, r.commands), { ok: true, state: r.state });
});
test('native rules: authored days and month boundaries charge once and retain replay/save state', async t => {
  const c = await setup(t), g = c.project.gameplay; c.events = [];
  g.actions = [action('month-start', [attribute('money', 100)]), action('period-start', [attribute('money', 10)]),
    action('day-start', [attribute('money', 1)]), action('day-end', [attribute('money', 2)]),
    action('period-end', [attribute('money', 20)]), action('month-end', [attribute('money', 200)])];
  g.rules = ['monthStart', 'periodStart', 'dayStart', 'dayEnd', 'periodEnd', 'monthEnd'].map((type, i) => rule('rule-' + i, type, g.actions[i].id));
  g.ruleCalendar = { daysPerMonth: 9 };
  const r = run(c); r.send({ type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] });
  for (let i = 0; i < 3; i++) {
    r.send({ type: 'executeActivity' });
    if (!i) {
      assert.equal(r.state.attributes.money, 319);
      const encoded = await encodeSave(c, { origin: r.origin, commands: r.commands, state: r.state }); assert.equal(encoded.ok, true, JSON.stringify(encoded));
      const loaded = await decodeSave(c, encoded.text); assert.equal(loaded.ok, true, JSON.stringify(loaded)); assert.deepEqual(loaded.session.state, r.state);
    }
    r.send({ type: 'acknowledgeResult' }); r.send({ type: 'resolvePeriodEnd' });
  }
  assert.equal(r.state.attributes.money, 617); assert.deepEqual(r.state.gameplay.rulePeriods, { periodStart: 2, periodEnd: 2 });
  assert.deepEqual(nativeReplay(c, r.origin, r.commands), { ok: true, state: r.state });
});
test('native rules: cycles and fan-out limits stop transactionally even in unvalidated content', async t => {
  const c = await setup(t), g = c.project.gameplay;
  g.actions = [action('loop', [attribute('money', 1)], { flow: [{ id: 'again', thenActionId: 'loop' }] })];
  run(c).send({ type: 'executeAction', actionId: 'loop', quantity: 1 }, 'FLOW_CYCLE');
  g.actions = [action('leaf', []), action('many', [attribute('money', 1)], { flow: Array.from({ length: 129 }, (_, i) => ({ id: 'step-' + i, thenActionId: 'leaf' })) })];
  run(c).send({ type: 'executeAction', actionId: 'many', quantity: 1 }, 'FLOW_LIMIT');
});
test('native inspect: reports actual operands, effects, branch and errors without committing state', async t => {
  const c = await setup(t), g = c.project.gameplay;
  g.actions = [action('gift', [inventory('flower', 1)], { requirements: { op: 'compare', comparator: 'gte', left: { op: 'read', target: { kind: 'attribute', attributeId: 'money' } }, right: literal(20) }, costs: [{ attributeId: 'money', amount: 20 }] })];
  const r = run(c), before = structuredClone(r.state), command = { type: 'executeAction', actionId: 'gift', quantity: 1 };
  const inspected = nativeRequest({ operation: 'inspect', content: c, state: r.state, command, expectedRevision: r.state.stateRevision });
  assert.equal(inspected.ok, true, JSON.stringify(inspected)); assert.deepEqual(r.state, before);
  assert.ok(inspected.trace.some(x => x.kind === 'condition' && x.leftValue === 200 && x.rightValue === 20 && x.status === 'passed'));
  assert.ok(inspected.trace.some(x => x.kind === 'effect' && x.after === 1));
  r.send(command); assert.deepEqual(inspected.state, r.state);
  const stale = nativeRequest({ operation: 'inspect', content: c, state: r.state, command, expectedRevision: 0 });
  assert.equal(stale.ok, false); assert.deepEqual(stale.state, r.state); assert.equal(stale.trace.at(-1).code, 'REVISION_CONFLICT');
  const afterGift = structuredClone(r.state);
  const event = nativeRequest({ operation: 'inspect', content: c, state: r.state, subject: { kind: 'event', id: 'gift-friendship' } });
  // Interaction events are inspected during planning/ready; unlike period-end
  // events they need an explicit invocation and can fail their own requirements.
  assert.equal(event.ok, true); assert.equal(event.reason, 'requirements-not-met');
  assert.equal(event.eligible, false); assert.deepEqual(r.state, afterGift);
});

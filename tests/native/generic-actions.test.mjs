import test from 'node:test';
import assert from 'node:assert/strict';
import { fixture } from '../gameplay/fixture.mjs';
import { nativeCreateRun, nativeDispatchCommand, nativeReplay, nativeRequest } from '../../dist/packages/native-bridge/index.js';
import { createRun, dispatchCommand } from '../../dist/packages/core/run.js';
import { getGameplayView } from '../../dist/packages/gameplay/service.js';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';

const effect = (attributeId, value) => ({ op: 'add', target: { kind: 'attribute', attributeId }, value: { op: 'literal', value } });
async function setup(t) {
  const { built } = await fixture(t); assert.equal(built.ok, true);
  const content = structuredClone(built.snapshot), g = content.project.gameplay;
  content.project.initialState.attributes.money = 200;
  content.project.initialState.attributes.stamina = 10;
  g.actions = [
    { id: 'meal', name: 'Meal', costs: [{ attributeId: 'money', amount: 7 }], effects: [effect('stamina', 2)] },
    { id: 'sleep', name: 'Sleep', costs: [], effects: [effect('stamina', 4)] },
    { id: 'study', name: 'Study', costs: [{ attributeId: 'money', amount: 3 }], effects: [effect('stamina', -5)] },
    { id: 'heal', name: 'Healing', locationIds: ['market'], maxQuantity: 3, rejectNoEffect: true, costs: [{ attributeId: 'money', amount: 5 }], effects: [effect('stamina', 10)] },
    { id: 'bless', name: 'Blessing', locationIds: ['market'], costs: [{ attributeId: 'money', amount: 4 }], effects: [{ op: 'add', target: { kind: 'relationship', characterId: 'mentor', relationshipId: 'trust' }, value: { op: 'literal', value: 15 } }], eventId: 'gift-friendship' },
  ];
  g.policyBudgets = [{ id: 'hours', name: 'Daily hours', capacity: 24, unit: 'hours' }];
  g.policyGroups = [
    { id: 'food', name: 'Food', locationIds: ['home'], trigger: { type: 'beforeActivity', intervalPeriods: 3 }, options: [{ id: 'balanced', name: 'Balanced', actionId: 'meal', budgetUses: [{ budgetId: 'hours', amount: 2 }] }] },
    { id: 'sleeping', name: 'Sleeping', trigger: { type: 'beforeActivity' }, options: [{ id: 'eight', name: '8 hours', actionId: 'sleep', budgetUses: [{ budgetId: 'hours', amount: 8 }] }] },
    { id: 'studying', name: 'Studying', trigger: { type: 'afterActivity' }, options: [
      { id: 'normal', name: 'Normal', actionId: 'study', budgetUses: [{ budgetId: 'hours', amount: 14 }] },
      { id: 'heavy', name: 'Heavy', actionId: 'study', budgetUses: [{ budgetId: 'hours', amount: 20 }] },
    ] },
  ];
  return content;
}
function run(content) {
  const origin = { seed: 42, mode: 'full', initialOverrides: { attributes: {}, flags: {}, relationships: {} } }, commands = [];
  let state = nativeCreateRun(content, 42, origin).state;
  assert.deepEqual(state, createRun(content, 42, origin).state);
  return {
    get state() { return state; }, commands, origin,
    command(command, errorCode) {
      const before = structuredClone(state), out = nativeDispatchCommand(content, state, command, state.stateRevision);
      const reference = dispatchCommand(content, state, command, state.stateRevision);
      if (out.ok) assert.deepEqual(out, reference);
      else { assert.equal(reference.ok, false); assert.equal(out.error.code, reference.error.code); assert.equal(out.error.path, reference.error.path); }
      if (errorCode) { assert.equal(out.ok, false, JSON.stringify(out)); assert.equal(out.error.code, errorCode); assert.deepEqual(out.state, before); }
      else { assert.equal(out.ok, true, JSON.stringify(out)); state = out.state; commands.push(command); }
      return out;
    },
  };
}
const select = (r, groupId, optionId, error) => r.command({ type: 'selectPolicy', groupId, optionId }, error);
const next = r => { r.command({ type: 'acknowledgeResult' }); r.command({ type: 'resolvePeriodEnd' }); };
test('repeatable interaction: defer, save, revisit and complete without duplicating completion history', async t => {
  for (const repeatable of [false, true]) for (const mechanism of ['action', 'interaction']) {
    const c = await setup(t);
    c.project.initialState.attributes.skill = 0;
    c.project.gameplay.items.find(item => item.id === 'flower').initialQuantity = 3;
    c.project.gameplay.interactions[0].oncePerPeriod = false;
    const visitCommand = mechanism === 'action'
      ? { type: 'executeAction', actionId: 'bless', quantity: 1 }
      : { type: 'interact', interactionId: 'gift-mentor' };
    const event = c.events.find(e => e.id === 'gift-friendship');
    event.repeatable = repeatable;
    event.trigger = 'interaction';
    event.requirements = { op: 'compare', left: { op: 'read', target: { kind: 'attribute', attributeId: 'skill' } }, comparator: 'lt', right: { op: 'literal', value: 2 } };
    event.entryNodeId = 'ask';
    event.nodes = [
      { id: 'ask', type: 'choice', prompt: 'Prepare medicine?', choices: [
        { id: 'defer', text: 'Later', requirements: { op: 'always' }, effects: [], next: 'end' },
        { id: 'accept', text: 'Prepare', requirements: { op: 'always' }, effects: [effect('skill', 2)], next: 'end' },
      ] },
      { id: 'end', type: 'end' },
    ];
    const r = run(c);
    r.command({ type: 'visit', locationId: 'city' });
    if (mechanism === 'action') r.command({ type: 'visit', locationId: 'market' });
    r.command(visitCommand);
    r.command({ type: 'choose', choiceId: 'defer' });
    r.command({ type: 'advanceNarrative' });
    assert.equal(r.state.phase, 'planning');
    assert.equal(r.state.completedEvents.filter(id => id === event.id).length, 1);
    r.command(visitCommand);
    assert.equal(r.state.phase, repeatable ? 'narrative' : 'planning');
    const encoded = await encodeSave(c, { origin: r.origin, commands: r.commands, state: r.state });
    assert.equal(encoded.ok, true, JSON.stringify(encoded));
    const loaded = await decodeSave(c, encoded.text);
    assert.equal(loaded.ok, true, JSON.stringify(loaded));
    assert.deepEqual(loaded.session.state, r.state);
    if (repeatable) {
      r.command({ type: 'choose', choiceId: 'accept' });
      r.command({ type: 'advanceNarrative' });
      r.command(visitCommand);
      assert.equal(r.state.phase, 'planning', 'completed treatment condition still gates repeat entry');
      assert.equal(r.state.attributes.skill, 2);
      assert.equal(r.state.completedEvents.filter(id => id === event.id).length, 1);
    }
    assert.deepEqual(nativeReplay(c, r.origin, r.commands), { ok: true, state: r.state });
  }
});
test('generic native: independently selected meal, sleep and study share 24-hour budget, ordered triggers and persisted cursors', async t => {
  const c = await setup(t); c.events = []; const r = run(c);
  select(r, 'food', 'balanced'); select(r, 'sleeping', 'eight'); select(r, 'studying', 'normal');
  assert.equal(r.state.attributes.money, 200);
  select(r, 'studying', 'heavy', 'POLICY_BUDGET_EXCEEDED');
  select(r, 'food', 'balanced', 'POLICY_UNCHANGED');
  r.command({ type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] });
  r.command({ type: 'executeActivity' });
  assert.equal(r.state.attributes.money, 190);
  assert.deepEqual(r.state.gameplay.policyAppliedPeriods, { food: 0, sleeping: 0, studying: 0 });
  assert.deepEqual(r.state.gameplay.policyNextPeriods, { food: 3, sleeping: 1, studying: 1 });
  const sources = r.state.lastActivityResult.changes.map(x => x.source);
  assert.ok(sources.indexOf('/project/gameplay/actions/sleep/effects/0') < sources.findIndex(x => x.startsWith('/activities/rest')));
  assert.ok(sources.indexOf('/project/gameplay/actions/study/effects/0') > sources.findIndex(x => x.startsWith('/activities/rest')));
  const encoded = await encodeSave(c, { origin: r.origin, commands: r.commands, state: r.state });
  assert.equal(encoded.ok, true, JSON.stringify(encoded));
  const loaded = await decodeSave(c, encoded.text); assert.equal(loaded.ok, true, JSON.stringify(loaded));
  assert.deepEqual(loaded.session.state, r.state);
  next(r); select(r, 'food', null); select(r, 'food', 'balanced');
  r.command({ type: 'executeActivity' }); assert.equal(r.state.attributes.money, 187);
  assert.equal(r.state.gameplay.policyAppliedPeriods.food, 0);
  next(r); r.command({ type: 'executeActivity' }); next(r);
  r.command({ type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] }); r.command({ type: 'executeActivity' });
  assert.equal(r.state.gameplay.policyAppliedPeriods.food, 3); assert.equal(r.state.gameplay.policyNextPeriods.food, 6);
  assert.deepEqual(nativeReplay(c, r.origin, r.commands), { ok: true, state: r.state });
  assert.deepEqual(nativeRequest({ operation: 'view', content: c, state: r.state }).gameplay, getGameplayView(c, r.state));
});
test('generic native: a shared action supports healing, blessing, location and quantity guards, and stale-confirmation rejection', async t => {
  const c = await setup(t), r = run(c);
  r.command({ type: 'executeAction', actionId: 'heal', quantity: 1 }, 'WRONG_LOCATION');
  r.command({ type: 'visit', locationId: 'city' }); r.command({ type: 'visit', locationId: 'market' });
  select(r, 'food', 'balanced', 'WRONG_LOCATION');
  r.command({ type: 'executeAction', actionId: 'heal', quantity: 4 }, 'QUANTITY_LIMIT');
  const revision = r.state.stateRevision;
  r.command({ type: 'executeAction', actionId: 'heal', quantity: 2 });
  assert.equal(r.state.attributes.money, 190); assert.equal(r.state.attributes.stamina, 30);
  const stale = nativeDispatchCommand(c, r.state, { type: 'executeAction', actionId: 'heal', quantity: 2 }, revision);
  assert.equal(stale.error.code, 'REVISION_CONFLICT'); assert.deepEqual(stale.state, r.state);
  r.command({ type: 'executeAction', actionId: 'bless', quantity: 1 });
  assert.equal(r.state.phase, 'narrative'); assert.equal(r.state.activeNarrative.id, 'gift-friendship');
  r.command({ type: 'advanceNarrative' }); r.command({ type: 'advanceNarrative' });
  assert.equal(r.state.phase, 'planning'); assert.equal(r.state.attributes.money, 186);
  assert.deepEqual(nativeReplay(c, r.origin, r.commands), { ok: true, state: r.state });
  assert.deepEqual(nativeRequest({ operation: 'view', content: c, state: r.state }).gameplay, getGameplayView(c, r.state));
});
test('generic native: failed later policies roll back earlier policies, activity effects, costs and RNG', async t => {
  const c = await setup(t); c.events = []; c.project.initialState.attributes.money = 9;
  const r = run(c); select(r, 'food', 'balanced'); select(r, 'studying', 'normal');
  r.command({ type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] });
  r.command({ type: 'executeActivity' }, 'INSUFFICIENT_FUNDS');
  assert.equal(r.state.attributes.money, 9); assert.equal(r.state.attributes.stamina, 10);
  assert.equal(r.state.gameplay.policyAppliedPeriods, undefined); assert.equal(r.state.rng.draws, 0);
});
test('generic native: no-effect healing and invalid commands reject without economic mutation', async t => {
  const c = await setup(t); c.project.initialState.attributes.stamina = c.attributes.find(x => x.id === 'stamina').max;
  const r = run(c); r.command({ type: 'visit', locationId: 'city' }); r.command({ type: 'visit', locationId: 'market' });
  r.command({ type: 'executeAction', actionId: 'heal', quantity: 1 }, 'ACTION_NO_EFFECT');
  r.command({ type: 'executeAction', actionId: 'heal', quantity: 0 }, 'INVALID_COMMAND');
  r.command({ type: 'executeAction', actionId: 'heal', quantity: 1, unknown: true }, 'INVALID_COMMAND');
  select(r, 'missing', 'nope', 'UNKNOWN_POLICY_GROUP'); select(r, 'sleeping', 'nope', 'UNKNOWN_POLICY_OPTION');
});
test('generic native: before-activity rules affect eligibility and success input, and failed eligibility rolls everything back', async t => {
  const c = await setup(t); c.events = [];
  const rest = c.activities.find(x => x.id === 'rest');
  rest.requirements = { op: 'compare', comparator: 'gte', left: { op: 'read', target: { kind: 'attribute', attributeId: 'stamina' } }, right: { op: 'literal', value: 14 } };
  rest.successCheck = { chance: { op: 'read', target: { kind: 'attribute', attributeId: 'stamina' } } };
  rest.successEffects = []; rest.failureEffects = [];
  const r = run(c); select(r, 'sleeping', 'eight');
  r.command({ type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] });
  r.command({ type: 'executeActivity' });
  assert.equal(r.state.lastActivityResult.success.requestedChance, 14);
  const blocked = structuredClone(c); blocked.activities.find(x => x.id === 'rest').requirements.right.value = 15;
  const b = run(blocked); select(b, 'sleeping', 'eight'); b.command({ type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] });
  b.command({ type: 'executeActivity' }, 'REQUIREMENTS_NOT_MET');
  assert.equal(b.state.attributes.stamina, 10); assert.equal(b.state.gameplay.policyAppliedPeriods, undefined);
});

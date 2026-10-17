import test from 'node:test';
import assert from 'node:assert/strict';
import { fixture } from '../gameplay/fixture.mjs';
import { createRun, dispatchCommand } from '../../dist/packages/core/run.js';
import { nativeCreateRun, nativeDispatchCommand, nativeReplay } from '../../dist/packages/native-bridge/index.js';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';

const add = (attributeId, value) => ({ op: 'add', target: { kind: 'attribute', attributeId }, value: { op: 'literal', value } });
const done = { op: 'setFlag', flagId: 'free-done', value: true };
async function content(t) {
  const { built } = await fixture(t); assert.equal(built.ok, true, JSON.stringify(built));
  const c = structuredClone(built.snapshot);
  c.flags.push({ id: 'free-done', label: '자유 행동 완료', initial: false });
  c.project.initialState.attributes.money = 10;
  c.project.initialState.attributes.stamina = 30;
  c.project.periodFlow = { preparePolicies: true, intermission: { screenId: 'city', completionFlagId: 'free-done', entryEffects: [add('stamina', -1)] } };
  c.project.gameplay.actions = [
    { id: 'meal', name: '생활', costs: [{ attributeId: 'money', amount: 5 }], effects: [add('stamina', 2)] },
    { id: 'visit', name: '방문', costs: [], effects: [], eventId: 'visit' },
  ];
  c.project.gameplay.policyGroups = [{ id: 'food', name: '식사', trigger: { type: 'beforeActivity' }, options: [{ id: 'normal', name: '보통', actionId: 'meal' }] }];
  c.activities = [
    { id: 'expensive', label: '수업', requirements: { op: 'always' }, costs: [{ attributeId: 'money', amount: 20 }], baseEffects: [] },
    { id: 'rest', label: '휴식', requirements: { op: 'always' }, costs: [], baseEffects: [add('stamina', 4)] },
  ];
  c.events = [{ id: 'visit', label: '방문 선택', trigger: 'interaction', priority: 10, window: { fromPeriod: 0, throughPeriod: 35 }, requirements: { op: 'always' }, entryNodeId: 'choice', nodes: [
    { id: 'choice', type: 'choice', prompt: '돌아갈까요?', choices: [{ id: 'yes', text: '귀가한다', requirements: { op: 'always' }, effects: [done], next: 'end' }] },
    { id: 'end', type: 'end' },
  ] }];
  return c;
}
function session(c, mode = 'full') {
  const origin = { seed: 42, mode, initialOverrides: { attributes: {}, flags: {}, relationships: {} } };
  let state = createRun(c, 42, origin).state;
  assert.deepEqual(nativeCreateRun(c, 42, origin).state, state);
  const commands = [];
  return {
    get state() { return state; },
    async checkpoint() {
      const saved = await encodeSave(c, { origin, commands, state }); assert.equal(saved.ok, true, JSON.stringify(saved));
      const loaded = await decodeSave(c, saved.text); assert.equal(loaded.ok, true, JSON.stringify(loaded));
      assert.deepEqual(loaded.session.state, state);
      assert.deepEqual(nativeReplay(c, origin, commands), { ok: true, state });
      state = loaded.session.state;
    },
    send(command, error) {
      const result = dispatchCommand(c, state, command, state.stateRevision);
      const native = nativeDispatchCommand(c, state, command, state.stateRevision);
      if (error) {
        assert.equal(result.ok, false); assert.equal(result.error.code, error);
        assert.equal(native.ok, false); assert.equal(native.error.code, error);
        assert.deepEqual(result.state, state); assert.deepEqual(native.state, state);
      } else { assert.equal(result.ok, true, JSON.stringify(result)); assert.deepEqual(native, result); state = result.state; commands.push(command); }
    },
  };
}
test('period stages: paid preparation survives a failed activity, replacement, choices and save/replay at each boundary', async t => {
  const c = await content(t), r = session(c);
  r.send({ type: 'selectPolicy', groupId: 'food', optionId: 'normal' });
  r.send({ type: 'setSchedule', activityIds: ['expensive', 'rest', 'rest'] });
  r.send({ type: 'executeActivity' });
  assert.equal(r.state.phase, 'ready'); assert.equal(r.state.attributes.money, 5); assert.equal(r.state.attributes.stamina, 32);
  await r.checkpoint();
  r.send({ type: 'executeActivity' }, 'INSUFFICIENT_COST');
  r.send({ type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] });
  r.send({ type: 'executeActivity' });
  assert.equal(r.state.attributes.money, 5); assert.equal(r.state.attributes.stamina, 36);
  await r.checkpoint();
  r.send({ type: 'acknowledgeResult' });
  assert.equal(r.state.gameplay.screenId, 'city'); assert.equal(r.state.attributes.stamina, 35);
  await r.checkpoint();
  r.send({ type: 'executeActivity' }, 'INTERMISSION_ACTIVE');
  r.send({ type: 'acknowledgeResult' }, 'INTERMISSION_PENDING');
  r.send({ type: 'setSchedule', activityIds: ['expensive', 'rest', 'rest'] }, 'COMPLETED_SLOT');
  r.send({ type: 'executeAction', actionId: 'visit', quantity: 1 });
  assert.equal(r.state.phase, 'narrative'); await r.checkpoint();
  r.send({ type: 'choose', choiceId: 'yes' });
  for (let i = 0; r.state.phase === 'narrative' && i < 5; i++) r.send({ type: 'advanceNarrative' });
  assert.equal(r.state.phase, 'ready'); await r.checkpoint();
  r.send({ type: 'acknowledgeResult' }); r.send({ type: 'resolvePeriodEnd' });
  assert.equal(r.state.periodIndex, 1); assert.equal(r.state.periodPreparation, undefined);
  await r.checkpoint(); r.send({ type: 'executeActivity' });
  assert.equal(r.state.attributes.money, 0); assert.equal(r.state.attributes.stamina, 37);
  await r.checkpoint();
});
test('period event sequence: opt-in drains eligible events, rechecks effects and restores between events without recharging boundaries', async t => {
  for (const enabled of [false, true]) {
    const c = await content(t);
    c.project.periodFlow = { allPeriodEndEvents: enabled };
    c.project.gameplay.actions.push({ id: 'week-cost', name: '주간 비용', costs: [], effects: [add('money', -1)] });
    c.project.gameplay.rules = [{ id: 'week-cost', name: '주간 비용', trigger: { type: 'periodEnd' }, actionId: 'week-cost' }];
    const event = (id, priority, requirements, effects = []) => ({ id, label: id, trigger: 'period-end', priority, window: { fromPeriod: 0, throughPeriod: 0 }, requirements, entryNodeId: 'effect', nodes: [{ id: 'effect', type: 'effect', effects, next: 'end' }, { id: 'end', type: 'end' }] });
    c.events = [event('first', 30, { op: 'always' }, [done]), event('second', 20, { op: 'flag', flagId: 'free-done', equals: true }, [add('stamina', 3)])];
    const r = session(c);
    r.send({ type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] });
    r.send({ type: 'executeActivity' }); r.send({ type: 'acknowledgeResult' }); r.send({ type: 'resolvePeriodEnd' });
    assert.equal(r.state.activeNarrative.id, 'first'); assert.equal(r.state.attributes.money, 9);
    r.send({ type: 'advanceNarrative' }); r.send({ type: 'advanceNarrative' });
    if (enabled) {
      assert.equal(r.state.periodIndex, 0); assert.equal(r.state.activeNarrative.id, 'second');
      await r.checkpoint();
      r.send({ type: 'advanceNarrative' }); r.send({ type: 'advanceNarrative' });
      assert.equal(r.state.attributes.stamina, 37);
      assert.deepEqual(r.state.completedEvents, ['first', 'second']);
    } else { assert.deepEqual(r.state.completedEvents, ['first']); assert.equal(r.state.attributes.stamina, 34); }
    assert.equal(r.state.periodIndex, 1); assert.equal(r.state.attributes.money, 9);
    await r.checkpoint();
  }
});

test('period stages: preparation is atomic when policy costs fail; activities-only does not enter free play', async t => {
  const c = await content(t); c.project.initialState.attributes.money = 4;
  const r = session(c);
  r.send({ type: 'selectPolicy', groupId: 'food', optionId: 'normal' });
  r.send({ type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] });
  r.send({ type: 'executeActivity' }, 'INSUFFICIENT_FUNDS');
  assert.equal(r.state.periodPreparation, undefined); assert.equal(r.state.attributes.stamina, 30);
  r.send({ type: 'selectPolicy', groupId: 'food', optionId: null });
  r.send({ type: 'executeActivity' }); r.send({ type: 'executeActivity' });
  await r.checkpoint();
  const a = session(c, 'activities-only');
  a.send({ type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] });
  a.send({ type: 'executeActivity' }); a.send({ type: 'executeActivity' }); a.send({ type: 'acknowledgeResult' });
  assert.equal(a.state.phase, 'period-end'); assert.equal(a.state.activityIntermission, undefined);
  await a.checkpoint();
});

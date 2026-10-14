import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fixture } from '../gameplay/fixture.mjs';
import { createRun, dispatchCommand } from '../../dist/packages/core/run.js';
import { nativeCreateRun, nativeDispatchCommand, nativeReplay } from '../../dist/packages/native-bridge/index.js';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';

// Deliberately synthetic values: this proves the generic composition, not PM2 balance.
const literal = value => ({ op: 'literal', value });
const target = attributeId => ({ kind: 'attribute', attributeId });
const read = attributeId => ({ op: 'read', target: target(attributeId) });
const set = (id, value) => ({ op: 'set', target: target(id), value: literal(value) });
const add = (id, value) => ({ op: 'add', target: target(id), value });
const atLeast = n => ({ op: 'compare', comparator: 'gte', left: read('course-xp'), right: literal(n) });
const action = (id, effects, flow) => ({ id, name: id, costs: [], effects, ...(flow ? { flow } : {}) });

async function course(t, money = 500) {
  const f = await fixture(t, p => {
    p.initialState.attributes.money = money;
    p.gameplay.actions = [
      action('course-first', [set('course-tier', 1), set('course-fee', 7), set('course-gain', 2), set('course-chance', 10000)]),
      action('course-second', [set('course-tier', 2), set('course-fee', 11), set('course-gain', 4), set('course-chance', 0)]),
      action('course-third', [set('course-tier', 3), set('course-fee', 13), set('course-gain', 6), set('course-chance', 7500)]),
      action('course-low', [], [{ id: 'select', condition: atLeast(2), thenActionId: 'course-second', elseActionId: 'course-first' }]),
      action('course-profile', [], [{ id: 'select', condition: atLeast(4), thenActionId: 'course-third', elseActionId: 'course-low' }]),
    ];
    p.gameplay.rules = ['periodStart', 'periodEnd'].map((type, index) => ({
      id: `course-profile-${index}`, name: 'Refresh course profile', trigger: { type }, actionId: 'course-profile',
    }));
  }, async root => {
    const file = path.join(root, 'attributes.json'), attrs = JSON.parse(await readFile(file, 'utf8'));
    for (const [id, initial, max] of [['course-xp', 0, 100], ['course-tier', 1, 3], ['course-fee', 7, 100], ['course-gain', 2, 100], ['course-chance', 10000, 10000]]) {
      attrs.attributes.push({ id, label: id, kind: 'stat', min: 0, max, initial });
    }
    await writeFile(file, JSON.stringify(attrs));
    await writeFile(path.join(root, 'activities/course.json'), JSON.stringify({
      formatVersion: '0.1.0', id: 'course', label: 'Generic staged course',
      description: 'Fee is course-fee, charged once per activity by the base effect. This is not a daily simulation.',
      requirements: { op: 'compare', comparator: 'gte', left: read('money'), right: read('course-fee') },
      costs: [], baseEffects: [add('money', { op: 'subtract', left: literal(0), right: read('course-fee') }), add('course-xp', literal(1))],
      successCheck: { chance: read('course-chance') }, successEffects: [add('skill', read('course-gain'))], failureEffects: [add('stress', literal(1))],
    }));
  });
  assert.equal(f.built.ok, true, JSON.stringify(f.built.diagnostics));
  return f.built.snapshot;
}

function run(content, seed) {
  const origin = { seed, mode: 'activities-only', initialOverrides: { attributes: {}, flags: {}, relationships: {} } };
  const created = nativeCreateRun(content, seed, origin);
  assert.equal(created.ok, true, JSON.stringify(created));
  assert.deepEqual(created, createRun(content, seed, origin));
  let state = created.state; const commands = [];
  return {
    get session() { return { origin, state, commands }; },
    send(command, error) {
      const before = state, actual = nativeDispatchCommand(content, state, command, state.stateRevision);
      const reference = dispatchCommand(content, state, command, state.stateRevision);
      if (error) {
        assert.equal(actual.ok, false, JSON.stringify(actual)); assert.equal(actual.error.code, error);
        assert.equal(reference.ok, false); assert.equal(reference.error.code, error);
        assert.deepEqual(actual.state, before); assert.equal(reference.state, before);
      } else {
        assert.equal(actual.ok, true, JSON.stringify(actual)); assert.deepEqual(actual, reference);
        state = actual.state; commands.push(command);
      }
      return actual;
    },
  };
}

test('existing contracts compose tier fees, effects, chances and progression under one scheduled activity ID', async t => {
  const c = await course(t), a = run(c, 123), b = run(c, 123);
  for (let i = 0; i < 9; i++) {
    if (i % 3 === 0) for (const r of [a, b]) r.send({ type: 'setSchedule', activityIds: ['course', 'course', 'course'] });
    const before = a.session.state;
    for (const r of [a, b]) r.send({ type: 'executeActivity' });
    const after = a.session.state, result = after.lastActivityResult;
    assert.equal(after.attributes.money, before.attributes.money - (i < 2 ? 7 : i < 4 ? 11 : 13));
    assert.equal(result.success.chance, i < 2 ? 10000 : i < 4 ? 0 : 7500);
    assert.equal(after.attributes['course-xp'], i + 1);
    assert.equal(after.attributes.skill - before.attributes.skill, result.success.passed ? (i < 2 ? 2 : i < 4 ? 4 : 6) : 0);
    assert.deepEqual(a.session, b.session);
    if (i === 2) {
      const saved = await encodeSave(c, a.session); assert.equal(saved.ok, true, JSON.stringify(saved));
      const loaded = await decodeSave(c, saved.text); assert.equal(loaded.ok, true, JSON.stringify(loaded));
      assert.deepEqual(loaded.session, a.session);
    }
    for (const r of [a, b]) { r.send({ type: 'acknowledgeResult' }); r.send({ type: 'completePeriodWithoutEvents' }); }
    assert.equal(a.session.state.attributes['course-tier'], i + 1 < 2 ? 1 : i + 1 < 4 ? 2 : 3);
  }
  assert.deepEqual(nativeReplay(c, a.session.origin, a.session.commands), { ok: true, state: a.session.state });
});

test('insufficient dynamic fee rolls back profile hooks, RNG, experience and revision; another activity remains executable', async t => {
  const c = await course(t, 20), r = run(c, 42);
  r.send({ type: 'setSchedule', activityIds: ['course', 'course', 'course'] });
  for (let i = 0; i < 2; i++) { r.send({ type: 'executeActivity' }); r.send({ type: 'acknowledgeResult' }); r.send({ type: 'completePeriodWithoutEvents' }); }
  assert.equal(r.session.state.attributes.money, 6);
  assert.equal(r.session.state.attributes['course-tier'], 2);
  const before = structuredClone(r.session);
  r.send({ type: 'executeActivity' }, 'REQUIREMENTS_NOT_MET');
  assert.deepEqual(r.session, before);
  r.send({ type: 'setSchedule', activityIds: ['course', 'course', 'rest'] });
  r.send({ type: 'executeActivity' });
  assert.equal(r.session.state.attributes.money, 6);
  assert.equal(r.session.state.attributes['course-xp'], 2);
  assert.deepEqual(nativeReplay(c, r.session.origin, r.session.commands), { ok: true, state: r.session.state });
});

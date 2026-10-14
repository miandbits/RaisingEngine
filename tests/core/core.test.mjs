import test from 'node:test';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
import { spawnSync } from 'node:child_process';
import { loadProject, buildSnapshot } from '../../dist/packages/project-service/index.js';
import { createRun, dispatchCommand, calendarTime, evaluateNumber, evaluateCondition } from '../../dist/packages/core/index.js';
import { seedRandom, nextUint32, sampleBelow } from '../../dist/packages/core/random.js';

const sample = fileURLToPath(new URL('../../examples/academy-demo', import.meta.url));
const loaded = await loadProject(sample);
assert.deepEqual(loaded.diagnostics, []);
const literal = value => ({ op: 'literal', value });
const attribute = attributeId => ({ kind: 'attribute', attributeId });
const read = id => ({ op: 'read', target: attribute(id) });
const effect = (id, value, op = 'add') => ({ op, target: attribute(id), value: typeof value === 'number' ? literal(value) : value });
const overrides = (attributes = {}, flags = {}, relationships = {}) => ({ attributes, flags, relationships });
const doc = (project, kind, id) => project.documents.find(d => d.kind === kind && (id === undefined || d.data.id === id)).data;
async function content(edit = () => {}) {
  const project = structuredClone(loaded);
  project.documents = project.documents.filter(d => d.kind !== 'scenario');
  edit(project);
  const result = await buildSnapshot(project);
  assert.equal(result.ok, true, JSON.stringify(result.diagnostics));
  return result.snapshot;
}
function start(snapshot, seed = 42, initialOverrides) {
  const result = createRun(snapshot, seed, { mode: 'activities-only', ...(initialOverrides ? { initialOverrides } : {}) });
  assert.equal(result.ok, true, JSON.stringify(result));
  return result.state;
}
function send(snapshot, state, command) {
  const result = dispatchCommand(snapshot, state, command, state.stateRevision);
  assert.equal(result.ok, true, JSON.stringify(result));
  return result.state;
}
const plan = (snapshot, state, activityIds = ['study', 'work', 'rest']) => send(snapshot, state, { type: 'setSchedule', activityIds });
const execute = (snapshot, state) => send(snapshot, state, { type: 'executeActivity' });
function finishPeriod(snapshot, state) {
  return send(snapshot, send(snapshot, state, { type: 'acknowledgeResult' }), { type: 'completePeriodWithoutEvents' });
}
function rejected(snapshot, state, command, code, revision = state.stateRevision) {
  const before = JSON.stringify(state);
  const result = dispatchCommand(snapshot, state, command, revision);
  assert.equal(result.ok, false);
  assert.equal(result.error.code, code);
  assert.equal(result.state, state);
  assert.equal(JSON.stringify(state), before);
  assert.ok(result.error.path.startsWith('/'));
  return result.error;
}

test('PCG32 standard seed42/stream54 vector and JSON continuation', () => {
  let rng = seedRandom(42);
  const actual = [];
  for (let i = 0; i < 6; i++) {
    const next = nextUint32(rng); actual.push(next.value.toString(16).padStart(8, '0')); rng = next.rng;
  }
  assert.deepEqual(actual, ['a15c02b7', '7b47f409', 'ba1d3330', '83d2f293', 'bfa4784b', 'cbed606e']);
  assert.equal(rng.draws, 6);
  assert.deepEqual(nextUint32(JSON.parse(JSON.stringify(rng))), nextUint32(rng));
});

test('bounded RNG rejects high tail and counts every raw draw', () => {
  const state = ((0xffffffffn << 27n) ^ (0x3fffn << 27n)).toString();
  const rng = { ...seedRandom(0), state };
  const first = nextUint32(rng);
  assert.equal(first.value, 0xffffffff);
  const second = nextUint32(first.rng);
  assert.ok(second.value < 4294960000);
  const sample = sampleBelow(rng, 10000);
  assert.equal(sample.value, second.value % 10000);
  assert.equal(sample.rng.draws, 2);
  assert.equal(sample.rng.state, second.rng.state);
});

test('initial state merges definition, project and run overrides without clamping', async () => {
  const snapshot = await content(p => { doc(p, 'project').initialState = overrides({ money: 41, skill: 3 }, { registered: true }, { mentor: { trust: 7 } }); });
  const state = start(snapshot, 0, overrides({ money: 42 }, {}, { mentor: { trust: 8 } }));
  assert.deepEqual(state.attributes, { skill: 3, money: 42, stamina: 50, stress: 0 });
  assert.equal(state.flags.registered, true);
  assert.equal(state.relationships.mentor.trust, 8);
  assert.equal(state.stateRevision, 0);
  assert.equal(state.phase, 'planning');
  assert.ok(Object.isFrozen(state.relationships.mentor));
  assert.throws(() => { state.attributes.money = 99; }, TypeError);
  for (const [input, code] of [
    [overrides({ money: -1 }), 'INITIAL_RANGE'], [overrides({ money: 0.5 }), 'INITIAL_RANGE'],
    [overrides({ missing: 1 }), 'UNKNOWN_TARGET'], [overrides({}, { registered: 1 }), 'INITIAL_TYPE'],
    [overrides({}, { missing: true }), 'UNKNOWN_FLAG'], [overrides({}, {}, { mentor: { trust: 101 } }), 'INITIAL_RANGE'],
    [overrides({}, {}, { mentor: { missing: 1 } }), 'UNKNOWN_TARGET'], [overrides({}, {}, { ghost: {} }), 'UNKNOWN_TARGET'],
    [{ attributes: {} }, 'INVALID_OVERRIDES'],
  ]) {
    const result = createRun(snapshot, 42, { mode: 'activities-only', initialOverrides: input });
    assert.equal(result.ok, false); assert.equal(result.error.code, code);
  }
});

test('seed and activities-only mode are explicit runtime contracts', async () => {
  const snapshot = await content();
  for (const seed of [-1, 4294967296, 0.1, NaN, Infinity, '42']) {
    assert.equal(createRun(snapshot, seed, { mode: 'activities-only' }).error.code, 'INVALID_SEED');
  }
  assert.equal(createRun(snapshot, 1, {}).error.code, 'UNSUPPORTED_MODE');
  assert.equal(createRun(snapshot, 1).error.code, 'UNSUPPORTED_MODE');
  assert.equal(start(snapshot, 0xffffffff).rng.seed, 0xffffffff);
});

test('six periods advance exactly once, roll December into January and stop at final valid period', async () => {
  const snapshot = await content(p => { doc(p, 'project').startYear = 7; doc(p, 'project').startMonth = 12; });
  let state = start(snapshot);
  const revisions = [0];
  for (let month = 0; month < 2; month++) {
    assert.equal(state.phase, 'planning'); assert.equal(state.schedule, null);
    state = plan(snapshot, state, month === 0 ? ['study', 'work', 'rest'] : ['study', 'rest', 'study']); revisions.push(state.stateRevision);
    for (let slot = 0; slot < 3; slot++) {
      const period = month * 3 + slot;
      assert.deepEqual(calendarTime(snapshot, state), { periodIndex: period, year: month === 0 ? 7 : 8, month: month === 0 ? 12 : 1, periodInMonth: slot });
      state = execute(snapshot, state); revisions.push(state.stateRevision);
      assert.equal(state.periodIndex, period);
      assert.equal(state.lastActivityResult.time.periodIndex, period);
      state = send(snapshot, state, { type: 'acknowledgeResult' }); revisions.push(state.stateRevision);
      assert.equal(state.phase, 'period-end'); assert.equal(state.periodIndex, period);
      rejected(snapshot, state, { type: 'executeActivity' }, 'WRONG_PHASE');
      state = send(snapshot, state, { type: 'completePeriodWithoutEvents' }); revisions.push(state.stateRevision);
    }
  }
  assert.deepEqual(revisions, Array.from({ length: 21 }, (_, i) => i));
  assert.equal(state.phase, 'finished'); assert.equal(state.periodIndex, 5);
  assert.deepEqual(state.attributes, { skill: 30, money: 27, stamina: 60, stress: 5 });
  assert.equal(state.rng.draws, 0);
  assert.deepEqual(state.completedEvents, []);
  assert.deepEqual(state.flags, { registered: false, certified: false });
  for (const command of [{ type: 'executeActivity' }, { type: 'acknowledgeResult' }, { type: 'completePeriodWithoutEvents' }, { type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] }]) {
    rejected(snapshot, state, command, 'WRONG_PHASE');
  }
});

test('stale or duplicate command, bad shape and wrong phase preserve input identity', async () => {
  const snapshot = await content();
  let state = start(snapshot);
  rejected(snapshot, state, { type: 'executeActivity' }, 'WRONG_PHASE');
  for (const command of [null, {}, { type: 'unknown' }, { type: 'executeActivity', extra: 1 }, { type: 'setSchedule', activityIds: ['study'] }, { type: 'setSchedule', activityIds: ['study', 3, 'rest'] }]) {
    rejected(snapshot, state, command, 'INVALID_COMMAND');
  }
  rejected(snapshot, state, { type: 'setSchedule', activityIds: ['study', 'missing', 'rest'] }, 'UNKNOWN_ACTIVITY');
  const command = { type: 'setSchedule', activityIds: ['study', 'work', 'rest'] };
  state = send(snapshot, state, command);
  command.activityIds[0] = 'rest';
  assert.equal(state.schedule[0], 'study');
  rejected(snapshot, state, command, 'REVISION_CONFLICT', 0);
  assert.equal(dispatchCommand(snapshot, state, command).error.code, 'INVALID_REVISION');
  rejected(snapshot, state, command, 'INVALID_REVISION', -1);
  state = execute(snapshot, state);
  rejected(snapshot, state, { type: 'executeActivity' }, 'REVISION_CONFLICT', state.stateRevision - 1);
  rejected(snapshot, state, command, 'WRONG_PHASE');
  rejected(snapshot, state, { type: 'completePeriodWithoutEvents' }, 'WRONG_PHASE');
});

test('midmonth replanning preserves completed slots and recovers from insufficient cost', async () => {
  const snapshot = await content();
  let state = plan(snapshot, start(snapshot, 42, overrides({ money: 5 })), ['study', 'study', 'rest']);
  state = finishPeriod(snapshot, execute(snapshot, state));
  const error = rejected(snapshot, state, { type: 'executeActivity' }, 'INSUFFICIENT_COST');
  assert.match(error.message, /money/);
  rejected(snapshot, state, { type: 'setSchedule', activityIds: ['rest', 'work', 'rest'] }, 'COMPLETED_SLOT');
  state = plan(snapshot, state, ['study', 'work', 'study']);
  state = finishPeriod(snapshot, execute(snapshot, state));
  state = execute(snapshot, state);
  assert.equal(state.attributes.money, 7);
  assert.equal(state.attributes.skill, 20);
});

test('all costs preflight atomically against configured minimum, even when a later cost fails', async () => {
  const snapshot = await content(p => {
    doc(p, 'attributes').attributes.find(a => a.id === 'stamina').min = 10;
    const activity = doc(p, 'activity', 'study');
    activity.successCheck = { chance: literal(5000) }; activity.successEffects = []; activity.failureEffects = [];
  });
  const state = plan(snapshot, start(snapshot, 42, overrides({ stamina: 19 })));
  const error = rejected(snapshot, state, { type: 'executeActivity' }, 'INSUFFICIENT_COST');
  assert.equal(error.path, '/activities/study/costs/1');
  assert.equal(state.attributes.money, 30); assert.equal(state.rng.draws, 0);
  const payable = execute(snapshot, plan(snapshot, start(snapshot, 42, overrides({ stamina: 20 }))));
  assert.equal(payable.attributes.stamina, 10);
});

test('conditions are rechecked at execution and do not consume random numbers', async () => {
  const snapshot = await content(p => { doc(p, 'activity', 'study').requirements = { op: 'flag', flagId: 'registered', equals: true }; });
  const state = plan(snapshot, start(snapshot));
  rejected(snapshot, state, { type: 'executeActivity' }, 'REQUIREMENTS_NOT_MET');
  assert.equal(execute(snapshot, plan(snapshot, start(snapshot, 42, overrides({}, { registered: true })))).attributes.skill, 10);
});

test('effects observe prior changes, including costs; relationships and flags are logged', async () => {
  const snapshot = await content(p => {
    doc(p, 'activity', 'study').baseEffects = [
      effect('skill', read('money'), 'set'), effect('skill', 100), effect('stress', -100),
      { op: 'add', target: { kind: 'relationship', characterId: 'mentor', relationshipId: 'trust' }, value: read('skill') },
      { op: 'setFlag', flagId: 'registered', value: true },
    ];
  });
  const state = execute(snapshot, plan(snapshot, start(snapshot)));
  assert.equal(state.attributes.skill, 100); assert.equal(state.attributes.stress, 0);
  assert.equal(state.relationships.mentor.trust, 100); assert.equal(state.flags.registered, true);
  const changes = state.lastActivityResult.changes;
  assert.equal(changes[2].after, 25);
  assert.deepEqual({ requested: changes[3].requested, after: changes[3].after, delta: changes[3].delta, clamped: changes[3].clamped },
    { requested: 125, after: 100, delta: 75, clamped: 'max' });
  assert.equal(changes[4].clamped, 'min');
  assert.equal(changes[6].kind, 'flag');
});

test('chance reads pre-cost/pre-base-effect state; success/failure branches see base effects', async () => {
  for (const [money, chance, passed] of [[30, 6000, true], [5, 1000, false]]) {
    const snapshot = await content(p => {
      const activity = doc(p, 'activity', 'study');
      activity.successCheck = { chance: { op: 'multiply', left: read('money'), right: literal(200) } };
      activity.baseEffects = [effect('money', 999, 'set'), effect('skill', 3, 'set')];
      activity.successEffects = [effect('skill', read('skill'))]; activity.failureEffects = [effect('skill', -1)];
    });
    const state = execute(snapshot, plan(snapshot, start(snapshot, 42, overrides({ money }))));
    assert.equal(state.lastActivityResult.success.chance, chance);
    assert.equal(state.lastActivityResult.success.passed, passed);
    assert.equal(state.attributes.skill, passed ? 6 : 2);
    assert.equal(state.rng.draws, 1);
  }
});

test('chance clamps to basis point endpoints and still consumes one bounded draw', async () => {
  for (const [chance, clamped, passed] of [[-1, 0, false], [0, 0, false], [1783, 1783, false], [1784, 1784, true], [10000, 10000, true], [20000, 10000, true]]) {
    const snapshot = await content(p => {
      const activity = doc(p, 'activity', 'rest');
      activity.successCheck = { chance: literal(chance) }; activity.successEffects = []; activity.failureEffects = [];
    });
    const state = execute(snapshot, plan(snapshot, start(snapshot), ['rest', 'rest', 'rest']));
    assert.deepEqual(state.lastActivityResult.success, { requestedChance: chance, chance: clamped, roll: 1783, passed, rawDraws: 1 });
    assert.equal(state.rng.draws, 1);
  }
});

test('overflow inside a clamp or after RNG/costs rolls back the whole transaction', async () => {
  const overflowing = { op: 'add', left: literal(2147483647), right: literal(1) };
  for (const field of ['baseEffects', 'successEffects', 'failureEffects', 'chance']) {
    const snapshot = await content(p => {
      const activity = doc(p, 'activity', 'study');
      activity.successCheck = { chance: literal(field === 'failureEffects' ? 0 : 10000) };
      activity.successEffects = []; activity.failureEffects = [];
      const expression = { op: 'clamp', value: overflowing, min: 0, max: 100 };
      if (field === 'chance') activity.successCheck.chance = expression;
      else activity[field] = [effect('money', 1), effect('skill', expression)];
    });
    const state = plan(snapshot, start(snapshot));
    const error = rejected(snapshot, state, { type: 'executeActivity' }, 'INTEGER_OVERFLOW');
    assert.match(error.path, /value/);
    assert.equal(state.rng.draws, 0); assert.equal(state.attributes.money, 30);
  }
});

test('adding to an int32 state must not overflow before target clamping', async () => {
  const snapshot = await content(p => {
    const skill = doc(p, 'attributes').attributes.find(a => a.id === 'skill');
    skill.max = 2147483647; skill.initial = 2147483647;
    doc(p, 'activity', 'study').baseEffects = [effect('skill', 1)];
  });
  const state = plan(snapshot, start(snapshot));
  rejected(snapshot, state, { type: 'executeActivity' }, 'INTEGER_OVERFLOW');
});

test('expressions and condition operators evaluate without eval or wall clock', async () => {
  const snapshot = await content(); const state = start(snapshot);
  const rel = { op: 'read', target: { kind: 'relationship', characterId: 'mentor', relationshipId: 'trust' } };
  assert.equal(evaluateNumber(snapshot, state, rel), 0);
  assert.equal(evaluateNumber(snapshot, state, { op: 'subtract', left: read('money'), right: literal(3) }), 27);
  assert.equal(evaluateNumber(snapshot, state, { op: 'clamp', value: literal(100), min: 2, max: 9 }), 9);
  for (const [field, value] of Object.entries(calendarTime(snapshot, state))) assert.equal(evaluateNumber(snapshot, state, { op: 'time', field }), value);
  for (const [comparator, expected] of [['eq', false], ['ne', true], ['gt', true], ['gte', true], ['lt', false], ['lte', false]]) {
    assert.equal(evaluateCondition(snapshot, state, { op: 'compare', comparator, left: literal(2), right: literal(1) }), expected);
  }
  assert.equal(evaluateCondition(snapshot, state, { op: 'all', conditions: [{ op: 'always' }, { op: 'not', condition: { op: 'eventCompleted', eventId: 'final-exam' } }] }), true);
  assert.equal(evaluateCondition(snapshot, state, { op: 'any', conditions: [{ op: 'flag', flagId: 'registered', equals: true }, { op: 'always' }] }), true);
});

test('content/version mismatches are rejected before any command', async () => {
  const snapshot = await content(); const state = start(snapshot);
  const other = await content(p => { doc(p, 'project').title = 'different'; });
  rejected(other, state, { type: 'executeActivity' }, 'CONTENT_MISMATCH');
  rejected(snapshot, { ...state, coreVersion: 'future' }, { type: 'executeActivity' }, 'INCOMPATIBLE_STATE');
});

test('same seed and command log reproduces byte-identical states AND transitions across processes/timezones', async () => {
  const snapshot = await content(p => {
    const activity = doc(p, 'activity', 'rest');
    activity.baseEffects = []; activity.successCheck = { chance: literal(5000) };
    activity.successEffects = [effect('skill', 1)]; activity.failureEffects = [effect('skill', 2)];
  });
  const commands = [];
  for (let month = 0; month < 2; month++) {
    commands.push({ type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] });
    for (let i = 0; i < 3; i++) commands.push({ type: 'executeActivity' }, { type: 'acknowledgeResult' }, { type: 'completePeriodWithoutEvents' });
  }
  const moduleUrl = new URL('../../dist/packages/core/index.js', import.meta.url).href;
  const source = `import { readFileSync } from 'node:fs';
    import { createRun, dispatchCommand } from ${JSON.stringify(moduleUrl)};
    const {snapshot, commands, seed} = JSON.parse(readFileSync(0, 'utf8'));
    let state = createRun(snapshot, seed, {mode:'activities-only'}).state;
    const history = [{state}];
    for (const command of commands) { const result = dispatchCommand(snapshot, state, command, state.stateRevision);
      if (!result.ok) throw Error(JSON.stringify(result.error)); state = result.state; history.push(result); }
    process.stdout.write(JSON.stringify(history));`;
  const run = (seed, timezone) => {
    const env = { ...process.env, TZ: timezone }; delete env.NODE_OPTIONS;
    const result = spawnSync(process.execPath, ['--input-type=module', '--eval', source], { input: JSON.stringify({ snapshot, commands, seed }), encoding: 'utf8', env });
    assert.equal(result.status, 0, result.stderr); return result.stdout;
  };
  const first = run(42, 'UTC');
  assert.equal(first, run(42, 'Asia/Seoul'));
  const changed = run(43, 'UTC');
  assert.notEqual(first, changed);
  const rolls = output => JSON.parse(output).filter(r => r.transition?.activity).map(r => r.transition.activity.success.roll);
  assert.notDeepEqual(rolls(first), rolls(changed));
  assert.equal(JSON.parse(first).at(-1).state.rng.draws, 6);
});

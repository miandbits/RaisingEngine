import test from 'node:test';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { createInterface } from 'node:readline';
import { readFile } from 'node:fs/promises';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { createRun, dispatchCommand } from '../../dist/packages/core/run.js';
import { runScenario } from '../../dist/packages/runtime-service/scenario.js';
import { content, doc, numeric, literal } from '../runtime/helpers.mjs';
import { fixture } from '../gameplay/fixture.mjs';

// Independent protocol consumer. Does not use the production bridge or a TS fallback.
class NativeOracle {
  constructor(t) {
    this.pending = [];
    const executable = process.env.RAISING_NATIVE_EXECUTABLE ?? resolve('dist/native', JSON.parse(readFileSync('dist/native/runtime.json', 'utf8')).executable);
    this.process = spawn(resolve(executable), [], { windowsHide: true });
    this.error = '';
    this.process.stderr.on('data', bytes => { this.error += bytes; });
    createInterface({ input: this.process.stdout }).on('line', line => {
      const next = this.pending.shift();
      if (!next) return;
      try { next.resolve(JSON.parse(line)); } catch (error) { next.reject(error); }
    });
    this.process.on('error', error => { for (const next of this.pending.splice(0)) next.reject(error); });
    this.process.on('exit', code => { for (const next of this.pending.splice(0)) next.reject(Error(`Native exit ${code}: ${this.error}`)); });
    t.after(() => { this.process.stdin.end(); this.process.kill(); });
  }
  request(value) {
    return new Promise((resolve, reject) => {
      this.pending.push({ resolve, reject });
      this.process.stdin.write(`${JSON.stringify(value)}\n`);
    });
  }
}

function equivalent(actual, expected, label) {
  assert.equal(actual.ok, expected.ok, `${label}: ${JSON.stringify(actual)}`);
  if (expected.ok) assert.deepEqual(actual, expected, label);
  else {
    assert.equal(actual.error.code, expected.error.code, `${label} error code`);
    assert.equal(actual.error.path, expected.error.path, `${label} error path`);
    if (expected.state) assert.deepEqual(actual.state, expected.state, `${label} error rollback`);
  }
}

for (const [sample, routes] of [
  ['academy-demo', ['certified-route', 'ordinary-route']],
  ['academy-year', ['knight-year', 'scholar-year', 'artisan-year', 'neighbor-year', 'wanderer-year']],
]) {
  for (const route of routes) test(`independent native full transition parity: ${sample}/${route}`, { timeout: 60000 }, async t => {
    const built = await buildSnapshot(await loadProject(resolve('examples', sample)));
    assert.equal(built.ok, true, JSON.stringify(built.diagnostics));
    const snapshot = built.snapshot;
    const scenario = JSON.parse(await readFile(resolve('examples', sample, 'scenarios', `${route}.json`), 'utf8'));
    const reference = await runScenario(snapshot, scenario);
    assert.equal(reference.ok, true, JSON.stringify(reference.error));
    const oracle = new NativeOracle(t);
    const options = { mode: 'full' };
    const created = createRun(snapshot, scenario.seed, options);
    const native = await oracle.request({ operation: 'create', content: snapshot, seed: scenario.seed, options });
    equivalent(native, created, 'initial state');
    let state = native.state;
    let jsState = created.state;
    for (const [index, command] of reference.session.commands.entries()) {
      const expected = dispatchCommand(snapshot, jsState, command, jsState.stateRevision);
      const actual = await oracle.request({ operation: 'command', content: snapshot, state, command, expectedRevision: state.stateRevision });
      equivalent(actual, expected, `command ${index} ${command.type}`);
      state = actual.state; jsState = expected.state;
    }
    assert.deepEqual(state, reference.session.state);
  });
}

test('independent native rejection parity and unchanged state for malformed/stale/phase commands', { timeout: 60000 }, async t => {
  const snapshot = await content();
  const created = createRun(snapshot, 42, { mode: 'full' });
  assert.equal(created.ok, true);
  const oracle = new NativeOracle(t);
  for (const [command, revision] of [
    [{ type: 'executeActivity' }, 0],
    [{ type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] }, 1],
    [{ type: 'setSchedule', activityIds: ['missing', 'rest', 'rest'] }, 0],
    [{ type: 'setSchedule', activityIds: ['rest', 'rest'] }, 0],
    [{ type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'], extra: true }, 0],
    [{ type: 'choose', choiceId: 'missing' }, 0],
    [{ type: 'no-such-command' }, 0], [null, 0], [{ type: 'executeActivity' }, -1],
  ]) {
    equivalent(await oracle.request({ operation: 'command', content: snapshot, state: created.state, command, expectedRevision: revision }),
      dispatchCommand(snapshot, created.state, command, revision), JSON.stringify(command));
  }
});

test('independent native transaction rollback after resource and effect failures', { timeout: 60000 }, async t => {
  const oracle = new NativeOracle(t);
  for (const mode of ['cost', 'overflow']) {
    const snapshot = await content(project => {
      const activity = doc(project, 'activity', 'study');
      if (mode === 'cost') activity.costs = [{ attributeId: 'money', amount: 2147483647 }];
      else activity.baseEffects = [numeric('money', literal(2147483647))];
    });
    const created = createRun(snapshot, 42, { mode: 'full' });
    const ready = dispatchCommand(snapshot, created.state, { type: 'setSchedule', activityIds: ['study', 'rest', 'rest'] }, 0);
    assert.equal(ready.ok, true);
    const command = { type: 'executeActivity' };
    const expected = dispatchCommand(snapshot, ready.state, command, ready.state.stateRevision);
    assert.equal(expected.ok, false, `${mode} must really fail`);
    equivalent(await oracle.request({ operation: 'command', content: snapshot, state: ready.state, command, expectedRevision: ready.state.stateRevision }), expected, mode);
  }
});

test('independent native replay of frozen pre-migration save reaches exact saved state', { timeout: 60000 }, async t => {
  const snapshot = await content();
  const saved = JSON.parse(await readFile(new URL('./fixtures/pre-migration-save.json', import.meta.url), 'utf8'));
  const oracle = new NativeOracle(t);
  let result = await oracle.request({ operation: 'create', content: snapshot, seed: saved.origin.seed, options: saved.origin });
  assert.equal(result.ok, true);
  for (const command of saved.commands) {
    result = await oracle.request({ operation: 'command', content: snapshot, state: result.state, command, expectedRevision: result.state.stateRevision });
    assert.equal(result.ok, true, JSON.stringify(result));
  }
  assert.deepEqual(result.state, saved.state);
});

test('independent native creation parity at seed and initial-state boundaries', { timeout: 60000 }, async t => {
  const snapshot = await content();
  const oracle = new NativeOracle(t);
  for (const seed of [0, 1, 42, 2147483647, 4294967295, -1, 4294967296, 1.5, '42', null]) {
    const options = { mode: 'full' };
    equivalent(await oracle.request({ operation: 'create', content: snapshot, seed, options }),
      createRun(snapshot, seed, options), `seed ${seed}`);
  }
  for (const options of [
    { mode: 'invalid' },
    { mode: 'full', initialOverrides: { attributes: { money: -1 }, flags: {}, relationships: {} } },
    { mode: 'full', initialOverrides: { attributes: { unknown: 0 }, flags: {}, relationships: {} } },
    { mode: 'full', initialOverrides: { attributes: {}, flags: { registered: 1 }, relationships: {} } },
    { mode: 'full', initialOverrides: { attributes: {}, flags: {}, relationships: { mentor: { trust: 1 } } } },
    { mode: 'full', initialOverrides: { attributes: {}, flags: {}, relationships: { mentor: { unknown: 1 } } } },
  ]) {
    equivalent(await oracle.request({ operation: 'create', content: snapshot, seed: 42, options }),
      createRun(snapshot, 42, options), `options ${JSON.stringify(options)}`);
  }
});

test('independent native gameplay parity includes failed commands, ready-phase gift event and continued schedule', { timeout: 60000 }, async t => {
  const { built } = await fixture(t, project => { project.gameplay.shops[0].id = 'constructor'; });
  assert.equal(built.ok, true, JSON.stringify(built.diagnostics));
  const snapshot = built.snapshot, oracle = new NativeOracle(t);
  let jsState = createRun(snapshot, 42, { mode: 'full' }).state;
  const initial = await oracle.request({ operation: 'create', content: snapshot, seed: 42, options: { mode: 'full' } });
  assert.equal(initial.ok, true); assert.deepEqual(initial.state, jsState);
  let state = initial.state;
  for (const command of [
    { type: 'visit', locationId: 'market' },
    { type: 'navigate', screenId: 'ending' },
    { type: 'navigate', screenId: 'schedule' },
    { type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] },
    { type: 'visit', locationId: 'city' },
    { type: 'interact', interactionId: 'gift-mentor' },
    { type: 'visit', locationId: 'market' },
    { type: 'buy', shopId: 'constructor', itemId: 'flower', quantity: 0 },
    { type: 'buy', shopId: 'constructor', itemId: 'flower', quantity: 5 },
    { type: 'buy', shopId: 'constructor', itemId: 'flower', quantity: 4 },
    { type: 'buy', shopId: 'constructor', itemId: 'flower', quantity: 2 },
    { type: 'buy', shopId: 'constructor', itemId: 'book', quantity: 2 },
    { type: 'visit', locationId: 'city' },
    { type: 'interact', interactionId: 'gift-mentor' },
    { type: 'visit', locationId: 'home' },
    { type: 'advanceNarrative' }, { type: 'advanceNarrative' },
    { type: 'interact', interactionId: 'gift-mentor' },
    { type: 'executeActivity' }, { type: 'acknowledgeResult' },
  ]) {
    const expected = dispatchCommand(snapshot, jsState, command, jsState.stateRevision);
    const actual = await oracle.request({ operation: 'command', content: snapshot, state, command, expectedRevision: state.stateRevision });
    equivalent(actual, expected, `gameplay ${JSON.stringify(command)}`);
    if (expected.ok) { state = actual.state; jsState = expected.state; }
  }
  assert.equal(state.gameplay.inventory.flower, 1);
  assert.equal(state.completedEvents.includes('gift-friendship'), true);
  assert.equal(state.phase, 'period-end');
});

test('independent native gameplay arithmetic and post-gift failure rollback', { timeout: 60000 }, async t => {
  const oracle = new NativeOracle(t);
  for (const mode of ['price-overflow', 'gift-overflow']) {
    const { built } = await fixture(t, project => {
      if (mode === 'price-overflow') {
        project.gameplay.items[0].maxStack = 2147483647;
        project.gameplay.shops[0].offers[0].stock = null;
      } else {
        project.gameplay.items[0].initialQuantity = 1;
        project.gameplay.interactions[0].effects.push(numeric('money', 2147483647));
      }
    });
    assert.equal(built.ok, true, JSON.stringify(built.diagnostics));
    const snapshot = built.snapshot;
    let state = createRun(snapshot, 42, { mode: 'full' }).state;
    state = dispatchCommand(snapshot, state, { type: 'visit', locationId: 'city' }, state.stateRevision).state;
    if (mode === 'price-overflow') state = dispatchCommand(snapshot, state, { type: 'visit', locationId: 'market' }, state.stateRevision).state;
    const command = mode === 'price-overflow'
      ? { type: 'buy', shopId: 'florist', itemId: 'flower', quantity: 2147483647 }
      : { type: 'interact', interactionId: 'gift-mentor' };
    const expected = dispatchCommand(snapshot, state, command, state.stateRevision);
    assert.equal(expected.ok, false); assert.equal(expected.error.code, 'INTEGER_OVERFLOW');
    equivalent(await oracle.request({ operation: 'command', content: snapshot, state, command, expectedRevision: state.stateRevision }), expected, mode);
  }
});

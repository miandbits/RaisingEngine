import test from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { runScenario } from '../../dist/packages/runtime-service/index.js';
import { content, scenarios, samplePath } from './helpers.mjs';

test('scenario rejects missing, extra and incorrectly addressed choices; never guesses', async () => {
  const snapshot = await content();
  for (const [edit, code] of [
    [s => { s.commands[3].choices = []; }, 'SCENARIO_CHOICE_MISSING'],
    [s => { s.commands[1].choices = structuredClone(s.commands[3].choices); }, 'SCENARIO_CHOICE_UNUSED'],
    [s => { s.commands[3].choices[0].nodeId = 'wrong'; }, 'SCENARIO_CHOICE_TARGET'],
    [s => { s.commands[3].choices[0].choiceId = 'wrong'; }, 'UNKNOWN_CHOICE'],
  ]) {
    const scenario = structuredClone(scenarios.find(s => s.id === 'certified-route')); edit(scenario);
    const result = await runScenario(snapshot, scenario);
    assert.equal(result.ok, false); assert.equal(result.error.code, code);
  }
});

test('stepLimit bounds expanded node commands and reports the exact stopping state', async () => {
  const snapshot = await content();
  const scenario = structuredClone(scenarios.find(s => s.id === 'certified-route'));
  scenario.stepLimit = 27;
  const stopped = await runScenario(snapshot, scenario);
  assert.equal(stopped.ok, false); assert.equal(stopped.error.code, 'SCENARIO_STEP_LIMIT');
  assert.equal(stopped.steps, 27); assert.equal(stopped.session.state.phase, 'narrative');
  scenario.stepLimit = 28;
  assert.equal((await runScenario(snapshot, scenario)).ok, true);
});

test('scenario checks assertions at period boundaries and reports failure pointer', async () => {
  const snapshot = await content();
  const scenario = structuredClone(scenarios.find(s => s.id === 'certified-route'));
  scenario.commands[1].assertions = [{ kind: 'condition', condition: { op: 'flag', flagId: 'registered', equals: true } }];
  const result = await runScenario(snapshot, scenario);
  assert.equal(result.ok, false); assert.equal(result.error.code, 'SCENARIO_ASSERTION_FAILED');
  assert.equal(result.error.path, '/commands/1/assertions/0');
  assert.equal(result.session.state.periodIndex, 1);
});

test('scenario structure and command phase mistakes fail as structured results', async () => {
  const snapshot = await content();
  assert.equal((await runScenario(snapshot, {})).error.code, 'SCENARIO_SCHEMA');
  const scenario = structuredClone(scenarios[0]); scenario.commands.shift();
  assert.equal((await runScenario(snapshot, scenario)).error.code, 'SCENARIO_PHASE');
});

test('checkpoint mutation is detected instead of silently changing replay inputs', async () => {
  const snapshot = await content();
  const result = await runScenario(snapshot, scenarios[0], { checkpoint: async session => {
    const changed = structuredClone(session); changed.state.attributes.money++;
    return { ok: true, session: changed };
  } });
  assert.equal(result.ok, false); assert.equal(result.error.code, 'CHECKPOINT_MISMATCH');
});

test('scenario input edits during async validation do not change the running scenario', async () => {
  const snapshot = await content();
  const scenario = structuredClone(scenarios.find(s => s.id === 'certified-route'));
  const pending = runScenario(snapshot, scenario);
  scenario.commands.length = 0; scenario.id = 'edited'; scenario.stepLimit = 1;
  const result = await pending;
  assert.equal(result.ok, true); assert.equal(result.scenarioId, 'certified-route');
});

test('scenario CLI exposes both routes, JSON results, roundtrip verification and exit codes', () => {
  const env = { ...process.env }; delete env.NODE_OPTIONS;
  const entry = fileURLToPath(new URL('../../dist/apps/tools/cli/scenarios.js', import.meta.url));
  const cli = args => spawnSync(process.execPath, [entry, ...args], { encoding: 'utf8', env });
  const result = cli([samplePath, '--json', '--roundtrip']);
  assert.equal(result.status, 0, result.stderr);
  const report = JSON.parse(result.stdout);
  assert.equal(report.ok, true); assert.equal(report.roundtrip, true); assert.equal(report.results.length, 2);
  assert.deepEqual(report.results.map(r => r.state.endingId).sort(), ['certified', 'ordinary']);
  assert.equal(cli([samplePath, 'missing']).status, 1);
  assert.equal(cli([]).status, 2); assert.equal(cli([samplePath, '--unknown']).status, 2);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import { fixture } from './fixture.mjs';
import { runAnalysis, replayTrace } from '../../dist/packages/analysis-service/runner.js';
import { traceScenario } from '../../dist/packages/analysis-service/scenario.js';
import { runScenario } from '../../dist/packages/runtime-service/scenario.js';

test('bounded search discovers and replays a purchased gift interaction event', async t => {
  const f = await fixture(t); assert.equal(f.built.ok, true, JSON.stringify(f.built.diagnostics));
  const result = await runAnalysis(f.built.snapshot, { runs: 1, seed: 42, strategy: 'explore', maxCommands: 100,
    timeoutMs: 30000, target: { kind: 'event', id: 'gift-friendship' }, beamWidth: 32, maxStates: 3000 });
  assert.equal(result.targetStatus, 'found', JSON.stringify({ stop: result.stopReason, explored: result.exploredStates, traces: result.traces.map(x => x.commands) }));
  const trace = result.traces.find(x => x.outcome === 'target'); assert.ok(trace);
  assert.ok(trace.commands.some(x => x.type === 'visit'));
  assert.ok(trace.commands.some(x => x.type === 'buy'));
  assert.ok(trace.commands.some(x => x.type === 'interact'));
  assert.ok(trace.eventOrder.includes('gift-friendship'));
  const replay = replayTrace(f.built.snapshot, trace);
  assert.deepEqual(replay.state, trace.state);
});

test('random exploration includes places, purchases and gifts; limited search remains not-found', async t => {
  const f = await fixture(t); assert.equal(f.built.ok, true);
  const result = await runAnalysis(f.built.snapshot, { runs: 10, seed: 100, strategy: 'explore', maxCommands: 200, timeoutMs: 30000 });
  const commands = result.traces.flatMap(x => x.commands);
  for (const type of ['visit', 'buy', 'interact']) assert.ok(commands.some(x => x.type === type), type);
  const limited = await runAnalysis(f.built.snapshot, { runs: 1, seed: 42, strategy: 'explore', maxCommands: 1,
    timeoutMs: 30000, target: { kind: 'event', id: 'gift-friendship' }, beamWidth: 1, maxStates: 1 });
  assert.equal(limited.targetStatus, 'not-found'); assert.equal(limited.proof, null);
});

test('completed gameplay trace becomes an exact regression scenario without dropping purchases or gifts', async t => {
  const f = await fixture(t); assert.equal(f.built.ok, true);
  const result = await runAnalysis(f.built.snapshot, { runs: 3, seed: 100, strategy: 'explore', maxCommands: 1000, timeoutMs: 30000 });
  const trace = result.traces.find(x => x.state.phase === 'finished' && x.commands.some(c => c.type === 'buy'));
  assert.ok(trace, JSON.stringify(result.outcomes));
  const scenario = await traceScenario(f.built.snapshot, trace, 'gameplay-regression');
  assert.ok(scenario.commands.some(x => x.type === 'dispatch' && x.command.type === 'buy'));
  const executed = await runScenario(f.built.snapshot, scenario); assert.equal(executed.ok, true, JSON.stringify(executed.error));
  assert.deepEqual(executed.session.state, trace.state);
});

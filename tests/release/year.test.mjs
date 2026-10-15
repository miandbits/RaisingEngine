import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { runScenario } from '../../dist/packages/runtime-service/scenario.js';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';
import { calendarTime } from '../../dist/packages/core/evaluation.js';
const root = resolve('examples/academy-year');
const expected = { knight: [106,0,0,134,15,100], scholar: [0,106,0,146,15,100], artisan: [0,0,106,158,15,100], neighbor: [0,0,0,192,40,95] };

for (const route of ['knight','scholar','artisan','neighbor','wanderer']) test(`annual ${route}: 36 periods, year rollover and every-command persistence parity`, { timeout: 60000 }, async () => {
  const loaded = await loadProject(root); const built = await buildSnapshot(loaded); assert.equal(built.ok, true);
  assert.equal(built.snapshot.project.durationMonths, 12); assert.equal(built.snapshot.events.length, 19);
  const scenario = JSON.parse(await readFile(`${root}/scenarios/${route}-year.json`, 'utf8'));
  const baseline = await runScenario(built.snapshot, scenario); assert.equal(baseline.ok, true, JSON.stringify(baseline.error));
  assert.equal(baseline.session.state.phase, 'finished'); assert.equal(baseline.session.state.endingId, route);
  assert.equal(baseline.session.state.periodIndex, 35);
  const a = baseline.session.state.attributes;
  if (expected[route]) assert.deepEqual([a.skill,a.knowledge,a.craftsmanship,a.money,a.reputation,a.stamina],expected[route]);
  const times = new Map(); let checkpoints = 0;
  const restored = await runScenario(built.snapshot, scenario, { checkpoint: async session => {
    times.set(session.state.periodIndex, calendarTime(built.snapshot, session.state)); checkpoints++;
    const encoded = await encodeSave(built.snapshot, session); assert.equal(encoded.ok, true);
    return decodeSave(built.snapshot, encoded.text);
  } });
  assert.equal(restored.ok, true, JSON.stringify(restored.error));
  assert.deepEqual(restored.session, baseline.session); assert.deepEqual(restored.transitions, baseline.transitions);
  assert.equal(checkpoints, baseline.steps + 1);
  assert.equal(times.size, 36); assert.equal(times.get(0).month, 4); assert.equal(times.get(27).year, 2); assert.equal(times.get(27).month, 1);
  assert.equal(times.get(35).month, 3); assert.equal(baseline.session.state.completedEvents.length, 12);
  if (route === 'wanderer') {
    // The serialized RNG state is part of the full-session parity check above.
    assert.ok(baseline.session.state.rng.draws >= 12);
    assert.ok(a.skill > 36 && a.skill < 84, 'seed 42 includes both success and failure');
  }
});

import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile, writeFile } from 'node:fs/promises';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { createRun, dispatchCommand } from '../../dist/packages/core/run.js';
import { nativeCreateRun, nativeDispatchCommand, nativeReplay } from '../../dist/packages/native-bridge/index.js';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';

// Opt-in independent verification of the unchanged project authored by the Qt wizard test.
test('Qt-authored progression project executes native tier changes and restores exact save state',
  { skip: !process.env.RAISING_PROGRESSION_PROOF }, async () => {
    const proof = JSON.parse(await readFile(process.env.RAISING_PROGRESSION_PROOF, 'utf8'));
    const built = await buildSnapshot(await loadProject(proof.root));
    assert.equal(built.ok, true, JSON.stringify(built.diagnostics));
    const content = built.snapshot;
    const origin = { seed: 42, mode: 'activities-only', initialOverrides: { attributes: {}, flags: {}, relationships: {} } };
    const created = nativeCreateRun(content, origin.seed, origin);
    assert.equal(created.ok, true, JSON.stringify(created));
    assert.deepEqual(created, createRun(content, origin.seed, origin));
    let state = created.state;
    const commands = [], results = [];
    const send = command => {
      const actual = nativeDispatchCommand(content, state, command, state.stateRevision);
      assert.equal(actual.ok, true, JSON.stringify(actual));
      assert.deepEqual(actual, dispatchCommand(content, state, command, state.stateRevision));
      state = actual.state; commands.push(command);
    };
    send({ type: 'setSchedule', activityIds: ['training-test', 'training-test', 'training-test'] });
    for (let i = 0; i < 3; i++) {
      const before = state;
      send({ type: 'executeActivity' });
      assert.equal(before.attributes.money - state.attributes.money, [7, 11, 13][i]);
      assert.equal(state.attributes.skill - before.attributes.skill, [2, 4, 6][i]);
      assert.equal(state.attributes['training-test-xp'], i + 1);
      assert.equal(state.lastActivityResult.success.chance, 10000);
      const session = { origin, state, commands: [...commands] };
      const saved = await encodeSave(content, session);
      assert.equal(saved.ok, true, JSON.stringify(saved));
      const restored = await decodeSave(content, saved.text);
      assert.equal(restored.ok, true, JSON.stringify(restored));
      assert.deepEqual(restored.session, session);
      results.push({ period: i, fee: before.attributes.money - state.attributes.money,
        gain: state.attributes.skill - before.attributes.skill, xp: state.attributes['training-test-xp'],
        chance: state.lastActivityResult.success.chance, saveRestored: true });
      send({ type: 'acknowledgeResult' }); send({ type: 'completePeriodWithoutEvents' });
      assert.equal(state.attributes['training-test-tier'], Math.min(i + 2, 3));
    }
    assert.deepEqual(nativeReplay(content, origin, commands), { ok: true, state });
    if (process.env.RAISING_PROGRESSION_NATIVE_EVIDENCE) await writeFile(process.env.RAISING_PROGRESSION_NATIVE_EVIDENCE,
      JSON.stringify({ root: proof.root, seed: origin.seed, commands: commands.length,
        nativeReferenceParity: true, replayExact: true, results, finalState: state }, null, 2));
  });

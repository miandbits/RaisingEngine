import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile, writeFile, readdir, mkdir } from 'node:fs/promises';
import { spawnSync } from 'node:child_process';
import path from 'node:path';
import { runScenario, saveRun, loadRun, encodeSave, decodeSave, MAX_SAVE_BYTES } from '../../dist/packages/runtime-service/index.js';
import { content, scenarios, temporary, start, send, doc, replaceEvents, numeric, literal, samplePath } from './helpers.mjs';

test('both sample routes produce byte-identical state and transitions with disk save/load after EVERY command', async t => {
  const snapshot = await content(); const root = await temporary(t);
  for (const scenario of scenarios) {
    const baseline = await runScenario(snapshot, scenario);
    assert.equal(baseline.ok, true);
    let checkpoints = 0;
    const seen = new Set();
    const restored = await runScenario(snapshot, scenario, { checkpoint: async session => {
      checkpoints++; seen.add(session.state.phase);
      const file = path.join(root, `${scenario.id}.save.json`);
      const saved = await saveRun(file, snapshot, session); assert.equal(saved.ok, true, JSON.stringify(saved));
      const loaded = await loadRun(file, snapshot); assert.equal(loaded.ok, true, JSON.stringify(loaded));
      assert.equal(JSON.stringify(loaded.session.state), JSON.stringify(session.state));
      assert.ok(Object.isFrozen(loaded.session.state));
      return loaded;
    } });
    assert.equal(restored.ok, true, JSON.stringify(restored.error));
    assert.equal(checkpoints, baseline.steps + 1);
    assert.ok(seen.has('narrative')); assert.ok(seen.has('activity-result')); assert.ok(seen.has('finished'));
    assert.equal(JSON.stringify(restored.session.state), JSON.stringify(baseline.session.state));
    assert.equal(JSON.stringify(restored.transitions), JSON.stringify(baseline.transitions));
    assert.deepEqual(restored.assertions, baseline.assertions);
  }
  assert.equal((await readdir(root)).filter(name => name.endsWith('.tmp')).length, 0);
});

test('save before choice resumes in a fresh process and applies the choice exactly once', async t => {
  const snapshot = await content(); const root = await temporary(t); let checkpoint;
  const baseline = await runScenario(snapshot, scenarios.find(s => s.id === 'certified-route'), { checkpoint: async session => {
    if (session.state.activeNarrative?.nodeId === 'registration') checkpoint = session;
    return { ok: true, session };
  } });
  assert.equal(baseline.ok, true); assert.ok(checkpoint);
  const file = path.join(root, 'choice.save.json');
  assert.equal((await saveRun(file, snapshot, checkpoint)).ok, true);
  const source = `import {readFileSync} from 'node:fs';
    import {loadProject,buildSnapshot} from ${JSON.stringify(new URL('../../dist/packages/project-service/index.js', import.meta.url).href)};
    import {loadRun,dispatchSession} from ${JSON.stringify(new URL('../../dist/packages/runtime-service/index.js', import.meta.url).href)};
    const {project,file,commands}=JSON.parse(readFileSync(0,'utf8'));
    const built=await buildSnapshot(await loadProject(project)); if(!built.ok)throw Error(JSON.stringify(built));
    const loaded=await loadRun(file,built.snapshot); if(!loaded.ok)throw Error(JSON.stringify(loaded));
    let session=loaded.session;
    for(const command of commands){const result=dispatchSession(built.snapshot,session,command,session.state.stateRevision);
      if(!result.ok)throw Error(JSON.stringify(result.error));session=result.session;}
    process.stdout.write(JSON.stringify(session.state));`;
  const env = { ...process.env, TZ: 'Asia/Seoul' }; delete env.NODE_OPTIONS;
  const child = spawnSync(process.execPath, ['--input-type=module', '--eval', source], {
    input: JSON.stringify({ project: samplePath, file, commands: baseline.session.commands.slice(checkpoint.commands.length) }), encoding: 'utf8', env,
  });
  assert.equal(child.status, 0, child.stderr);
  assert.equal(child.stdout, JSON.stringify(baseline.session.state));
  assert.equal(JSON.parse(child.stdout).relationships.mentor.trust, 1);
});

test('randomized activities preserve RNG draws, outcomes and every continuation transition through saves', async () => {
  const snapshot = await content(p => {
    replaceEvents(p, []);
    const rest = doc(p, 'activity', 'rest'); rest.baseEffects = [];
    rest.successCheck = { chance: literal(5000) }; rest.successEffects = [numeric('skill', 1)]; rest.failureEffects = [numeric('skill', 2)];
  });
  const scenario = { formatVersion: '0.1.0', id: 'random-route', seed: 42, initialOverrides: { attributes: {}, flags: {}, relationships: {} },
    commands: Array.from({ length: 2 }, () => [{ type: 'planMonth', activityIds: ['rest', 'rest', 'rest'] }, ...Array.from({ length: 3 }, () => ({ type: 'runPeriod', choices: [] }))]).flat(),
    assertions: [{ kind: 'ending', endingId: 'ordinary' }], stepLimit: 100 };
  const baseline = await runScenario(snapshot, scenario);
  const restored = await runScenario(snapshot, scenario, { checkpoint: async session => {
    const encoded = await encodeSave(snapshot, session); assert.equal(encoded.ok, true);
    return decodeSave(snapshot, encoded.text);
  } });
  assert.equal(baseline.ok, true); assert.equal(restored.ok, true);
  assert.equal(restored.session.state.rng.draws, 6);
  assert.equal(JSON.stringify(restored.transitions), JSON.stringify(baseline.transitions));
  assert.deepEqual(restored.session.state, baseline.session.state);
});

test('save validation rejects unsupported versions and mismatching content without guessing a migration', async () => {
  const snapshot = await content();
  const encoded = await encodeSave(snapshot, start(snapshot)); assert.equal(encoded.ok, true);
  for (const [field, value, code] of [['saveVersion', '9.0.0', 'SAVE_VERSION'], ['coreVersion', '0.1.0', 'SAVE_CORE_VERSION'], ['contentFingerprint', `sha256:${'0'.repeat(64)}`, 'CONTENT_MISMATCH']]) {
    const saved = JSON.parse(encoded.text); saved[field] = value;
    const result = await decodeSave(snapshot, JSON.stringify(saved));
    assert.equal(result.ok, false); assert.equal(result.error.code, code);
  }
  const other = await content(p => { doc(p, 'project').title = 'changed'; });
  assert.equal((await decodeSave(other, encoded.text)).error.code, 'CONTENT_MISMATCH');
});

test('structural checks and replay reject corrupt cursors, values, RNG, revisions, completed events and journals', async () => {
  const snapshot = await content();
  let session = send(snapshot, start(snapshot), { type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] });
  session = send(snapshot, session, { type: 'executeActivity' });
  const encoded = await encodeSave(snapshot, session); assert.equal(encoded.ok, true);
  const mutations = [
    saved => { saved.state.attributes.money = 999; },
    saved => { saved.state.attributes.unknown = 0; },
    saved => { saved.state.flags.registered = 'true'; },
    saved => { saved.state.completedEvents.push('final-exam'); },
    saved => { saved.state.activeNarrative = { kind: 'event', id: 'final-exam', nodeId: 'missing' }; },
    saved => { saved.state.rng.state = '18446744073709551616'; },
    saved => { saved.state.rng.draws++; },
    saved => { saved.state.stateRevision++; },
    saved => { saved.state.lastActivityResult.changes[0].after++; },
    saved => { saved.state.endingId = 'certified'; },
    saved => { saved.origin.initialOverrides.attributes.money = 1000; },
    saved => { saved.commands[0] = { type: 'advanceNarrative' }; },
    saved => { saved.commands.push({ type: 'arbitraryCode', code: 'bad' }); },
    saved => { saved.extra = 1; },
  ];
  for (const mutate of mutations) {
    const saved = JSON.parse(encoded.text); mutate(saved);
    const restored = await decodeSave(snapshot, JSON.stringify(saved));
    assert.equal(restored.ok, false, JSON.stringify(saved));
    assert.ok(['SAVE_SCHEMA', 'SAVE_STATE_MISMATCH', 'SAVE_REPLAY', 'INITIAL_RANGE'].includes(restored.error.code), restored.error.code);
  }
});

test('invalid JSON, duplicate keys, invalid UTF8, missing files and size limits return errors', async t => {
  const snapshot = await content(); const root = await temporary(t);
  for (const text of ['{', '{"saveVersion":"1.0.0","saveVersion":"1.0.0"}', '{/*comment*/}', '{"a":1,}']) {
    assert.equal((await decodeSave(snapshot, text)).error.code, 'SAVE_JSON');
  }
  assert.equal((await decodeSave(snapshot, ' '.repeat(MAX_SAVE_BYTES + 1))).error.code, 'SAVE_TOO_LARGE');
  assert.equal((await loadRun(path.join(root, 'missing.json'), snapshot)).error.code, 'SAVE_IO');
  const invalid = path.join(root, 'invalid.json'); await writeFile(invalid, Buffer.from([0xff]));
  assert.equal((await loadRun(invalid, snapshot)).error.code, 'SAVE_UTF8');
  const large = path.join(root, 'large.json'); await writeFile(large, Buffer.alloc(MAX_SAVE_BYTES + 1));
  assert.equal((await loadRun(large, snapshot)).error.code, 'SAVE_TOO_LARGE');
});

test('failed validation or failed replacement preserves existing save and cleans temporary file', async t => {
  const snapshot = await content(); const root = await temporary(t); const file = path.join(root, 'safe.json');
  const session = start(snapshot);
  assert.equal((await saveRun(file, snapshot, session)).ok, true);
  const before = await readFile(file, 'utf8');
  const corrupt = structuredClone(session); corrupt.state.attributes.money = 99;
  assert.equal((await saveRun(file, snapshot, corrupt)).error.code, 'SAVE_STATE_MISMATCH');
  assert.equal(await readFile(file, 'utf8'), before);
  const directory = path.join(root, 'directory.json'); await mkdir(directory);
  assert.equal((await saveRun(directory, snapshot, session)).error.code, 'SAVE_IO');
  assert.equal((await readdir(root)).filter(name => name.endsWith('.tmp')).length, 0);
  assert.equal((await saveRun(path.join(root, 'missing', 'save.json'), snapshot, session)).error.code, 'SAVE_IO');
});

test('activities-only sessions also roundtrip without adding event or ending execution', async () => {
  const snapshot = await content();
  const session = start(snapshot, { mode: 'activities-only' });
  const encoded = await encodeSave(snapshot, session);
  assert.equal(encoded.ok, true);
  const restored = await decodeSave(snapshot, encoded.text);
  assert.equal(restored.ok, true); assert.deepEqual(restored.session.state, session.state);
  assert.equal(restored.session.state.endingId, null);
});

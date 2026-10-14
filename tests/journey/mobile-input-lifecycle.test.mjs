import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdir, mkdtemp, cp, writeFile, readFile, rename } from 'node:fs/promises';
import { resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { exportGame } from '../../dist/packages/export-service/export.js';

test('native touch cancellation, letterbox mapping and lifecycle autosave preserve committed state once', { timeout: 60000 }, async () => {
  const output = resolve('.cache/journey-mobile-input'); await mkdir(output, { recursive: true });
  const root = resolve(await mkdtemp(`${output}/run-`)), projectRoot = resolve(root, 'project'), destination = resolve(root, 'game');
  await cp(resolve('examples/academy-journey'), projectRoot, { recursive: true });
  const built = await buildSnapshot(await loadProject(projectRoot)); assert.equal(built.ok, true);
  await exportGame({ content: built.snapshot, projectRoot, destination, target: 'windows', runtimes: { windows: resolve(process.env.NATIVE_PLAYER_OUT || 'dist/native-player-app'), web: resolve('dist/web') } });
  const steps = [
    { lifecycle: ['background', 'background', 'foreground'], checkpoint: 'title-background' },
    { touchWindow: 'start', cancelTouch: true }, { checkpoint: 'cancelled-start' },
    { touchWindow: 'start' }, { checkpoint: 'started' },
    { resize: { width: 1280, height: 900 } }, { waitFrames: 5 },
    { touchWindow: 'save' }, { checkpoint: 'manual-save' },
    { touchWindow: 'visit-market' }, { touchWindow: 'buy-florist-flower' }, { checkpoint: 'bought' },
    { lifecycle: ['background', 'background', 'foreground'], checkpoint: 'after-lifecycle' },
    { touchWindow: 'visit-city' }, { touchWindow: 'interact-gift-yuna' }, { checkpoint: 'gift' },
    { lifecycle: ['background', 'background', 'foreground'], checkpoint: 'after-gift-lifecycle' },
    { waitFrames: 60 }, { screenshot: resolve(output, 'touch-relationship.png') },
  ];
  const input = resolve(root, 'steps.json'), reportFile = resolve(root, 'report.json'), manualFile = resolve(root, 'manual.json');
  await writeFile(input, JSON.stringify(steps));
  const result = spawnSync(resolve(destination, 'RaisingPlayer.exe'), ['--bundle', destination, '--save', manualFile, '--hidden', '--automation', input, '--report', reportFile], { timeout: 30000, windowsHide: true, encoding: 'utf8' });
  assert.equal(result.status, 0, result.stderr || result.stdout);
  const report = JSON.parse(await readFile(reportFile, 'utf8'));
  const checkpoint = name => report.steps.find(step => step.command.checkpoint === name).ui;
  assert.equal(checkpoint('title-background').lifecycleSaves, 0);
  assert.equal(checkpoint('cancelled-start').state, null);
  assert.equal(checkpoint('started').screenId, 'home');
  assert.equal(checkpoint('bought').state.attributes.money, 20);
  assert.equal(checkpoint('bought').state.gameplay.inventory.flower, 1);
  assert.equal(checkpoint('after-lifecycle').lifecycleSaves, 1);
  assert.equal(checkpoint('after-lifecycle').backgrounded, false);
  assert.deepEqual(checkpoint('after-lifecycle').state, checkpoint('bought').state);
  assert.equal(checkpoint('gift').state.relationships.mentor.trust, 15);
  assert.equal(checkpoint('after-gift-lifecycle').lifecycleSaves, 2);
  const manual = JSON.parse(await readFile(manualFile, 'utf8'));
  assert.deepEqual(manual.state, checkpoint('manual-save').state, 'backgrounding preserves the explicit manual slot');
  const autosave = JSON.parse(await readFile(resolve(root, 'autosave.json'), 'utf8'));
  assert.deepEqual(autosave.state, checkpoint('gift').state);
  assert.deepEqual(report.final.state, autosave.state);
  const replay = async (name, extraSteps = []) => {
    const stepsFile=resolve(root, `${name}.steps.json`), reportPath=resolve(root, `${name}.report.json`);
    await writeFile(stepsFile, JSON.stringify([{ restoreAutosave: true }, ...extraSteps, { checkpoint: name }]));
    const restored=spawnSync(resolve(destination, 'RaisingPlayer.exe'), ['--bundle', destination, '--save', manualFile, '--hidden', '--automation', stepsFile, '--report', reportPath], { timeout: 15000, windowsHide: true, encoding: 'utf8' });
    assert.equal(restored.status, 0, restored.stderr || restored.stdout);
    return JSON.parse(await readFile(reportPath, 'utf8')).final;
  };
  await rename(resolve(root, 'autosave.json'), resolve(root, 'saved-autosave.json'));
  assert.deepEqual((await replay('missing-primary')).state, checkpoint('bought').state, 'interrupted replacement restores the prior backup');
  await writeFile(resolve(root, 'autosave.json'), '{}');
  assert.deepEqual((await replay('corrupt-primary')).state, checkpoint('bought').state);
  await writeFile(resolve(root, 'autosave.json.bak'), '{}');
  const rejected = await replay('both-invalid');
  assert.equal(rejected.state, null); assert.match(rejected.message, /자동 저장을 불러오지 못했습니다/);
  const incompatible = structuredClone(autosave); incompatible.contentFingerprint = 'sha256:old-project';
  await writeFile(resolve(root, 'autosave.json'), JSON.stringify(incompatible));
  const restarted = await replay('incompatible-new-run', [
    { lifecycle: ['background', 'foreground'] }, { touchWindow: 'start' }, { touchWindow: 'visit-market' },
    { touchWindow: 'buy-florist-flower' }, { lifecycle: ['background', 'background', 'foreground'] },
  ]);
  assert.equal(restarted.lifecycleSaves, 1);
  assert.equal(restarted.state.attributes.money, 20);
  assert.deepEqual(JSON.parse(await readFile(resolve(root, 'autosave.json'), 'utf8')).state, restarted.state, 'new play after incompatible autosave replaces old autosave on background');
  await writeFile(resolve(output, 'evidence.json'), JSON.stringify({ root, reportFile, manualFile, autosave: resolve(root, 'saved-autosave.json'), finalState: report.final.state, lifecycleSaves: report.final.lifecycleSaves }, null, 2));
});

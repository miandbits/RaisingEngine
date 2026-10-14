import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdir, mkdtemp, cp, writeFile, readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { exportGame } from '../../dist/packages/export-service/export.js';

test('native schedule dropdown opens visible choices and selects activities using real SDL pointer events', { timeout: 60000 }, async () => {
  const output = resolve('.cache/journey-native-dropdown'); await mkdir(output, { recursive: true });
  const root = await mkdtemp(`${output}/run-`), projectRoot = resolve(root, 'project'), destination = resolve(root, 'game');
  await cp(resolve('examples/academy-journey'), projectRoot, { recursive: true });
  const built = await buildSnapshot(await loadProject(projectRoot)); assert.equal(built.ok, true);
  await exportGame({ content: built.snapshot, projectRoot, destination, target: 'windows', runtimes: { windows: resolve('dist/native-player-app'), web: resolve('dist/web') } });
  const steps = [
    { clickWindow: 'start' }, { clickWindow: 'schedule-link' }, { clickWindow: 'plan-0' },
    { checkpoint: 'opened' }, { screenshot: resolve(output, 'verified-dropdown.png') },
    { clickWindow: 'plan-0-option-work' }, { clickWindow: 'plan-2' },
    { clickWindow: 'plan-2-option-rest' }, { clickWindow: 'advance' }, { checkpoint: 'confirmed' },
  ];
  const input = resolve(root, 'steps.json'), reportFile = resolve(root, 'report.json'); await writeFile(input, JSON.stringify(steps));
  const result = spawnSync(resolve(destination, 'RaisingPlayer.exe'), ['--bundle', destination, '--save', resolve(root, 'save.json'), '--hidden', '--automation', input, '--report', reportFile], { encoding: 'utf8', timeout: 30000, windowsHide: true });
  assert.equal(result.status, 0, result.stderr || result.stdout);
  const report = JSON.parse(await readFile(reportFile, 'utf8')); assert.equal(report.ok, true);
  const options = report.steps.find(step => step.command.checkpoint === 'opened').ui.options.filter(option => option.id.startsWith('plan-0-option-'));
  assert.equal(options.length, built.snapshot.activities.length);
  for (const option of options) {
    assert.equal(option.visible, true, option.id);
    assert.ok(option.width >= 150 && option.height >= 20, `${option.id} readable geometry`);
    assert.ok(option.x >= 0 && option.y >= 0 && option.x + option.width <= 1280 && option.y + option.height <= 720, `${option.id} inside viewport`);
    assert.ok(option.y >= option.clipY - 1 && option.y + option.height <= option.clipY + option.clipHeight + 1, `${option.id} not clipped`);
  }
  assert.equal(report.final.state.phase, 'ready');
  assert.equal(report.final.state.schedule[0], 'work');
  assert.equal(report.final.state.schedule[2], 'rest');
});

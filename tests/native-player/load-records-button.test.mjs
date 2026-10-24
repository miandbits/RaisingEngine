import test from 'node:test';
import assert from 'node:assert/strict';
import { cp, mkdir, mkdtemp, readFile, writeFile, access } from 'node:fs/promises';
import { resolve, join } from 'node:path';
import { spawnSync } from 'node:child_process';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { exportGame } from '../../dist/packages/export-service/export.js';
import { desktopTarget, gameDirectory, playerExecutable } from '../../dist/packages/export-service/targets.js';

test('touch Load opens empty records and restores autosaves without any manual save; legacy Load stays disabled', { timeout: 120000 }, async () => {
  await mkdir('.cache/load-records-button', { recursive: true });
  const out = await mkdtemp(resolve('.cache/load-records-button/한글 run-')), projectRoot = join(out, 'project');
  await cp(resolve('examples/my-first-raising-game'), projectRoot, { recursive: true });
  const projectFile = join(projectRoot, 'project.json'), project = JSON.parse(await readFile(projectFile, 'utf8'));
  const target = desktopTarget(), runtime = resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME ?? process.env.RAISING_NATIVE_PLAYER_RUNTIME ?? 'dist/native-player-app');
  const publish = async name => {
    const built = await buildSnapshot(await loadProject(projectRoot)); assert.equal(built.ok, true, JSON.stringify(built.diagnostics));
    const destination = join(out, name);
    await exportGame({ projectRoot, content: built.snapshot, target, destination, runtimes: { windows: runtime, macos: runtime, web: runtime } });
    return destination;
  };
  const run = async (destination, name, steps, save) => {
    const script = join(out, name + '.json'), reportFile = join(out, name + '-report.json');
    await writeFile(script, JSON.stringify(steps));
    const child = spawnSync(playerExecutable(target, destination), ['--bundle', gameDirectory(target, destination), '--save', save, '--automation', script, '--report', reportFile, '--hidden'], { encoding: 'utf8', windowsHide: true, timeout: 30000 });
    assert.equal(child.status, 0, child.error?.message ?? child.stderr);
    const report = JSON.parse(await readFile(reportFile, 'utf8')); assert.equal(report.ok, true);
    return report;
  };
  const legacy = await publish('legacy');
  const legacyReport = await run(legacy, 'legacy', [{ checkpoint: 'title' }], join(out, 'legacy-save.json'));
  assert.equal(legacyReport.final.elements.find(e => e.id === 'load').enabled, false);
  project.visual.saveSystem = { manualSlots: 6, autoSlots: 3 };
  const schedule = project.visual.screens.find(screen => screen.id === 'schedule');
  schedule.elements = schedule.elements.filter(element => element.id !== 'save-controls');
  schedule.elements.push({ id: 'load', kind: 'button', action: 'load', text: 'Load', x: 880, y: 24, width: 270, height: 45 });
  project.visual.screens.push({ id: 'load-records', label: 'Load records', modal: true, elements: [
    { id: 'slots', kind: 'loadSlots', x: 40, y: 30, width: 1200, height: 590 },
    { id: 'close', kind: 'button', action: 'closeModal', text: 'Close', x: 30, y: 650, width: 270, height: 45 },
  ] });
  await writeFile(projectFile, JSON.stringify(project));
  const slotted = await publish('slots'), save = join(out, '기록 save.json');
  // First inspect the affordance so failures identify disabled touch UI directly.
  const title = await run(slotted, 'slot-title', [{ checkpoint: 'title' }], save);
  assert.equal(title.final.elements.find(e => e.id === 'load').enabled, true, 'Record browser must open before any manual save exists');
  const report = await run(slotted, 'empty-and-auto', [
    { clickWindow: 'load' }, { waitFrames: 2 }, { checkpoint: 'empty' }, { clickWindow: 'load-records__close' },
    { clickWindow: 'start' }, { waitFrames: 4 }, { checkpoint: 'initial' }, { clickWindow: 'load' }, { waitFrames: 2 },
    { clickWindow: 'load-records__slots-auto' }, { waitFrames: 2 }, { checkpoint: 'automatic' },
    { clickWindow: 'load-records__slots-request' }, { waitFrames: 2 }, { clickWindow: 'load-records__slots-confirm' }, { waitFrames: 2 }, { checkpoint: 'restored' },
  ], save);
  const at = label => report.steps.find(step => step.command.checkpoint === label).ui;
  assert.equal(at('empty').uiModal, 'load-records');
  assert.equal(at('empty').elements.filter(e => /^load-records__slots-slot-\d+$/.test(e.id)).length, 6);
  assert.equal(at('empty').elements.find(e => e.id === 'load-records__slots-request').enabled, false, 'Empty records cannot be loaded');
  assert.equal(at('automatic').elements.find(e => e.id === 'load-records__slots-request').enabled, true);
  assert.deepEqual(at('restored').state, at('initial').state);
  await assert.rejects(access(save), { code: 'ENOENT' });
  const automaticFile = join(out, '기록 save.slots', 'auto-1.json');
  const automaticRecord = JSON.parse(await readFile(automaticFile, 'utf8'));
  assert.deepEqual(automaticRecord.state, at('initial').state, 'The rotating slot keeps the Unicode save filename stem');
  const fresh = await run(slotted, 'fresh-auto', [{ clickWindow: 'load' }, { waitFrames: 2 }, { clickWindow: 'load-records__slots-auto' }, { waitFrames: 2 }, { clickWindow: 'load-records__slots-request' }, { waitFrames: 2 }, { clickWindow: 'load-records__slots-confirm' }, { waitFrames: 2 }], save);
  assert.deepEqual(fresh.final.state, at('initial').state, 'A fresh process restores an automatic record using only pointer controls');
  await writeFile(resolve('.cache/load-records-button/latest.json'), JSON.stringify({ out, target, ok: true }, null, 2));
});

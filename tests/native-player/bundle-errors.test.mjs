import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdir, mkdtemp, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { exportGame } from '../../dist/packages/export-service/export.js';

test('native player reports corrupt bundle and invalid render geometry without process crashes', { timeout: 90000 }, async () => {
  await mkdir(resolve('.cache/native-bundle-errors'), { recursive: true });
  const root = await mkdtemp(resolve('.cache/native-bundle-errors/run-')), destination = resolve(root, 'game');
  const projectRoot = resolve('examples/academy-journey'), built = await buildSnapshot(await loadProject(projectRoot));
  assert.equal(built.ok, true);
  await exportGame({ content: built.snapshot, projectRoot, destination, target: 'windows',
    runtimes: { windows: resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app'), web: resolve('dist/web') } });
  const gameFile = resolve(destination, 'game.json'), original = JSON.parse(await readFile(gameFile, 'utf8'));
  const cases = [
    ['invalid-json', () => '{broken'],
    ['null', () => 'null'],
    ['assets', b => { delete b.assets; }],
    ['missing-image', b => { b.assets[Object.keys(b.assets)[0]] = 'assets/missing.png'; }],
    ['outside-image', b => { b.assets[Object.keys(b.assets)[0]] = '../private.png'; }],
    ['width-zero', b => { b.content.project.visual.width = 0; }],
    ['height-negative', b => { b.content.project.visual.height = -1; }],
    ['width-overflow', b => { b.content.project.visual.width = 1e20; }],
    ['screens-empty', b => { b.content.project.visual.screens = []; }],
    ['duplicate-screen', b => { b.content.project.visual.screens[1].id = 'title'; }],
    ['element-geometry', b => { b.content.project.visual.screens[0].elements[0].width = 0; }],
    ['no-activities', b => { b.content.activities = []; }],
  ];
  for (const [name, mutate] of cases) {
    const value = structuredClone(original), raw = mutate(value), reportFile = resolve(root, `${name}.json`);
    await writeFile(gameFile, typeof raw === 'string' ? raw : JSON.stringify(value));
    const child = spawnSync(resolve(destination, 'RaisingPlayer.exe'), ['--hidden', '--report', reportFile,
      '--bundle', destination, '--save', resolve(root, 'save.json'), '--frames', '3'], { encoding: 'utf8', timeout: 10000, windowsHide: true });
    assert.equal(child.status, 1, `${name}: ${child.error?.message ?? child.stderr}`);
    const report = JSON.parse(await readFile(reportFile, 'utf8'));
    assert.equal(report.ok, false, name); assert.equal(typeof report.error, 'string');
    assert.ok(report.error.length > 0, name);
  }
  await writeFile(gameFile, JSON.stringify(original));
  const reportFile = resolve(root, 'invalid-cli.json');
  const child = spawnSync(resolve(destination, 'RaisingPlayer.exe'), ['--hidden', '--report', reportFile, '--frames', 'invalid'],
    { encoding: 'utf8', timeout: 10000, windowsHide: true });
  assert.equal(child.status, 1); assert.equal(JSON.parse(await readFile(reportFile, 'utf8')).ok, false);
});

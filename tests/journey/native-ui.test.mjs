import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdir, mkdtemp, readFile, writeFile, cp } from 'node:fs/promises';
import { resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { exportGame } from '../../dist/packages/export-service/export.js';
import { PlayerController } from '../../dist/packages/player-service/controller.js';

test('exported SDL3/RmlUi player uses real controls for schedule, shopping, gifts and native save replay', { timeout: 120000 }, async () => {
  const output = resolve('.cache/journey-native-ui'); await mkdir(output, { recursive: true });
  const root = await mkdtemp(`${output}/run-`), destination = resolve(root, 'windows'), save = resolve(root, 'save.json');
  const projectRoot = resolve(root, 'project');
  await cp(resolve(process.env.JOURNEY_PROJECT_ROOT ?? 'examples/academy-journey'), projectRoot, { recursive: true });
  const built = await buildSnapshot(await loadProject(projectRoot));
  assert.equal(built.ok, true, JSON.stringify(built.diagnostics));
  await exportGame({ content: built.snapshot, projectRoot, destination, target: 'windows', runtimes: { web: resolve('dist/web'), windows: resolve('dist/native-player-app') } });
  async function run(name, steps, savePath = save) {
    const input = resolve(root, `${name}.steps.json`), reportFile = resolve(root, `${name}.report.json`);
    await writeFile(input, JSON.stringify(steps));
    const result = spawnSync(resolve(destination, 'RaisingPlayer.exe'), ['--bundle', destination, '--save', savePath, '--hidden', '--automation', input, '--report', reportFile], { timeout: 60000, encoding: 'utf8', windowsHide: true });
    assert.equal(result.status, 0, result.stderr || result.stdout);
    const report = JSON.parse(await readFile(reportFile, 'utf8'));
    assert.equal(report.ok, true); assert.equal(report.final.runtime, 'cpp-sdl3-rmlui');
    return report;
  }
  const check = (screen, tag) => ({ expectScreen: screen, checkpoint: tag });
  const report = await run('journey', [
    { click: 'start' }, { waitFrames: 120 }, { screenshot: resolve(output, 'verified-home.png') },
    { click: 'schedule-link' }, { select: 'plan-0', value: 'rest' },
    { click: 'advance' }, { click: 'advance' }, { click: 'advance' },
    { click: 'button-new' }, check('city', 'empty'),
    { click: 'button-2' }, { click: 'buy-florist-flower' }, check('market', 'purchased'),
    { waitFrames: 120 }, { screenshot: resolve(output, 'verified-shop.png') },
    { click: 'button-new' }, { click: 'save' }, check('city', 'before-gift'),
    { click: 'interact-gift-yuna' }, check('story', 'gift'),
    { waitFrames: 120 }, { screenshot: resolve(output, 'verified-relationship.png') },
    { click: 'load' }, check('city', 'restored'),
    { click: 'interact-gift-yuna' }, { click: 'advance' }, check('city', 'finished'),
    { click: 'save' }, check('city', 'saved'),
  ]);
  const at = tag => report.steps.find(step => step.command.checkpoint === tag).ui;
  assert.equal(at('empty').elements.find(e => e.id === 'interact-gift-yuna').enabled, false);
  assert.equal(at('purchased').state.gameplay.inventory.flower, 1);
  assert.equal(at('purchased').state.attributes.money, 20);
  assert.equal(at('purchased').elements.find(e => e.id === 'buy-florist-flower').enabled, false);
  assert.deepEqual(at('restored').state, at('before-gift').state);
  assert.equal(at('gift').state.phase, 'narrative');
  assert.equal(at('finished').state.relationships.mentor.trust, 15);
  assert.equal(at('finished').state.gameplay.inventory.flower, 0);
  assert.equal(at('finished').state.periodIndex, 1);
  assert.equal(at('finished').elements.find(e => e.id === 'interact-gift-yuna').enabled, false);
  const controller = new PlayerController(); await controller.openProject(projectRoot); controller.newRun(null);
  const loaded = await controller.load(save, { generation: controller.generation, revision: controller.session.state.stateRevision });
  assert.equal(loaded.error, null); assert.deepEqual(loaded.view.state, at('saved').state);
  const envelope = JSON.parse(await readFile(save, 'utf8')); envelope.state.attributes.money += 1;
  const badSave = resolve(root, 'bad-save.json'); await writeFile(badSave, JSON.stringify(envelope));
  const rejected = await run('tampered', [{ click: 'start' }, check('home', 'before'), { click: 'load' }, check('home', 'after')], badSave);
  assert.deepEqual(rejected.steps[1].ui.state, rejected.final.state);
  assert.match(rejected.final.message, /replay|저장|일치|다릅니다|state/i);
});

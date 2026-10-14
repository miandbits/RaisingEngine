import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdir, mkdtemp, cp, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { exportGame } from '../../dist/packages/export-service/export.js';

test('GUI-authored adventure exports and plays equipment, quest, combat, loot, save replay and relationship event in native SDL', { timeout: 120000 }, async () => {
  const output = resolve('.cache/journey-adventure-native'); await mkdir(output, { recursive: true });
  const root = await mkdtemp(`${output}/run-`), projectRoot = resolve(root, 'project');
  await cp(resolve('examples/academy-adventure'), projectRoot, { recursive: true });
  const built = await buildSnapshot(await loadProject(projectRoot));
  assert.equal(built.ok, true, JSON.stringify(built.diagnostics));
  const runtimes = { windows: resolve(process.env.NATIVE_PLAYER_OUT || 'dist/native-player-mobile-check-app'), web: resolve('dist/web') };
  const windows = resolve(root, 'windows'), web = resolve(root, 'web');
  await exportGame({ content: built.snapshot, projectRoot, destination: windows, target: 'windows', runtimes });
  await exportGame({ content: built.snapshot, projectRoot, destination: web, target: 'web', runtimes });
  const steps = [
    { clickWindow: 'start' }, { clickWindow: 'adventure-link' }, { clickWindow: 'accept-quest-first-expedition' }, { checkpoint: 'accepted' },
    { clickWindow: 'schedule' }, { clickWindow: 'button-new' }, { clickWindow: 'button-2' },
    { clickWindow: 'buy-florist-training-sword' }, { checkpoint: 'purchased' }, { clickWindow: 'visit-home' },
    { clickWindow: 'adventure-link' }, { clickWindow: 'equip-wooden-sword' }, { checkpoint: 'equipped' },
    { screenshot: resolve(output, 'equipped.png') }, { clickWindow: 'schedule' },
    { select: 'plan-0', value: 'forest-trip' }, { select: 'plan-1', value: 'rest' }, { select: 'plan-2', value: 'rest' },
    { clickWindow: 'advance' }, { clickWindow: 'advance' }, { checkpoint: 'expedition' },
    { clickWindow: 'move-expedition-clearing' }, { checkpoint: 'battle' }, { screenshot: resolve(output, 'battle.png') },
    { clickWindow: 'save' }, { checkpoint: 'saved' }, { clickWindow: 'battle-attack' }, { clickWindow: 'battle-attack' }, { checkpoint: 'victory' },
    { clickWindow: 'load' }, { checkpoint: 'restored' }, { clickWindow: 'battle-attack' }, { clickWindow: 'battle-attack' }, { checkpoint: 'victory-replayed' },
    { screenshot: resolve(output, 'victory.png') }, { clickWindow: 'move-expedition-entrance' }, { clickWindow: 'return-expedition' },
    { clickWindow: 'advance' }, { clickWindow: 'button-new' }, { clickWindow: 'visit-home' }, { clickWindow: 'adventure-link' },
    { checkpoint: 'completed-quest' }, { clickWindow: 'claim-quest-first-expedition' }, { checkpoint: 'relationship-event' },
    { waitFrames: 40 }, { screenshot: resolve(output, 'relationship-event.png') }, { clickWindow: 'advance' },
    { checkpoint: 'finished' }, { screenshot: resolve(output, 'claimed.png') },
  ];
  const automation = resolve(root, 'steps.json'), reportFile = resolve(root, 'report.json');
  await writeFile(automation, JSON.stringify(steps));
  const result = spawnSync(resolve(windows, 'RaisingPlayer.exe'), ['--bundle', windows, '--save', resolve(root, 'save.json'), '--hidden', '--automation', automation, '--report', reportFile], { timeout: 60000, encoding: 'utf8', windowsHide: true });
  assert.equal(result.status, 0, result.stderr || result.stdout);
  const report = JSON.parse(await readFile(reportFile, 'utf8'));
  const at = name => report.steps.find(step => step.command.checkpoint === name).ui;
  assert.equal(at('accepted').state.systems.quests['first-expedition'].status, 'accepted');
  assert.equal(at('purchased').state.gameplay.inventory['training-sword'], 1);
  assert.equal(at('equipped').state.systems.equipment.weapon, 'wooden-sword');
  assert.equal(at('expedition').screenId, 'expedition'); assert.equal(at('battle').screenId, 'battle');
  assert.deepEqual(at('restored').state, at('saved').state);
  assert.deepEqual(at('victory-replayed').state, at('victory').state);
  assert.equal(at('victory').state.systems.lastBattle.outcome, 'victory');
  assert.equal(at('victory').state.gameplay.inventory.flower, 1);
  assert.equal(at('completed-quest').state.systems.quests['first-expedition'].status, 'completed');
  assert.equal(at('relationship-event').state.phase, 'narrative');
  assert.match(JSON.stringify(at('relationship-event').state.activeNarrative), /mentor-recognition/);
  assert.equal(report.final.state.systems.quests['first-expedition'].status, 'claimed');
  assert.equal(report.final.state.relationships.mentor.trust, 15);
  assert.ok(report.final.state.systems.unlockedStages.includes('mentor-friend'));
  await writeFile(resolve(output, 'evidence.json'), JSON.stringify({ checkedAt: new Date().toISOString(), source: 'examples/academy-adventure', projectRoot, windows, web, reportFile, contentFingerprint: built.snapshot.contentFingerprint, runtime: report.final.runtime, checks: ['purchase', 'equip', 'quest', 'expedition', 'combat', 'loot', 'save-replay', 'claim', 'relationship-event'], finalState: report.final.state }, null, 2));
});

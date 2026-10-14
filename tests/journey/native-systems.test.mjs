import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdir, writeFile, readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import { fixture } from '../systems/fixture.mjs';
import { exportGame } from '../../dist/packages/export-service/export.js';

const element = (id, kind, x, y, width, height, extra = {}) => ({ id, kind, x, y, width, height, ...extra });
for (const mode of ['fallback', 'authored', 'incomplete']) test(`native ${mode} systems UI plays equipment, quest, exploration, combat and relationship rewards`, { timeout: 90000 }, async () => {
  const sample = await fixture(project => {
    project.visual.screens = project.visual.screens.filter(screen => !['battle', 'expedition'].includes(screen.id));
    const home = project.visual.screens.find(screen => screen.id === 'home');
    home.elements = [
      element('panel', 'panel', 20, 20, 1240, 680, { fill: '#f5f8ef', radius: 12 }),
      element('equipment', 'equipment', 40, 60, 350, 440),
      element('quests', 'quests', 440, 60, 380, 440),
      element('stages', 'relationship-stages', 870, 60, 330, 440),
      element('quick-equip', 'button', 40, 610, 300, 60, { text: '철 검 바로 장착', action: 'equip', equipmentId: 'iron-sword', fill: '#305b4c' }),
      element('home-schedule', 'button', 880, 610, 300, 60, { text: '일정', action: 'navigate', screenId: 'schedule', fill: '#305b4c' }),
    ];
    project.visual.screens.find(screen => screen.id === 'schedule').elements.push(element('home-link', 'button', 980, 160, 240, 60, { text: '집으로', action: 'navigate', screenId: 'home', fill: '#305b4c' }));
    project.gameplay.items.find(item => item.id === 'gem').initialQuantity = 2;
    if (mode !== 'fallback') for (const phase of ['battle', 'expedition']) project.visual.screens.push({ id: phase, label: phase, elements: mode === 'incomplete' ? [] : [
      element('panel', 'panel', 20, 20, 1240, 680, { fill: '#f5f8ef' }),
      element(`actual-${phase}`, phase, 40, 80, 1100, 560),
      element('save', 'button', 820, 20, 180, 50, { text: '저장', action: 'save', fill: '#305b4c' }),
      element('load', 'button', 1020, 20, 180, 50, { text: '불러오기', action: 'load', fill: '#305b4c' }),
    ] });
  });
  assert.equal(sample.built.ok, true, JSON.stringify(sample.built));
  const destination = resolve(sample.root, 'native-game'), output = resolve('.cache/journey-native-systems'); await mkdir(output, { recursive: true });
  await exportGame({ content: sample.built.snapshot, projectRoot: sample.root, destination, target: 'windows', runtimes: { windows: resolve(process.env.NATIVE_PLAYER_OUT || 'dist/native-player-mobile-check-app'), web: resolve('dist/web') } });
  const steps = [
    { clickWindow: 'start' }, { clickWindow: 'equip-iron-sword' }, { checkpoint: 'equipped' },
    { clickWindow: 'unequip-weapon' }, { checkpoint: 'unequipped' }, { clickWindow: 'quick-equip' },
    { clickWindow: 'accept-quest-forest-help' }, { checkpoint: 'accepted' },
    { clickWindow: 'home-schedule' }, { select: 'plan-0', value: 'forest-trip' }, { select: 'plan-1', value: 'rest' }, { select: 'plan-2', value: 'rest' },
    { clickWindow: 'advance' }, { clickWindow: 'advance' }, { checkpoint: 'expedition' },
    { clickWindow: 'move-expedition-clearing' }, { checkpoint: 'battle' }, { clickWindow: 'battle-guard' }, { clickWindow: 'battle-item-potion' },
    { clickWindow: 'battle-attack' }, { clickWindow: 'save' }, { checkpoint: 'battle-save' },
    { clickWindow: 'battle-attack' }, { checkpoint: 'victory-first' }, { clickWindow: 'load' }, { checkpoint: 'battle-restored' },
    { clickWindow: 'battle-attack' }, { checkpoint: 'victory-replayed' },
    { screenshot: resolve(output, `${mode}-victory.png`) },
    { clickWindow: 'move-expedition-exit' }, { clickWindow: 'return-expedition' }, { clickWindow: 'advance' },
    { clickWindow: 'home-link' }, { checkpoint: 'quest-ready' }, { clickWindow: 'claim-quest-forest-help' }, { checkpoint: 'relationship-event' },
    { clickWindow: 'advance' }, { checkpoint: 'finished' }, { screenshot: resolve(output, `${mode}-home.png`) },
  ];
  const automation = resolve(sample.root, 'steps.json'), reportFile = resolve(output, `${mode}.report.json`);
  await writeFile(automation, JSON.stringify(steps));
  const run = spawnSync(resolve(destination, 'RaisingPlayer.exe'), ['--bundle', destination, '--save', resolve(sample.root, 'battle-save.json'), '--hidden', '--automation', automation, '--report', reportFile], { timeout: 45000, windowsHide: true, encoding: 'utf8' });
  assert.equal(run.status, 0, run.stderr || run.stdout);
  const report = JSON.parse(await readFile(reportFile, 'utf8')); const at = name => report.steps.find(step => step.command.checkpoint === name).ui;
  assert.equal(at('equipped').state.systems.equipment.weapon, 'iron-sword'); assert.equal(at('unequipped').state.systems.equipment.weapon, null);
  assert.equal(at('accepted').state.systems.quests['forest-help'].status, 'accepted');
  assert.equal(at('expedition').screenId, 'expedition'); assert.equal(at('battle').screenId, 'battle');
  assert.ok(at('battle').texts[mode === 'authored' ? 'actual-battle' : 'system-widget']);
  assert.equal(at('battle-save').state.gameplay.inventory.potion, 1);
  assert.deepEqual(at('battle-restored').state, at('battle-save').state);
  assert.deepEqual(at('victory-replayed').state, at('victory-first').state, 'save replay keeps combat RNG/rewards exact');
  assert.equal(at('victory-replayed').state.systems.lastBattle.outcome, 'victory');
  const victoryText = Object.values(at('victory-replayed').texts).join(' ');
  assert.match(victoryText, /가방 공간 부족/); assert.match(victoryText, /보석/);
  assert.equal(at('quest-ready').state.systems.quests['forest-help'].status, 'completed');
  assert.equal(at('relationship-event').state.phase, 'narrative');
  assert.equal(report.final.state.systems.quests['forest-help'].status, 'claimed');
  assert.equal(report.final.state.relationships.mentor.trust, 15);
  assert.ok(report.final.state.systems.unlockedStages.includes('friendship'));
  assert.match(report.final.texts.stages, /도달/);
});

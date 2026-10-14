import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile, writeFile } from 'node:fs/promises';
import { join } from 'node:path';
import { fixture } from './fixture.mjs';
import { createRun, dispatchCommand } from '../../dist/packages/core/run.js';
import { getSystemsView, heroStats } from '../../dist/packages/systems/service.js';
import { nativeCreateRun, nativeDispatchCommand, nativeRequest, closeNativeRuntimes } from '../../dist/packages/native-bridge/index.js';
import { WasmRuntime } from '../../dist/packages/native-bridge/wasm.js';
import { driver, start, win, settle } from './driver.mjs';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { runAnalysis, replayTrace } from '../../dist/packages/analysis-service/runner.js';

test('C++ and reference agree through quest, equipment, battle, loot, period and relationship event with every-step save replay', async () => {
  const f = await fixture(); assert.ok(f.built.ok, JSON.stringify(f.loaded.diagnostics)); const c = f.built.snapshot, d = await driver(c);
  await start(d); assert.equal(d.state.phase, 'expedition'); assert.equal(d.state.attributes.money, 25); assert.equal(d.state.periodIndex, 0);
  assert.equal(d.state.attributes.skill, 5); assert.equal(heroStats(c, d.state).attack, 13);
  await d.send({ type: 'moveExpedition', nodeId: 'exit' }, 'NODE_DISCONNECTED');
  await d.send({ type: 'moveExpedition', nodeId: 'clearing' });
  await d.send({ type: 'battleAction', action: 'useItem', itemId: 'potion' }, 'HP_FULL');
  await d.send({ type: 'returnFromExpedition' }, 'WRONG_PHASE');
  await d.send({ type: 'battleAction', action: 'guard' }); assert.ok(d.state.systems.expedition.heroHp < 60);
  await d.send({ type: 'battleAction', action: 'useItem', itemId: 'potion' }); assert.equal(d.state.gameplay.inventory.potion, 1);
  await win(d); assert.equal(d.state.gameplay.inventory.gem, 1); assert.equal(d.state.systems.quests['forest-help'].status, 'completed');
  await d.send({ type: 'moveExpedition', nodeId: 'gate' }); await d.send({ type: 'moveExpedition', nodeId: 'clearing' });
  assert.equal(d.state.phase, 'expedition'); assert.equal(d.state.gameplay.inventory.gem, 1, 'cleared encounter cannot farm rewards');
  await d.send({ type: 'moveExpedition', nodeId: 'exit' }); await d.send({ type: 'returnFromExpedition' }); assert.equal(d.state.periodIndex, 0);
  await settle(d, c); assert.equal(d.state.periodIndex, 1); await d.send({ type: 'returnFromExpedition' }, 'NO_EXPEDITION');
  await d.send({ type: 'claimQuest', questId: 'forest-help' }); assert.equal(d.state.phase, 'narrative'); assert.equal(d.state.activeNarrative.id, 'friendship-unlock');
  await settle(d, c); assert.equal(d.state.periodIndex, 1); assert.deepEqual(d.state.systems.unlockedStages, ['friendship']);
  await d.send({ type: 'claimQuest', questId: 'forest-help' }, 'QUEST_NOT_COMPLETED');
  await d.send({ type: 'interact', interactionId: 'gift-sword' }, 'INSUFFICIENT_ITEMS');
  await d.send({ type: 'unequip', slot: 'weapon' }); assert.equal(heroStats(c, d.state).attack, d.state.attributes.skill);
  await d.send({ type: 'interact', interactionId: 'gift-sword' }); assert.equal(d.state.gameplay.inventory.sword, 0);
  closeNativeRuntimes();
});
test('flee and defeat each spend the scheduled period once and never grant victory objectives or loot', async () => {
  for (const defeat of [false, true]) {
    const f = await fixture(p => { if (defeat) p.systems.enemies[0].attack = 1000; }); assert.ok(f.built.ok);
    const c = f.built.snapshot, d = await driver(c); await start(d); await d.send({ type: 'moveExpedition', nodeId: 'clearing' });
    await d.send({ type: 'battleAction', action: defeat ? 'guard' : 'flee' });
    assert.equal(d.state.phase, 'activity-result'); assert.equal(d.state.systems.lastExpedition.outcome, defeat ? 'defeated' : 'retreated');
    assert.equal(d.state.gameplay.inventory.gem, 0); assert.equal(d.state.systems.quests['forest-help'].progress, 0);
    await settle(d, c); assert.equal(d.state.periodIndex, 1); await d.send({ type: 'returnFromExpedition' }, 'NO_EXPEDITION');
  }
  closeNativeRuntimes();
});
test('loot overflow is reported without softlock; claim overflow and version conflicts are atomic; pre-accept wins do not count', async () => {
  const f = await fixture(p => { p.gameplay.items.find(i => i.id === 'gem').initialQuantity = 2; p.gameplay.items.find(i => i.id === 'flower').initialQuantity = 2; }); assert.ok(f.built.ok);
  const c = f.built.snapshot, d = await driver(c); await start(d, false); await d.send({ type: 'moveExpedition', nodeId: 'clearing' }); await win(d);
  assert.deepEqual(d.state.systems.lastBattle.discardedItems, [{ itemId: 'gem', quantity: 1 }]);
  await d.send({ type: 'returnFromExpedition' }); await settle(d, c); await d.send({ type: 'acceptQuest', questId: 'forest-help' }); assert.equal(d.state.systems.quests['forest-help'].progress, 0);
  await d.send({ type: 'setSchedule', activityIds: ['forest-trip', 'forest-trip', 'rest'] }); await d.send({ type: 'executeActivity' }); await d.send({ type: 'moveExpedition', nodeId: 'clearing' }); await win(d);
  await d.send({ type: 'returnFromExpedition' }); await settle(d, c);
  await d.send({ type: 'claimQuest', questId: 'forest-help' }, 'INVENTORY_FULL'); assert.equal(d.state.systems.quests['forest-help'].status, 'completed');
  const before = d.state; const conflict = nativeDispatchCommand(c, before, { type: 'claimQuest', questId: 'forest-help' }, before.stateRevision - 1); assert.equal(conflict.error.code, 'REVISION_CONFLICT'); assert.deepEqual(conflict.state, before);
  closeNativeRuntimes();
});
test('systems validation locates missing references, duplicate equipment, invalid exit graphs, and stage trigger errors', async () => {
  const f = await fixture(p => {
    p.systems.hero.attackAttributeId = 'missing-stat'; p.systems.equipment.push({ ...p.systems.equipment[0], id: 'duplicate-item' });
    p.systems.quests[0].enemyId = 'missing-enemy'; p.systems.expeditions[0].nodes[2].exit = false;
    p.systems.relationshipStages[0].threshold = 9999;
  });
  const codes = new Set(f.built.diagnostics.map(d => d.code)); assert.ok(codes.has('REFERENCE_MISSING')); assert.ok(codes.has('EQUIPMENT_ITEM_DUPLICATE')); assert.ok(codes.has('EXPEDITION_NO_EXIT')); assert.ok(codes.has('STAGE_RANGE'));
  assert.ok(f.built.diagnostics.some(d => d.pointer === '/systems/hero/attackAttributeId'));
  assert.equal(f.built.ok, false);
});

test('relationship events run before final-period ending and before a period-event window expires', async () => {
  for (const source of ['battle-final', 'narrative-final', 'narrative-first']) {
    const finalPeriod = source === 'narrative-first' ? 0 : 5;
    const gain = { op: 'add', target: { kind: 'relationship', characterId: 'mentor', relationshipId: 'trust' }, value: { op: 'literal', value: 15 } };
    const f = await fixture(p => { if (source === 'battle-final') p.systems.enemies[0].rewards.effects = [gain]; }, async root => {
      const path = join(root, 'events/friendship-unlock.json'); const stage = JSON.parse(await readFile(path, 'utf8')); stage.window = { fromPeriod: finalPeriod, throughPeriod: finalPeriod }; await writeFile(path, JSON.stringify(stage));
      if (source !== 'battle-final') await writeFile(join(root, 'events/ordinary-stage.json'), JSON.stringify({ formatVersion: '0.1.0', id: 'ordinary-stage', label: '관계가 변한 하루', priority: 100, window: { fromPeriod: finalPeriod, throughPeriod: finalPeriod }, requirements: { op: 'always' }, entryNodeId: 'gain', nodes: [{ id: 'gain', type: 'effect', effects: [gain], next: 'end' }, { id: 'end', type: 'end' }] }));
    });
    assert.ok(f.built.ok, JSON.stringify(f.built)); const c = f.built.snapshot, d = await driver(c);
    if (source === 'battle-final') await d.send({ type: 'equip', equipmentId: 'iron-sword' });
    for (let period = 0; period <= finalPeriod; period++) {
      const ids = ['rest', 'rest', 'rest']; if (source === 'battle-final' && period === finalPeriod) ids[period % 3] = 'forest-trip';
      await d.send({ type: 'setSchedule', activityIds: ids }); await d.send({ type: 'executeActivity' });
      if (source === 'battle-final' && period === finalPeriod) { await d.send({ type: 'moveExpedition', nodeId: 'clearing' }); await win(d); await d.send({ type: 'returnFromExpedition' }); }
      await settle(d, c);
    }
    assert.ok(d.state.completedEvents.includes('friendship-unlock'), source);
    assert.deepEqual(d.state.systems.pendingStageEvents, []);
    assert.equal(d.state.periodIndex, source === 'narrative-first' ? 1 : 5);
    if (source !== 'narrative-first') assert.equal(d.state.phase, 'finished');
  }
  closeNativeRuntimes();
});

test('analysis traverses real expedition/combat actions and finds a replayable relationship-stage event', async () => {
  const f = await fixture(); assert.ok(f.built.ok); const c = f.built.snapshot;
  const options = { runs: 1, seed: 17, strategy: 'explore', maxCommands: 300, timeoutMs: 30000, target: { kind: 'event', id: 'friendship-unlock' }, maxStates: 5000, beamWidth: 16 };
  const result = await runAnalysis(c, options);
  assert.equal(result.targetStatus, 'found', JSON.stringify({ status: result.targetStatus, reason: result.stopReason, explored: result.exploredStates }));
  const trace = result.traces[0];
  for (const type of ['acceptQuest', 'executeActivity', 'moveExpedition', 'battleAction', 'claimQuest']) assert.ok(trace.commands.some(c => c.type === type), type);
  assert.deepEqual(replayTrace(c, trace).state, trace.state);
  const random = await runAnalysis(c, { runs: 8, seed: 31, strategy: 'random', maxCommands: 600, timeoutMs: 30000 });
  assert.equal(random.completed, 8); assert.equal(random.outcomes.finished, 8); assert.equal(random.outcomes.blocked ?? 0, 0);
  closeNativeRuntimes();
});

test('equipment view exposes authoritative unequip availability separately from equip availability', async () => {
  const f = await fixture(); assert.ok(f.built.ok); const content = f.built.snapshot, d = await driver(content);
  const equipment = () => getSystemsView(content, d.state).equipment.find(e => e.id === 'iron-sword');
  assert.equal(equipment().enabled, true); assert.equal(equipment().canUnequip, false);
  await d.send({ type: 'equip', equipmentId: 'iron-sword' });
  assert.equal(equipment().equipped, true); assert.equal(equipment().enabled, false);
  assert.equal(equipment().canUnequip, true, 'already equipped must not disable the separate unequip action');
  await d.send({ type: 'setSchedule', activityIds: ['forest-trip', 'rest', 'rest'] });
  await d.send({ type: 'executeActivity' }); assert.equal(equipment().canUnequip, false);
  await d.send({ type: 'moveExpedition', nodeId: 'clearing' }); assert.equal(equipment().canUnequip, false);
  await d.send({ type: 'unequip', slot: 'weapon' }, 'WRONG_PHASE');
  await d.send({ type: 'battleAction', action: 'flee' }); assert.equal(equipment().canUnequip, false);
  await settle(d, content); assert.equal(equipment().canUnequip, true);
  await d.send({ type: 'unequip', slot: 'weapon' }); assert.equal(equipment().equipped, false); assert.equal(equipment().canUnequip, false);
  assert.equal(equipment().enabled, true);
  closeNativeRuntimes();
});

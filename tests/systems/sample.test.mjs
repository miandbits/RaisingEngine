import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { nativeRequest, closeNativeRuntimes } from '../../dist/packages/native-bridge/index.js';
import { driver } from './driver.mjs';

function route(nodes, from, to) {
  const pending = [[from]], seen = new Set();
  while (pending.length) {
    const path = pending.shift(), id = path.at(-1);
    if (id === to) return path.slice(1);
    if (seen.has(id)) continue;
    seen.add(id);
    for (const next of nodes.find(n => n.id === id)?.connections ?? []) pending.push([...path, next]);
  }
  assert.fail(`No authored route from ${from} to ${to}`);
}

test('GUI-authored academy-adventure completes quest/equipment/expedition/combat/relationship route with exact native saves', { timeout: 30000 }, async () => {
  // This acceptance reads the authored project and builds an immutable snapshot.
  // No source document, asset, project save, or analysis job is modified.
  const root = resolve('examples/academy-adventure');
  const loaded = await loadProject(root), built = await buildSnapshot(loaded);
  assert.ok(built.ok, JSON.stringify(built));
  const content = built.snapshot, rules = content.project.systems, gameplay = content.project.gameplay;
  assert.ok(rules && gameplay, 'Representative project must author both system and location data');
  const quest = rules.quests[0], equipment = rules.equipment[0];
  assert.ok(quest && equipment);
  const expedition = rules.expeditions.find(e => e.nodes.some(n => n.enemyId === quest.enemyId));
  assert.ok(expedition, 'Quest enemy must exist in a scheduled expedition');
  const activity = content.activities.find(a => a.expeditionId === expedition.id);
  assert.ok(activity);
  const stage = rules.relationshipStages.find(s => s.eventId && quest.rewards.effects.some(e => e.op === 'add' && e.target.kind === 'relationship' && e.target.characterId === s.characterId && e.target.relationshipId === s.relationshipId));
  assert.ok(stage, 'Quest reward must connect to an authored relationship event');
  const d = await driver(content), commands = [];
  const send = async command => { await d.send(command); commands.push(command); };
  const visit = async id => { for (const locationId of route(gameplay.locations, d.state.gameplay.locationId, id)) await send({ type: 'visit', locationId }); };
  const settle = async () => {
    for (let i = 0; i < 200; i++) {
      if (d.state.phase === 'activity-result') await send({ type: 'acknowledgeResult' });
      else if (d.state.phase === 'period-end') await send({ type: 'resolvePeriodEnd' });
      else if (d.state.phase === 'narrative') {
        const view = nativeRequest({ operation: 'view', content, state: d.state }).narrative;
        if (view.node.type === 'choice') { const choice = view.choices.find(c => c.enabled); assert.ok(choice); await send({ type: 'choose', choiceId: choice.id }); }
        else await send({ type: 'advanceNarrative' });
      } else return;
    }
    assert.fail('Authored narrative did not reach an input boundary');
  };
  const purchase = async itemId => {
    const shop = gameplay.shops.find(s => s.offers.some(o => o.itemId === itemId));
    assert.ok(shop, `Missing authored shop for ${itemId}`);
    await visit(shop.locationId); await send({ type: 'buy', shopId: shop.id, itemId, quantity: 1 });
  };
  try {
    await visit(quest.locationId); await send({ type: 'acceptQuest', questId: quest.id }); await settle();
    if ((d.state.gameplay.inventory[equipment.itemId] ?? 0) === 0) await purchase(equipment.itemId);
    await send({ type: 'equip', equipmentId: equipment.id });
    assert.equal(d.state.systems.equipment[equipment.slot], equipment.id);
    const recovery = rules.consumables[0];
    if (recovery && !(d.state.gameplay.inventory[recovery.itemId] > 0) && gameplay.shops.some(s => s.offers.some(o => o.itemId === recovery.itemId))) await purchase(recovery.itemId);
    const initialPeriod = d.state.periodIndex;
    for (let trip = 0; trip < quest.targetCount; trip++) {
      await visit(expedition.locationId);
      const activityIds = [...(d.state.schedule ?? [activity.id, activity.id, activity.id])];
      activityIds[d.state.periodIndex % 3] = activity.id;
      await send({ type: 'setSchedule', activityIds }); await send({ type: 'executeActivity' });
      const fight = async () => {
        for (let turn = 0; d.state.phase === 'battle' && turn < 100; turn++) {
          const view = nativeRequest({ operation: 'view', content, state: d.state }).systems;
          const usable = view.consumables.find(i => i.enabled);
          if (usable && view.battle.heroHp < view.battle.heroMaxHp / 2) await send({ type: 'battleAction', action: 'useItem', itemId: usable.itemId });
          else await send({ type: 'battleAction', action: 'attack' });
        }
        assert.equal(d.state.phase, 'expedition', 'Authored route must survive its encounter');
      };
      await fight();
      const enemyNode = expedition.nodes.find(n => n.enemyId === quest.enemyId);
      for (const nodeId of route(expedition.nodes, d.state.systems.expedition.nodeId, enemyNode.id)) { await send({ type: 'moveExpedition', nodeId }); await fight(); }
      assert.equal(d.state.systems.lastBattle?.outcome, 'victory');
      const exit = expedition.nodes.find(n => n.exit); assert.ok(exit);
      for (const nodeId of route(expedition.nodes, d.state.systems.expedition.nodeId, exit.id)) { await send({ type: 'moveExpedition', nodeId }); await fight(); }
      await send({ type: 'returnFromExpedition' }); await settle();
      assert.equal(d.state.periodIndex, initialPeriod + trip + 1, 'Each expedition settles exactly one period');
    }
    assert.equal(d.state.systems.quests[quest.id].status, 'completed');
    await visit(quest.locationId); await send({ type: 'claimQuest', questId: quest.id }); await settle();
    assert.equal(d.state.systems.quests[quest.id].status, 'claimed');
    assert.ok(d.state.systems.unlockedStages.includes(stage.id));
    assert.ok(d.state.completedEvents.includes(stage.eventId));
    await mkdir('.cache/systems-sample', { recursive: true });
    await writeFile('.cache/systems-sample/acceptance.json', JSON.stringify({ project: root, contentFingerprint: content.contentFingerprint, seed: 129,
      questId: quest.id, equipmentId: equipment.id, expeditionId: expedition.id, relationshipEventId: stage.eventId, commands,
      finalState: d.state, validation: 'native/reference state, native view, and native journal save replay checked after every command' }, null, 2));
  } finally { closeNativeRuntimes(); }
});

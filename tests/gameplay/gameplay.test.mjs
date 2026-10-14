import test from 'node:test';
import assert from 'node:assert/strict';
import { fixture } from './fixture.mjs';
import { createSession, dispatchSession } from '../../dist/packages/runtime-service/session.js';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';
import { inspectEvents } from '../../dist/packages/core/narrative.js';
import { getGameplayView } from '../../dist/packages/gameplay/service.js';
import { PlayerController } from '../../dist/packages/player-service/controller.js';
import { cp, readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';

async function setup(t, mutate) {
  const f = await fixture(t, mutate);
  assert.equal(f.built.ok, true, JSON.stringify(f.built.diagnostics));
  const content = f.built.snapshot, started = createSession(content, 42);
  assert.equal(started.ok, true);
  let session = started.session;
  return { ...f, content, get session() { return session; }, set session(value) { session = value; },
    send(command) { const result = dispatchSession(content, session, command, session.state.stateRevision); assert.equal(result.ok, true, JSON.stringify(result.error)); session = result.session; return session.state; },
    fail(command, code, revision = session.state.stateRevision) {
      const before = JSON.stringify(session), result = dispatchSession(content, session, command, revision);
      assert.equal(result.ok, false); assert.equal(result.error.code, code); assert.equal(JSON.stringify(session), before);
    },
  };
}

test('authored home → schedule → city → shop → gift → relationship event and exact save replay', async t => {
  const g = await setup(t);
  assert.equal(g.session.state.gameplay.screenId, 'home');
  g.send({ type: 'navigate', screenId: 'schedule' });
  g.send({ type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] });
  g.send({ type: 'visit', locationId: 'city' });
  g.send({ type: 'visit', locationId: 'market' });
  const money = g.session.state.attributes.money, stamina = g.session.state.attributes.stamina;
  g.send({ type: 'buy', shopId: 'florist', itemId: 'flower', quantity: 2 });
  assert.equal(g.session.state.attributes.money, money - 20);
  assert.equal(g.session.state.gameplay.inventory.flower, 2);
  g.send({ type: 'visit', locationId: 'city' });
  g.send({ type: 'interact', interactionId: 'gift-mentor' });
  assert.equal(g.session.state.relationships.mentor.trust, 15);
  assert.equal(g.session.state.attributes.stamina, stamina - 2);
  assert.equal(g.session.state.gameplay.inventory.flower, 1);
  assert.equal(g.session.state.activeNarrative.returnPhase, 'ready');
  const encoded = await encodeSave(g.content, g.session); assert.equal(encoded.ok, true, JSON.stringify(encoded));
  const restored = await decodeSave(g.content, encoded.text); assert.equal(restored.ok, true, JSON.stringify(restored));
  assert.deepEqual(restored.session, g.session);
  for (const command of [{ type: 'advanceNarrative' }, { type: 'advanceNarrative' }]) {
    const other = dispatchSession(g.content, restored.session, command, restored.session.state.stateRevision);
    assert.equal(other.ok, true); restored.session = other.session; g.send(command);
  }
  assert.deepEqual(restored.session, g.session);
  assert.equal(g.session.state.periodIndex, 0); assert.equal(g.session.state.phase, 'ready');
  assert.ok(g.session.state.completedEvents.includes('gift-friendship'));
  g.fail({ type: 'interact', interactionId: 'gift-mentor' }, 'INTERACTION_COOLDOWN');
});

test('invalid location, navigation, commands, phase and revision cannot change state', async t => {
  const g = await setup(t);
  g.fail({ type: 'visit', locationId: 'market' }, 'LOCATION_DISCONNECTED');
  g.fail({ type: 'visit', locationId: 'missing' }, 'UNKNOWN_LOCATION');
  g.fail({ type: 'navigate', screenId: 'missing' }, 'UNKNOWN_SCREEN');
  g.fail({ type: 'navigate', screenId: 'ending' }, 'SCREEN_RESERVED');
  g.fail({ type: 'visit', locationId: 'city' }, 'REVISION_CONFLICT', 1);
  for (const quantity of [0, -1, 0.5, 2147483648, null]) g.fail({ type: 'buy', shopId: 'florist', itemId: 'flower', quantity }, 'INVALID_COMMAND');
  g.fail({ type: 'interact', interactionId: 'gift-mentor', extra: true }, 'INVALID_COMMAND');
  g.send({ type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] });
  g.send({ type: 'executeActivity' });
  g.fail({ type: 'visit', locationId: 'city' }, 'WRONG_PHASE');
});

test('purchase affordability, stock, item capacity and missing gifts are atomic', async t => {
  const g = await setup(t);
  g.fail({ type: 'buy', shopId: 'florist', itemId: 'flower', quantity: 1 }, 'WRONG_LOCATION');
  g.send({ type: 'visit', locationId: 'city' });
  g.fail({ type: 'interact', interactionId: 'gift-mentor' }, 'INSUFFICIENT_ITEMS');
  g.send({ type: 'visit', locationId: 'market' });
  g.fail({ type: 'buy', shopId: 'missing', itemId: 'flower', quantity: 1 }, 'UNKNOWN_SHOP');
  g.fail({ type: 'buy', shopId: 'florist', itemId: 'missing', quantity: 1 }, 'UNKNOWN_OFFER');
  g.fail({ type: 'buy', shopId: 'florist', itemId: 'flower', quantity: 5 }, 'INSUFFICIENT_STOCK');
  g.fail({ type: 'buy', shopId: 'florist', itemId: 'flower', quantity: 4 }, 'INVENTORY_FULL');
  g.send({ type: 'buy', shopId: 'florist', itemId: 'flower', quantity: 1 });
  g.fail({ type: 'buy', shopId: 'florist', itemId: 'book', quantity: 2 }, 'INSUFFICIENT_FUNDS');
  assert.equal(g.session.state.gameplay.purchased.florist.flower, 1);
  assert.equal(getGameplayView(g.content, g.session.state).shops[0].offers[0].stock, 3);
});

test('interaction event is not a period event and triggers only after threshold, once', async t => {
  const g = await setup(t, p => {
    p.gameplay.items[0].initialQuantity = 3;
    p.gameplay.interactions[0].oncePerPeriod = false;
    p.gameplay.interactions[0].effects[0].value.value = 8;
  });
  assert.equal(inspectEvents(g.content, g.session.state).find(x => x.eventId === 'gift-friendship').reason, 'interaction-only');
  g.send({ type: 'visit', locationId: 'city' });
  g.send({ type: 'interact', interactionId: 'gift-mentor' });
  assert.equal(g.session.state.phase, 'planning');
  g.send({ type: 'interact', interactionId: 'gift-mentor' });
  assert.equal(g.session.state.phase, 'narrative');
  g.send({ type: 'advanceNarrative' }); g.send({ type: 'advanceNarrative' });
  g.send({ type: 'interact', interactionId: 'gift-mentor' });
  assert.equal(g.session.state.phase, 'planning');
  assert.equal(g.session.state.completedEvents.filter(x => x === 'gift-friendship').length, 1);
  assert.equal(g.session.state.relationships.mentor.trust, 24);
});

test('cost/requirements failures do not consume item, money, cooldown or relationship', async t => {
  const g = await setup(t, p => { p.gameplay.items[0].initialQuantity = 1; p.gameplay.interactions[0].costs.push({ attributeId: 'money', amount: 1000 }); });
  g.send({ type: 'visit', locationId: 'city' });
  g.fail({ type: 'interact', interactionId: 'gift-mentor' }, 'INSUFFICIENT_FUNDS');
  assert.equal(g.session.state.gameplay.inventory.flower, 1);
  assert.equal(g.session.state.relationships.mentor.trust, 0);
  assert.deepEqual(g.session.state.gameplay.interactionPeriods, {});
  const view = getGameplayView(g.content, g.session.state);
  assert.equal(view.interactions[0].enabled, false); assert.match(view.interactions[0].reason, /부족/);
});

test('same seed and gameplay commands yield identical immutable state and reject tampered saves', async t => {
  const a = await setup(t);
  let b = createSession(a.content, 42).session;
  for (const command of [{ type: 'visit', locationId: 'city' }, { type: 'visit', locationId: 'market' }, { type: 'buy', shopId: 'florist', itemId: 'flower', quantity: 1 }]) {
    a.send(command); const result = dispatchSession(a.content, b, command, b.state.stateRevision); assert.equal(result.ok, true); b = result.session;
  }
  assert.deepEqual(a.session, b); assert.ok(Object.isFrozen(a.session.state.gameplay.inventory));
  const encoded = await encodeSave(a.content, a.session); assert.equal(encoded.ok, true);
  const tampered = JSON.parse(encoded.text); tampered.state.gameplay.inventory.flower = 99;
  assert.equal((await decodeSave(a.content, JSON.stringify(tampered))).ok, false);
});

test('semantic validator reports duplicate IDs, item/screen refs, inventory and cost errors at JSON pointers', async t => {
  const f = await fixture(t, p => {
    const g = p.gameplay;
    g.locations.push({ ...g.locations[0] }); g.locations[1].screenId = 'missing';
    g.items[0].initialQuantity = 100; g.shops[0].offers[0].itemId = 'missing';
    g.shops[0].currencyAttributeId = 'skill';
    g.interactions[0].costs.push({ attributeId: 'stamina', amount: 1 });
  });
  assert.equal(f.built.ok, false);
  for (const [code, pointer] of [['ID_DUPLICATE','/gameplay/locations/3/id'], ['SCREEN_REFERENCE','/gameplay/locations/1/screenId'],
    ['INVENTORY_RANGE','/gameplay/items/0/initialQuantity'], ['REFERENCE_MISSING','/gameplay/shops/0/offers/0/itemId'],
    ['COST_KIND','/gameplay/shops/0/currencyAttributeId'], ['COST_DUPLICATE','/gameplay/interactions/0/costs/1/attributeId']]) {
    const issue = f.built.diagnostics.find(x => x.code === code && x.pointer === pointer); assert.ok(issue, `${code} ${pointer}`); assert.ok(issue.line > 0);
  }
});

test('PlayerController exposes shared availability and executes gift dialogue without consuming a period', async t => {
  const f = await setup(t, p => { p.gameplay.items[0].initialQuantity = 1; });
  const controller = new PlayerController();
  assert.equal((await controller.openProject(f.root)).error, null);
  let reply = controller.newRun(null); assert.equal(reply.error, null);
  const act = action => {
    reply = controller.act(action, { generation: reply.view.generation, revision: reply.view.state.stateRevision });
    assert.equal(reply.error, null); return reply.view;
  };
  assert.ok(reply.view.gameplayDefinition);
  act({ type: 'visit', locationId: 'city' });
  assert.equal(reply.view.gameplay.interactions[0].enabled, true);
  act({ type: 'interact', interactionId: 'gift-mentor' });
  assert.equal(reply.view.narrative.node.type, 'dialogue');
  act({ type: 'continue' });
  assert.equal(reply.view.state.phase, 'planning'); assert.equal(reply.view.state.periodIndex, 0);
  assert.equal(reply.view.gameplay.interactions[0].enabled, false);
});

test('interaction scene restores prior location presentation even after a mid-dialogue save', async t => {
  const f = await fixture(t, p => {
    p.gameplay.items[0].initialQuantity = 1;
    p.gameplay.locations[1].presentation = { backgroundAssetId: 'scene-background', portraits: [] };
  }, async root => {
    await cp(path.resolve('assets/visual-sample/courtyard.png'), path.join(root, 'assets/scene.png'));
    await writeFile(path.join(root, 'assets/manifest.json'), JSON.stringify({ formatVersion: '0.1.0', assets: [{ id: 'scene-background', kind: 'image', path: 'assets/scene.png' }] }));
    const file = path.join(root, 'events/gift-friendship.json'), event = JSON.parse(await readFile(file, 'utf8'));
    event.nodes.unshift({ id: 'scene', type: 'scene', scene: { backgroundAssetId: null, portraits: [] }, next: event.entryNodeId }); event.entryNodeId = 'scene';
    await writeFile(file, JSON.stringify(event));
  });
  assert.equal(f.built.ok, true, JSON.stringify(f.built.diagnostics)); const content = f.built.snapshot;
  let session = createSession(content, 42).session;
  const send = command => { const next = dispatchSession(content, session, command, session.state.stateRevision); assert.equal(next.ok, true, JSON.stringify(next)); session = next.session; };
  send({ type: 'visit', locationId: 'city' }); send({ type: 'interact', interactionId: 'gift-mentor' });
  send({ type: 'advanceNarrative' }); assert.equal(session.state.presentation.backgroundAssetId, null);
  const saved = await encodeSave(content, session); assert.equal(saved.ok, true);
  const loaded = await decodeSave(content, saved.text); assert.equal(loaded.ok, true); session = loaded.session;
  send({ type: 'advanceNarrative' }); send({ type: 'advanceNarrative' });
  assert.equal(session.state.presentation.backgroundAssetId, 'scene-background');
  assert.equal(session.state.phase, 'planning'); assert.equal(session.state.periodIndex, 0);
});

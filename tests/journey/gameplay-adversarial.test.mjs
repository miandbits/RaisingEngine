import test from 'node:test';
import assert from 'node:assert/strict';
import { fixture } from '../gameplay/fixture.mjs';
import { createSession, dispatchSession } from '../../dist/packages/runtime-service/session.js';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';
import { getGameplayView } from '../../dist/packages/gameplay/service.js';

function start(built) {
  assert.equal(built.ok, true, JSON.stringify(built.diagnostics));
  const result = createSession(built.snapshot, 42);
  assert.equal(result.ok, true, JSON.stringify(result));
  return result.session;
}
function send(snapshot, session, command) {
  const result = dispatchSession(snapshot, session, command, session.state.stateRevision);
  assert.equal(result.ok, true, JSON.stringify(result));
  return result.session;
}
function market(snapshot, session) {
  session = send(snapshot, session, { type: 'visit', locationId: 'city' });
  return send(snapshot, session, { type: 'visit', locationId: 'market' });
}
function rejectsUnchanged(snapshot, session, command, code) {
  const before = JSON.stringify(session);
  const result = dispatchSession(snapshot, session, command, session.state.stateRevision);
  assert.equal(result.ok, false, `${JSON.stringify(command)} unexpectedly accepted`);
  if (code) assert.equal(result.error.code, code);
  assert.equal(result.session, session);
  assert.equal(JSON.stringify(session), before);
  return result;
}

test('legal IDs matching Object prototype names retain their own purchase state through save/replay', async t => {
  const { built } = await fixture(t, p => { p.gameplay.shops[0].id = 'constructor'; });
  let session = market(built.snapshot, start(built));
  const descriptor = Object.getOwnPropertyDescriptor(Object, 'flower');
  t.after(() => { if (descriptor) Object.defineProperty(Object, 'flower', descriptor); else delete Object.flower; });
  session = send(built.snapshot, session, { type: 'buy', shopId: 'constructor', itemId: 'flower', quantity: 1 });
  assert.equal(Object.hasOwn(session.state.gameplay.purchased, 'constructor'), true);
  assert.deepEqual(session.state.gameplay.purchased.constructor, { flower: 1 });
  assert.deepEqual(Object.getOwnPropertyDescriptor(Object, 'flower'), descriptor, 'must not mutate built-in prototypes/constructors');
  const saved = await encodeSave(built.snapshot, session);
  assert.equal(saved.ok, true, JSON.stringify(saved));
  const restored = await decodeSave(built.snapshot, saved.text);
  assert.equal(restored.ok, true, JSON.stringify(restored));
  assert.deepEqual(restored.session, session);
});

test('invalid quantities and remote purchase cannot consume currency, time, inventory or RNG', async t => {
  const { built } = await fixture(t);
  let session = start(built);
  rejectsUnchanged(built.snapshot, session, { type: 'buy', shopId: 'florist', itemId: 'flower', quantity: 1 }, 'WRONG_LOCATION');
  session = market(built.snapshot, session);
  for (const quantity of [0, -1, 1.5, Number.MAX_SAFE_INTEGER, '1', null]) {
    rejectsUnchanged(built.snapshot, session, { type: 'buy', shopId: 'florist', itemId: 'flower', quantity });
  }
});

test('effect overflow after gift item and stamina costs rolls back the entire interaction', async t => {
  const { built } = await fixture(t, p => {
    p.gameplay.items[0].initialQuantity = 1;
    p.gameplay.interactions[0].effects.push({ op: 'add', target: { kind: 'attribute', attributeId: 'money' }, value: { op: 'literal', value: 2147483647 } });
  });
  let session = start(built);
  session = send(built.snapshot, session, { type: 'visit', locationId: 'city' });
  rejectsUnchanged(built.snapshot, session, { type: 'interact', interactionId: 'gift-mentor' }, 'INTEGER_OVERFLOW');
});

test('sold-out and inventory-full offers expose disabled reasons matching executable rules', async t => {
  const { built } = await fixture(t, p => {
    p.gameplay.shops[0].offers[0].stock = 0;
    p.gameplay.items[1].initialQuantity = p.gameplay.items[1].maxStack;
  });
  const session = market(built.snapshot, start(built));
  const view = getGameplayView(built.snapshot, session.state);
  const flower = view.shops[0].offers.find(x => x.itemId === 'flower');
  const book = view.shops[0].offers.find(x => x.itemId === 'book');
  assert.equal(flower.enabled, false); assert.ok(flower.reason);
  assert.equal(book.enabled, false); assert.ok(book.reason);
  rejectsUnchanged(built.snapshot, session, { type: 'buy', shopId: 'florist', itemId: 'flower', quantity: 1 }, 'INSUFFICIENT_STOCK');
  rejectsUnchanged(built.snapshot, session, { type: 'buy', shopId: 'florist', itemId: 'book', quantity: 1 }, 'INVENTORY_FULL');
});

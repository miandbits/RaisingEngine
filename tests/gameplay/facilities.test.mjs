import test from 'node:test';
import assert from 'node:assert/strict';
import { fixture } from './fixture.mjs';
import { createSession, dispatchSession } from '../../dist/packages/runtime-service/session.js';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';
import { getGameplayView } from '../../dist/packages/gameplay/service.js';

const add = (id, value) => ({ op: 'add', target: { kind: 'attribute', attributeId: id }, value: { op: 'literal', value } });
const confirmation = { title: '거래 확인', message: '정말 진행할까요?', confirmLabel: '확인', cancelLabel: '취소' };
export function facilities(p) {
  p.gameplay.diets = [{ id: 'balanced', name: '균형식', locationId: 'home', intervalPeriods: 3, costs: [{ attributeId: 'money', amount: 5 }], effects: [add('stamina', 2)], confirmation }];
  p.gameplay.services = [{ id: 'treatment', name: '진료', locationId: 'market', maxQuantity: 3, costs: [{ attributeId: 'money', amount: 4 }], effects: [add('stamina', 3)], confirmation }];
  p.gameplay.shops[0].sellOffers = [{ itemId: 'flower', price: 6, maxQuantity: 3, confirmation }];
  p.gameplay.shops[0].offers[0].maxQuantity = 3;
  for (const [i, kind] of ['diets', 'services', 'sell'].entries()) p.visual.screens.find(x => x.id === 'home').elements.push({ id: `facility-${i}`, kind, x: i*300, y: 0, width: 280, height: 300 });
}
async function setup(t, mutate = () => {}) {
  const f = await fixture(t, p => { facilities(p); mutate(p); });
  assert.equal(f.built.ok, true, JSON.stringify(f.built.diagnostics));
  const content = f.built.snapshot; let session = createSession(content, 42).session;
  return { content, get session() { return session; },
    send(command) { const r = dispatchSession(content, session, command, session.state.stateRevision); assert.equal(r.ok, true, JSON.stringify(r.error)); session = r.session; return session.state; },
    fail(command, code, revision = session.state.stateRevision) { const before = session; const r = dispatchSession(content, session, command, revision); assert.equal(r.ok, false); assert.equal(r.error.code, code); assert.equal(r.session, before); },
    async restore() { const e = await encodeSave(content, session); assert.equal(e.ok, true, JSON.stringify(e)); const d = await decodeSave(content, e.text); assert.equal(d.ok, true, JSON.stringify(d)); assert.deepEqual(d.session, session); return d.session; },
  };
}
test('facility buy/sell/treatment quantities and configured confirmations survive exact save replay', async t => {
  const g = await setup(t); const initial = g.session.state.attributes;
  g.send({ type: 'visit', locationId: 'city' }); g.send({ type: 'visit', locationId: 'market' });
  let v = getGameplayView(g.content, g.session.state);
  assert.deepEqual(v.services[0].confirmation, confirmation); assert.equal(v.services[0].maxQuantity, 3);
  assert.equal(v.shops[0].sellOffers[0].enabled, false);
  g.send({ type: 'buy', shopId: 'florist', itemId: 'flower', quantity: 3 });
  g.send({ type: 'sell', shopId: 'florist', itemId: 'flower', quantity: 2 });
  const revision = g.session.state.stateRevision;
  g.send({ type: 'useService', serviceId: 'treatment', quantity: 2 });
  assert.equal(g.session.state.attributes.money, initial.money - 30 + 12 - 8);
  assert.equal(g.session.state.attributes.stamina, initial.stamina + 6);
  assert.equal(g.session.state.gameplay.inventory.flower, 1);
  g.fail({ type: 'useService', serviceId: 'treatment', quantity: 2 }, 'REVISION_CONFLICT', revision);
  await g.restore();
});
test('invalid transactions reject atomically including stock, quantities, affordability and healing saturation', async t => {
  const g = await setup(t);
  g.fail({ type: 'useService', serviceId: 'treatment', quantity: 1 }, 'WRONG_LOCATION');
  g.send({ type: 'visit', locationId: 'city' }); g.send({ type: 'visit', locationId: 'market' });
  for (const quantity of [0, -1, 1.5]) g.fail({ type: 'useService', serviceId: 'treatment', quantity }, 'INVALID_COMMAND');
  g.fail({ type: 'useService', serviceId: 'treatment', quantity: 4 }, 'QUANTITY_LIMIT');
  g.fail({ type: 'sell', shopId: 'florist', itemId: 'flower', quantity: 1 }, 'INSUFFICIENT_ITEMS');
  g.fail({ type: 'buy', shopId: 'florist', itemId: 'flower', quantity: 4 }, 'QUANTITY_LIMIT');
  const capped = await setup(t, p => { p.initialState.attributes.stamina = 100; });
  capped.send({ type: 'visit', locationId: 'city' }); capped.send({ type: 'visit', locationId: 'market' });
  capped.fail({ type: 'useService', serviceId: 'treatment', quantity: 1 }, 'SERVICE_NO_EFFECT');
});
test('persistent diet applies once per authored interval, switching cannot farm effects, saves replay exact', async t => {
  const g = await setup(t); const money = g.session.state.attributes.money;
  g.send({ type: 'setDiet', dietId: 'balanced' }); assert.equal(g.session.state.attributes.money, money);
  g.fail({ type: 'setDiet', dietId: 'balanced' }, 'DIET_UNCHANGED');
  g.send({ type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] });
  g.send({ type: 'executeActivity' });
  assert.equal(g.session.state.attributes.money, money - 5);
  assert.equal(g.session.state.gameplay.dietAppliedPeriod, 0); assert.equal(g.session.state.gameplay.dietNextPeriod, 3);
  assert.ok(g.session.state.lastActivityResult.changes.some(x => x.source === '/project/gameplay/diets/balanced/costs/0'));
  await g.restore();
  g.send({ type: 'acknowledgeResult' }); g.send({ type: 'resolvePeriodEnd' });
  g.send({ type: 'setDiet', dietId: null }); g.send({ type: 'setDiet', dietId: 'balanced' });
  g.send({ type: 'executeActivity' }); assert.equal(g.session.state.attributes.money, money - 5);
  await g.restore();
});
test('diet and activity charges rollback together, leaving time and RNG untouched', async t => {
  const g = await setup(t, p => { p.gameplay.diets[0].costs[0].amount = 10000; });
  g.send({ type: 'setDiet', dietId: 'balanced' }); g.send({ type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] });
  g.fail({ type: 'executeActivity' }, 'INSUFFICIENT_FUNDS');
  assert.equal(g.session.state.gameplay.dietAppliedPeriod, undefined);
  assert.equal(g.session.state.periodIndex, 0);
});
test('unaffordable services and full currency reject without consuming goods or applying effects', async t => {
  const costly = await setup(t, p => { p.gameplay.services[0].costs[0].amount = 10000; });
  costly.send({ type: 'visit', locationId: 'city' }); costly.send({ type: 'visit', locationId: 'market' });
  costly.fail({ type: 'useService', serviceId: 'treatment', quantity: 1 }, 'INSUFFICIENT_FUNDS');
  const full = await setup(t, p => { p.gameplay.items[0].initialQuantity = 1; p.gameplay.shops[0].sellOffers[0].price = 2147483647; });
  full.send({ type: 'visit', locationId: 'city' }); full.send({ type: 'visit', locationId: 'market' });
  // Integer overflow is rejected before the currency capacity check.
  const before = full.session;
  const result = dispatchSession(full.content, before, { type: 'sell', shopId: 'florist', itemId: 'flower', quantity: 1 }, before.state.stateRevision);
  assert.equal(result.ok, false); assert.equal(result.session, before);
  assert.equal(before.state.gameplay.inventory.flower, 1);
});
test('facility semantic validation points to duplicate policies, missing location/currency and sell item', async t => {
  const f = await fixture(t, p => { facilities(p); p.gameplay.diets.push(structuredClone(p.gameplay.diets[0])); p.gameplay.services[0].locationId = 'absent'; p.gameplay.services[0].costs.push({ attributeId: 'money', amount: 1 }); p.gameplay.shops[0].sellOffers[0].itemId = 'absent'; });
  assert.equal(f.built.ok, false);
  for (const [code, pointer] of [['ID_DUPLICATE','/gameplay/diets/1/id'],['REFERENCE_MISSING','/gameplay/services/0/locationId'],['COST_DUPLICATE','/gameplay/services/0/costs/1/attributeId'],['REFERENCE_MISSING','/gameplay/shops/0/sellOffers/0/itemId']]) assert.ok(f.built.diagnostics.some(d => d.code === code && d.pointer === pointer), `${code} ${pointer}`);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import { fixture } from '../systems/fixture.mjs';
import { driver, start, win, settle } from '../systems/driver.mjs';
import { closeNativeRuntimes } from '../../dist/packages/native-bridge/index.js';

test('Wasm, native C++, and reference match full systems route and every intermediate save/projection', async () => {
  const f = await fixture(); assert.ok(f.built.ok, JSON.stringify(f.built));
  const c = f.built.snapshot, d = await driver(c, true);
  await start(d);
  await d.send({ type: 'moveExpedition', nodeId: 'clearing' });
  await d.send({ type: 'battleAction', action: 'guard' });
  await d.send({ type: 'battleAction', action: 'useItem', itemId: 'potion' });
  await win(d);
  await d.send({ type: 'moveExpedition', nodeId: 'exit' });
  await d.send({ type: 'returnFromExpedition' }); await settle(d, c);
  await d.send({ type: 'claimQuest', questId: 'forest-help' }); await settle(d, c);
  assert.equal(d.state.periodIndex, 1);
  assert.equal(d.state.systems.quests['forest-help'].status, 'claimed');
  assert.ok(d.state.completedEvents.includes('friendship-unlock'));
  closeNativeRuntimes();
});

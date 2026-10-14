import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { PlayerController } from '../../dist/packages/player-service/controller.js';
import { createSession, dispatchSession } from '../../dist/packages/runtime-service/session.js';
import { saveRun } from '../../dist/packages/runtime-service/save.js';

const sample = resolve('examples/academy-demo');
const version = c => ({ generation: c.generation, revision: c.session.state.stateRevision });
async function start() {
  const c = new PlayerController();
  assert.equal((await c.openProject(sample)).error, null);
  assert.equal(c.newRun(null).error, null);
  return c;
}
async function temp() { await mkdir('.cache/player-tests', { recursive: true }); return mkdtemp(resolve('.cache/player-tests/run-')); }

test('stale generation and revision, malformed actions cannot mutate state', async () => {
  const c = await start(); const first = version(c);
  assert.equal(c.act({ type: 'schedule', activityIds: ['study', 'work', 'rest'] }, first).error, null);
  const state = c.session;
  assert.ok(c.act({ type: 'execute' }, first).error);
  assert.equal(c.session, state);
  assert.ok(c.act({ type: 'schedule', activityIds: ['bad'] }, version(c)).error);
  assert.equal(c.session, state);
  const beforeRestart = version(c);
  assert.equal(c.newRun(beforeRestart).error, null);
  assert.ok(c.act({ type: 'execute' }, { ...beforeRestart, revision: 0 }).error);
  assert.equal(c.session.state.stateRevision, 0);
});

test('insufficient cost preserves run and remaining schedule can be corrected', async () => {
  const c = await start();
  c.session = createSession(c.content, 42, { initialOverrides: { attributes: { money: 0 }, flags: {}, relationships: {} } }).session;
  c.act({ type: 'schedule', activityIds: ['study', 'rest', 'rest'] }, version(c));
  const before = c.session;
  assert.ok(c.act({ type: 'execute' }, version(c)).error);
  assert.equal(c.session, before);
  c.act({ type: 'schedule', activityIds: ['rest', 'rest', 'rest'] }, version(c));
  assert.equal(c.act({ type: 'execute' }, version(c)).error, null);
  assert.equal(c.session.state.phase, 'activity-result');
});

test('invalid project and invalid save preserve active content and session', async () => {
  const c = await start(); const before = c.session; const content = c.content;
  const directory = await temp();
  assert.ok((await c.openProject(directory)).error);
  assert.equal(c.session, before); assert.equal(c.content, content);
  await writeFile(`${directory}/bad.json`, '{bad');
  assert.ok((await c.load(`${directory}/bad.json`, version(c))).error);
  assert.equal(c.session, before);
});

test('disk round trip restores state and invalidates previous generation', async () => {
  const c = await start();
  c.act({ type: 'schedule', activityIds: ['study', 'work', 'rest'] }, version(c));
  c.act({ type: 'execute' }, version(c));
  const before = structuredClone(c.session.state); const token = version(c);
  const file = `${await temp()}/game.json`;
  assert.equal((await c.save(file, token)).error, null);
  c.act({ type: 'continue' }, version(c));
  assert.equal((await c.load(file, version(c))).error, null);
  assert.deepEqual(c.session.state, before);
  assert.ok(c.act({ type: 'continue' }, token).error);
});

test('headless period-end saves settle to a visible boundary; activities-only saves are rejected', async () => {
  const c = await start();
  c.act({ type: 'schedule', activityIds: ['study', 'work', 'rest'] }, version(c));
  c.act({ type: 'execute' }, version(c));
  const end = dispatchSession(c.content, c.session, { type: 'acknowledgeResult' }, c.session.state.stateRevision);
  assert.equal(end.ok, true);
  const file = `${await temp()}/game.json`;
  assert.equal((await saveRun(file, c.content, end.session)).ok, true);
  assert.equal((await c.load(file, version(c))).error, null);
  assert.equal(c.session.state.phase, 'ready'); assert.equal(c.session.state.periodIndex, 1);
  const before = c.session;
  const activities = createSession(c.content, 42, { mode: 'activities-only' });
  await saveRun(file, c.content, activities.session);
  assert.ok((await c.load(file, version(c))).error); assert.equal(c.session, before);
});

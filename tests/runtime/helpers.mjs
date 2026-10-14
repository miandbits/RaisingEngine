import assert from 'node:assert/strict';
import { mkdir, mkdtemp, rm } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { loadProject, buildSnapshot } from '../../dist/packages/project-service/index.js';
import { createSession, dispatchSession } from '../../dist/packages/runtime-service/index.js';

export const samplePath = fileURLToPath(new URL('../../examples/academy-demo', import.meta.url));
export const loaded = await loadProject(samplePath);
export const scenarios = loaded.documents.filter(d => d.kind === 'scenario').map(d => d.data);
export const doc = (project, kind, id) => project.documents.find(d => d.kind === kind && (id === undefined || d.data.id === id)).data;
export const literal = value => ({ op: 'literal', value });
export const numeric = (id, value, op = 'add') => ({ op, target: { kind: 'attribute', attributeId: id }, value: typeof value === 'number' ? literal(value) : value });
export const event = (id, nodes = [{ id: 'end', type: 'end' }], extra = {}) => ({ formatVersion: '0.1.0', id, label: id,
  requirements: { op: 'always' }, priority: 1, window: { fromPeriod: 0, throughPeriod: 5 }, entryNodeId: nodes[0].id, nodes, ...extra });
export function replaceEvents(project, events) {
  project.documents = project.documents.filter(d => d.kind !== 'event');
  for (const data of events) project.documents.push({ kind: 'event', file: `events/${data.id}.json`, data, version: '', locations: new Map() });
}
export async function content(edit = () => {}) {
  const project = structuredClone(loaded);
  project.documents = project.documents.filter(d => d.kind !== 'scenario');
  await edit(project);
  const result = await buildSnapshot(project);
  assert.equal(result.ok, true, JSON.stringify(result.diagnostics));
  return result.snapshot;
}
export function start(snapshot, options = {}, seed = 42) {
  const result = createSession(snapshot, seed, options);
  assert.equal(result.ok, true, JSON.stringify(result)); return result.session;
}
export function step(snapshot, session, command) {
  const result = dispatchSession(snapshot, session, command, session.state.stateRevision);
  assert.equal(result.ok, true, JSON.stringify(result)); return result;
}
export const send = (snapshot, session, command) => step(snapshot, session, command).session;
export function boundary(snapshot, session = start(snapshot)) {
  if (session.state.phase === 'planning') session = send(snapshot, session, { type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] });
  session = send(snapshot, session, { type: 'executeActivity' });
  return send(snapshot, session, { type: 'acknowledgeResult' });
}
export function reject(snapshot, session, command, code, revision = session.state.stateRevision) {
  const bytes = JSON.stringify(session);
  const result = dispatchSession(snapshot, session, command, revision);
  assert.equal(result.ok, false); assert.equal(result.error.code, code);
  assert.equal(result.session, session); assert.equal(JSON.stringify(session), bytes);
  return result.error;
}
export async function temporary(t) {
  const cache = fileURLToPath(new URL('../../.cache/runtime-tests', import.meta.url));
  await mkdir(cache, { recursive: true });
  const root = await mkdtemp(path.join(cache, 'run-'));
  t.after(async () => {
    assert.ok(path.resolve(root).startsWith(path.resolve(cache) + path.sep));
    await rm(root, { recursive: true, force: true });
  });
  return root;
}

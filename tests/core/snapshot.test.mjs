import test from 'node:test';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
import { mkdir, mkdtemp, writeFile, rm } from 'node:fs/promises';
import path from 'node:path';
import { loadProject, buildSnapshot } from '../../dist/packages/project-service/index.js';
import { canonicalJson } from '../../dist/packages/project-service/snapshot.js';

const loaded = await loadProject(fileURLToPath(new URL('../../examples/academy-demo', import.meta.url)));
const projectData = project => project.documents.find(d => d.kind === 'project').data;
const assetDocument = project => project.documents.find(d => d.kind === 'assets');
const successful = async project => {
  const result = await buildSnapshot(project);
  assert.equal(result.ok, true, JSON.stringify(result.diagnostics));
  return result.snapshot;
};

test('canonical v1 key ordering, arrays and SHA256 vector are fixed', () => {
  assert.equal(canonicalJson({ z: 2, a: { b: true, a: null }, list: [3, 1] }), '{"a":{"a":null,"b":true},"list":[3,1],"z":2}');
  assert.equal(createHash('sha256').update(canonicalJson({})).digest('hex'), '44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a');
});

test('snapshot is detached, deeply frozen and includes a fingerprint of canonical payload', async () => {
  const input = structuredClone(loaded);
  input.root = path.resolve(input.root, '..', 'same-content-other-location');
  const promise = buildSnapshot(input);
  projectData(input).title = 'edited while awaiting validation';
  const built = await promise;
  assert.equal(built.ok, true);
  const snapshot = built.snapshot;
  assert.equal(snapshot.project.title, projectData(loaded).title);
  assert.ok(Object.isFrozen(snapshot.activities[0].baseEffects));
  assert.throws(() => { snapshot.attributes[0].initial = 99; }, TypeError);
  const { contentFingerprint, ...payload } = snapshot;
  assert.equal(contentFingerprint, `sha256:${createHash('sha256').update(canonicalJson(payload)).digest('hex')}`);
});

test('key order, source location, file version, schema hint and scenario edits do not change fingerprint', async () => {
  const original = await successful(loaded);
  const input = structuredClone(loaded);
  input.root = path.resolve(input.root, '..', 'same-content-other-location');
  const reorder = value => Array.isArray(value) ? value.map(reorder)
    : value && typeof value === 'object' ? Object.fromEntries(Object.entries(value).reverse().map(([k, v]) => [k, reorder(v)])) : value;
  input.documents.reverse();
  for (const doc of input.documents) {
    doc.data = reorder(doc.data);
    doc.data.$schema = 'editor-hint';
    doc.locations = new Map([['', { offset: 999, line: 100, column: 9 }]]);
    doc.version = 'whitespace-only-edit';
    if (doc.kind === 'scenario') doc.data.seed++;
  }
  assert.equal(JSON.stringify(await successful(input)), JSON.stringify(original));
});

test('effect order and content edits change fingerprint; invalid content cannot produce a snapshot', async () => {
  const original = await successful(loaded);
  const input = structuredClone(loaded);
  const activity = input.documents.find(d => d.kind === 'activity' && d.data.id === 'study').data;
  activity.baseEffects.reverse();
  assert.notEqual((await successful(input)).contentFingerprint, original.contentFingerprint);
  activity.costs[0].attributeId = 'missing';
  const bad = await buildSnapshot(input);
  assert.equal(bad.ok, false);
  assert.ok(bad.diagnostics.some(d => d.code === 'REFERENCE_MISSING'));
  assert.equal('snapshot' in bad, false);
});

test('asset bytes affect fingerprint and missing/escaping assets fail before core use', async t => {
  const cache = path.resolve(fileURLToPath(new URL('../../.cache/core-tests', import.meta.url)));
  await mkdir(cache, { recursive: true });
  const root = await mkdtemp(path.join(cache, 'assets-'));
  t.after(async () => {
    const resolved = path.resolve(root);
    assert.ok(resolved.startsWith(cache + path.sep));
    await rm(resolved, { recursive: true, force: true });
  });
  const input = structuredClone(loaded); input.root = root;
  const doc = assetDocument(input);
  doc.data.assets = [{ id: 'test-image', kind: 'image', path: 'assets/image.png' }];
  await mkdir(path.join(root, 'assets'));
  await writeFile(path.join(root, 'assets/image.png'), 'asset-one');
  const first = await successful(input);
  await writeFile(path.join(root, 'assets/image.png'), 'asset-two');
  const second = await successful(input);
  assert.notEqual(first.contentFingerprint, second.contentFingerprint);
  assert.notEqual(first.assetHashes['test-image'], second.assetHashes['test-image']);
  doc.data.assets[0].path = 'assets/missing.png';
  const missing = await buildSnapshot(input);
  assert.equal(missing.ok, false);
  assert.ok(missing.diagnostics.some(d => d.code === 'ASSET_MISSING' && d.pointer === '/assets/0/path'));
  doc.data.assets[0].path = '../outside.png';
  assert.equal((await buildSnapshot(input)).ok, false);
});

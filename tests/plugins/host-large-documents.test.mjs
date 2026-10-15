// Large plugin documents (plugin 1.9.0 large-rig work, engine side): an 8 MiB limit for extension
// documents (loader and editor agree), source locations computed from the value for the editor's
// own serialization (identical to a full parse), and the editor's immutable drafts (an edit never
// clones or re-parses the documents it does not touch; views stay detached copies).
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { parseDocument } from '../../dist/packages/project-service/json-document.js';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot, detachProject } from '../../dist/packages/project-service/snapshot.js';
import { validateProject } from '../../dist/packages/validation/index.js';
import { StudioEditor } from '../../dist/packages/studio-service/editor.js';
import { addCutout, envelope, lock, newProject, serialize } from './host-helpers.mjs';
import { stressRig } from './stress-fixture.mjs';

const RIG = 'plugins/cutout-animation/rig/hero.json';

test('locations of the editor serialization equal a full parse (every pointer, odd keys, misses)', () => {
  const samples = [
    envelope('rig', 'hero', stressRig().rig),
    { a: [], b: {}, 'k/~ey': [1, [2, []], { 한글: 'x', '😀': [null, true] }], 'x"y': { z: [{}] }, 0: 1, 10: 2, ['__proto__']: { q: 1 } },
    [[], [[[]]], { '': { '': 0 } }],
  ];
  for (const data of samples) {
    const text = JSON.stringify(data, null, 2) + '\n';
    const full = parseDocument('f.json', 'extension', text).document.locations;
    const fast = parseDocument('f.json', 'extension', text, { serialized: true }).document.locations;
    let n = 0;
    for (const [pointer, location] of full) { n++; assert.deepEqual(fast.get(pointer), location, pointer); }
    assert.ok(n > 3);
    for (const pointer of ['/nope', '/a/0', '/0/9', '/data/bones/01', '/data/bones/-1', 'x']) assert.equal(fast.has(pointer), full.has(pointer), pointer);
    assert.equal(fast.size, full.size, 'iteration falls back to the full map');
  }
  // Text that is not JSON after all takes the full parse and its diagnostics (the option is only for
  // JSON.stringify output, which cannot repeat a key).
  assert.equal(parseDocument('f.json', 'extension', '{"a":', { serialized: true }).diagnostics[0].code, 'JSON_SYNTAX');
});

test('extension documents up to 8 MiB: created, saved and reloaded; larger ones refused', async () => {
  const root = await newProject();
  await addCutout(root, { rig: null, controller: null, image: null, lockData: lock() });
  const editor = new StudioEditor({ pluginStore: null });
  await editor.open(root);
  const big = envelope('rig', 'hero', stressRig().rig);
  assert.ok(Buffer.byteLength(serialize(big)) > 2 * 1024 * 1024, 'indented form above the general 2 MiB limit');
  editor.edit({ type: 'create', file: RIG, data: big }, editor.revision);
  await editor.save(editor.revision);
  const loaded = await loadProject(root);
  assert.equal(loaded.diagnostics.some(d => d.code === 'DOCUMENT_SIZE_LIMIT'), false);
  assert.deepEqual(loaded.documents.find(d => d.file === RIG).data, big);
  const huge = structuredClone(big);
  huge.data.name = 'x'.repeat(9 * 1024 * 1024);
  assert.throws(() => editor.edit({ type: 'replace', file: RIG, data: huge }, editor.revision), /8 MiB/);
  await writeFile(path.join(root, RIG), serialize(huge));
  const refused = await loadProject(root);
  assert.equal(refused.diagnostics.find(d => d.file === RIG).code, 'DOCUMENT_SIZE_LIMIT');
  // Other documents keep the 2 MiB limit.
  const project = JSON.parse(await readFile(path.join(root, 'project.json'), 'utf8'));
  project.title = 'x'.repeat(3 * 1024 * 1024);
  assert.throws(() => editor.edit({ type: 'replace', file: 'project.json', data: project }, editor.revision), /2 MiB/);
});

test('drafts are immutable values: untouched documents are shared, views are detached copies', async () => {
  const root = await newProject();
  await addCutout(root);
  const editor = new StudioEditor({ pluginStore: null });
  await editor.open(root);
  const before = editor.documents, rig = before.find(d => d.file === RIG);
  const changed = structuredClone(rig.data);
  changed.data.name = 'edited';
  editor.edit({ type: 'replace', file: RIG, data: changed }, editor.revision);
  assert.notEqual(editor.documents, before);
  for (const doc of editor.documents) if (doc.file !== RIG) assert.equal(doc, before.find(d => d.file === doc.file), `${doc.file} shared, not cloned`);
  assert.equal(editor.documents.find(d => d.file === RIG).data.data.name, 'edited');
  assert.equal(rig.data.data.name, undefined, 'the previous revision is untouched');
  const view = editor.view();
  view.documents.find(d => d.file === RIG).data.data.name = 'mutated by a caller';
  assert.equal(editor.documents.find(d => d.file === RIG).data.data.name, 'edited', 'views are copies');
  assert.equal(JSON.parse(JSON.stringify(editor.reply())).view.documents.length, editor.documents.length, 'serializes like a plain object');
  assert.equal(editor.view({ shared: true }).documents, editor.documents, 'shared views (stdio service) skip the copy');
  const draft = editor.view().draftVersion;
  editor.edit({ type: 'undo' }, editor.revision);
  assert.equal(editor.documents, before, 'undo restores the exact previous draft');
  assert.notEqual(editor.view().draftVersion, draft);
  editor.edit({ type: 'redo' }, editor.revision);
  assert.equal(editor.view().draftVersion, draft, 'draft versions follow the content');
  assert.equal(editor.view().dirty, true);
  editor.edit({ type: 'replace', file: RIG, data: rig.data }, editor.revision);
  assert.equal(editor.view().dirty, false, 'back to the saved content');
});

test('everything the editor hands out stays structuredClone-safe (lazy locations are plain Maps)', async () => {
  const root = await newProject();
  await addCutout(root);
  const editor = new StudioEditor({ pluginStore: null });
  await editor.open(root);
  const rig = structuredClone(editor.documents.find(d => d.file === RIG).data);
  rig.data.name = 'edited';
  editor.edit({ type: 'replace', file: RIG, data: rig }, editor.revision);
  // Callers (Qt preview, Electron IPC, MCP) clone these: no WeakMap, class state or getter may leak.
  for (const value of [editor.loaded(), editor.view(), editor.reply(), editor.view({ shared: true }), await buildSnapshot(editor.loaded())]) structuredClone(value);
  const loaded = editor.loaded(), doc = loaded.documents.find(d => d.file === RIG);
  const pointer = '/data/name', exact = parseDocument(RIG, 'extension', JSON.stringify(rig, null, 2) + '\n').document.locations.get(pointer);
  assert.ok(doc.locations instanceof Map);
  assert.equal(JSON.stringify(doc.locations), '{}', 'no private state serialized');
  // A clone carries the entries built so far; detachProject shares the locations, so they stay exact.
  const detached = detachProject(loaded);
  assert.equal(detached.documents.find(d => d.file === RIG).locations, doc.locations);
  assert.deepEqual(detached.documents.find(d => d.file === RIG).locations.get(pointer), exact);
  detached.documents.find(d => d.file === RIG).data.data.name = 'detached';
  assert.equal(doc.data.data.name, 'edited', 'detached data is a copy');
  assert.ok((await validateProject(detached)).diagnostics.every(d => d.severity !== 'error'));
  assert.equal(doc.locations.size, [...doc.locations].length, 'iteration builds the full map');
  assert.deepEqual(structuredClone(doc.locations).get(pointer), exact, 'a clone of a built map is exact');
});

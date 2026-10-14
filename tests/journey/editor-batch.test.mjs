import test from 'node:test';
import assert from 'node:assert/strict';
import { StudioEditor, MAX_EDIT_BATCH } from '../../dist/packages/studio-service/editor.js';
import { fixture } from '../gameplay/fixture.mjs';

async function opened(t) {
  const { root } = await fixture(t);
  const editor = new StudioEditor();
  await editor.open(root);
  return editor;
}

function relationBatch(view, id = 'atomic-friendship') {
  const project = view.documents.find(d => d.kind === 'project');
  const event = structuredClone(view.documents.find(d => d.file === 'events/gift-friendship.json').data);
  event.id = id;
  event.label = '원자적 관계 사건';
  const data = structuredClone(project.data);
  data.gameplay.interactions.find(i => i.id === 'gift-mentor').eventId = id;
  return { type: 'batch', edits: [
    { type: 'add', kind: 'event', id },
    { type: 'replace', file: `events/${id}.json`, data: event },
    { type: 'replace', file: project.file, data },
  ] };
}

test('relation event creation and interaction link commit as one revision and undo/redo entry', async t => {
  const editor = await opened(t), before = editor.view();
  const batch = relationBatch(before);
  editor.edit(batch, before.revision);
  const after = editor.view();
  assert.equal(after.revision, before.revision + 1);
  assert.equal(after.dirty, true);
  assert.equal(after.documents.find(d => d.kind === 'project').data.gameplay.interactions[0].eventId, 'atomic-friendship');
  assert.equal(after.documents.find(d => d.file === 'events/atomic-friendship.json').data.label, '원자적 관계 사건');
  await editor.validate(after.revision);
  assert.equal(editor.view().diagnostics.filter(d => d.severity === 'error').length, 0);
  editor.edit({ type: 'undo' }, after.revision);
  const undone = editor.view();
  assert.deepEqual(undone.documents, before.documents);
  assert.equal(undone.draftVersion, before.draftVersion);
  assert.equal(undone.dirty, false);
  editor.edit({ type: 'undo' }, undone.revision);
  assert.equal(editor.view().revision, undone.revision, 'there is no second undo entry');
  editor.edit({ type: 'redo' }, undone.revision);
  assert.deepEqual(editor.view().documents, after.documents);
  batch.edits[1].data.label = 'caller mutation';
  assert.equal(editor.view().documents.find(d => d.file === 'events/atomic-friendship.json').data.label, '원자적 관계 사건');
});

test('failed substeps preserve documents, diagnostics, revision and redo history', async t => {
  const editor = await opened(t);
  editor.edit(relationBatch(editor.view()), editor.view().revision);
  editor.edit({ type: 'undo' }, editor.view().revision);
  const before = editor.view();
  const badSteps = [
    { type: 'add', kind: 'event', id: 'gift-friendship' },
    { type: 'replace', file: 'events/missing.json', data: {} },
    { type: 'delete', file: 'project.json' },
    { type: 'replace', file: 'events/atomic-friendship.json', data: { text: 'x'.repeat(2 * 1024 * 1024) } },
  ];
  for (const step of badSteps) {
    const batch = relationBatch(before);
    batch.edits.push(step);
    assert.throws(() => editor.edit(batch, before.revision));
    assert.deepEqual(editor.view(), before);
  }
  editor.edit({ type: 'redo' }, before.revision);
  assert.ok(editor.view().documents.some(d => d.file === 'events/atomic-friendship.json'));
});

test('batch rejects stale versions, nested/history edits and invalid bounds without mutation', async t => {
  const editor = await opened(t), before = editor.view();
  assert.throws(() => editor.edit(relationBatch(before), before.revision - 1));
  for (const edits of [[], null, {}, Array(MAX_EDIT_BATCH + 1).fill({ type: 'add', kind: 'event', id: 'too-many' }),
    [{ type: 'undo' }], [{ type: 'redo' }], [{ type: 'batch', edits: [] }], [null]]) {
    assert.throws(() => editor.edit({ type: 'batch', edits }, before.revision));
    assert.deepEqual(editor.view(), before);
  }
});

test('no-op batch preserves redo while maximum bounded batch commits once', async t => {
  const editor = await opened(t);
  editor.edit({ type: 'add', kind: 'event', id: 'primitive' }, editor.view().revision);
  editor.edit({ type: 'undo' }, editor.view().revision);
  const before = editor.view();
  editor.edit({ type: 'batch', edits: [
    { type: 'add', kind: 'event', id: 'temporary' },
    { type: 'delete', file: 'events/temporary.json' },
  ] }, before.revision);
  assert.deepEqual(editor.view(), before);
  editor.edit({ type: 'redo' }, before.revision);
  const revision = editor.view().revision;
  editor.edit({ type: 'batch', edits: Array.from({ length: MAX_EDIT_BATCH }, (_, i) => ({
    type: 'add', kind: 'event', id: `bounded-${i}`,
  })) }, revision);
  assert.equal(editor.view().revision, revision + 1);
  editor.edit({ type: 'undo' }, revision + 1);
  assert.equal(editor.view().documents.filter(d => d.file.startsWith('events/bounded-')).length, 0);
});

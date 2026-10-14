import test from 'node:test';
import assert from 'node:assert/strict';
import { cp, mkdir, mkdtemp, readFile, writeFile, access } from 'node:fs/promises';
import { resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import { StudioEditor } from '../../dist/packages/studio-service/editor.js';
import { commitProject, fileHash, recoverProject, safeFile } from '../../dist/packages/project-service/transaction.js';
const sample = resolve('examples/academy-demo');
async function fixture() { await mkdir('.cache/studio-tests', { recursive: true }); const root = await mkdtemp(resolve('.cache/studio-tests/project-')); await cp(sample, root, { recursive: true }); const editor = new StudioEditor(); await editor.open(root); return { root, editor }; }
const change = (editor, file, fn) => { const data = structuredClone(editor.documents.find(d => d.file === file).data); fn(data); return editor.edit({ type: 'replace', file, data }, editor.revision); };

test('author adds activity, event, ending and character, saves and reloads without losing content', async () => {
  const { root, editor } = await fixture();
  const untouched = JSON.stringify(JSON.parse(await readFile(`${root}/activities/work.json`, 'utf8')), null, 4) + '\r\n';
  await writeFile(`${root}/activities/work.json`, untouched); await editor.open(root);
  for (const kind of ['activity', 'event', 'ending', 'character']) editor.edit({ type: 'add', kind, id: `new-${kind}` }, editor.revision);
  assert.equal((await editor.snapshot(editor.revision)).ok, true);
  await editor.save(editor.revision); assert.equal(editor.view().dirty, false);
  assert.equal(await readFile(`${root}/activities/work.json`, 'utf8'), untouched);
  const reopened = new StudioEditor(); await reopened.open(root);
  assert.deepEqual(reopened.documents, editor.documents.slice().sort((a, b) => reopened.documents.findIndex(d => d.file === a.file) - reopened.documents.findIndex(d => d.file === b.file)));
  assert.equal((await reopened.snapshot(reopened.revision)).ok, true);
});
test('undo/redo, branching history and stale revision do not lose edits', async () => {
  const { editor } = await fixture(); const revision = editor.revision;
  change(editor, 'activities/study.json', d => d.label = '검술 심화');
  assert.throws(() => editor.edit({ type: 'undo' }, revision), /갱신/);
  editor.edit({ type: 'undo' }, editor.revision); assert.equal(editor.view().dirty, false);
  editor.edit({ type: 'redo' }, editor.revision); assert.equal(editor.view().dirty, true);
  editor.edit({ type: 'undo' }, editor.revision);
  change(editor, 'activities/rest.json', d => d.label = '휴양'); assert.equal(editor.view().redo, false);
});
test('external modification of any loaded file blocks save and retains disk bytes and draft', async () => {
  const { root, editor } = await fixture();
  change(editor, 'activities/study.json', d => d.label = '초안');
  const study = await readFile(`${root}/activities/study.json`, 'utf8');
  const work = JSON.parse(await readFile(`${root}/activities/work.json`, 'utf8')); work.description = '외부 작성';
  const bytes = JSON.stringify(work); await writeFile(`${root}/activities/work.json`, bytes);
  await assert.rejects(editor.save(editor.revision), /외부 변경 충돌/);
  assert.equal(await readFile(`${root}/activities/study.json`, 'utf8'), study);
  assert.equal(await readFile(`${root}/activities/work.json`, 'utf8'), bytes);
  assert.equal(editor.view().dirty, true);
});
test('external document additions are detected even when edited files did not change', async () => {
  const { root, editor } = await fixture();
  change(editor, 'activities/study.json', d => d.label = '초안');
  const copy = JSON.parse(await readFile(`${root}/activities/rest.json`, 'utf8')); copy.id = 'external';
  await writeFile(`${root}/activities/external.json`, JSON.stringify(copy));
  await assert.rejects(editor.save(editor.revision), /추가·삭제/);
});
test('structural errors block saves; semantic errors can be saved but cannot run', async () => {
  const { editor, root } = await fixture();
  change(editor, 'activities/study.json', d => d.costs[0].amount = -1);
  await assert.rejects(editor.save(editor.revision), /구조 오류/);
  editor.edit({ type: 'undo' }, editor.revision);
  change(editor, 'events/exam-invitation.json', d => d.nodes[0].next = 'missing-node');
  await editor.save(editor.revision);
  assert.equal((await editor.snapshot(editor.revision)).ok, false);
  assert.equal(JSON.parse(await readFile(`${root}/events/exam-invitation.json`, 'utf8')).nodes[0].next, 'missing-node');
});
test('draft deletion is undoable and removing a referenced document leaves diagnostics', async () => {
  const { editor } = await fixture();
  editor.edit({ type: 'delete', file: 'activities/study.json' }, editor.revision);
  assert.equal((await editor.snapshot(editor.revision)).ok, false);
  editor.edit({ type: 'undo' }, editor.revision); assert.equal((await editor.snapshot(editor.revision)).ok, true);
});
test('read-only sample and path traversal cannot write files', async () => {
  const editor = new StudioEditor(); await editor.open(sample, true);
  change(editor, 'activities/study.json', d => d.label = '초안');
  await assert.rejects(editor.save(editor.revision), /읽기 전용/);
  await assert.rejects(safeFile(sample, '../outside.json'), /허용되지/);
  assert.throws(() => editor.edit({ type: 'add', kind: 'activity', id: '../bad' }, editor.revision), /ID/);
});
test('transaction preflight failure changes no file and cleans the journal', async () => {
  const { root } = await fixture(); const file = 'activities/study.json'; const before = await readFile(`${root}/${file}`);
  await assert.rejects(commitProject(root, [{ file, expected: fileHash(before), text: '{}' }, { file: 'activities/rest.json', expected: 'wrong', text: '{}' }]), /충돌/);
  assert.deepEqual(await readFile(`${root}/${file}`), before);
  await assert.rejects(access(`${root}/.studio/pending-save.json`));
});
async function interrupted(root, external = false) {
  const env = { ...process.env }; delete env.NODE_OPTIONS;
  const dead = Number(spawnSync(process.execPath, ['-e', 'console.log(process.pid)'], { env, encoding: 'utf8' }).stdout.trim());
  const entries = [];
  for (const file of ['activities/study.json', 'activities/rest.json']) {
    const before = await readFile(`${root}/${file}`); const after = Buffer.from('{}');
    entries.push({ file, before: before.toString('base64'), after: after.toString('base64') });
  }
  await writeFile(`${root}/activities/study.json`, external ? 'external' : '{}');
  await writeFile(`${root}/.studio/pending-save.json`, JSON.stringify({ version: 1, pid: dead, entries }));
  return entries;
}
test('reopening after partial multi-file save restores original bytes before loading', async () => {
  const { root } = await fixture(); const entries = await interrupted(root);
  const reopened = new StudioEditor(); assert.match((await reopened.open(root)).notice, /복구/);
  for (const e of entries) assert.deepEqual(await readFile(`${root}/${e.file}`), Buffer.from(e.before, 'base64'));
  assert.equal((await reopened.snapshot(reopened.revision)).ok, true);
});
test('recovery preserves unknown external bytes and keeps journal for manual resolution', async () => {
  const { root } = await fixture(); await interrupted(root, true);
  await assert.rejects(recoverProject(root), /복구 충돌/);
  assert.equal(await readFile(`${root}/activities/study.json`, 'utf8'), 'external');
  await access(`${root}/.studio/pending-save.json`);
});
test('preview snapshot is frozen and never mixes later draft edits into an existing run', async () => {
  const { editor } = await fixture(); const before = await editor.snapshot(editor.revision);
  change(editor, 'activities/study.json', d => d.label = '변경됨');
  const after = await editor.snapshot(editor.revision);
  assert.notEqual(before.snapshot.contentFingerprint, after.snapshot.contentFingerprint);
  assert.equal(before.snapshot.activities.find(a => a.id === 'study').label, '검술 수업');
});

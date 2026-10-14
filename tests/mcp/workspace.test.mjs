import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile, writeFile, access } from 'node:fs/promises';
import { StudioEditor } from '../../dist/packages/studio-service/editor.js';
import { McpWorkspace } from '../../dist/packages/mcp-service/workspace.js';
import { fixture } from './helpers.mjs';

const file = 'activities/study.json';
async function editFor(workspace, version, label, name = file) {
  const { document } = await workspace.readDocument(name, version);
  return { type: 'replace', file: name, data: { ...document.data, label } };
}
const disk = async (root, name = file) => JSON.parse(await readFile(`${root}/${name}`, 'utf8'));

test('MCP and GUI share validation, byte versions and unchanged formatting', async () => {
  const root = await fixture(); const mcp = new McpWorkspace(root); const gui = new StudioEditor(); await gui.open(root);
  const first = await mcp.readProject(); assert.equal(first.version, gui.diskVersion());
  const result = await mcp.validate(first.version); assert.equal(result.ok, true);
  assert.equal(result.contentFingerprint, (await gui.snapshot(gui.revision)).snapshot.contentFingerprint);
  const untouched = await readFile(`${root}/activities/work.json`, 'utf8');
  const saved = await mcp.apply(first.version, [await editFor(mcp, first.version, 'LLM 수업')], '수업 이름 변경');
  assert.equal(saved.ok, true); assert.notEqual(saved.version, first.version); assert.equal(saved.changes.length, 1);
  assert.equal(await readFile(`${root}/activities/work.json`, 'utf8'), untouched);
  await gui.open(root); assert.equal(gui.diskVersion(), saved.version); assert.equal((await disk(root)).label, 'LLM 수업');
});

test('GUI saved after MCP read: stale MCP batch cannot overwrite GUI bytes', async () => {
  const root = await fixture(); const mcp = new McpWorkspace(root); const { version } = await mcp.readProject();
  const edit = await editFor(mcp, version, '오래된 LLM 변경');
  const gui = new StudioEditor(); await gui.open(root); gui.edit({ ...edit, data: { ...edit.data, label: '인간 변경' } }, gui.revision); await gui.save(gui.revision);
  await assert.rejects(mcp.apply(version, [edit], 'stale'), e => e.code === 'VERSION_CONFLICT' && e.details.actualVersion === gui.diskVersion());
  assert.equal((await disk(root)).label, '인간 변경');
  // Reviewed retry is based on current data and preserves the human's label.
  const latest = await mcp.readProject(); const current = await mcp.readDocument(file, latest.version);
  await mcp.apply(latest.version, [{ type: 'replace', file, data: { ...current.document.data, description: '함께 편집' } }], '검토 후 설명 추가');
  assert.equal((await disk(root)).label, '인간 변경');
});

test('MCP saved while GUI is dirty: GUI save rejects and preserves draft and disk', async () => {
  const root = await fixture(); const mcp = new McpWorkspace(root); const { version } = await mcp.readProject();
  const gui = new StudioEditor(); await gui.open(root);
  gui.edit(await editFor(mcp, version, '인간 초안'), gui.revision);
  await mcp.apply(version, [await editFor(mcp, version, 'LLM 저장')], '병행 편집');
  await assert.rejects(gui.save(gui.revision), e => e.code === 'VERSION_CONFLICT' && e.files.includes(file));
  assert.equal(gui.view().dirty, true); assert.equal(gui.documents.find(d => d.file === file).data.label, '인간 초안');
  assert.equal((await disk(root)).label, 'LLM 저장');
});

test('whole read set is checked INSIDE the lock even for disjoint document changes', async () => {
  const root = await fixture(); const first = new StudioEditor(); const second = new StudioEditor(); await first.open(root); await second.open(root);
  const mcp = new McpWorkspace(root); const { version } = await mcp.readProject();
  first.edit(await editFor(mcp, version, 'first'), first.revision);
  second.edit(await editFor(mcp, version, 'second', 'activities/work.json'), second.revision);
  const original = first.assertDiskUnchanged.bind(first); let injected = false;
  first.assertDiskUnchanged = async ownsLock => {
    await original(ownsLock);
    if (!ownsLock && !injected) { injected = true; await second.save(second.revision); }
  };
  await assert.rejects(first.save(first.revision), e => e.code === 'VERSION_CONFLICT' && e.files.includes('activities/work.json'));
  assert.equal((await disk(root, 'activities/work.json')).label, 'second');
  assert.notEqual((await disk(root)).label, 'first'); assert.equal(first.view().dirty, true);
  await assert.rejects(access(`${root}/.studio/pending-save.json`));
});

test('invalid batch never partially saves, including add followed by an invalid replace', async () => {
  const root = await fixture(); const mcp = new McpWorkspace(root); const { version } = await mcp.readProject();
  const invalid = await editFor(mcp, version, 'invalid'); invalid.data.costs[0].amount = -1;
  await assert.rejects(mcp.apply(version, [{ type: 'add', kind: 'activity', id: 'practice' }, invalid], 'invalid'), e => e.code === 'VALIDATION_FAILED' && e.details.diagnostics.some(d => d.file === file && d.pointer && d.line > 0));
  assert.equal((await mcp.readProject()).version, version); await assert.rejects(access(`${root}/activities/practice.json`));
  await assert.rejects(mcp.apply(version, [invalid], 'structural draft', true), e => e.code === 'CHANGE_REJECTED');
});

test('semantic invalid drafts require explicit opt-in and use editor diagnostics', async () => {
  const root = await fixture(); const mcp = new McpWorkspace(root); const { version } = await mcp.readProject();
  const edit = await editFor(mcp, version, 'dangling'); edit.data.requirements = { op: 'flag', flagId: 'missing', equals: true };
  await assert.rejects(mcp.apply(version, [edit], 'invalid'), e => e.code === 'VALIDATION_FAILED');
  const saved = await mcp.apply(version, [edit], '의미 오류가 있는 초안', true); assert.equal(saved.ok, true);
  const gui = new StudioEditor(); await gui.open(root); assert.deepEqual(saved.diagnostics, gui.diagnostics);
  assert.equal((await mcp.validate(saved.version)).ok, false);
});

test('add/replace/delete batch uses GUI templates and rejects path traversal', async () => {
  const root = await fixture(); const mcp = new McpWorkspace(root); const { version } = await mcp.readProject();
  const saved = await mcp.apply(version, [{ type: 'add', kind: 'activity', id: 'practice' }], '연습 추가');
  assert.equal((await disk(root, 'activities/practice.json')).label, '새 콘텐츠');
  await assert.rejects(mcp.readDocument('../package.json', saved.version), e => e.code === 'DOCUMENT_NOT_FOUND');
  await assert.rejects(mcp.apply(saved.version, [{ type: 'replace', file: '../package.json', data: {} }], 'bad'), e => e.code === 'CHANGE_REJECTED');
  await mcp.apply(saved.version, [{ type: 'delete', file: 'activities/practice.json' }], '연습 삭제');
  assert.equal((await mcp.readProject()).version, version);
});

test('byte formatting and file membership changes invalidate tokens across workspace restarts', async () => {
  const root = await fixture(); const mcp = new McpWorkspace(root); const { version } = await mcp.readProject();
  const edit = await editFor(mcp, version, 'stale');
  await writeFile(`${root}/activities/work.json`, JSON.stringify(await disk(root, 'activities/work.json'), null, 4));
  const restarted = new McpWorkspace(root);
  await assert.rejects(restarted.apply(version, [edit], 'restart'), e => e.code === 'VERSION_CONFLICT');
  const next = await restarted.readProject();
  await writeFile(`${root}/activities/external.json`, JSON.stringify({ ...(await disk(root)), id: 'external' }));
  await assert.rejects(restarted.apply(next.version, [edit], 'addition'), e => e.code === 'VERSION_CONFLICT');
});

test('read-only mode rejects changes and authoritative schemas are accessible', async () => {
  const root = await fixture(); const mcp = new McpWorkspace(root, true); const { version } = await mcp.readProject();
  await assert.rejects(mcp.apply(version, [], 'readonly'), e => e.code === 'READ_ONLY');
  for (const name of ['common','project','attributes','flags','character','activity','event','ending','assets','scenario'])
    assert.ok((await mcp.readSchema(name)).schema.$id);
});

test('both sample scenarios remain deterministic through MCP service', async () => {
  const mcp = new McpWorkspace(await fixture()); const { version } = await mcp.readProject();
  for (const [id, ending, skill, money] of [['certified-route', 'certified', 35, 22], ['ordinary-route', 'ordinary', 10, 61]]) {
    const first = await mcp.runScenario(version, id); assert.equal(first.ok, true); assert.equal(first.state.endingId, ending);
    assert.equal(first.state.attributes.skill, skill); assert.equal(first.state.attributes.money, money);
    assert.deepEqual(await mcp.runScenario(version, id), first);
  }
});

test('event explanations replay core command prefix with actual condition values', async () => {
  const mcp = new McpWorkspace(await fixture()); const { version } = await mcp.readProject();
  const result = await mcp.explainEvents(version, 'certified-route', 0);
  assert.equal(result.phase, 'planning'); assert.equal(result.decisionAppliesNow, false);
  assert.ok(result.events.some(e => e.reason === 'before-window'));
  assert.ok(result.events.every(e => typeof e.condition.passed === 'boolean'));
  const eligible = await mcp.explainEvents(version, 'certified-route', 9);
  assert.equal(eligible.periodIndex, 2); assert.equal(eligible.decisionAppliesNow, true);
  const selected = eligible.events.find(e => e.reason === 'selected'); assert.ok(selected);
  assert.equal(selected.condition.passed, true);
  const comparisons = [];
  const collect = node => { if (node.op === 'compare') comparisons.push(node); for (const child of node.children ?? []) collect(child); if (node.child) collect(node.child); };
  collect(selected.condition);
  assert.ok(comparisons.some(c => c.actual === 10 && c.expected === 10 && c.passed));
  await assert.rejects(mcp.explainEvents(version, 'certified-route', 999), e => e.code === 'INVALID_ARGUMENT');
});

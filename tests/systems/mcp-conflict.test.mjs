import test from 'node:test';
import assert from 'node:assert/strict';
import { cp, mkdir, mkdtemp, readFile } from 'node:fs/promises';
import path from 'node:path';
import { StudioEditor } from '../../dist/packages/studio-service/editor.js';
import { McpWorkspace } from '../../dist/packages/mcp-service/workspace.js';

const source = path.resolve('examples/academy-adventure');
async function fixture() {
  const cache = path.resolve('.cache/systems-mcp-conflict');
  await mkdir(cache, { recursive: true });
  const root = await mkdtemp(path.join(cache, 'project-'));
  await cp(source, root, { recursive: true, filter: file => !path.relative(source, file).split(path.sep).includes('.studio') });
  const editor = new StudioEditor();
  await editor.open(root);
  return { root, editor, mcp: new McpWorkspace(root) };
}
const project = editor => structuredClone(editor.documents.find(document => document.kind === 'project'));
const edit = (editor, document) => editor.edit({ type: 'replace', file: document.file, data: document.data }, editor.revision);
const disk = root => readFile(path.join(root, 'project.json'), 'utf8');

test('systems Studio save rejects stale MCP replacement; refreshed retry preserves equipment and quest/stage edits', async () => {
  const { root, editor, mcp } = await fixture();
  const initial = await mcp.readProject();
  const stale = (await mcp.readDocument('project.json', initial.version)).document;
  const human = project(editor);
  const oldAttack = human.data.systems.equipment[0].modifiers.attack;
  human.data.systems.equipment[0].modifiers.attack = oldAttack + 1;
  edit(editor, human);
  await editor.save(editor.revision);
  const humanBytes = await disk(root);
  const oldThreshold = stale.data.systems.relationshipStages[0].threshold;
  stale.data.systems.relationshipStages[0].threshold = oldThreshold + 1;
  stale.data.systems.quests[0].targetCount += 1;
  await assert.rejects(mcp.apply(initial.version, [{ type: 'replace', ...stale }], 'Adjust quest and relationship pacing'), { code: 'VERSION_CONFLICT' });
  assert.equal(await disk(root), humanBytes, 'Rejected stale project replacement must write no bytes');

  const refreshed = await mcp.readProject();
  assert.notEqual(refreshed.version, initial.version);
  const merged = (await mcp.readDocument('project.json', refreshed.version)).document;
  assert.equal(merged.data.systems.equipment[0].modifiers.attack, oldAttack + 1);
  merged.data.systems.relationshipStages[0].threshold = oldThreshold + 1;
  merged.data.systems.quests[0].targetCount = stale.data.systems.quests[0].targetCount;
  const applied = await mcp.apply(refreshed.version, [{ type: 'replace', ...merged }], 'Retry after reviewing current equipment changes');
  assert.equal(applied.ok, true);
  assert.notEqual(applied.version, refreshed.version);
  await editor.open(root);
  const result = project(editor).data.systems;
  assert.equal(result.equipment[0].modifiers.attack, oldAttack + 1);
  assert.equal(result.relationshipStages[0].threshold, oldThreshold + 1);
  assert.equal(result.quests[0].targetCount, stale.data.systems.quests[0].targetCount);
  assert.equal((await mcp.validate(applied.version)).ok, true);
});

test('systems MCP save protects an unsaved Studio draft; shared validation rejects invalid equipment references', async () => {
  const { root, editor, mcp } = await fixture();
  const human = project(editor);
  human.data.systems.equipment[0].modifiers.defense += 2;
  edit(editor, human);
  const draftBefore = editor.view();
  const current = await mcp.readProject();
  const agent = (await mcp.readDocument('project.json', current.version)).document;
  agent.data.systems.quests[0].description = 'A revised quest briefing from MCP';
  await mcp.apply(current.version, [{ type: 'replace', ...agent }], 'Revise quest briefing');
  const agentBytes = await disk(root);
  await assert.rejects(editor.save(editor.revision), { code: 'VERSION_CONFLICT' });
  assert.equal(await disk(root), agentBytes);
  assert.deepEqual(editor.view().documents, draftBefore.documents, 'Conflict must retain the unsaved human draft');
  assert.equal(editor.view().dirty, true);
  await editor.open(root);
  const merged = project(editor);
  merged.data.systems.equipment[0].modifiers.defense = human.data.systems.equipment[0].modifiers.defense;
  edit(editor, merged);
  await editor.save(editor.revision);
  const preserved = project(editor).data.systems;
  assert.equal(preserved.quests[0].description, agent.data.systems.quests[0].description);
  assert.equal(preserved.equipment[0].modifiers.defense, human.data.systems.equipment[0].modifiers.defense);

  const latest = await mcp.readProject();
  const invalid = (await mcp.readDocument('project.json', latest.version)).document;
  invalid.data.systems.equipment[0].itemId = 'nonexistent-systems-item';
  const validBytes = await disk(root);
  edit(editor, invalid);
  await editor.validate(editor.revision);
  assert.ok(editor.diagnostics.some(d => d.severity === 'error' && d.pointer?.includes('equipment')));
  await assert.rejects(mcp.apply(latest.version, [{ type: 'replace', ...invalid }], 'Invalid item reference'), { code: 'VALIDATION_FAILED' });
  assert.equal(await disk(root), validBytes, 'Invalid systems reference must not be persisted by MCP');
});

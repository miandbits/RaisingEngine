import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { fixture } from '../gameplay/fixture.mjs';
import { StudioEditor } from '../../dist/packages/studio-service/editor.js';
import { McpWorkspace } from '../../dist/packages/mcp-service/workspace.js';

test('human shop draft and MCP relationship edit conflict without losing either version', async t => {
  const { root } = await fixture(t);
  const editor = new StudioEditor(); await editor.open(root);
  const workspace = new McpWorkspace(root), baseline = await workspace.readProject();
  const document = editor.documents.find(d => d.kind === 'project');
  const human = structuredClone(document.data), machine = structuredClone(document.data);
  human.gameplay.shops[0].offers[0].price = 17;
  machine.gameplay.interactions[0].effects[0].value.value = 22;
  editor.edit({ type: 'replace', file: 'project.json', data: human }, editor.revision);
  const applied = await workspace.apply(baseline.version, [{ type: 'replace', file: 'project.json', data: machine }], 'Tune relationship gift');
  assert.equal(applied.ok, true);
  const disk = await readFile(resolve(root, 'project.json'), 'utf8');
  await assert.rejects(() => editor.save(editor.revision));
  assert.equal(editor.documents.find(d => d.kind === 'project').data.gameplay.shops[0].offers[0].price, 17);
  assert.equal(JSON.parse(disk).gameplay.shops[0].offers[0].price, 10);
  assert.equal(JSON.parse(disk).gameplay.interactions[0].effects[0].value.value, 22);
  await assert.rejects(() => workspace.apply(baseline.version, [{ type: 'replace', file: 'project.json', data: human }], 'Stale shop edit'));
  assert.equal(await readFile(resolve(root, 'project.json'), 'utf8'), disk);
});

test('MCP uses gameplay semantic validation and cannot partially save broken shop references', async t => {
  const { root } = await fixture(t);
  const workspace = new McpWorkspace(root), baseline = await workspace.readProject();
  const before = await readFile(resolve(root, 'project.json'), 'utf8');
  const next = JSON.parse(before);
  next.gameplay.shops[0].locationId = 'nonexistent-place';
  next.gameplay.shops[0].offers[0].price = 1;
  await assert.rejects(() => workspace.apply(baseline.version, [{ type: 'replace', file: 'project.json', data: next }], 'Invalid shop location'));
  assert.equal(await readFile(resolve(root, 'project.json'), 'utf8'), before);
  const current = await workspace.readProject();
  assert.equal(current.version, baseline.version);
});

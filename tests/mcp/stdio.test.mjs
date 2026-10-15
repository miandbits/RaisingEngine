import test from 'node:test';
import assert from 'node:assert/strict';
import { connect, fixture } from './helpers.mjs';

for (const modern of [false, true]) test(`stdio SDK ${modern ? '2026' : 'legacy'} connects, discovers tools, validates input and executes scenarios`, { timeout: 30000 }, async t => {
  const root = await fixture(); const mcp = await connect(t, root, { modern });
  const tools = await mcp.client.listTools(); assert.equal(tools.tools.length, 18);
  assert.ok(['plugin_status', 'plugin_call', 'plugin_import'].every(name => tools.tools.some(t => t.name === name)));
  assert.ok(tools.tools.find(t => t.name === 'import_sprites').inputSchema.required.includes('expectedVersion'));
  assert.ok(tools.tools.find(t => t.name === 'apply_project_changes').inputSchema.required.includes('expectedVersion'));
  const project = await mcp.call('read_project'); assert.equal(project.ok, true);
  const doc = await mcp.call('read_document', { file: 'activities/study.json', expectedVersion: project.version });
  const invalid = await mcp.client.callTool({ name: 'apply_project_changes', arguments: { edits: [], reason: 'missing version' } });
  assert.equal(invalid.isError, true);
  const saved = await mcp.call('apply_project_changes', { expectedVersion: project.version,
    edits: [{ type: 'replace', file: doc.document.file, data: { ...doc.document.data, label: 'MCP 검술' } }], reason: '대사 스타일 통일' });
  assert.equal(saved.ok, true); assert.equal(saved.isError, false);
  const stale = await mcp.call('validate_project', { expectedVersion: project.version });
  assert.equal(stale.isError, true); assert.equal(stale.error.code, 'VERSION_CONFLICT'); assert.equal(stale.error.actualVersion, saved.version);
  const scenario = await mcp.call('run_scenario', { expectedVersion: saved.version, scenarioId: 'certified-route' });
  assert.equal(scenario.ok, true); assert.equal(scenario.state.endingId, 'certified');
  const explanation = await mcp.call('explain_events', { expectedVersion: saved.version, scenarioId: 'certified-route', afterCommands: 0 });
  assert.equal(explanation.ok, true); assert.ok(explanation.events.length);
  assert.equal(mcp.stderr(), '');
});

test('two independent stdio writers cannot both commit from the same project version', { timeout: 30000 }, async t => {
  const root = await fixture(); const a = await connect(t, root); const b = await connect(t, root);
  const { version } = await a.call('read_project');
  const requests = await Promise.all([['activities/study.json', a], ['activities/work.json', b]].map(async ([file, client], i) => {
    const { document } = await client.call('read_document', { file, expectedVersion: version });
    return { expectedVersion: version, edits: [{ type: 'replace', file, data: { ...document.data, label: `writer-${i}` } }], reason: 'concurrent' };
  }));
  const results = await Promise.all([a.call('apply_project_changes', requests[0]), b.call('apply_project_changes', requests[1])]);
  assert.equal(results.filter(r => r.ok).length, 1);
  assert.ok(['VERSION_CONFLICT', 'PROJECT_BUSY'].includes(results.find(r => !r.ok).error.code));
  const current = await a.call('read_project');
  for (let i = 0; i < 2; i++) {
    const doc = await a.call('read_document', { file: requests[i].edits[0].file, expectedVersion: current.version });
    assert.equal(doc.document.data.label === `writer-${i}`, results[i].ok);
  }
});

test('read-only stdio server refuses writes without changing version', { timeout: 30000 }, async t => {
  const mcp = await connect(t, await fixture(), { readOnly: true }); const { version } = await mcp.call('read_project');
  const reply = await mcp.call('apply_project_changes', { expectedVersion: version, edits: [{ type: 'add', kind: 'activity', id: 'forbidden' }], reason: 'readonly' });
  assert.equal(reply.error.code, 'READ_ONLY'); assert.equal((await mcp.call('read_project')).version, version);
});

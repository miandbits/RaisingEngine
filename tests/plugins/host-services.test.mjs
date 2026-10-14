import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { QtStudioService } from '../../apps/qt-studio/service.mjs';
import { McpWorkspace, toolError } from '../../dist/packages/mcp-service/workspace.js';
import { addCutout, CUTOUT_VERSION, cutoutPackage, envelope, newProject, rigData, serialize, temp } from './host-helpers.mjs';

const RIG = 'plugins/cutout-animation/rig/hero.json';
const conflict = async (promise, code = 'VERSION_CONFLICT') => {
  try { await promise; } catch (error) { assert.equal(error.code ?? toolError(error).error.code, code, error.message); return; }
  assert.fail('expected a conflict');
};

test('Qt service: plugins in every reply, install, undoable enable/disable via the lock document, pluginCall', async () => {
  const root = await newProject(), store = await temp('store');
  const service = new QtStudioService({ pluginStore: store });
  let reply = await service.handle({ method: 'open', params: { root } });
  assert.deepEqual(Object.keys(reply.plugins).sort(), ['engineApi', 'installed', 'project', 'store']);
  assert.equal(reply.plugins.store, store); assert.equal(reply.plugins.engineApi, 1);
  assert.deepEqual(reply.plugins.installed, []); assert.deepEqual(reply.plugins.project, []);
  await assert.rejects(service.handle({ method: 'pluginEnable', params: { id: 'cutout-animation', revision: reply.view.revision } }), e => e.code === 'PLUGIN_MISSING');

  reply = await service.handle({ method: 'pluginInstall', params: { file: await cutoutPackage() } });
  assert.deepEqual([reply.pluginInstalled.id, reply.pluginInstalled.version, reply.pluginInstalled.alreadyInstalled], ['cutout-animation', CUTOUT_VERSION, false]);
  const installed = reply.plugins.installed[0];
  assert.deepEqual(Object.keys(installed).sort(), ['directory', 'documentTypes', 'elementTypes', 'id', 'modules', 'name', 'version']);
  assert.deepEqual(installed.documentTypes.map(t => t.type), ['rig', 'controller']);
  assert.ok(installed.modules.some(m => m.role === 'runtime' && m.platform === 'web-wasm'));

  reply = await service.handle({ method: 'pluginEnable', params: { id: 'cutout-animation', revision: reply.view.revision } });
  assert.equal(reply.view.dirty, true);
  assert.deepEqual(reply.view.documents.find(d => d.file === 'plugins/lock.json').data.plugins, [{ id: 'cutout-animation', version: CUTOUT_VERSION, enabled: true }]);
  const project = reply.plugins.project[0];
  assert.deepEqual(Object.keys(project).sort(), ['authoring', 'diagnostics', 'directory', 'enabled', 'id', 'state', 'version']);
  assert.deepEqual([project.state, project.enabled, project.authoring], ['active', true, null]);
  reply = await service.handle({ method: 'edit', params: { revision: reply.view.revision, edit: { type: 'undo' } } });
  assert.equal(reply.view.documents.some(d => d.kind === 'pluginLock'), false);
  assert.deepEqual(reply.plugins.project, []);
  reply = await service.handle({ method: 'edit', params: { revision: reply.view.revision, edit: { type: 'redo' } } });
  reply = await service.handle({ method: 'pluginDisable', params: { id: 'cutout-animation', revision: reply.view.revision } });
  assert.deepEqual([reply.plugins.project[0].state, reply.plugins.project[0].diagnostics[0].code], ['disabled', 'PLUGIN_DISABLED']);
  reply = await service.handle({ method: 'pluginEnable', params: { id: 'cutout-animation', revision: reply.view.revision } });
  reply = await service.handle({ method: 'save', params: { revision: reply.view.revision } });
  assert.deepEqual(JSON.parse(await readFile(path.join(root, 'plugins/lock.json'), 'utf8')).plugins[0].enabled, true);

  reply = await service.handle({ method: 'pluginCall', params: { plugin: 'cutout-animation', op: 'sample', payload: { rig: rigData(), clip: 'idle', timeMs: 250 } } });
  assert.equal(reply.pluginResult.ok, true);
  assert.equal(reply.pluginResult.pose.bones.body.y, -102);
  // LLM edit path (plugin 1.2.0 ops): a refused edit answers {ok:false} and the module keeps working.
  reply = await service.handle({ method: 'pluginCall', params: { plugin: 'cutout-animation', op: 'edit', payload: { rig: rigData(), operation: { op: 'addSkin', id: 'Bad Id' } } } });
  assert.equal(reply.pluginResult.ok, false); assert.match(reply.pluginResult.error, /스킨 ID/);
  reply = await service.handle({ method: 'pluginCall', params: { plugin: 'cutout-animation', op: 'edit', payload: { rig: rigData(), operation: { op: 'addSkin', id: 'winter', name: '겨울' } } } });
  assert.equal(reply.pluginResult.ok, true); assert.deepEqual(reply.pluginResult.rig.skins, [{ id: 'winter', name: '겨울', attachments: [] }]);
  // IK ops (plugin 1.3.0) on the same path: a one-bone aim with a created target, then a refused chain.
  reply = await service.handle({ method: 'pluginCall', params: { plugin: 'cutout-animation', op: 'edit', payload: { rig: rigData(), operation: { op: 'addIk', bones: ['body'], createTarget: true } } } });
  assert.equal(reply.pluginResult.ok, true, reply.pluginResult.error);
  assert.deepEqual(reply.pluginResult.rig.ik, [{ id: 'body-ik', name: 'body-ik', bones: ['body'], target: 'body-target', bendPositive: true }]);
  reply = await service.handle({ method: 'pluginCall', params: { plugin: 'cutout-animation', op: 'edit', payload: { rig: rigData(), operation: { op: 'addIk', bones: ['root', 'root'], target: 'body' } } } });
  assert.equal(reply.pluginResult.ok, false); assert.match(reply.pluginResult.error, /부모 → 자식/);
  // Mesh ops (plugin 1.4.0): convert, bind with auto weights and a deform key on the same path.
  reply = await service.handle({ method: 'pluginCall', params: { plugin: 'cutout-animation', op: 'edit', payload: { rig: rigData(), operation: { op: 'batch', operations: [
    { op: 'convertToMesh', slot: 'torso', rows: 2, cols: 1 }, { op: 'bindMesh', slot: 'torso', bones: ['root', 'body'] },
    { op: 'setKey', clip: 'idle', target: 'slot:torso', property: 'deform', attachment: 'torso', timeMs: 500, offsets: [1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0] }] } } } });
  assert.equal(reply.pluginResult.ok, true, reply.pluginResult.error);
  const torso = reply.pluginResult.rig.attachments[0];
  assert.deepEqual([torso.type, torso.vertices.length, torso.triangles.length, torso.weights.length, torso.hull], ['mesh', 12, 12, 6, 6]);
  assert.deepEqual(reply.pluginResult.diagnostics.filter(d => d.severity === 'error'), []);
  assert.equal(reply.pluginResult.rig.clips[0].tracks.at(-1).attachment, 'torso');
  reply = await service.handle({ method: 'pluginCall', params: { plugin: 'cutout-animation', op: 'edit', payload: { rig: rigData(), operation: { op: 'bindMesh', slot: 'torso', bones: ['body'] } } } });
  assert.equal(reply.pluginResult.ok, false); assert.match(reply.pluginResult.error, /메시가 아닌/);
  reply = await service.handle({ method: 'pluginStatus', params: {} });
  assert.equal(reply.plugins.project[0].state, 'active');
});

test('Qt service: stale revision and external change of an extension document are conflicts', async () => {
  const root = await newProject(), store = await temp('store');
  await addCutout(root);
  const service = new QtStudioService({ pluginStore: store });
  await service.handle({ method: 'pluginInstall', params: { file: await cutoutPackage() } });
  let reply = await service.handle({ method: 'open', params: { root } });
  const stale = reply.view.revision;
  const rig = structuredClone(reply.view.documents.find(d => d.file === RIG).data); rig.data.name = 'Qt draft';
  reply = await service.handle({ method: 'edit', params: { revision: stale, edit: { type: 'replace', file: RIG, data: rig } } });
  await assert.rejects(service.handle({ method: 'edit', params: { revision: stale, edit: { type: 'replace', file: RIG, data: rig } } }), /갱신/);
  const external = envelope('rig', 'hero', { ...rigData(), name: 'external' });
  await writeFile(path.join(root, RIG), serialize(external));
  await conflict(service.handle({ method: 'save', params: { revision: reply.view.revision } }));
  assert.equal(JSON.parse(await readFile(path.join(root, RIG), 'utf8')).data.name, 'external');
  assert.equal(service.editor.documents.find(d => d.file === RIG).data.data.name, 'Qt draft', 'draft kept after the conflict');
});

test('MCP: create through apply_project_changes, schema/status/call tools, version conflicts on extension documents', async () => {
  const root = await newProject(), store = await temp('store');
  await addCutout(root, { rig: null, controller: null });
  const qt = new QtStudioService({ pluginStore: store });
  await qt.handle({ method: 'pluginInstall', params: { file: await cutoutPackage() } });
  const mcp = new McpWorkspace(root, false, undefined, store);
  let { version } = await mcp.readProject();
  const created = await mcp.apply(version, [{ type: 'create', file: RIG, data: envelope('rig', 'hero', rigData()) }], 'add rig');
  assert.deepEqual(created.changes.map(c => [c.file, c.before]), [[RIG, null]]);
  assert.equal(await readFile(path.join(root, RIG), 'utf8'), serialize(envelope('rig', 'hero', rigData())));
  await conflict(mcp.apply(version, [{ type: 'delete', file: RIG }], 'stale'));
  version = created.version;
  await conflict(mcp.apply(version, [{ type: 'create', file: RIG, data: envelope('rig', 'hero', rigData()) }], 'dup'), 'CHANGE_REJECTED');

  const schema = await mcp.readSchema('extension', 'cutout-animation', 'rig');
  assert.equal(schema.version, CUTOUT_VERSION); assert.match(schema.schema.title, /rig/i);
  assert.equal((await mcp.readSchema('extension')).schema.title, 'Extension document');
  assert.equal((await mcp.readSchema('plugin-lock')).schema.title, 'Plugin lock');
  const status = await mcp.pluginStatus();
  assert.deepEqual(status.project.map(p => [p.id, p.state]), [['cutout-animation', 'active']]);
  const called = await mcp.pluginCall(version, 'cutout-animation', 'validate', { rig: { ...rigData(), bones: [{ id: 'a', parent: 'a' }] }, rigPointer: '/data' });
  assert.ok(called.result.diagnostics.some(d => d.code === 'ANIM_PARENT_CYCLE' && d.pointer === '/data/bones/0/parent'));
  await conflict(mcp.pluginCall(version, 'not-locked', 'sample', {}), 'PLUGIN_NOT_LOCKED');

  // An external edit of the extension document invalidates the version for reads and writes.
  await writeFile(path.join(root, RIG), serialize(envelope('rig', 'hero', { ...rigData(), name: 'external' })));
  await conflict(mcp.readDocument(RIG, version));
  await conflict(mcp.apply(version, [{ type: 'delete', file: RIG }], 'stale after external'));
  const fresh = await mcp.readProject();
  assert.notEqual(fresh.version, version);
  assert.ok(fresh.documents.some(d => d.file === RIG && d.kind === 'extension'));
});

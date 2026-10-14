// The Qt plugin host (native/studio/plugin-host.cpp) codes against these reply
// shapes; this drives the real Qt service end to end with a throwaway store.
import test from 'node:test';
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { mkdir, mkdtemp, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { QtStudioService } from '../../apps/qt-studio/service.mjs';
import { writeZip } from '../../dist/packages/plugin-host/index.js';

const sha = data => createHash('sha256').update(data).digest('hex');
async function probePackage(directory) {
  const files = { 'schema/controller.schema.json': Buffer.from(JSON.stringify({ type: 'object' })),
    // Not a real DLL: the editor must reject it from metadata and keep working.
    'authoring/windows/probe-studio.dll': Buffer.from('not a library') };
  const manifest = { manifestVersion: 1, id: 'host-probe', name: '호스트 점검', version: '1.0.0', engineApi: { min: 1, max: 1 },
    documentTypes: [{ type: 'controller', label: '연결', schema: 'schema/controller.schema.json' }],
    elementTypes: [{ type: 'character', label: '애니메이션 캐릭터', refType: 'controller' }],
    modules: [{ role: 'authoring', platform: 'windows-qt6.10-mingw64', path: 'authoring/windows/probe-studio.dll' }],
    files: Object.fromEntries(Object.entries(files).map(([name, data]) => [name, sha(data)])) };
  const file = path.join(directory, 'host-probe.raiseplugin');
  await writeFile(file, writeZip([{ name: 'plugin.json', data: Buffer.from(JSON.stringify(manifest)) },
    ...Object.entries(files).map(([name, data]) => ({ name, data }))]));
  return file;
}

test('Qt plugin host reply contract: status, install, enable, documents, disable', async () => {
  await mkdir(path.resolve('.cache/qt-ui'), { recursive: true });
  const work = await mkdtemp(path.resolve('.cache/qt-ui/plugin-host-'));
  const store = path.join(work, 'store'), root = path.join(work, 'project');
  const service = new QtStudioService({ pluginStore: store });
  let reply = await service.handle({ method: 'create', params: { root } });
  let revision = reply.view.revision;
  const call = async (method, params = {}) => { reply = await service.handle({ method, params: { revision, ...params } }); revision = reply.view.revision; return reply; };

  await call('pluginStatus');
  assert.equal(reply.plugins.engineApi, 1);
  assert.equal(typeof reply.plugins.store, 'string');
  assert.deepEqual(reply.plugins.project, []);
  assert.deepEqual(reply.plugins.installed, []);

  await call('pluginInstall', { file: await probePackage(work) });
  assert.deepEqual([reply.pluginInstalled.id, reply.pluginInstalled.version], ['host-probe', '1.0.0']);
  const installed = reply.plugins.installed.find(p => p.id === 'host-probe');
  assert.equal(installed.name, '호스트 점검');
  assert.ok(path.isAbsolute(installed.directory));
  assert.deepEqual(installed.elementTypes, [{ type: 'character', label: '애니메이션 캐릭터', refType: 'controller' }]);

  await call('pluginEnable', { id: 'host-probe' });
  let entry = reply.plugins.project.find(p => p.id === 'host-probe');
  assert.equal(entry.state, 'active');
  assert.equal(entry.version, '1.0.0');
  assert.ok(path.isAbsolute(entry.authoring) && entry.authoring.endsWith('probe-studio.dll'), entry.authoring);
  assert.ok(reply.view.documents.some(d => d.kind === 'pluginLock' && d.file === 'plugins/lock.json'));

  const envelope = { formatVersion: '0.1.0', plugin: 'host-probe', type: 'controller', id: 'hero-main', data: { name: 'Hero' } };
  await call('edit', { edit: { type: 'create', file: 'plugins/host-probe/controller/hero-main.json', data: envelope } });
  assert.equal(reply.error, null);
  const doc = reply.view.documents.find(d => d.file === 'plugins/host-probe/controller/hero-main.json');
  assert.equal(doc.kind, 'extension');
  assert.deepEqual([doc.data.plugin, doc.data.type, doc.data.id], ['host-probe', 'controller', 'hero-main']);

  // The element the palette entry creates (StudioWindow::addPluginElement).
  const project = reply.view.documents.find(d => d.kind === 'project');
  const data = structuredClone(project.data);
  const screen = data.visual.screens.find(s => s.id === 'home') ?? data.visual.screens[0];
  screen.elements.push({ id: 'character-1', kind: 'plugin', label: '애니메이션 캐릭터', x: 80, y: 100, width: 300, height: 450,
    extension: { plugin: 'host-probe', type: 'character', ref: 'hero-main', props: {} } });
  await call('edit', { edit: { type: 'replace', file: project.file, data } });
  assert.equal(reply.error, null);
  const elementIndex = screen.elements.length - 1;
  const schemaErrors = reply.view.diagnostics.filter(d => d.severity === 'error' && d.pointer?.includes(`/elements/${elementIndex}`));
  assert.deepEqual(schemaErrors, []);

  await call('pluginDisable', { id: 'host-probe' });
  entry = reply.plugins.project.find(p => p.id === 'host-probe');
  assert.equal(entry.state, 'disabled');
  assert.equal(entry.authoring, null);
  assert.ok(reply.view.documents.some(d => d.kind === 'extension'), 'disabling keeps plugin documents');

  // The stdio wrapper attaches service.plugins() to error replies too, so the editor keeps its state.
  await assert.rejects(service.handle({ method: 'pluginEnable', params: { revision, id: 'not-installed' } }));
  assert.equal((await service.plugins()).project.find(p => p.id === 'host-probe').state, 'disabled');
});

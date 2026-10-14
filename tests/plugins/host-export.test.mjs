import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdir, readFile, readdir, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { installPackage } from '../../dist/packages/plugin-host/index.js';
import { StudioEditor } from '../../dist/packages/studio-service/editor.js';
import { exportGame } from '../../dist/packages/export-service/export.js';
import { createSession } from '../../dist/packages/runtime-service/session.js';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';
import { QtStudioService } from '../../apps/qt-studio/service.mjs';
import { addCutout, addElement, cutoutPackage, fakeRuntimes, lock, makePackage, newProject, serialize, sha256, temp, CUTOUT_VERSION } from './host-helpers.mjs';

let installedStore;
const store = () => installedStore ??= (async () => { const dir = await temp('store'); await installPackage(dir, await cutoutPackage()); return dir; })();
async function prepare(root, pluginStore) {
  const editor = new StudioEditor({ pluginStore }); await editor.open(root);
  const built = await editor.snapshot(editor.revision);
  assert.equal(built.ok, true, JSON.stringify(built.diagnostics?.filter(d => d.severity === 'error')));
  return { editor, content: built.snapshot };
}
const files = async dir => (await readdir(dir, { recursive: true, withFileTypes: true })).filter(e => e.isFile()).map(e => path.relative(dir, path.join(e.parentPath, e.name)).replaceAll('\\', '/')).sort();
const code = async (promise, expected) => assert.rejects(promise, e => { assert.equal(e.code, expected, e.message); return true; });

test('plugin-free projects export byte-identically with no extensions key and no plugins folder', async () => {
  const root = await newProject(), runtimes = await fakeRuntimes([]), out = await temp('export');
  const { editor, content } = await prepare(root, await store());
  const extensions = await editor.exportExtensions(editor.revision, content, 'web');
  assert.equal(extensions, null);
  const a = await exportGame({ content, projectRoot: root, destination: path.join(out, 'a'), target: 'web', runtimes, extensions });
  const b = await exportGame({ content, projectRoot: root, destination: path.join(out, 'b'), target: 'web', runtimes });
  const gameA = await readFile(path.join(a.directory, 'game.json')), gameB = await readFile(path.join(b.directory, 'game.json'));
  assert.deepEqual(gameA, gameB);
  assert.equal(Object.hasOwn(JSON.parse(gameA), 'extensions'), false);
  assert.equal((await files(a.directory)).some(f => f.startsWith('plugins/')), false);
});

test('export copies only the target runtime module and embeds plugin documents in game.json', async () => {
  const root = await newProject(), runtimes = await fakeRuntimes(), out = await temp('export'), pluginStore = await store();
  await addCutout(root); await addElement(root);
  const { editor, content } = await prepare(root, pluginStore);
  for (const [target, expected, entry] of [['windows', ['plugins/cutout-animation/cutout-runtime.dll'], 'cutout-runtime.dll'],
    ['web', ['plugins/cutout-animation/cutout-runtime.mjs', 'plugins/cutout-animation/cutout-runtime.wasm'], 'cutout-runtime.mjs']]) {
    const extensions = await editor.exportExtensions(editor.revision, content, target);
    const result = await exportGame({ content, projectRoot: root, destination: path.join(out, target), target, runtimes, extensions });
    const listed = await files(result.directory);
    assert.deepEqual(listed.filter(f => f.startsWith('plugins/')), expected);
    assert.equal(listed.some(f => /service|authoring|index\.mjs|schema\.json|contract\.md/.test(f)), false, listed.join('\n'));
    const game = JSON.parse(await readFile(path.join(result.directory, 'game.json'), 'utf8'));
    assert.deepEqual(game.extensions.plugins.map(p => [p.id, p.version, p.abi, p.entry]), [['cutout-animation', CUTOUT_VERSION, 'raising-extension-1', `plugins/cutout-animation/${entry}`]]);
    assert.deepEqual(game.extensions.plugins[0].documents.map(d => `${d.type}/${d.id}`), ['controller/hero-main', 'rig/hero']);
    assert.equal(game.extensions.version, 1);
    const installed = path.join(pluginStore, `installed/cutout-animation/${CUTOUT_VERSION}/runtime`, target === 'web' ? 'web' : 'windows', entry);
    assert.equal(sha256(await readFile(path.join(result.directory, expected[0]))), sha256(await readFile(installed)));
    assert.equal(game.content.contentFingerprint, content.contentFingerprint);
  }
});

test('missing feature, disabled plugin, absent runtime module and unprepared extensions refuse the export', async () => {
  const root = await newProject(), out = await temp('export'), pluginStore = await store();
  await addCutout(root); await addElement(root);
  let { editor, content } = await prepare(root, pluginStore);
  const extensions = await editor.exportExtensions(editor.revision, content, 'web');
  await assert.rejects(exportGame({ content, projectRoot: root, destination: path.join(out, 'nofeature'), target: 'web', runtimes: await fakeRuntimes([]), extensions }), /plugin-elements-v1/);
  await code(exportGame({ content, projectRoot: root, destination: path.join(out, 'unprepared'), target: 'web', runtimes: await fakeRuntimes() }), 'PLUGIN_RUNTIME_MISSING');

  await writeFile(path.join(root, 'plugins/lock.json'), serialize(lock([{ id: 'cutout-animation', version: CUTOUT_VERSION, enabled: false }])));
  ({ editor, content } = await prepare(root, pluginStore));
  await code(editor.exportExtensions(editor.revision, content, 'windows'), 'PLUGIN_RUNTIME_MISSING');
  // The Qt export path refuses with the same code (warnings alone never block validation).
  const qt = new QtStudioService({ pluginStore, runtimes: await fakeRuntimes() });
  const opened = await qt.handle({ method: 'open', params: { root } });
  await code(qt.handle({ method: 'export', params: { revision: opened.view.revision, target: 'web', destination: path.join(out, 'qt') } }), 'PLUGIN_RUNTIME_MISSING');
  assert.equal((await readdir(out)).includes('qt'), false);

  // A plugin that ships only a web runtime cannot be exported to Windows; its authoring and
  // service modules never reach an export, and its node service validates like cutout's.
  const webOnly = await temp('webonly'), pkgDir = await temp('pkg');
  const element = { type: 'figure', label: '그림', refType: 'thing' };
  const service = 'export async function createService({ manifest }) { return { validate: ({ documents }) => documents.filter(d => !d.data.ok).map(d => ({ file: d.file, pointer: "/data", code: "THING_NOT_OK", severity: "warning", message: manifest.id })) }; }';
  await installPackage(webOnly, await makePackage(pkgDir, { id: 'web-only', elementTypes: [element],
    modules: [{ role: 'runtime', platform: 'web-wasm', abi: 'raising-extension-1', path: 'runtime/web/x.mjs' },
      { role: 'authoring', platform: 'windows-qt6.10-mingw64', path: 'authoring/windows/x-studio.dll' }, { role: 'service', platform: 'node', path: 'service/index.mjs' }] },
    { 'schema/thing.schema.json': '{"type":"object"}', 'runtime/web/x.mjs': 'export default 1;', 'authoring/windows/x-studio.dll': 'MZ', 'service/index.mjs': service }));
  const other = await newProject();
  await addCutout(other, { lockData: lock([{ id: 'web-only', version: '1.0.0', enabled: true }]), rig: null, controller: null, image: null });
  await mkdir(path.join(other, 'plugins/web-only/thing'), { recursive: true });
  await writeFile(path.join(other, 'plugins/web-only/thing/a.json'), serialize({ formatVersion: '0.1.0', plugin: 'web-only', type: 'thing', id: 'a', data: {} }));
  await addElement(other, { plugin: 'web-only', type: 'figure', ref: 'a' });
  ({ editor, content } = await prepare(other, webOnly));
  assert.deepEqual(editor.diagnostics.filter(d => d.code === 'THING_NOT_OK').map(d => [d.file, d.pointer, d.message]), [['plugins/web-only/thing/a.json', '/data', 'web-only']]);
  const status = await editor.plugins();
  assert.equal(status.project[0].authoring, path.join(webOnly, 'installed/web-only/1.0.0/authoring/windows/x-studio.dll'));
  const prepared = await editor.exportExtensions(editor.revision, content, 'web');
  assert.deepEqual(prepared.files.map(f => f.target), ['plugins/web-only/x.mjs']);
  const result = await exportGame({ content, projectRoot: other, destination: path.join(out, 'web-only'), target: 'web', runtimes: await fakeRuntimes(), extensions: prepared });
  assert.deepEqual((await files(result.directory)).filter(f => f.startsWith('plugins/')), ['plugins/web-only/x.mjs']);
  await code(editor.exportExtensions(editor.revision, content, 'windows'), 'PLUGIN_RUNTIME_MISSING');
});

test('save envelope carries opaque playerPresentation.extensions (unknown plugins too) and caps its size', async () => {
  const root = await newProject();
  const { content } = await prepare(root, await temp('empty'));
  const created = createSession(content, 7);
  assert.equal(created.ok, true);
  const extensions = { 'cutout-animation': { version: 1, instances: { 'hero-on-title': { ref: 'hero-main', state: { tracks: [] } } } }, 'not-installed': { any: ['thing', 1, null] } };
  const encoded = await encodeSave(content, created.session, { version: 1, timeline: null, extensions });
  assert.equal(encoded.ok, true, JSON.stringify(encoded.error));
  const decoded = await decodeSave(content, encoded.text);
  assert.equal(decoded.ok, true, JSON.stringify(decoded.error));
  assert.deepEqual(decoded.playerPresentation.extensions, extensions);
  const again = await encodeSave(content, decoded.session, decoded.playerPresentation);
  assert.equal(again.text, encoded.text);
  const plain = await encodeSave(content, created.session, { version: 1, timeline: null });
  assert.equal(plain.text.includes('extensions'), false);
  const huge = await encodeSave(content, created.session, { version: 1, timeline: null, extensions: { 'cutout-animation': { blob: 'x'.repeat(1024 * 1024 + 1) } } });
  assert.equal(huge.ok, false); assert.equal(huge.error.code, 'SAVE_PRESENTATION');
  const badKey = await decodeSave(content, encoded.text.replace('"not-installed"', '"Bad Id"'));
  assert.equal(badKey.ok, false); assert.equal(badKey.error.code, 'SAVE_SCHEMA');
});

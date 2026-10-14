import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdir, readFile, readdir, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { crc32 } from 'node:zlib';
import { installPackage, inspectPackage, listInstalled, readRegistry, uninstallPlugin, resolveProjectPlugins, readZip, writeZip, satisfies, ENGINE_PLUGIN_API } from '../../dist/packages/plugin-host/index.js';
import { cutoutPackage, makePackage, temp, sha256, readJson } from './host-helpers.mjs';

const rejects = (promise, code) => assert.rejects(promise, error => { assert.equal(error.code, code, error.message); return true; });

test('cutout package installs into a staged, immutable store; reinstall of the same bytes is idempotent', async () => {
  const store = await temp('store'), file = await cutoutPackage();
  const inspected = await inspectPackage(file);
  assert.equal(inspected.manifest.id, 'cutout-animation');
  assert.ok(Object.keys(inspected.manifest.files).includes('runtime/web/cutout-runtime.wasm'));
  const first = await installPackage(store, file);
  assert.equal(first.alreadyInstalled, false);
  assert.equal(first.plugin.directory, path.join(store, 'installed/cutout-animation/1.0.0'));
  for (const [name, hash] of Object.entries(first.plugin.manifest.files)) assert.equal(sha256(await readFile(path.join(first.plugin.directory, name))), hash, name);
  const registry = await readRegistry(store);
  assert.deepEqual(registry.installed.map(e => [e.id, e.version, e.source, e.sha256]), [['cutout-animation', '1.0.0', 'local', sha256(await readFile(file))]]);
  assert.equal((await readdir(store)).filter(n => n.startsWith('.staging-')).length, 0);
  const again = await installPackage(store, file);
  assert.equal(again.alreadyInstalled, true);
  assert.equal((await readRegistry(store)).installed.length, 1);
  assert.deepEqual((await listInstalled(store)).map(p => p.id), ['cutout-animation']);
});

test('same id+version with different bytes is PLUGIN_VERSION_CONFLICT and leaves the installed files untouched', async () => {
  const store = await temp('store'), dir = await temp('pkg');
  const a = await makePackage(dir, {}, { 'schema/thing.schema.json': '{"type":"object"}' });
  const b = await makePackage(dir, {}, { 'schema/thing.schema.json': '{"type":"object","title":"changed"}' });
  await installPackage(store, a);
  await rejects(installPackage(store, b), 'PLUGIN_VERSION_CONFLICT');
  assert.equal(await readFile(path.join(store, 'installed/test-plugin/1.0.0/schema/thing.schema.json'), 'utf8'), '{"type":"object"}');
});

test('hash mismatch, unlisted and missing files are rejected before anything is written', async () => {
  const store = await temp('store'), dir = await temp('pkg');
  await rejects(installPackage(store, await makePackage(dir, {}, undefined, { tamper: m => { m.files['schema/thing.schema.json'] = '0'.repeat(64); } })), 'PLUGIN_FILE_HASH');
  await rejects(installPackage(store, await makePackage(dir, {}, undefined, { tamper: (m, entries) => entries.push({ name: 'extra.txt', data: Buffer.from('x') }) })), 'PLUGIN_FILE_UNLISTED');
  await rejects(installPackage(store, await makePackage(dir, {}, undefined, { tamper: m => { m.files['missing.txt'] = '0'.repeat(64); } })), 'PLUGIN_FILE_MISSING');
  assert.deepEqual((await readRegistry(store)).installed, []);
  assert.deepEqual((await readdir(store).catch(() => [])).filter(n => n !== '.lock' && n !== 'registry.json'), []);
});

test('ZIP reader rejects traversal, absolute, drive, duplicate, symlink and oversize entries', async () => {
  const ok = { name: 'plugin.json', data: Buffer.from('{}') };
  for (const name of ['../evil.txt', 'a/../../evil.txt', '/abs.txt', 'C:/win.txt', 'a\\b.txt', 'con.txt', 'a/./b'])
    assert.throws(() => readZip(writeZipUnchecked([ok, { name, data: Buffer.from('x') }])), e => e.code === 'PLUGIN_PACKAGE_PATH', name);
  assert.throws(() => readZip(writeZipUnchecked([ok, { name: 'A.txt', data: Buffer.from('1') }, { name: 'a.txt', data: Buffer.from('2') }])), e => e.code === 'PLUGIN_PACKAGE_PATH');
  // Unix "made by" + S_IFLNK in the external attributes = symlink entry.
  const link = writeZip([ok, { name: 'link', data: Buffer.from('/etc/passwd') }]);
  patchCentral(link, 1, (buf, at) => { buf.writeUInt16LE(0x0314, at + 4); buf.writeUInt32LE((0o120777 << 16) >>> 0, at + 38); });
  assert.throws(() => readZip(link), e => e.code === 'PLUGIN_PACKAGE_PATH' && /심볼릭/.test(e.message));
  // A lying declared size larger than the 64 MiB unpacked limit is refused before inflating.
  const big = writeZip([ok, { name: 'big.bin', data: Buffer.alloc(1000) }]);
  patchCentral(big, 1, (buf, at) => buf.writeUInt32LE(65 * 1024 * 1024, at + 24));
  assert.throws(() => readZip(big), e => e.code === 'PLUGIN_PACKAGE_LIMIT');
  const crc = writeZip([ok, { name: 'x.bin', data: Buffer.from('abc') }]);
  patchCentral(crc, 1, (buf, at) => buf.writeUInt32LE(1, at + 16));
  assert.throws(() => readZip(crc), e => e.code === 'PLUGIN_PACKAGE_CRC');
  assert.throws(() => readZip(Buffer.from('not a zip at all, definitely not')), e => e.code === 'PLUGIN_PACKAGE_FORMAT');
  // The installer surfaces the same errors for a package file.
  const store = await temp('store'), dir = await temp('pkg'), evil = path.join(dir, 'evil.raiseplugin');
  await writeFile(evil, writeZipUnchecked([ok, { name: '../escape.txt', data: Buffer.from('x') }]));
  await rejects(installPackage(store, evil), 'PLUGIN_PACKAGE_PATH');
  await assert.rejects(readFile(path.join(store, '..', 'escape.txt')));
});

test('engineApi out of range: install refused; an incompatible installed version resolves PLUGIN_API_INCOMPATIBLE', async () => {
  const store = await temp('store'), dir = await temp('pkg');
  await rejects(installPackage(store, await makePackage(dir, { engineApi: { min: ENGINE_PLUGIN_API + 1, max: ENGINE_PLUGIN_API + 1 } })), 'PLUGIN_API_INCOMPATIBLE');
  // A store shared with another engine build can hold such a version.
  const installed = path.join(store, 'installed/test-plugin/2.0.0');
  await mkdir(installed, { recursive: true });
  await writeFile(path.join(installed, 'plugin.json'), JSON.stringify({ manifestVersion: 1, id: 'test-plugin', name: 't', version: '2.0.0', engineApi: { min: 9, max: 9 }, documentTypes: [], elementTypes: [], modules: [], files: {} }));
  await writeFile(path.join(store, 'registry.json'), JSON.stringify({ version: 1, installed: [{ id: 'test-plugin', version: '2.0.0', sha256: '0'.repeat(64), source: 'local', installedAt: new Date().toISOString() }] }));
  const [resolved] = await resolveProjectPlugins(store, { formatVersion: '0.1.0', plugins: [{ id: 'test-plugin', version: '2.0.0', enabled: true }] });
  assert.equal(resolved.state, 'incompatible');
  assert.deepEqual(resolved.diagnostics.map(d => [d.code, d.severity]), [['PLUGIN_API_INCOMPATIBLE', 'error']]);
});

test('dependencies resolve against the project pins (missing, out of range, satisfied)', async () => {
  const store = await temp('store'), dir = await temp('pkg');
  await installPackage(store, await makePackage(dir, { id: 'base', version: '1.4.2' }));
  await installPackage(store, await makePackage(dir, { id: 'addon', dependencies: [{ id: 'base', range: '^1.2.0' }] }));
  const resolve = plugins => resolveProjectPlugins(store, { formatVersion: '0.1.0', plugins });
  let result = await resolve([{ id: 'addon', version: '1.0.0', enabled: true }]);
  assert.equal(result[0].state, 'incompatible');
  assert.equal(result[0].diagnostics[0].code, 'PLUGIN_DEPENDENCY');
  result = await resolve([{ id: 'addon', version: '1.0.0', enabled: true }, { id: 'base', version: '1.4.2', enabled: false }]);
  assert.equal(result.find(p => p.id === 'addon').diagnostics.at(-1).code, 'PLUGIN_DEPENDENCY');
  result = await resolve([{ id: 'addon', version: '1.0.0', enabled: true }, { id: 'base', version: '1.4.2', enabled: true }]);
  assert.deepEqual(result.map(p => p.state), ['active', 'active']);
  assert.ok(satisfies('1.9.0', '^1.2.0') && !satisfies('2.0.0', '^1.2.0') && satisfies('1.2.9', '~1.2.0') && !satisfies('1.3.0', '~1.2.0') && !satisfies('0.3.0', '^0.2.0'));
});

test('lock pins one of two installed versions; uninstall makes it missing', async () => {
  const store = await temp('store'), dir = await temp('pkg');
  await installPackage(store, await makePackage(dir, { version: '1.0.0', name: 'v1' }));
  await installPackage(store, await makePackage(dir, { version: '1.1.0', name: 'v11' }));
  assert.deepEqual((await listInstalled(store)).map(p => p.version), ['1.0.0', '1.1.0']);
  const lock = { formatVersion: '0.1.0', plugins: [{ id: 'test-plugin', version: '1.0.0', enabled: true }] };
  const [pinned] = await resolveProjectPlugins(store, lock);
  assert.equal(pinned.state, 'active');
  assert.equal(pinned.manifest.name, 'v1');
  assert.equal(pinned.directory, path.join(store, 'installed/test-plugin/1.0.0'));
  assert.equal(await uninstallPlugin(store, 'test-plugin', '1.0.0'), true);
  const [missing] = await resolveProjectPlugins(store, lock);
  assert.deepEqual([missing.state, missing.diagnostics[0].code, missing.diagnostics[0].severity], ['missing', 'PLUGIN_MISSING', 'warning']);
  assert.deepEqual((await readJson(path.join(store, 'registry.json'))).installed.map(e => e.version), ['1.1.0']);
});

/** writeZip refuses unsafe names itself; a stored-only builder produces hostile archives for the reader. */
function writeZipUnchecked(entries) {
  const locals = [], central = [];
  let offset = 0;
  for (const { name, data } of entries) {
    const n = Buffer.from(name), crc = crc32(data) >>> 0, h = Buffer.alloc(30), c = Buffer.alloc(46);
    h.writeUInt32LE(0x04034b50, 0); h.writeUInt16LE(20, 4); h.writeUInt32LE(crc, 14); h.writeUInt32LE(data.length, 18); h.writeUInt32LE(data.length, 22); h.writeUInt16LE(n.length, 26);
    c.writeUInt32LE(0x02014b50, 0); c.writeUInt16LE(20, 4); c.writeUInt16LE(20, 6); c.writeUInt32LE(crc, 16); c.writeUInt32LE(data.length, 20); c.writeUInt32LE(data.length, 24);
    c.writeUInt16LE(n.length, 28); c.writeUInt32LE(offset, 42);
    locals.push(h, n, data); central.push(c, n); offset += 30 + n.length + data.length;
  }
  const dir = Buffer.concat(central), end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0); end.writeUInt16LE(entries.length, 8); end.writeUInt16LE(entries.length, 10); end.writeUInt32LE(dir.length, 12); end.writeUInt32LE(offset, 16);
  return Buffer.concat([...locals, dir, end]);
}
function patchCentral(zip, index, patch) {
  let at = zip.lastIndexOf(Buffer.from([0x50, 0x4b, 0x05, 0x06]));
  at = zip.readUInt32LE(at + 16);
  for (let i = 0; i < index; i++) at += 46 + zip.readUInt16LE(at + 28) + zip.readUInt16LE(at + 30) + zip.readUInt16LE(at + 32);
  patch(zip, at);
}

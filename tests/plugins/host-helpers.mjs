// Shared fixtures for tests/plugins/host-*.test.mjs (temp dirs under .cache/anim-tests).
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { readFileSync } from 'node:fs';
import { mkdir, mkdtemp, readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { crc32, deflateSync } from 'node:zlib';
import { writeZip } from '../../dist/packages/plugin-host/index.js';
import { createTemplateProject } from '../../dist/packages/studio-service/templates.js';

export const engineRoot = fileURLToPath(new URL('../../', import.meta.url));
export const cache = path.join(engineRoot, '.cache/anim-tests');
export const temp = async prefix => { await mkdir(cache, { recursive: true }); return mkdtemp(path.join(cache, `${prefix}-`)); };
export const sha256 = bytes => createHash('sha256').update(bytes).digest('hex');
export const readJson = async file => JSON.parse(await readFile(file, 'utf8'));
export const serialize = data => JSON.stringify(data, null, 2) + '\n';
/** Version of the real cutout plugin (plugin.json): projects pin exact versions, so tests pin this one. */
export const CUTOUT_VERSION = JSON.parse(readFileSync(path.join(engineRoot, 'plugins/cutout-animation/plugin.json'), 'utf8')).version;

let cutout;
/** The real cutout package, built once per process from dist/cutout artifacts. */
export function cutoutPackage() {
  return cutout ??= (async () => {
    const out = await temp('package');
    const r = spawnSync(process.execPath, [path.join(engineRoot, 'plugins/cutout-animation/tools/package.mjs'), '--out', out, '--no-authoring'], { encoding: 'utf8' });
    if (r.status !== 0) throw new Error(`package.mjs failed: ${r.stderr || r.stdout}`);
    return JSON.parse(r.stdout.trim().split('\n').at(-1)).package;
  })();
}

/** Synthetic plugin package: manifest fields + files (path → string|Buffer); files hashes computed unless `rawFiles`. */
export async function makePackage(directory, overrides = {}, files = { 'schema/thing.schema.json': '{"type":"object"}' }, { tamper } = {}) {
  const entries = Object.entries(files).map(([name, data]) => ({ name, data: Buffer.from(data) }));
  const manifest = { manifestVersion: 1, id: 'test-plugin', name: '테스트 플러그인', version: '1.0.0', engineApi: { min: 1, max: 1 },
    documentTypes: [{ type: 'thing', label: '물건', schema: 'schema/thing.schema.json' }], elementTypes: [], modules: [],
    files: Object.fromEntries(entries.map(e => [e.name, sha256(e.data)])), ...overrides };
  tamper?.(manifest, entries);
  const file = path.join(directory, `${manifest.id}-${manifest.version}-${Math.random().toString(36).slice(2)}.raiseplugin`);
  await writeFile(file, writeZip([{ name: 'plugin.json', data: Buffer.from(JSON.stringify(manifest)) }, ...entries]));
  return file;
}

export async function newProject(template = 'academy-visual') {
  const root = path.join(await temp('project'), 'game');
  await createTemplateProject(path.join(engineRoot, 'examples'), template, root);
  return root;
}

/** RGBA PNG of the given size (single transparent color). */
export function png(width, height) {
  const chunk = (type, data) => { const body = Buffer.concat([Buffer.from(type, 'ascii'), data]); const out = Buffer.alloc(12 + data.length);
    out.writeUInt32BE(data.length, 0); body.copy(out, 4); out.writeUInt32BE(crc32(body) >>> 0, 8 + data.length); return out; };
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(width, 0); ihdr.writeUInt32BE(height, 4); ihdr[8] = 8; ihdr[9] = 6;
  const raw = Buffer.alloc((width * 4 + 1) * height);
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
}

/** RGBA PNG whose alpha is alphaAt(x, y) (0..255), white: shapes for alpha tracing (plugin 1.6.0). */
export function shapePng(width, height, alphaAt) {
  const chunk = (type, data) => { const body = Buffer.concat([Buffer.from(type, 'ascii'), data]); const out = Buffer.alloc(12 + data.length);
    out.writeUInt32BE(data.length, 0); body.copy(out, 4); out.writeUInt32BE(crc32(body) >>> 0, 8 + data.length); return out; };
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(width, 0); ihdr.writeUInt32BE(height, 4); ihdr[8] = 8; ihdr[9] = 6;
  const stride = width * 4, raw = Buffer.alloc((stride + 1) * height);
  let prev = Buffer.alloc(stride);
  for (let y = 0; y < height; y++) {
    const row = Buffer.alloc(stride), at = y * (stride + 1);
    for (let x = 0; x < width; x++) row.set([255, 255, 255, alphaAt(x, y)], x * 4);
    // Odd rows are Paeth-filtered (filter 4) so the service's decoder runs a real predictor.
    raw[at] = y % 2 ? 4 : 0;
    for (let i = 0; i < stride; i++) {
      const a = i >= 4 ? row[i - 4] : 0, b = prev[i], c = i >= 4 ? prev[i - 4] : 0;
      const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
      raw[at + 1 + i] = y % 2 ? (row[i] - (pa <= pb && pa <= pc ? a : pb <= pc ? b : c)) & 255 : row[i];
    }
    prev = row;
  }
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
}

export const rigData = (overrides = {}) => ({
  schemaVersion: 1, canvas: { width: 200, height: 300, originX: 100, originY: 280 },
  bones: [{ id: 'root', parent: null }, { id: 'body', parent: 'root', y: -100 }],
  slots: [{ id: 'torso', bone: 'body', attachment: 'torso' }],
  attachments: [{ slot: 'torso', name: 'torso', imageAssetId: 'hero-torso', width: 40, height: 60 }],
  clips: [{ id: 'idle', durationMs: 1000, loop: true, tracks: [{ target: 'bone:body', property: 'y', keys: [{ timeMs: 0, value: -100 }, { timeMs: 500, value: -104 }, { timeMs: 1000, value: -100 }] }] }],
  ...overrides,
});
export const controllerData = (overrides = {}) => ({ schemaVersion: 1, rig: 'hero', defaultClip: 'idle', layers: [], bindings: [], ...overrides });
export const envelope = (type, id, data) => ({ formatVersion: '0.1.0', plugin: 'cutout-animation', type, id, data });
export const lock = (plugins = [{ id: 'cutout-animation', version: CUTOUT_VERSION, enabled: true }]) => ({ formatVersion: '0.1.0', plugins });

/** Write lock + hero rig/controller + PNG asset into a project on disk. */
export async function addCutout(root, { lockData = lock(), rig = rigData(), controller = controllerData(), image = png(40, 60) } = {}) {
  await mkdir(path.join(root, 'plugins/cutout-animation/rig'), { recursive: true });
  await mkdir(path.join(root, 'plugins/cutout-animation/controller'), { recursive: true });
  if (lockData) await writeFile(path.join(root, 'plugins/lock.json'), serialize(lockData));
  if (rig) await writeFile(path.join(root, 'plugins/cutout-animation/rig/hero.json'), serialize(envelope('rig', 'hero', rig)));
  if (controller) await writeFile(path.join(root, 'plugins/cutout-animation/controller/hero-main.json'), serialize(envelope('controller', 'hero-main', controller)));
  if (image) {
    await mkdir(path.join(root, 'assets/hero'), { recursive: true });
    await writeFile(path.join(root, 'assets/hero/torso.png'), image);
    const manifestFile = path.join(root, 'assets/manifest.json'), manifest = await readJson(manifestFile);
    manifest.assets.push({ id: 'hero-torso', kind: 'image', path: 'assets/hero/torso.png' });
    await writeFile(manifestFile, serialize(manifest));
  }
}
/** Put a plugin element on the first screen of project.json. */
export async function addElement(root, extension = { plugin: 'cutout-animation', type: 'character', ref: 'hero-main', props: { scale: 1, flipX: false } }) {
  const file = path.join(root, 'project.json'), project = await readJson(file);
  project.visual.screens[0].elements.push({ id: 'hero-on-title', kind: 'plugin', x: 40, y: 120, width: 300, height: 450, extension });
  await writeFile(file, serialize(project));
}

/** Player runtimes good enough for exportGame (entry file + capabilities). */
export async function fakeRuntimes(features = ['plugin-elements-v1']) {
  const base = await temp('runtimes'), web = path.join(base, 'web'), windows = path.join(base, 'windows');
  const capabilities = { formatVersion: '1', visualFeatures: features, imageExtensions: ['.png'], audioExtensions: ['.ogg'], fontExtensions: ['.ttf'] };
  for (const [dir, entry] of [[web, 'index.html'], [windows, 'RaisingPlayer.exe']]) {
    await mkdir(dir, { recursive: true });
    await writeFile(path.join(dir, entry), 'runtime');
    await writeFile(path.join(dir, 'capabilities.json'), JSON.stringify(capabilities));
  }
  return { web, windows };
}

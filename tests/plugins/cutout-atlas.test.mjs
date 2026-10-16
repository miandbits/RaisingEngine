// Atlas packing on export (plugin 1.9.0): every PNG the exported rigs draw goes into page images
// (service hook prepareExport, packages/plugin-host CONTRACT.md §10), rig attachments name their
// region, both players draw regions (plugin-atlas-v1). Checked here: exact pixel copies and draw
// geometry (core draw items with and without atlas), the export's file count and the images left
// alone (unused, colour-managed, too large), the settings document, and screenshots of the native
// and web players with and without atlas (mean difference <= 1/255).
import test from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createServer } from 'node:http';
import { mkdir, readFile, readdir, writeFile } from 'node:fs/promises';
import path, { extname, resolve, sep } from 'node:path';
import { crc32, deflateSync, inflateSync } from 'node:zlib';
import { buildAtlas, decodePng, encodePng, packCells } from '../../plugins/cutout-animation/service/atlas.mjs';
import { StudioEditor } from '../../dist/packages/studio-service/editor.js';
import { exportGame } from '../../dist/packages/export-service/export.js';
import { installPackage } from '../../dist/packages/plugin-host/index.js';
import { CUTOUT_VERSION, cutoutPackage, envelope, fakeRuntimes, lock, newProject, readJson, serialize, temp } from './host-helpers.mjs';
import { NATIVE_APP, WEB_APP, referenceRuntime } from './player-extension-fixture.mjs';

// ---- PNG fixtures (no external assets): gradients, soft alpha edges, palette + tRNS, gray, gAMA.
const chunk = (type, data) => {
  const head = Buffer.alloc(8); head.writeUInt32BE(data.length, 0); head.write(type, 4, 'ascii');
  const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(Buffer.concat([head.subarray(4), data])) >>> 0, 0);
  return Buffer.concat([head, data, crc]);
};
const SIG = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
const rgba = (w, h, f) => { const px = Buffer.alloc(w * h * 4); for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) px.set(f(x, y), (y * w + x) * 4); return encodePng(w, h, px); };
function rawPng(w, h, color, rows, extra = []) {
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr.set([8, color, 0, 0, 0], 8);
  const raw = Buffer.concat(rows.map(r => Buffer.concat([Buffer.from([0]), r])));
  return Buffer.concat([SIG, chunk('IHDR', ihdr), ...extra, chunk('IDAT', deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
}
const clamp = v => Math.max(0, Math.min(255, Math.round(v)));
/** A body with a 10 px transparent margin and a soft (antialiased) alpha edge: trimmed. */
const body = (w, h, tint) => rgba(w, h, (x, y) => {
  const dx = Math.min(x - 10, w - 11 - x), dy = Math.min(y - 10, h - 11 - y), edge = Math.min(dx, dy);
  const a = edge < 0 ? 0 : edge < 2 ? (edge + 1) * 85 : 255;
  return [clamp(tint[0] + x * 1.3), clamp(tint[1] + y), clamp(tint[2] + (x ^ y) % 40), a];
});
const disc = (n, tint) => rgba(n, n, (x, y) => { const r = Math.hypot(x + 0.5 - n / 2, y + 0.5 - n / 2), a = clamp((n / 2 - 2 - r) * 128); return [clamp(tint[0] + x * 2), tint[1], clamp(tint[2] + y * 2), a]; });
const stripes = (w, h) => rgba(w, h, (x, y) => (Math.floor(x / 6) + Math.floor(y / 6)) % 2 ? [230, 80, 40, 255] : [40, 90, 220, 255]);
const palette = () => rawPng(30, 10, 3, Array.from({ length: 10 }, (_, y) => Buffer.from(Array.from({ length: 30 }, (_, x) => (x + y) % 3))),
  [chunk('PLTE', Buffer.from([20, 20, 60, 250, 250, 250, 255, 0, 0])), chunk('tRNS', Buffer.from([255, 128, 0]))]);
const gray = () => rawPng(16, 16, 0, Array.from({ length: 16 }, (_, y) => Buffer.from(Array.from({ length: 16 }, (_, x) => (x * 16 + y * 3) & 255))));
const gamma = () => { const p = stripes(24, 24), at = 33; const g = Buffer.alloc(4); g.writeUInt32BE(45455, 0); return Buffer.concat([p.subarray(0, at), chunk('gAMA', g), p.subarray(at)]); };
const IMAGES = {
  'atlas-body': () => body(100, 140, [200, 120, 70]), 'atlas-body-alt': () => body(100, 140, [70, 120, 200]), 'atlas-head': () => disc(64, [240, 200, 160]),
  'atlas-arm': () => stripes(120, 30), 'atlas-badge': () => stripes(20, 20), 'atlas-eyes': palette, 'atlas-mark': gray, 'atlas-gamma': gamma,
  'atlas-unused': () => stripes(50, 50), 'atlas-title-art': () => stripes(30, 30),
};
/** Parts: trimmed regions (body, head, eyes), a scaled region (badge 20 px drawn at 40), a mesh, a skin, a gAMA PNG. */
function atlasRig() {
  const grid = { type: 'mesh', vertices: [], uvs: [], triangles: [] };
  for (let r = 0; r <= 1; r++) for (let c = 0; c <= 4; c++) { grid.vertices.push(c * 30, r * 30); grid.uvs.push(c / 4, r); }
  for (let c = 0; c < 4; c++) grid.triangles.push(c, c + 1, c + 6, c, c + 6, c + 5);
  return { schemaVersion: 1, canvas: { width: 300, height: 300, originX: 150, originY: 290 },
    bones: [{ id: 'root', parent: null, x: 150, y: 290 }, { id: 'body', parent: 'root', y: -60, rotation: 4 }, { id: 'head', parent: 'body', y: -120, rotation: -12 }, { id: 'arm', parent: 'body', x: 30, y: -90, rotation: 25 }],
    slots: [{ id: 'body', bone: 'body', attachment: 'body' }, { id: 'arm', bone: 'arm', attachment: 'arm' }, { id: 'head', bone: 'head', attachment: 'head' },
      { id: 'eyes', bone: 'head', attachment: 'eyes' }, { id: 'badge', bone: 'body', attachment: 'badge' }, { id: 'flag', bone: 'arm', attachment: 'flag' }, { id: 'mark', bone: 'body', attachment: 'mark' }, { id: 'gamma', bone: 'root', attachment: 'gamma' }],
    attachments: [
      { slot: 'body', name: 'body', imageAssetId: 'atlas-body', width: 100, height: 140, pivotX: 0.5, pivotY: 0.85 },
      { slot: 'arm', name: 'arm', imageAssetId: 'atlas-arm', width: 120, height: 30, pivotX: 0, pivotY: 0.5, ...grid },
      { slot: 'head', name: 'head', imageAssetId: 'atlas-head', width: 64, height: 64, pivotX: 0.5, pivotY: 0.9, rotation: 7 },
      { slot: 'eyes', name: 'eyes', imageAssetId: 'atlas-eyes', width: 30, height: 10, pivotX: 0.5, pivotY: 3.2 },
      { slot: 'badge', name: 'badge', imageAssetId: 'atlas-badge', width: 40, height: 40, x: -20, y: -60, scaleX: 1.2, rotation: 30 },
      { slot: 'mark', name: 'mark', imageAssetId: 'atlas-mark', width: 16, height: 16, x: 25, y: -40 },
      { slot: 'flag', name: 'flag', imageAssetId: 'atlas-title-art', width: 30, height: 30, x: 110, y: -15 },
      { slot: 'gamma', name: 'gamma', imageAssetId: 'atlas-gamma', width: 24, height: 24, x: -120, y: -20 }],
    skins: [{ id: 'alt', attachments: [{ slot: 'body', name: 'body', imageAssetId: 'atlas-body-alt', width: 100, height: 140, pivotX: 0.5, pivotY: 0.85 }] }],
    // A still pose: screenshots must not depend on how much presentation time the players ran.
    clips: [{ id: 'idle', durationMs: 1000, loop: true, tracks: [{ target: 'bone:arm', property: 'rotation', keys: [{ timeMs: 0, value: 32 }] }] }] };
}
const controller = { schemaVersion: 1, rig: 'hero', defaultClip: 'idle', layers: [], bindings: [] };
const settingsDoc = atlas => envelope('settings', 'project', { schemaVersion: 1, export: { atlas } });

test('packing: exact copies, edge padding, trimmed rings, and the same draw geometry', async () => {
  const images = new Map(Object.entries(IMAGES).map(([id, make]) => [id, decodePng(make())]));
  assert.equal(images.get('atlas-gamma').reason, '색 보정 정보(gAMA)가 있는 PNG');
  assert.equal(images.get('atlas-eyes').width, 30, 'palette + tRNS decodes');
  const ids = [];
  const documents = [{ type: 'rig', id: 'hero', data: atlasRig() }, { type: 'controller', id: 'hero-main', data: controller }];
  const atlas = buildAtlas({ documents, images, settings: { maxPageSize: 128, padding: 2 }, pageId: i => (ids[i] ??= `cutout-atlas-${i + 1}`) });
  assert.deepEqual(atlas.packed, ['atlas-arm', 'atlas-badge', 'atlas-body', 'atlas-body-alt', 'atlas-eyes', 'atlas-head', 'atlas-mark', 'atlas-title-art'], 'what the rig draws, nothing else');
  assert.deepEqual(atlas.skipped.map(s => s.id), ['atlas-gamma']);
  assert.ok(atlas.pages.length >= 2, '128 px pages: one body per page');
  const pages = atlas.pages.map(p => decodePng(p.png));
  const regions = atlas.regions;
  // Trimmed: the bodies lose their transparent margin but keep a 1 px transparent ring; the scaled
  // badge and the mesh never trim; everything outside a trim rect was fully transparent.
  assert.deepEqual(regions['atlas-body'].trim, { x: 9, y: 9, width: 82, height: 122 });
  assert.equal(regions['atlas-badge'].trim, undefined); assert.equal(regions['atlas-arm'].trim, undefined);
  for (const [id, region] of Object.entries(regions)) {
    const src = images.get(id), page = pages[ids.indexOf(region.page)], t = region.trim ?? { x: 0, y: 0, width: src.width, height: src.height };
    const at = (img, x, y) => [...img.rgba.subarray((y * img.width + x) * 4, (y * img.width + x) * 4 + 4)];
    for (let y = -2; y < t.height + 2; y++) for (let x = -2; x < t.width + 2; x++) {
      const sx = t.x + Math.min(t.width - 1, Math.max(0, x)), sy = t.y + Math.min(t.height - 1, Math.max(0, y));
      assert.deepEqual(at(page, region.x + x, region.y + y), at(src, sx, sy), `${id} pixel ${x},${y} (padding repeats the edge)`);
    }
    for (let y = 0; y < src.height; y++) for (let x = 0; x < src.width; x++)
      if (x < t.x || y < t.y || x >= t.x + t.width || y >= t.y + t.height) assert.equal(at(src, x, y)[3], 0, `${id} trimmed pixel ${x},${y} was transparent`);
  }
  // Draw items: the atlas rig names pages and regions; quads keep their on-screen rectangle, meshes their triangles.
  const request = await referenceRuntime();
  const items = rigData => {
    request({ op: 'init', documents: [{ type: 'rig', id: 'hero', data: rigData }, { type: 'controller', id: 'hero-main', data: controller }] });
    request({ op: 'mount', instance: 'a', element: { type: 'character', ref: 'hero-main', width: 450, height: 450, props: { skin: 'alt' } } });
    request({ op: 'update', dtMs: 200 });
    return request({ op: 'frame', instance: 'a' }).items;
  };
  const plain = items(atlasRig()), packed = items(atlas.documents[0].data);
  assert.equal(packed.length, plain.length);
  plain.forEach((a, k) => {
    const b = packed[k], region = regions[a.imageAssetId];
    if (!region) { assert.deepEqual(b, a, `${a.slot} (not packed) unchanged`); return; }
    assert.equal(b.imageAssetId, region.page);
    assert.deepEqual(b.region, [region.x, region.y, region.width, region.height]);
    const t = region.trim ?? { x: 0, y: 0, width: a.width, height: a.height };
    const map = (m, x, y) => [m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5]];
    // The kept rectangle lands exactly where it was in the original picture.
    for (const [x, y] of [[0, 0], [t.width, 0], [0, t.height], [t.width, t.height]]) {
      const [px, py] = map(b.matrix, x, y), [qx, qy] = map(a.matrix, t.x + x, t.y + y);
      assert.ok(Math.abs(px - qx) < 1e-9 && Math.abs(py - qy) < 1e-9, `${a.slot} corner`);
    }
    assert.deepEqual([b.width, b.height], [t.width, t.height]);
    if (a.mesh) assert.deepEqual(b.mesh, a.mesh, 'mesh triangles and uvs unchanged (uvs address the region)');
  });
  assert.ok(packed.some(i => i.imageAssetId.startsWith('cutout-atlas-') && i.mesh), 'the mesh is packed too');
  // Packing is deterministic and fits.
  const cells = Array.from({ length: 40 }, (_, i) => ({ w: 10 + (i * 7) % 50, h: 8 + (i * 11) % 40 }));
  const layout = packCells(cells, 128);
  for (const c of cells) assert.ok(c.x + c.w <= layout[c.page].width && c.y + c.h <= layout[c.page].height);
  for (let i = 0; i < cells.length; i++) for (let j = i + 1; j < cells.length; j++) {
    const a = cells[i], b = cells[j];
    if (a.page === b.page) assert.ok(a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y, 'no overlap');
  }
});

test('linked mesh (1.18.0): its own picture is packed whole (no trim) and drawn with the parent mesh shape', async () => {
  const images = new Map(Object.entries({ ...IMAGES, 'atlas-arm-alt': () => body(60, 15, [30, 200, 90]) }).map(([id, make]) => [id, decodePng(make())]));
  const rig = atlasRig();
  rig.skins[0].attachments.push({ slot: 'arm', name: 'arm', type: 'mesh', imageAssetId: 'atlas-arm-alt', width: 60, height: 15, linkedMesh: { parent: 'arm/arm' } });
  const documents = [{ type: 'rig', id: 'hero', data: rig }, { type: 'controller', id: 'hero-main', data: controller }];
  const ids = [];
  const atlas = buildAtlas({ documents, images, settings: { maxPageSize: 256, padding: 2 }, pageId: i => (ids[i] ??= `cutout-atlas-${i + 1}`) });
  assert.ok(atlas.packed.includes('atlas-arm-alt'), 'the linked mesh picture is packed');
  assert.equal(atlas.regions['atlas-arm-alt'].trim, undefined, 'a mesh picture is never trimmed (its uvs address all of it)');
  const linked = atlas.documents[0].data.skins[0].attachments.find(a => a.slot === 'arm');
  assert.deepEqual(linked.linkedMesh, { parent: 'arm/arm' });
  assert.equal(linked.atlas.page, atlas.regions['atlas-arm-alt'].page);
  const request = await referenceRuntime();
  const arm = rigData => {
    request({ op: 'init', documents: [{ type: 'rig', id: 'hero', data: rigData }, { type: 'controller', id: 'hero-main', data: controller }] });
    request({ op: 'mount', instance: 'a', element: { type: 'character', ref: 'hero-main', width: 450, height: 450, props: { skin: 'alt' } } });
    return request({ op: 'frame', instance: 'a' }).items.find(i => i.slot === 'arm');
  };
  const plain = arm(rig), packed = arm(atlas.documents[0].data), parent = arm(atlasRig());
  assert.equal(plain.imageAssetId, 'atlas-arm-alt');
  assert.deepEqual(plain.mesh, parent.mesh, 'the linked mesh draws the parent mesh (shape and uvs)');
  assert.equal(packed.imageAssetId, atlas.regions['atlas-arm-alt'].page);
  assert.deepEqual(packed.mesh, plain.mesh);
});

/** Project with the atlas rig on the title screen (450×450 box: scale 1.5), its images, and a store. */
async function atlasProject({ atlas } = {}) {
  const root = await newProject(), store = await temp('atlas-store');
  await installPackage(store, await cutoutPackage());
  await mkdir(path.join(root, 'plugins/cutout-animation/rig'), { recursive: true });
  await mkdir(path.join(root, 'plugins/cutout-animation/controller'), { recursive: true });
  await writeFile(path.join(root, 'plugins/lock.json'), serialize(lock()));
  await writeFile(path.join(root, 'plugins/cutout-animation/rig/hero.json'), serialize(envelope('rig', 'hero', atlasRig())));
  await writeFile(path.join(root, 'plugins/cutout-animation/controller/hero-main.json'), serialize(envelope('controller', 'hero-main', controller)));
  if (atlas) { await mkdir(path.join(root, 'plugins/cutout-animation/settings'), { recursive: true }); await writeFile(path.join(root, 'plugins/cutout-animation/settings/project.json'), serialize(settingsDoc(atlas))); }
  await mkdir(path.join(root, 'assets/atlas'), { recursive: true });
  const manifestFile = path.join(root, 'assets/manifest.json'), manifest = await readJson(manifestFile);
  for (const [id, make] of Object.entries(IMAGES)) { await writeFile(path.join(root, `assets/atlas/${id}.png`), make()); manifest.assets.push({ id, kind: 'image', path: `assets/atlas/${id}.png` }); }
  await writeFile(manifestFile, serialize(manifest));
  const file = path.join(root, 'project.json'), project = await readJson(file);
  // A still screen behind the character: the template's portraits fade in (CSS / player motion).
  project.visual.screens[0].elements = project.visual.screens[0].elements.filter(e => !e.id.startsWith('portrait-')).map(({ motion: _motion, ...e }) => e);
  project.visual.screens[0].elements.push({ id: 'atlas-hero', kind: 'plugin', x: 700, y: 120, width: 450, height: 450,
    extension: { plugin: 'cutout-animation', type: 'character', ref: 'hero-main', props: { scale: 1, flipX: false, skin: 'alt' } } });
  // An image the content itself shows: packed for the rig, but its own file must stay.
  project.visual.screens[0].elements.push({ id: 'atlas-art', kind: 'image', imageSource: 'asset', imageAssetId: 'atlas-title-art', x: 40, y: 40, width: 60, height: 60 });
  await writeFile(file, serialize(project));
  return { root, store };
}
async function exportAtlasProject(project, target, runtimes, name, features) {
  const editor = new StudioEditor({ pluginStore: project.store });
  await editor.open(project.root);
  assert.deepEqual(editor.diagnostics.filter(d => d.severity === 'error'), []);
  const built = await editor.snapshot(editor.revision);
  assert.equal(built.ok, true, JSON.stringify(built.diagnostics));
  const capabilities = await readJson(path.join(runtimes[target], 'capabilities.json'));
  const extensions = await editor.exportExtensions(editor.revision, built.snapshot, target, features ?? capabilities.visualFeatures);
  const out = await temp(`atlas-${name}`);
  const result = await exportGame({ content: built.snapshot, projectRoot: project.root, destination: path.join(out, 'game'), target, runtimes, extensions });
  return { dir: result.directory, game: await readJson(path.join(result.directory, 'game.json')), extensions, content: built.snapshot };
}

test('export: pages replace the rig images, unused and colour-managed images stay, settings and old players', async () => {
  const runtimes = await fakeRuntimes(['plugin-elements-v1', 'plugin-mesh-v1', 'plugin-atlas-v1']);
  const on = await exportAtlasProject(await atlasProject(), 'web', runtimes, 'on');
  const off = await exportAtlasProject(await atlasProject({ atlas: { enabled: false } }), 'web', runtimes, 'off');
  const files = async dir => (await readdir(path.join(dir, 'assets'))).sort();
  const onFiles = await files(on.dir), offFiles = await files(off.dir);
  const rig = game => game.extensions.plugins[0].documents.find(d => d.type === 'rig').data;
  const pages = onFiles.filter(f => f.startsWith('cutout-atlas-'));
  assert.equal(pages.length, 1, '2048 px default: one page');
  // Eight rig images packed: seven files left out (the title art is also on the title screen), one page added.
  assert.equal(offFiles.length - onFiles.length, 6, JSON.stringify({ onFiles, offFiles }));
  for (const id of ['atlas-body', 'atlas-body-alt', 'atlas-head', 'atlas-arm', 'atlas-eyes']) assert.ok(!onFiles.includes(`${id}.png`) && offFiles.includes(`${id}.png`), id);
  for (const id of ['atlas-unused', 'atlas-gamma', 'atlas-title-art']) assert.ok(onFiles.includes(`${id}.png`), `${id} keeps its own file`);
  assert.deepEqual(on.extensions.reports['cutout-animation'].packed, ['atlas-arm', 'atlas-badge', 'atlas-body', 'atlas-body-alt', 'atlas-eyes', 'atlas-head', 'atlas-mark', 'atlas-title-art']);
  assert.ok(!on.extensions.reports['cutout-animation'].packed.includes('atlas-unused'), 'images no rig draws are never packed');
  assert.equal(on.game.assets[pages[0].replace('.png', '')], `assets/${pages[0]}`);
  assert.equal(on.game.content.contentFingerprint, off.game.content.contentFingerprint, 'the content snapshot is unchanged');
  assert.ok(rig(on.game).attachments.find(a => a.slot === 'body').atlas.trim, 'trimmed body');
  assert.equal(rig(on.game).attachments.find(a => a.slot === 'gamma').atlas, undefined);
  assert.equal(rig(off.game).attachments.some(a => a.atlas), false);
  assert.deepEqual(on.extensions.features, ['plugin-atlas-v1', 'plugin-mesh-v1']);
  assert.deepEqual(off.extensions.features, ['plugin-mesh-v1']);
  assert.equal(on.game.extensions.plugins[0].documents.some(d => d.type === 'settings'), false, 'settings stay in the project');
  assert.equal(off.extensions.reports['cutout-animation'].reason, 'settings');
  // A player without plugin-atlas-v1 gets separate files (atlas on by default only where drawable).
  const old = await exportAtlasProject(await atlasProject(), 'web', await fakeRuntimes(['plugin-elements-v1', 'plugin-mesh-v1']), 'old');
  assert.equal(old.extensions.reports['cutout-animation'].reason, 'player');
  assert.equal((await files(old.dir)).length, offFiles.length);
  // Prepared for an atlas player, refused by a player without it (nothing written).
  const project = await atlasProject();
  await assert.rejects(exportAtlasProject(project, 'web', await fakeRuntimes(['plugin-elements-v1', 'plugin-mesh-v1']), 'refused', ['plugin-elements-v1', 'plugin-mesh-v1', 'plugin-atlas-v1']), /plugin-atlas-v1/);
  // 4096 px pages, no trimming: the settings document is read.
  const big = await exportAtlasProject(await atlasProject({ atlas: { enabled: true, maxPageSize: 512, trim: false, padding: 4 } }), 'web', runtimes, 'settings');
  assert.equal(rig(big.game).attachments.find(a => a.slot === 'body').atlas.trim, undefined);
  assert.ok(big.extensions.reports['cutout-animation'].pages.every(p => p.width <= 512 && p.height <= 512));
});

// ---- Player screenshots with and without atlas.
function decodeScreenshot(bytes) {
  const width = bytes.readUInt32BE(16), height = bytes.readUInt32BE(20), channels = bytes[25] === 6 ? 4 : 3, data = [];
  for (let at = 8; at < bytes.length;) { const length = bytes.readUInt32BE(at), type = bytes.toString('ascii', at + 4, at + 8); if (type === 'IDAT') data.push(bytes.subarray(at + 8, at + 8 + length)); at += length + 12; }
  const raw = inflateSync(Buffer.concat(data)), stride = width * channels, out = Buffer.alloc(stride * height);
  for (let y = 0; y < height; y++) for (let i = 0; i < stride; i++) {
    const f = raw[y * (stride + 1)], o = y * stride + i, a = i >= channels ? out[o - channels] : 0, b = y ? out[o - stride] : 0, c = y && i >= channels ? out[o - stride - channels] : 0;
    const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
    out[o] = (raw[y * (stride + 1) + 1 + i] + [0, a, b, (a + b) >> 1, pa <= pb && pa <= pc ? a : pb <= pc ? b : c][f]) & 255;
  }
  return { width, height, channels, at: (x, y) => [out[(y * width + x) * channels], out[(y * width + x) * channels + 1], out[(y * width + x) * channels + 2]] };
}
/** Mean |a - b| over RGB (0..255) in a box, and how many pixels differ at all. */
function compare(a, b, box) {
  let sum = 0, differ = 0, n = 0, max = 0;
  for (let y = box.y; y < box.y + box.h; y++) for (let x = box.x; x < box.x + box.w; x++) {
    const p = a.at(x, y), q = b.at(x, y); let d = 0;
    for (let i = 0; i < 3; i++) { const v = Math.abs(p[i] - q[i]); sum += v; d = Math.max(d, v); }
    n++; if (d) differ++; max = Math.max(max, d);
  }
  return { mean: sum / (n * 3), differ, max, pixels: n };
}
const SCREEN = { x: 700, y: 120, w: 450, h: 450 };

test('native player: the same pixels with and without atlas (mean difference <= 1/255)', { timeout: 300000 }, async t => {
  const runtimes = { windows: NATIVE_APP, web: WEB_APP };
  const shots = {}, reports = {};
  for (const [name, atlas] of [['on', undefined], ['off', { enabled: false }]]) {
    const exported = await exportAtlasProject(await atlasProject({ atlas }), 'windows', runtimes, `native-${name}`);
    const steps = path.join(exported.dir, '..', 'steps.json'), report = path.join(exported.dir, '..', 'report.json'), shot = path.join(exported.dir, '..', 'shot.png');
    await writeFile(steps, JSON.stringify([{ extensionAdvanceMs: 200 }, { waitFrames: 20 }, { checkpoint: 'shot', screenshot: shot }]));
    const child = spawnSync(path.join(exported.dir, 'RaisingPlayer.exe'), ['--bundle', exported.dir, '--save', path.join(exported.dir, '..', 'save.json'), '--seed', '1', '--hidden', '--automation', steps, '--report', report], { encoding: 'utf8', timeout: 120000, windowsHide: true });
    assert.equal(child.status, 0, child.stderr);
    reports[name] = (await readJson(report)).steps.find(s => s.command.checkpoint === 'shot').ui.extensions.instances.find(i => i.instance.endsWith('atlas-hero'));
    await mkdir(resolve('.cache/anim-evidence/v3'), { recursive: true });
    await writeFile(resolve(`.cache/anim-evidence/v3/windows-atlas-${name}.png`), await readFile(shot));
    shots[name] = decodeScreenshot(await readFile(shot));
    t.diagnostic(`${name}: ${exported.dir}`);
  }
  assert.equal(reports.on.available, true, reports.on.error);
  assert.ok(reports.on.items.filter(i => i.region && i.imageAssetId.startsWith('cutout-atlas-')).length >= 6, 'drawn from the page');
  assert.equal(reports.off.items.some(i => i.region), false);
  const result = compare(shots.on, shots.off, SCREEN);
  t.diagnostic(JSON.stringify(result));
  // Measured: mean 0.0013 (574 of 202500 px differ, by <= 2: texture coordinates of the page).
  assert.ok(result.mean <= 1 && result.differ < result.pixels * 0.02, JSON.stringify(result));
  // The character is really there (not two empty screenshots).
  const background = compare(shots.off, { at: () => shots.off.at(SCREEN.x + 2, SCREEN.y + 2) }, SCREEN);
  assert.ok(background.differ > 5000, 'the character covers the box');
});

test('web player: the same pixels with and without atlas (mean difference <= 1/255)', { timeout: 300000 }, async t => {
  const { chromium } = await import('playwright');
  const runtimes = { windows: NATIVE_APP, web: WEB_APP };
  const browser = await chromium.launch({ headless: true, channel: process.env.PLAYWRIGHT_CHANNEL ?? 'chrome' }); t.after(() => browser.close());
  const mime = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.wasm': 'application/wasm', '.png': 'image/png', '.ttf': 'font/ttf' };
  const shots = {}, states = {};
  for (const [name, atlas] of [['on', undefined], ['off', { enabled: false }]]) {
    const exported = await exportAtlasProject(await atlasProject({ atlas }), 'web', runtimes, `web-${name}`);
    const root = exported.dir;
    const server = createServer(async (req, res) => {
      try {
        const pathname = decodeURIComponent(new URL(req.url, 'http://127.0.0.1').pathname), file = resolve(root, `.${pathname === '/' ? '/index.html' : pathname}`);
        if (!file.startsWith(`${root}${sep}`)) { res.writeHead(403); res.end(); return; }
        const bytes = await readFile(file);
        res.writeHead(200, { 'Content-Type': mime[extname(file)] ?? 'application/octet-stream' }); res.end(bytes);
      } catch { res.writeHead(404); res.end(); }
    });
    await new Promise(done => server.listen(0, '127.0.0.1', done));
    const page = await browser.newPage({ viewport: { width: 1280, height: 720 }, deviceScaleFactor: 1 });
    const errors = []; page.on('pageerror', e => errors.push(e.message));
    await page.addInitScript(() => { window.raisingExtensionManualClock = true; });
    await page.goto(`http://127.0.0.1:${server.address().port}/`);
    await page.waitForFunction(() => window.raisingExtensions?.describe().instances.some(i => i.available && i.items.length), null, { timeout: 60000 });
    await page.evaluate(() => window.raisingExtensions.tick(200));
    await page.waitForFunction(() => [...document.querySelectorAll('.visual-plugin-layer img')].every(i => i.complete && i.naturalWidth > 0) &&
      [...document.querySelectorAll('canvas.visual-plugin-mesh')].every(c => c.dataset.drawn === 'true'), null, { timeout: 30000 });
    await page.waitForFunction(() => document.getAnimations().every(a => a.playState === 'finished'), null, { timeout: 10000 });
    await page.waitForTimeout(200);
    states[name] = await page.evaluate(() => ({ regions: document.querySelectorAll('.visual-plugin-region').length, imgs: document.querySelectorAll('img.visual-plugin-item').length,
      box: (() => { const r = document.querySelector('[data-element-id="atlas-hero"]').getBoundingClientRect(); return { x: Math.round(r.x), y: Math.round(r.y), w: Math.round(r.width), h: Math.round(r.height) }; })() }));
    const png = await page.screenshot();
    await mkdir(resolve('.cache/anim-evidence/v3'), { recursive: true });
    await writeFile(resolve(`.cache/anim-evidence/v3/web-atlas-${name}.png`), png);
    shots[name] = decodeScreenshot(png);
    assert.deepEqual(errors, []);
    await page.close(); server.closeAllConnections(); server.close();
  }
  assert.ok(states.on.regions >= 5 && states.off.regions === 0, JSON.stringify(states));
  const box = states.on.box;
  assert.deepEqual(box, states.off.box);
  const result = compare(shots.on, shots.off, { x: Math.max(0, box.x), y: Math.max(0, box.y), w: Math.min(box.w, 1280 - box.x), h: Math.min(box.h, 720 - box.y) });
  t.diagnostic(JSON.stringify(result));
  // Measured: mean 0.004 (~1100 px at the antialiased edges of the clipped sprite boxes).
  assert.ok(result.mean <= 1 && result.differ < result.pixels * 0.02, JSON.stringify(result));
  const inside = { x: Math.max(0, box.x), y: Math.max(0, box.y), w: Math.min(box.w, 1280 - box.x), h: Math.min(box.h, 720 - box.y) };
  assert.ok(compare(shots.off, { at: () => shots.off.at(inside.x + 2, inside.y + 2) }, inside).differ > 5000, 'the character covers the box');
});

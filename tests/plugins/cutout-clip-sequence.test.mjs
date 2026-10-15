// Clipping + image sequences in exported games (plugin 1.12.0): both players draw a clipped part
// only inside its polygon (clipped items are plain plugin-mesh-v1 meshes, no new host capability)
// and swap sequence frames at the expected presentation times; the project service checks frame
// images; the export atlas packs every frame and the clipped items keep drawing from the pages.
import test from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createServer } from 'node:http';
import { copyFile, mkdir, readFile, writeFile } from 'node:fs/promises';
import path, { extname, resolve, sep } from 'node:path';
import { chromium } from 'playwright';
import { buildAtlas, decodePng } from '../../plugins/cutout-animation/service/atlas.mjs';
import { StudioEditor } from '../../dist/packages/studio-service/editor.js';
import { exportGame } from '../../dist/packages/export-service/export.js';
import { installPackage } from '../../dist/packages/plugin-host/index.js';
import { cutoutPackage, envelope, fakeRuntimes, lock, newProject, readJson, serialize, temp } from './host-helpers.mjs';
import { EVIDENCE, makeGame, pluginElement, png, pngPixel, referenceRuntime, close, RED } from './player-extension-fixture.mjs';

const INSTANCE = 'home/hero-anim';
const GREEN = [40, 160, 60], FRAMES = { 'seq-a': [40, 80, 220], 'seq-b': [230, 200, 40], 'seq-c': [200, 40, 200] };
/** 300x450 canvas = the 300x450 element (element px = rig px). A red 200x200 card under a triangle
 *  clip (50,100)-(250,100)-(150,300), a green square below the clip slot (never clipped), and a
 *  3-frame sequence part above the range keyed loop / 100 ms from 0. */
function clipRig() {
  return { schemaVersion: 1, canvas: { width: 300, height: 450, originX: 150, originY: 440 },
    bones: [{ id: 'root', parent: null }],
    slots: [{ id: 'back', bone: 'root', attachment: 'back' }, { id: 'mask', bone: 'root', attachment: 'mask' }, { id: 'card', bone: 'root', attachment: 'card' }, { id: 'film', bone: 'root', attachment: 'film' }],
    attachments: [
      { slot: 'back', name: 'back', imageAssetId: 'clip-back', width: 60, height: 60, x: 40, y: 60 },
      { slot: 'mask', name: 'mask', type: 'clipping', end: 'card', vertices: [50, 100, 250, 100, 150, 300] },
      { slot: 'card', name: 'card', imageAssetId: 'clip-card', width: 200, height: 200, x: 150, y: 200 },
      { slot: 'film', name: 'film', imageAssetId: 'seq-a', width: 60, height: 60, x: 150, y: 380, sequence: { frames: ['seq-a', 'seq-b', 'seq-c'] } }],
    clips: [{ id: 'flip', durationMs: 300, loop: true, tracks: [{ target: 'slot:film', property: 'sequence', keys: [{ timeMs: 0, mode: 'loop', index: 0, delay: 100 }] }] }] };
}
const clipController = { schemaVersion: 1, rig: 'hero', defaultClip: 'flip', layers: [], bindings: [] };
const images = () => ({ 'clip-back': png(60, 60, GREEN), 'clip-card': png(200, 200, RED), ...Object.fromEntries(Object.entries(FRAMES).map(([id, c]) => [id, png(60, 60, c)])) });
/** Rig px inside the triangle, inside the card but outside the triangle, the green part, the film part. */
const INSIDE = [150, 150], OUTSIDE = [[62, 280], [238, 280]], BACK = [40, 60], FILM = [150, 380];
const inTriangle = (x, y) => {
  const t = [[50, 100], [250, 100], [150, 300]];
  const s = (a, b) => (b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0]);
  const d = [s(t[0], t[1]), s(t[1], t[2]), s(t[2], t[0])];
  return d.every(v => v >= -1e-3) || d.every(v => v <= 1e-3);
};
/** The core (Wasm) frame after `ms` of presentation time: what both players are given. */
async function reference(ms) {
  const request = await referenceRuntime();
  request({ op: 'init', documents: [{ type: 'rig', id: 'hero', data: clipRig() }, { type: 'controller', id: 'hero-main', data: clipController }] });
  request({ op: 'mount', instance: INSTANCE, element: { type: 'character', ref: 'hero-main', width: pluginElement.width, height: pluginElement.height, props: pluginElement.extension.props } });
  if (ms) request({ op: 'update', dtMs: ms });
  return request({ op: 'frame', instance: INSTANCE }).items;
}
function checkItems(items, frame, what) {
  assert.deepEqual(items.map(i => i.imageAssetId), ['clip-back', 'clip-card', frame], `${what}: items`);
  const [back, card, film] = items;
  assert.ok(!back.mesh && !film.mesh, `${what}: parts outside the range stay regions`);
  assert.ok(card.mesh && card.mesh.indices.length >= 3, `${what}: the clipped card is a mesh item`);
  for (let v = 0; v < card.mesh.positions.length; v += 2) assert.ok(inTriangle(card.mesh.positions[v], card.mesh.positions[v + 1]), `${what}: vertex ${v / 2} inside the clip`);
}
function checkPixels(at, frame, what) {
  assert.ok(close(at(...INSIDE), RED), `${what}: card inside the clip ${at(...INSIDE)}`);
  for (const p of OUTSIDE) assert.ok(!close(at(...p), RED, 60), `${what}: no card outside the clip at ${p}: ${at(...p)}`);
  assert.ok(close(at(...BACK), GREEN), `${what}: the part below the clip slot is whole ${at(...BACK)}`);
  assert.ok(close(at(...FILM), FRAMES[frame]), `${what}: sequence frame ${frame} ${at(...FILM)}`);
}
const STEPS = [[50, 'seq-a'], [100, 'seq-b'], [100, 'seq-c'], [100, 'seq-a']];  // 50, 150, 250, 350 (loop wraps) ms

test('native player: clipped part only inside the polygon, sequence frames swap on time', { timeout: 180000 }, async t => {
  const f = await makeGame({ target: 'windows', name: 'native-clip', documents: { rig: clipRig(), controller: clipController }, images: images() });
  t.diagnostic(`fixture: ${f.root}`);
  const steps = [{ click: 'start' }], shots = [];
  STEPS.forEach(([ms], i) => { const shot = resolve(f.root, `clip-${i}.png`); shots.push(shot); steps.push({ extensionAdvanceMs: ms }, { checkpoint: `s${i}`, screenshot: shot }); });
  const input = resolve(f.root, 'steps.json'), output = resolve(f.root, 'report.json');
  await writeFile(input, JSON.stringify(steps));
  const child = spawnSync(resolve(f.game, 'RaisingPlayer.exe'), ['--bundle', f.game, '--save', resolve(f.root, 'save.json'), '--seed', '42', '--hidden', '--automation', input, '--report', output], { encoding: 'utf8', timeout: 120000, windowsHide: true });
  assert.equal(child.status, 0, child.stderr);
  const report = JSON.parse(await readFile(output, 'utf8'));
  assert.equal(report.ok, true, report.error);
  let elapsed = 0;
  for (const [i, [ms, frame]] of STEPS.entries()) {
    elapsed += ms;
    const instance = report.steps.find(s => s.command.checkpoint === `s${i}`).ui.extensions.instances.find(x => x.instance === INSTANCE);
    assert.equal(instance.available, true, instance.error);
    checkItems(instance.items, frame, `native ${elapsed} ms`);
    // Same triangles as the core compiled to Wasm (compact frames: 1/1000 px).
    const want = (await reference(elapsed))[1].mesh;
    assert.deepEqual(instance.items[1].mesh.indices, want.indices, 'clipped topology parity');
    instance.items[1].mesh.positions.forEach((v, k) => assert.ok(Math.abs(v - want.positions[k]) < 6e-4, 'clipped positions parity'));
    const shot = await readFile(shots[i]);
    checkPixels((x, y) => pngPixel(shot, pluginElement.x + x, pluginElement.y + y), frame, `native ${elapsed} ms`);
  }
  await mkdir(resolve(EVIDENCE, 'v4'), { recursive: true });
  await copyFile(shots[1], resolve(EVIDENCE, 'v4', 'windows-clip-sequence.png'));
});

test('web player: clipped part only inside the polygon, sequence frames swap on time', { timeout: 180000 }, async t => {
  const f = await makeGame({ target: 'web', name: 'web-clip', documents: { rig: clipRig(), controller: clipController }, images: images() });
  t.diagnostic(`fixture: ${f.root}`);
  const mime = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.wasm': 'application/wasm', '.png': 'image/png', '.ttf': 'font/ttf', '.otf': 'font/otf', '.svg': 'image/svg+xml' };
  const server = createServer(async (req, res) => {
    try {
      const pathname = decodeURIComponent(new URL(req.url, 'http://127.0.0.1').pathname), file = resolve(f.game, `.${pathname === '/' ? '/index.html' : pathname}`);
      if (!file.startsWith(`${f.game}${sep}`)) { res.writeHead(403); res.end(); return; }
      const bytes = await readFile(file);
      res.writeHead(200, { 'Content-Type': mime[extname(file)] ?? 'application/octet-stream', 'Cache-Control': 'no-store' }); res.end(bytes);
    } catch { res.writeHead(404); res.end(); }
  });
  await new Promise(done => server.listen(0, '127.0.0.1', done));
  t.after(() => { server.closeAllConnections(); server.close(); });
  const browser = await chromium.launch({ headless: true, channel: process.env.PLAYWRIGHT_CHANNEL ?? 'chrome' });
  t.after(() => browser.close());
  const page = await browser.newPage({ viewport: { width: 1280, height: 760 } });
  const errors = []; page.on('pageerror', e => errors.push(e.message));
  await page.addInitScript(() => { window.raisingExtensionManualClock = true; });
  await page.goto(`http://127.0.0.1:${server.address().port}/`);
  await page.locator('[data-element-id="start"] button').click();
  await page.locator('[data-element-id="cheer"] button').waitFor();
  let elapsed = 0;
  for (const [i, [ms, frame]] of STEPS.entries()) {
    elapsed += ms;
    await page.evaluate(ms => window.raisingExtensions.tick(ms), ms);
    await page.waitForFunction(() => [...document.querySelectorAll('.visual-plugin-layer img')].every(i => i.complete && i.naturalWidth > 0) &&
      [...document.querySelectorAll('canvas.visual-plugin-mesh')].every(c => c.dataset.drawn === 'true'), null, { timeout: 30000 });
    const d = await page.evaluate(id => window.raisingExtensions.describe().instances.find(x => x.instance === id), INSTANCE);
    checkItems(d.items, frame, `web ${elapsed} ms`);
    const want = (await reference(elapsed))[1].mesh;
    assert.deepEqual(d.items[1].mesh.indices, want.indices, 'clipped topology parity');
    const map = await page.evaluate(() => { const el = document.querySelector('[data-element-id="hero-anim"]').getBoundingClientRect(), stage = document.querySelector('.visual-stage'); return { x: el.x, y: el.y, scale: stage.getBoundingClientRect().width / stage.offsetWidth }; });
    const shot = await page.screenshot();
    checkPixels((x, y) => pngPixel(shot, Math.round(map.x + x * map.scale), Math.round(map.y + y * map.scale)), frame, `web ${elapsed} ms`);
    if (i === 1) { await mkdir(resolve(EVIDENCE, 'v4'), { recursive: true }); await writeFile(resolve(EVIDENCE, 'v4', 'web-clip-sequence.png'), shot); }
  }
  assert.equal(await page.locator('img.visual-plugin-item').count() >= 2, true, 'regions stay <img> items');
  assert.deepEqual(errors, []);
});

// ---- service checks and export
async function clipProject(rig = clipRig(), { atlas } = {}) {
  const root = await newProject(), store = await temp('clip-store');
  await installPackage(store, await cutoutPackage());
  for (const dir of ['rig', 'controller', 'settings']) await mkdir(path.join(root, `plugins/cutout-animation/${dir}`), { recursive: true });
  await writeFile(path.join(root, 'plugins/lock.json'), serialize(lock()));
  await writeFile(path.join(root, 'plugins/cutout-animation/rig/hero.json'), serialize(envelope('rig', 'hero', rig)));
  await writeFile(path.join(root, 'plugins/cutout-animation/controller/hero-main.json'), serialize(envelope('controller', 'hero-main', clipController)));
  if (atlas) await writeFile(path.join(root, 'plugins/cutout-animation/settings/project.json'), serialize(envelope('settings', 'project', { schemaVersion: 1, export: { atlas } })));
  await mkdir(path.join(root, 'assets/clip'), { recursive: true });
  const manifestFile = path.join(root, 'assets/manifest.json'), manifest = await readJson(manifestFile);
  for (const [id, bytes] of Object.entries({ ...images(), 'seq-wide': png(64, 60, RED) })) { await writeFile(path.join(root, `assets/clip/${id}.png`), bytes); manifest.assets.push({ id, kind: 'image', path: `assets/clip/${id}.png` }); }
  await writeFile(manifestFile, serialize(manifest));
  const file = path.join(root, 'project.json'), project = await readJson(file);
  project.visual.screens[0].elements.push({ id: 'clip-hero', kind: 'plugin', x: 700, y: 120, width: 300, height: 450, extension: { plugin: 'cutout-animation', type: 'character', ref: 'hero-main', props: { scale: 1, flipX: false } } });
  await writeFile(file, serialize(project));
  const editor = new StudioEditor({ pluginStore: store });
  await editor.open(root);
  return { root, editor };
}

test('project service: clipping and sequence documents validate; frame sizes must match', async () => {
  const { editor } = await clipProject();
  assert.deepEqual(editor.diagnostics.filter(d => d.severity === 'error').map(d => `${d.code} ${d.pointer}`), [], 'schema + core + service accept the documents');
  const wide = clipRig();
  wide.attachments[3].sequence.frames = ['seq-a', 'seq-wide', 'seq-c'];
  const { editor: bad } = await clipProject(wide);
  const codes = bad.diagnostics.map(d => `${d.severity}:${d.code}:${d.pointer}`);
  assert.ok(codes.includes('error:ANIM_SEQUENCE_FRAME_SIZE:/data/attachments/3/sequence/frames/1'), codes.join('\n'));
  const missing = clipRig();
  missing.attachments[3].sequence.frames = ['seq-a', 'seq-nope'];
  const { editor: gone } = await clipProject(missing);
  assert.ok(gone.diagnostics.some(d => d.code === 'ANIM_IMAGE_MISSING' && d.pointer === '/data/attachments/3/sequence/frames/1'));
});

test('export: frames packed into atlas pages (one region per frame), clipped items draw from the page', async () => {
  // Pure packing first: every frame packed, sequence.atlas written in frame order.
  const all = images();
  const decoded = new Map(Object.entries(all).map(([id, bytes]) => [id, decodePng(bytes)]));
  const atlas = buildAtlas({ documents: [{ type: 'rig', id: 'hero', data: clipRig() }], images: decoded, settings: { maxPageSize: 512 }, pageId: i => `page-${i + 1}` });
  assert.deepEqual(atlas.packed, ['clip-back', 'clip-card', 'seq-a', 'seq-b', 'seq-c']);
  const packed = atlas.documents[0].data, film = packed.attachments[3];
  assert.equal(film.sequence.atlas.length, 3);
  film.sequence.atlas.forEach((r, i) => assert.deepEqual(r, atlas.regions[['seq-a', 'seq-b', 'seq-c'][i]]));
  assert.equal(packed.attachments[1].atlas, undefined, 'the clipping attachment has no picture');
  // Draw items: the packed rig names pages; the clipped card is a mesh over its page region with the same triangles.
  const request = await referenceRuntime();
  const items = (data, ms) => {
    request({ op: 'init', documents: [{ type: 'rig', id: 'hero', data }, { type: 'controller', id: 'hero-main', data: clipController }] });
    request({ op: 'mount', instance: 'a', element: { type: 'character', ref: 'hero-main', width: 300, height: 450 } });
    request({ op: 'update', dtMs: ms });
    return request({ op: 'frame', instance: 'a' }).items;
  };
  for (const [ms, frame] of [[50, 'seq-a'], [150, 'seq-b'], [250, 'seq-c']]) {
    const plain = items(clipRig(), ms), page = items(packed, ms);
    assert.equal(plain[2].imageAssetId, frame);
    assert.equal(page[2].imageAssetId, 'page-1');
    assert.deepEqual(page[2].region, [atlas.regions[frame].x, atlas.regions[frame].y, atlas.regions[frame].width, atlas.regions[frame].height], `${ms} ms: frame ${frame} region`);
    assert.ok(page[1].mesh && page[1].region, 'clipped card: mesh on the page region');
    assert.deepEqual(page[1].mesh.indices, plain[1].mesh.indices);
    page[1].mesh.positions.forEach((v, k) => assert.ok(Math.abs(v - plain[1].mesh.positions[k]) < 1e-9));
    page[1].mesh.uvs.forEach((v, k) => assert.ok(Math.abs(v - plain[1].mesh.uvs[k]) < 1e-9, 'uvs address the sub-image exactly as the whole image'));
  }
  // The real export: pages replace the frame files; clipping needs plugin-mesh-v1 only.
  const { root, editor } = await clipProject();
  const built = await editor.snapshot(editor.revision);
  assert.equal(built.ok, true, JSON.stringify(built.diagnostics));
  const runtimes = await fakeRuntimes(['plugin-elements-v1', 'plugin-mesh-v1', 'plugin-atlas-v1']);
  const extensions = await editor.exportExtensions(editor.revision, built.snapshot, 'web', ['plugin-elements-v1', 'plugin-mesh-v1', 'plugin-atlas-v1']);
  assert.deepEqual(extensions.features, ['plugin-atlas-v1', 'plugin-mesh-v1']);
  assert.deepEqual(extensions.reports['cutout-animation'].packed, ['clip-back', 'clip-card', 'seq-a', 'seq-b', 'seq-c']);
  const out = await temp('clip-export');
  const result = await exportGame({ content: built.snapshot, projectRoot: root, destination: path.join(out, 'game'), target: 'web', runtimes, extensions });
  const game = await readJson(path.join(result.directory, 'game.json'));
  const rig = game.extensions.plugins[0].documents.find(d => d.type === 'rig').data;
  assert.ok(rig.attachments[3].sequence.atlas.every(r => r && r.page.startsWith('cutout-atlas-')), 'every frame names its page region');
  for (const id of ['seq-a', 'seq-b', 'seq-c']) assert.equal(game.assets[id], undefined, `${id}: its pixels live in the page`);
  // A clipping-only rig (no mesh attachment) still asks for plugin-mesh-v1: clipped parts are meshes.
  const noSequence = clipRig();
  delete noSequence.attachments[3].sequence;
  noSequence.clips[0].tracks = [];
  const { editor: clipOnly } = await clipProject(noSequence);
  const snap = await clipOnly.snapshot(clipOnly.revision);
  assert.deepEqual((await clipOnly.exportExtensions(clipOnly.revision, snap.snapshot, 'web', ['plugin-elements-v1', 'plugin-mesh-v1'])).features, ['plugin-mesh-v1']);
});

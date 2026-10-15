// Physics in exported games (plugin 1.13.0): no host changes — the core computes the poses, so both
// players draw the simulated flag exactly where the core (Wasm reference) puts it, and the frame
// pacing never changes it: 60 Hz pieces and irregular pieces of the same time give identical items
// (native: equal compact numbers; web: equal documented numbers), and the pixels show the flag
// trailing the swinging pole (away from where it would hang without physics).
import test from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createServer } from 'node:http';
import { copyFile, mkdir, readFile, writeFile } from 'node:fs/promises';
import { extname, resolve, sep } from 'node:path';
import { chromium } from 'playwright';
import { EVIDENCE, makeGame, pluginElement, png, pngPixel, referenceRuntime, close, RED, BLUE } from './player-extension-fixture.mjs';

const INSTANCE = 'home/hero-anim';
/** 300x450 canvas = the element. A blue pole hanging from the root swings ±35° (600 ms); a red flag
 *  on its tip has rotate physics (it trails and overshoots the swing). */
function physicsRig({ physics = true } = {}) {
  const rig = { schemaVersion: 1, canvas: { width: 300, height: 450, originX: 150, originY: 440 },
    bones: [{ id: 'root', parent: null, x: 150, y: 80 }, { id: 'pole', parent: 'root', rotation: 90, length: 140 }, { id: 'flag', parent: 'pole', x: 140, length: 90 }],
    slots: [{ id: 'pole', bone: 'pole', attachment: 'pole' }, { id: 'flag', bone: 'flag', attachment: 'flag' }],
    attachments: [
      { slot: 'pole', name: 'pole', imageAssetId: 'phys-pole', width: 140, height: 12, pivotX: 0, pivotY: 0.5 },
      { slot: 'flag', name: 'flag', imageAssetId: 'phys-flag', width: 90, height: 30, pivotX: 0, pivotY: 0.5 }],
    clips: [{ id: 'swing', durationMs: 600, loop: true, tracks: [{ target: 'bone:pole', property: 'rotation', keys: [
      { timeMs: 0, value: 55, interp: 'bezier', curve: [0.42, 0, 0.58, 1] }, { timeMs: 300, value: 125, interp: 'bezier', curve: [0.42, 0, 0.58, 1] }, { timeMs: 600, value: 55 }] }] }] };
  if (physics) rig.physics = [{ id: 'flag-physics', bone: 'flag', rotate: 1, inertia: 1, strength: 30, damping: 0.95, gravity: 300 }];
  return rig;
}
const physicsController = { schemaVersion: 1, rig: 'hero', defaultClip: 'swing', layers: [], bindings: [] };
const images = () => ({ 'phys-pole': png(140, 12, BLUE), 'phys-flag': png(90, 30, RED) });
const CHECKPOINTS = [500, 1000];
const IRREGULAR = [7.3, 11.1, 3.9, 20, 0.25, 33, 5.45, 19, 16, 30];
/** Irregular pieces cut to land on every checkpoint. */
function irregularPieces() {
  const out = [];
  let now = 0, i = 0;
  for (const cp of CHECKPOINTS) {
    const pieces = [];
    while (cp - now > 1e-9) { const dt = Math.min(IRREGULAR[i++ % IRREGULAR.length], cp - now); pieces.push(dt); now += dt; }
    out.push(pieces);
  }
  return out;
}
/** Core (Wasm) items after `ms` at 60 Hz, with or without physics: what both players are given. */
async function reference(ms, physics = true) {
  const request = await referenceRuntime();
  request({ op: 'init', documents: [{ type: 'rig', id: 'hero', data: physicsRig({ physics }) }, { type: 'controller', id: 'hero-main', data: physicsController }] });
  request({ op: 'mount', instance: INSTANCE, element: { type: 'character', ref: 'hero-main', width: pluginElement.width, height: pluginElement.height, props: pluginElement.extension.props } });
  for (let t = 0; t < ms - 1e-9;) { const dt = Math.min(1000 / 60, ms - t); request({ op: 'update', dtMs: dt }); t += dt; }
  return request({ op: 'frame', instance: INSTANCE }).items;
}
/** Element px of image point (u, v) of an item. */
const at = (item, u, v) => [item.matrix[0] * u + item.matrix[2] * v + item.matrix[4], item.matrix[1] * u + item.matrix[3] * v + item.matrix[5]];
const FLAG_POINT = [70, 15];  // near the flag's free end
function checkAgainstReference(items, want, what) {
  assert.deepEqual(items.map(i => i.imageAssetId), ['phys-pole', 'phys-flag'], `${what}: items`);
  items.forEach((item, k) => item.matrix.forEach((v, j) => assert.ok(Math.abs(v - want[k].matrix[j]) < 2e-3, `${what}: ${item.slot} matrix[${j}] ${v} vs ${want[k].matrix[j]}`)));
}

test('native player: physics follows the core and never depends on frame pacing', { timeout: 240000 }, async t => {
  const runs = {};
  for (const pacing of ['60hz', 'irregular']) {
    const f = await makeGame({ target: 'windows', name: `native-physics-${pacing}`, documents: { rig: physicsRig(), controller: physicsController }, images: images() });
    t.diagnostic(`fixture ${pacing}: ${f.root}`);
    const steps = [{ click: 'start' }], shots = [];
    const pieces = irregularPieces();
    let last = 0;
    CHECKPOINTS.forEach((cp, i) => {
      if (pacing === '60hz') steps.push({ extensionAdvanceMs: cp - last, extensionSteps: Math.round((cp - last) * 60 / 1000) });
      else for (const dt of pieces[i]) steps.push({ extensionAdvanceMs: dt });
      last = cp;
      const shot = resolve(f.root, `physics-${i}.png`);
      shots.push(shot);
      steps.push({ checkpoint: `p${i}`, screenshot: shot });
    });
    const input = resolve(f.root, 'steps.json'), output = resolve(f.root, 'report.json');
    await writeFile(input, JSON.stringify(steps));
    const child = spawnSync(resolve(f.game, 'RaisingPlayer.exe'), ['--bundle', f.game, '--save', resolve(f.root, 'save.json'), '--seed', '42', '--hidden', '--automation', input, '--report', output], { encoding: 'utf8', timeout: 120000, windowsHide: true });
    assert.equal(child.status, 0, child.stderr);
    const report = JSON.parse(await readFile(output, 'utf8'));
    assert.equal(report.ok, true, report.error);
    runs[pacing] = CHECKPOINTS.map((_, i) => report.steps.find(s => s.command.checkpoint === `p${i}`).ui.extensions.instances.find(x => x.instance === INSTANCE));
    runs[pacing].shots = shots;
  }
  for (const [i, cp] of CHECKPOINTS.entries()) {
    const a = runs['60hz'][i], b = runs.irregular[i];
    assert.equal(a.available, true, a.error);
    assert.deepEqual(b.items, a.items, `${cp} ms: 60 Hz and irregular pacing draw the same items`);
    checkAgainstReference(a.items, await reference(cp), `native ${cp} ms`);
    // The flag is where the simulation put it, visibly off its no-physics place.
    const flag = a.items[1], plain = (await reference(cp, false))[1];
    const [x, y] = at(flag, ...FLAG_POINT), [px, py] = at(plain, ...FLAG_POINT);
    assert.ok(Math.hypot(x - px, y - py) > 10, `${cp} ms: physics moved the flag (${Math.hypot(x - px, y - py).toFixed(1)} px)`);
    for (const pacing of ['60hz', 'irregular']) {
      const shot = await readFile(runs[pacing].shots[i]);
      const rgb = pngPixel(shot, Math.round(pluginElement.x + x), Math.round(pluginElement.y + y));
      assert.ok(close(rgb, RED), `${pacing} ${cp} ms: red flag pixel at the simulated place ${rgb}`);
    }
  }
  await mkdir(resolve(EVIDENCE, 'v4'), { recursive: true });
  await copyFile(runs['60hz'].shots[0], resolve(EVIDENCE, 'v4', 'windows-physics.png'));
});

test('web player: physics follows the core and never depends on frame pacing', { timeout: 240000 }, async t => {
  const f = await makeGame({ target: 'web', name: 'web-physics', documents: { rig: physicsRig(), controller: physicsController }, images: images() });
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
  const runs = {};
  for (const pacing of ['60hz', 'irregular']) {
    const page = await browser.newPage({ viewport: { width: 1280, height: 760 } });
    const errors = []; page.on('pageerror', e => errors.push(e.message));
    await page.addInitScript(() => { window.raisingExtensionManualClock = true; });
    await page.goto(`http://127.0.0.1:${server.address().port}/`);
    await page.locator('[data-element-id="start"] button').click();
    await page.locator('[data-element-id="cheer"] button').waitFor();
    const pieces = irregularPieces();
    runs[pacing] = [];
    let last = 0;
    for (const [i, cp] of CHECKPOINTS.entries()) {
      const dts = pacing === '60hz' ? Array.from({ length: Math.round((cp - last) * 60 / 1000) }, () => (cp - last) / Math.round((cp - last) * 60 / 1000)) : pieces[i];
      last = cp;
      await page.evaluate(list => { for (const dt of list) window.raisingExtensions.tick(dt); }, dts);
      await page.waitForFunction(() => [...document.querySelectorAll('.visual-plugin-layer img')].every(i => i.complete && i.naturalWidth > 0), null, { timeout: 30000 });
      const d = await page.evaluate(id => window.raisingExtensions.describe().instances.find(x => x.instance === id), INSTANCE);
      const map = await page.evaluate(() => { const el = document.querySelector('[data-element-id="hero-anim"]').getBoundingClientRect(), stage = document.querySelector('.visual-stage'); return { x: el.x, y: el.y, scale: stage.getBoundingClientRect().width / stage.offsetWidth }; });
      const shot = await page.screenshot();
      runs[pacing].push({ items: d.items, map, shot });
      if (pacing === '60hz' && i === 0) { await mkdir(resolve(EVIDENCE, 'v4'), { recursive: true }); await writeFile(resolve(EVIDENCE, 'v4', 'web-physics.png'), shot); }
    }
    assert.deepEqual(errors, []);
    await page.close();
  }
  for (const [i, cp] of CHECKPOINTS.entries()) {
    const a = runs['60hz'][i], b = runs.irregular[i];
    assert.deepEqual(b.items, a.items, `${cp} ms: 60 Hz and irregular ticks draw the same items`);
    checkAgainstReference(a.items, await reference(cp), `web ${cp} ms`);
    const [x, y] = at(a.items[1], ...FLAG_POINT);
    for (const run of [a, b]) {
      const rgb = pngPixel(run.shot, Math.round(run.map.x + x * run.map.scale), Math.round(run.map.y + y * run.map.scale));
      assert.ok(close(rgb, RED), `web ${cp} ms: red flag pixel at the simulated place ${rgb}`);
    }
  }
});

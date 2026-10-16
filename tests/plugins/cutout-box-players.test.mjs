// Bounding boxes and points in exported games (plugin 1.17.0): a real mouse click on the plugin
// element of the native player (SDL mouse events → RmlUi click → ExtensionElement::observe_click)
// and of the web player (page.mouse → the layer's capturing listener) is hit-tested by the core;
// a click on the head box fires the controller binding `partClicked {box: head}`, a miss does
// nothing. Both players then hold exactly the clip state and draw exactly the items the core
// (Wasm reference) computes for that click, the screen (game state) is untouched, and both report
// the point attachment's element position before and after the head moved (UI anchors).
import test from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createServer } from 'node:http';
import { copyFile, mkdir, readFile, writeFile } from 'node:fs/promises';
import { extname, resolve, sep } from 'node:path';
import { chromium } from 'playwright';
import { EVIDENCE, makeGame, pluginElement, png, pngPixel, referenceRuntime, close, BLUE } from './player-extension-fixture.mjs';

const INSTANCE = 'home/hero-anim';
/** 300x450 canvas = the element (element px = rig px). The head (blue 80x80) at (150, 100) with a
 *  ±40 px box and a `mouth` point 20 px below its centre; `nod` slides it 60 px right and holds. */
const boxRig = {
  schemaVersion: 1, canvas: { width: 300, height: 450, originX: 150, originY: 440 },
  bones: [{ id: 'root', parent: null, x: 150, y: 440 }, { id: 'head', parent: 'root', y: -340 }],
  slots: [{ id: 'head', bone: 'head', attachment: 'head' }, { id: 'head-box', bone: 'head', attachment: 'head' }, { id: 'mouth', bone: 'head', attachment: 'mouth' }],
  attachments: [
    { slot: 'head', name: 'head', imageAssetId: 'box-head', width: 80, height: 80 },
    { slot: 'head-box', name: 'head', type: 'boundingBox', vertices: [-40, -40, 40, -40, 40, 40, -40, 40] },
    { slot: 'mouth', name: 'mouth', type: 'point', y: 20, rotation: 90 }],
  clips: [
    { id: 'idle', durationMs: 1000, loop: true, tracks: [] },
    { id: 'nod', durationMs: 300, loop: false, tracks: [{ target: 'bone:head', property: 'x', keys: [{ timeMs: 0, value: 0 }, { timeMs: 300, value: 60 }] }] }] };
const boxController = { schemaVersion: 1, rig: 'hero', defaultClip: 'idle', layers: [],
  bindings: [{ id: 'pat-head', signal: 'partClicked', match: { box: 'head' }, clip: 'nod', then: 'hold' }] };
const images = () => ({ 'box-head': png(80, 80, BLUE) });
const HEAD = [150, 100], MISS = [20, 420];
/** The core's state for the same story: 100 ms, a miss, 50 ms, the head click, 300 ms. */
async function reference() {
  const request = await referenceRuntime();
  request({ op: 'init', documents: [{ type: 'rig', id: 'hero', data: boxRig }, { type: 'controller', id: 'hero-main', data: boxController }] });
  const mount = request({ op: 'mount', instance: INSTANCE, element: { type: 'character', ref: 'hero-main', width: pluginElement.width, height: pluginElement.height, props: pluginElement.extension.props } });
  assert.equal(mount.interactive, true, 'a rig with boxes mounts interactive');
  const out = {};
  request({ op: 'update', dtMs: 100 });
  out.before = { points: request({ op: 'points', instance: INSTANCE }).points, items: request({ op: 'frame', instance: INSTANCE }).items };
  out.miss = request({ op: 'hitTest', instance: INSTANCE, x: MISS[0], y: MISS[1], dispatch: true });
  request({ op: 'update', dtMs: 50 });
  out.hit = request({ op: 'hitTest', instance: INSTANCE, x: HEAD[0], y: HEAD[1], dispatch: true });
  for (let i = 0; i < 18; i++) request({ op: 'update', dtMs: 300 / 18 });
  out.after = { points: request({ op: 'points', instance: INSTANCE }).points, items: request({ op: 'frame', instance: INSTANCE }).items,
    tracks: request({ op: 'describeAll' }).instances[INSTANCE].tracks.map(t => [t.track, t.clip]) };
  return out;
}
const matrixClose = (items, want, what) => {
  assert.deepEqual(items.map(i => i.imageAssetId), want.map(i => i.imageAssetId), `${what}: items (boxes and points never draw)`);
  items.forEach((item, k) => item.matrix.forEach((v, j) => assert.ok(Math.abs(v - want[k].matrix[j]) < 2e-3, `${what}: matrix[${j}] ${v} vs ${want[k].matrix[j]}`)));
};
const pointsClose = (got, want, what) => {
  assert.deepEqual(got.map(p => [p.slot, p.name]), want.map(p => [p.slot, p.name]), `${what}: points`);
  got.forEach((p, k) => { for (const f of ['x', 'y', 'rotation']) assert.ok(Math.abs(p[f] - want[k][f]) < 1e-6, `${what}: ${p.slot}.${f} ${p[f]} vs ${want[k][f]}`); });
};
function checkStory(run, ref, what) {
  assert.equal(run.before.available, true, run.before.error);
  assert.equal(run.before.interactive, true, `${what}: interactive mount reported`);
  pointsClose(run.before.points, ref.before.points, `${what} before`);
  assert.deepEqual(run.before.points.map(p => [p.x, p.y]), [[150, 120]], `${what}: the mouth anchor in element px`);
  matrixClose(run.before.items, ref.before.items, `${what} before`);
  assert.deepEqual(run.missed.lastClick?.hit ?? null, null, `${what}: the miss hit nothing`);
  assert.deepEqual(run.missed.state.tracks.map(t => [t.track, t.clip]), [[0, 'idle']], `${what}: a miss starts nothing`);
  assert.deepEqual(run.after.lastClick.hit, ref.hit.hit, `${what}: the head box was hit`);
  assert.deepEqual(run.after.lastClick.matched, ['pat-head'], `${what}: partClicked {box: head} matched`);
  assert.deepEqual(run.after.state.tracks.map(t => [t.track, t.clip]), ref.after.tracks, `${what}: same clip state as the core`);
  matrixClose(run.after.items, ref.after.items, `${what} after`);
  pointsClose(run.after.points, ref.after.points, `${what} after`);
  assert.deepEqual(run.after.points.map(p => [p.x, p.y]), [[210, 120]], `${what}: the anchor followed the head`);
}

test('native player: a click on a bounding box starts the bound clip like the core; points are reported', { timeout: 240000 }, async t => {
  const ref = await reference();
  const f = await makeGame({ target: 'windows', name: 'native-boxes', documents: { rig: boxRig, controller: boxController }, images: images() });
  t.diagnostic(`fixture: ${f.root}`);
  const shot = resolve(f.root, 'boxes.png');
  const element = 'extension-hero-anim';
  const steps = [{ click: 'start' }, { extensionAdvanceMs: 100 }, { checkpoint: 'before' },
    { clickPoint: { id: element, x: MISS[0], y: MISS[1] } }, { extensionAdvanceMs: 50 }, { checkpoint: 'missed' },
    { clickPoint: { id: element, x: HEAD[0], y: HEAD[1] } }, { extensionAdvanceMs: 300, extensionSteps: 18 }, { checkpoint: 'after', screenshot: shot, expectScreen: 'home' }];
  const input = resolve(f.root, 'steps.json'), output = resolve(f.root, 'report.json');
  await writeFile(input, JSON.stringify(steps));
  const child = spawnSync(resolve(f.game, 'RaisingPlayer.exe'), ['--bundle', f.game, '--save', resolve(f.root, 'save.json'), '--seed', '42', '--hidden', '--automation', input, '--report', output], { encoding: 'utf8', timeout: 120000, windowsHide: true });
  assert.equal(child.status, 0, child.stderr);
  const report = JSON.parse(await readFile(output, 'utf8'));
  assert.equal(report.ok, true, report.error);
  const at = name => report.steps.find(s => s.command.checkpoint === name).ui.extensions.instances.find(x => x.instance === INSTANCE);
  const run = { before: at('before'), missed: at('missed'), after: at('after') };
  checkStory(run, ref, 'native');
  assert.equal(report.steps.find(s => s.command.checkpoint === 'after').ui.extensions.clicks, 2, 'both clicks reached the plugin');
  // The moved head is drawn where the core puts it (pixels), and the game state never saw the clicks.
  const rgb = pngPixel(await readFile(shot), pluginElement.x + 210, pluginElement.y + 100);
  assert.ok(close(rgb, BLUE), `blue head at its nodded place ${rgb}`);
  const ui = report.steps.find(s => s.command.checkpoint === 'before').ui, after = report.steps.find(s => s.command.checkpoint === 'after').ui;
  assert.equal(JSON.stringify(after.state ?? null), JSON.stringify(ui.state ?? null), 'clicks never change the game state');
  assert.deepEqual([after.screenId, after.commands], [ui.screenId, ui.commands], 'no command was submitted');
  await mkdir(resolve(EVIDENCE, 'v6'), { recursive: true });
  await copyFile(shot, resolve(EVIDENCE, 'v6', 'windows-boxes.png'));
});

test('web player: a click on a bounding box starts the bound clip like the core; points are reported', { timeout: 240000 }, async t => {
  const ref = await reference();
  const f = await makeGame({ target: 'web', name: 'web-boxes', documents: { rig: boxRig, controller: boxController }, images: images() });
  t.diagnostic(`fixture: ${f.root}`);
  const mime = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.wasm': 'application/wasm', '.png': 'image/png', '.ttf': 'font/ttf', '.otf': 'font/otf', '.svg': 'image/svg+xml', '.wav': 'audio/wav' };
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
  const describe = () => page.evaluate(id => window.raisingExtensions.describe().instances.find(x => x.instance === id), INSTANCE);
  const map = await page.evaluate(() => { const el = document.querySelector('[data-element-id="hero-anim"]').getBoundingClientRect(), stage = document.querySelector('.visual-stage'); return { x: el.x, y: el.y, scale: stage.getBoundingClientRect().width / stage.offsetWidth }; });
  const clickAt = ([x, y]) => page.mouse.click(map.x + x * map.scale, map.y + y * map.scale);
  const tick = (ms, n = 1) => page.evaluate(([dt, count]) => { for (let i = 0; i < count; i++) window.raisingExtensions.tick(dt); }, [ms / n, n]);
  const screen = () => page.evaluate(() => document.querySelector('.visual-stage')?.getAttribute('data-screen') ?? null);
  const screenBefore = await screen();
  await tick(100);
  const run = { before: await describe() };
  await clickAt(MISS);
  await tick(50);
  run.missed = await describe();
  await clickAt(HEAD);
  await tick(300, 18);
  await page.waitForFunction(() => [...document.querySelectorAll('.visual-plugin-layer img')].every(i => i.complete && i.naturalWidth > 0), null, { timeout: 30000 });
  run.after = await describe();
  checkStory(run, ref, 'web');
  assert.equal(await page.evaluate(() => window.raisingExtensions.describe().clicks), 2, 'both clicks reached the plugin');
  assert.equal(await screen(), screenBefore, 'clicks never change the screen');
  const shot = await page.screenshot();
  const rgb = pngPixel(shot, Math.round(map.x + 210 * map.scale), Math.round(map.y + 100 * map.scale));
  assert.ok(close(rgb, BLUE), `blue head at its nodded place ${rgb}`);
  await mkdir(resolve(EVIDENCE, 'v6'), { recursive: true });
  await writeFile(resolve(EVIDENCE, 'v6', 'web-boxes.png'), shot);
  assert.deepEqual(errors, []);
});

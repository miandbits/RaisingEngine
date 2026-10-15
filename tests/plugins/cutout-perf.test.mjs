// Large-rig performance (plugin 1.9.0): the stress character (tests/plugins/stress-fixture.mjs) through
// every layer, medians of N on this machine, with regression budgets ~2-3x the measured values so CI
// here catches large regressions without flaking. Writes .cache/anim-perf/report.{json,md}.
//   core (native)   cutout-core-tests --perf: validate, parse, evaluate+drawList, ABI frames, edit ops
//   core (Wasm)     the web/service runtime: documented vs compact frames for 1 and 20 instances
//   service         Qt studio service: one rig edit request -> validated reply (what every gesture costs)
//   players         20 characters: frame request + reply parse + decode per presentation frame
//   Qt panel        studio-tests largeRigPerformance (repaints, refresh after an edit, gestures)
import test, { after } from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import { createServer } from 'node:http';
import path, { resolve, extname, sep } from 'node:path';
import { pathToFileURL } from 'node:url';
import { QtStudioService } from '../../apps/qt-studio/service.mjs';
import { addCutout, cutoutPackage, newProject, temp, png, serialize } from './host-helpers.mjs';
import { CORE_TESTS, makeStressGame, stressInstance, stressRig } from './stress-fixture.mjs';

const OUT = resolve('.cache/anim-perf');
const results = {};
const median = (runs, body) => { body(); const ms = []; for (let i = 0; i < runs; i++) { const t = performance.now(); body(); ms.push(performance.now() - t); } ms.sort((a, b) => a - b); return ms[ms.length >> 1]; };
const within = (value, budget, what) => assert.ok(value <= budget, `${what}: ${value.toFixed(3)} > budget ${budget}`);

after(async () => {
  await mkdir(OUT, { recursive: true });
  await writeFile(path.join(OUT, 'report.json'), JSON.stringify(results, null, 2));
  const rows = [];
  const add = (layer, what, value, unit = 'ms') => { if (value !== undefined) rows.push(`| ${layer} | ${what} | ${typeof value === 'number' ? (unit === 'B' ? value : value.toFixed(3)) : value} ${unit} |`); };
  const c = results.core;
  if (c) {
    for (const [k, label] of [['validateMs', 'validate'], ['parseMs', 'parse (validate + build)'], ['evaluate1Ms', 'evaluate + drawList, 1 instance'], ['evaluate20Ms', 'evaluate + drawList, 20 instances'],
      ['host1Ms', 'ABI frame (documented items), 1'], ['host20Ms', 'ABI frame (documented items), 20'], ['host1V2Ms', 'ABI frame (compact), 1'], ['host20V2Ms', 'ABI frame (compact), 20'],
      ['nativeParseMs', 'nlohmann parse of 1 reply (documented)'], ['nativeParseV2Ms', 'nlohmann parse of 1 reply (compact)']]) add('core native', label, c[k]);
    for (const [k, label] of [['framePayloadBytes', 'frame reply (documented)'], ['framePayloadV2Bytes', 'frame reply (compact, animating)'], ['framePayloadV2IdleBytes', 'frame reply (compact, paused)']]) add('core native', label, c[k], 'B');
    for (const [k, v] of Object.entries(c.edit ?? {})) add('core native', `edit ${k.replace(/Ms$/, '')}`, v);
  }
  for (const [k, v] of Object.entries(results.wasm ?? {})) add('core Wasm', k, v, /Bytes$/.test(k) ? 'B' : 'ms');
  for (const [k, v] of Object.entries(results.service ?? {})) add('service', k, v, /Bytes$/.test(k) ? 'B' : 'ms');
  for (const [k, v] of Object.entries(results.native ?? {})) add('native player', k, v, /Bytes$/.test(k) ? 'B' : 'ms');
  for (const [k, v] of Object.entries(results.web ?? {})) add('web player', k, v, /Bytes$/.test(k) ? 'B' : 'ms');
  for (const [k, v] of Object.entries(results.qt ?? {})) add('Qt panel', k, v, k === 'dopesheetKeys' ? '' : 'ms');
  await writeFile(path.join(OUT, 'report.md'), `# Large-rig performance (${new Date().toISOString()})\n\n${results.core ? `Rig: ${JSON.stringify(results.core.rig)}\n\n` : ''}| layer | measurement | median |\n|---|---|---|\n${rows.join('\n')}\n`);
});

test('core (native): large-rig medians stay within their budgets', { timeout: 300000 }, () => {
  assert.ok(existsSync(CORE_TESTS), 'build first: node plugins/cutout-animation/tools/build.mjs win wasm test');
  const r = spawnSync(CORE_TESTS, ['--perf'], { encoding: 'utf8', maxBuffer: 16 * 1024 * 1024 });
  results.core = JSON.parse(r.stdout.trim());
  assert.equal(r.status, 0, r.stderr);
  const c = results.core;
  assert.deepEqual([c.rig.bones, c.rig.slots, c.rig.meshes, c.rig.clips, c.rig.denseClipKeys >= 3000, c.rig.weightedVertices >= 1000], [200, 150, 40, 30, true, true]);
  // Compact frames: a quarter of the documented payload, a paused instance sends only "unchanged".
  assert.ok(c.framePayloadV2Bytes * 3 < c.framePayloadBytes, JSON.stringify(c));
  assert.ok(c.framePayloadV2IdleBytes < 1000);
});

/** The runtime module as the service and the web player load it (heap-passed requests). */
async function wasmRuntime() {
  const { default: factory } = await import(pathToFileURL(resolve('dist/cutout/build-wasm/cutout-runtime.mjs')).href);
  const m = await factory();
  assert.equal(typeof m.stringToUTF8, 'function', 'runtime exports the heap helpers');
  const raw = text => { const n = m.lengthBytesUTF8(text) + 1, at = m._malloc(n); try { m.stringToUTF8(text, at, n); return m.UTF8ToString(m._raising_extension_request(at)); } finally { m._free(at); } };
  return { raw, request: v => JSON.parse(raw(JSON.stringify(v))) };
}

test('core (Wasm): compact frames, 1 and 20 instances, and requests larger than the Wasm stack', { timeout: 300000 }, async () => {
  const { rig, controller } = stressRig();
  const { raw, request } = await wasmRuntime();
  const init = request({ op: 'init', frameFormats: [2], documents: [{ type: 'rig', id: 'stress', data: rig }, { type: 'controller', id: 'stress-main', data: { ...controller, rig: 'stress' } }] });
  assert.equal(init.ok, true, init.error);
  assert.equal(init.frameFormat, 2);
  assert.equal(init.meshes.length, 40, 'one topology per mesh attachment');
  for (let i = 0; i < 20; i++) assert.equal(request({ op: 'mount', instance: `i${i}`, element: { type: 'character', ref: 'stress-main', width: 400, height: 400, props: {} } }).ok, true);
  const w = results.wasm = {};
  for (const format of [1, 2]) for (const n of [1, 20]) {
    let bytes = 0; const since = [];
    // One presentation frame as the web player runs it: update, then frame + JSON.parse per instance.
    w[`frame${n}${format === 2 ? 'Compact' : 'Documented'}Ms`] = median(30, () => {
      raw(JSON.stringify({ op: 'update', dtMs: 16.7 })); bytes = 0;
      for (let i = 0; i < n; i++) {
        const text = raw(JSON.stringify(format === 2 ? { op: 'frame', instance: `i${i}`, format: 2, ...(since[i] ? { since: since[i] } : {}) } : { op: 'frame', instance: `i${i}` }));
        bytes += text.length; since[i] = JSON.parse(text).frame;
      }
    });
    if (n === 1) w[`frame${format === 2 ? 'Compact' : 'Documented'}Bytes`] = bytes;
  }
  const documented = raw(JSON.stringify({ op: 'frame', instance: 'i0' })), compact = raw(JSON.stringify({ op: 'frame', instance: 'i0', format: 2 }));
  w.webParseDocumentedMs = median(50, () => JSON.parse(documented));
  w.webParseCompactMs = median(50, () => JSON.parse(compact));
  w.validateMs = median(5, () => request({ op: 'validate', rig }));
  // Requests above the 1 MB Wasm stack (cwrap would overflow it and break the module).
  const big = structuredClone(rig);
  for (const clip of big.clips.slice(1)) clip.name = 'x'.repeat(40000);
  assert.ok(JSON.stringify(big).length > 1.5 * 1024 * 1024);
  assert.equal(request({ op: 'validate', rig: big }).ok, true);
  assert.equal(request({ op: 'version' }).ok, true, 'module still healthy');
  // Compact frames carry the same pictures as the documented items (rounded to 1/1000 px).
  const a = JSON.parse(documented).items, b = JSON.parse(compact).items;
  assert.equal(a.length, b.length);
  a.forEach((item, k) => {
    const e = b[k];
    assert.equal(e[0], item.imageAssetId);
    item.matrix.forEach((v, i) => assert.ok(Math.abs(v - e[1 + i]) <= (i < 4 ? 6e-7 : 6e-4), `item ${k} matrix ${i}`));
    if (item.mesh) item.mesh.positions.forEach((v, i) => assert.ok(Math.abs(v - e[12][i]) <= 6e-4));
    if (item.mesh) assert.deepEqual(init.meshes[e[11]].indices, item.mesh.indices);
  });
  for (const [k, budget] of [['frame1DocumentedMs', 15], ['frame20DocumentedMs', 250], ['frame1CompactMs', 4], ['frame20CompactMs', 60], ['webParseCompactMs', 1], ['validateMs', 200]]) within(w[k], budget, k);
  assert.ok(w.frame20CompactMs * 2.5 < w.frame20DocumentedMs, JSON.stringify(w));
});

test('service: a large rig edit round trip (Qt studio service, stdio reply)', { timeout: 600000 }, async () => {
  const { rig, controller, images } = stressRig();
  const root = await newProject(), store = await temp('store');
  await addCutout(root, { rig, controller: { ...controller, rig: 'hero' }, image: null });
  const manifestFile = path.join(root, 'assets/manifest.json'), manifest = JSON.parse(await readFile(manifestFile, 'utf8'));
  await mkdir(path.join(root, 'assets/stress'), { recursive: true });
  for (const id of images) { await writeFile(path.join(root, `assets/stress/${id}.png`), png(8, 8)); manifest.assets.push({ id, kind: 'image', path: `assets/stress/${id}.png` }); }
  await writeFile(manifestFile, serialize(manifest));
  // The indented file of this rig is ~2.7 MB: plugin documents may be up to 8 MiB (loader + editor).
  assert.ok((await readFile(path.join(root, 'plugins/cutout-animation/rig/hero.json'))).length > 2 * 1024 * 1024);
  const service = new QtStudioService({ pluginStore: store, shareViews: true });
  await service.handle({ method: 'pluginInstall', params: { file: await cutoutPackage() } });
  let reply = await service.handle({ method: 'open', params: { root } });
  assert.deepEqual(reply.view.diagnostics.filter(d => d.severity === 'error'), []);
  const file = 'plugins/cutout-animation/rig/hero.json';
  const doc = structuredClone(reply.view.documents.find(d => d.file === file));
  assert.ok(doc, 'large rig loaded');
  const times = [];
  for (let i = 0; i < 6; i++) {
    doc.data.data.clips[1].tracks[0].keys[1].value = i;
    const line = JSON.stringify({ id: i, method: 'edit', params: { revision: reply.view.revision, edit: { type: 'replace', file, data: doc.data } } });
    const t = performance.now();
    reply = await service.handle(JSON.parse(line));
    const text = JSON.stringify({ id: i, ...reply });
    times.push(performance.now() - t);
    results.service = { ...(results.service ?? {}), replyBytes: text.length, requestBytes: line.length };
    assert.equal(reply.error, null);
  }
  times.sort((a, b) => a - b);
  results.service.editRoundTripMs = times[3];
  // Diagnostics still point at lines of the saved (indented) file: the location of a pointer deep in
  // the rig matches a full parse of that text.
  const warning = reply.view.diagnostics.find(d => d.file === file && d.pointer.startsWith('/data/attachments/'));
  assert.ok(warning && warning.line > 1, JSON.stringify(warning));
  reply = await service.handle({ method: 'save', params: { revision: reply.view.revision } });
  const reopened = await new QtStudioService({ pluginStore: store }).handle({ method: 'open', params: { root } });
  assert.deepEqual(reopened.view.documents.find(d => d.file === file).data.data.clips[1].tracks[0].keys[1].value, 5, 'saved and reloaded');
  within(results.service.editRoundTripMs, 500, 'editRoundTripMs');
});

async function serve(t, root) {
  const mime = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.css': 'text/css', '.json': 'application/json', '.wasm': 'application/wasm', '.png': 'image/png', '.ttf': 'font/ttf', '.wav': 'audio/wav' };
  const server = createServer(async (req, res) => {
    try {
      const pathname = decodeURIComponent(new URL(req.url, 'http://127.0.0.1').pathname), file = resolve(root, `.${pathname === '/' ? '/index.html' : pathname}`);
      if (!file.startsWith(`${root}${sep}`)) { res.writeHead(403); res.end(); return; }
      const bytes = await readFile(file);
      res.writeHead(200, { 'Content-Type': mime[extname(file)] ?? 'application/octet-stream', 'Cache-Control': 'no-store' }); res.end(bytes);
    } catch { res.writeHead(404); res.end(); }
  });
  await new Promise(done => server.listen(0, '127.0.0.1', done)); t.after(() => { server.closeAllConnections(); server.close(); });
  return `http://127.0.0.1:${server.address().port}/`;
}

test('native player: 20 large characters per frame (compact frames vs documented items)', { timeout: 600000 }, async t => {
  const f = await makeStressGame({ target: 'windows', name: 'perf-native' });
  t.diagnostic(`fixture: ${f.root}`);
  const steps = [{ click: 'start' }, { checkpoint: 'home' }, { extensionAdvanceMs: 1000, extensionSteps: 60 }, { checkpoint: 'done' }];
  const n = results.native = {};
  for (const [label, env] of [['Compact', {}], ['Documented', { RAISING_EXTENSION_FRAME_FORMATS: '1' }]]) {
    const input = resolve(f.root, `${label}-steps.json`), output = resolve(f.root, `${label}-report.json`);
    await writeFile(input, JSON.stringify(steps));
    const child = spawnSync(resolve(f.game, 'RaisingPlayer.exe'), ['--bundle', f.game, '--save', resolve(f.root, `${label}-save.json`), '--seed', '1', '--hidden', '--automation', input, '--report', output],
      { encoding: 'utf8', timeout: 300000, windowsHide: true, env: { ...process.env, ...env } });
    assert.equal(child.status, 0, child.error?.message ?? child.stderr);
    const report = JSON.parse(await readFile(output, 'utf8'));
    assert.equal(report.ok, true, report.error);
    const at = tag => report.steps.find(s => s.command.checkpoint === tag).ui.extensions;
    const before = at('home').timing, done = at('done');
    assert.equal(done.timing.frameFormats, label === 'Compact' ? '2' : '1');
    assert.equal(done.instances.length, 20);
    assert.ok(done.instances.every(i => i.available && i.items.length === 149), 'every character draws all its parts');
    const frames = done.timing.frames - before.frames;
    // 60 advance steps plus the player's own frames in between: always whole frames of 20 instances.
    assert.ok(frames >= 60 * 20 && frames % 20 === 0, String(frames));
    // One presentation frame = 20 frame replies (request, reply parse, decode into draw items).
    n[`frame20${label}Ms`] = (done.timing.frameMs - before.frameMs) / (frames / 20);
    n[`reply${label}Bytes`] = Math.round((done.timing.bytes - before.bytes) / frames);
    if (label === 'Compact') {
      const mesh = done.instances[0].items.find(i => i.mesh);
      assert.ok(mesh.mesh.indices.length > 0 && mesh.mesh.uvs.length === mesh.mesh.positions.length, 'topology from the init table');
    }
  }
  // ~9 ms measured (95 ms with documented items: nlohmann reads every number through strtod).
  within(n.frame20CompactMs, 27, 'frame20CompactMs');
  assert.ok(n.frame20CompactMs * 4 < n.frame20DocumentedMs, JSON.stringify(n));
});

test('web player: 20 large characters per frame (compact frames vs documented items)', { timeout: 600000 }, async t => {
  const { chromium } = await import('playwright');
  const f = await makeStressGame({ target: 'web', name: 'perf-web' });
  const url = await serve(t, f.game);
  const browser = await chromium.launch({ headless: true, channel: process.env.PLAYWRIGHT_CHANNEL ?? 'chrome' }); t.after(() => browser.close());
  const w = results.web = {};
  for (const [label, formats] of [['Compact', [2]], ['Documented', [1]]]) {
    const page = await browser.newPage({ viewport: { width: 1280, height: 760 } });
    const errors = []; page.on('pageerror', e => errors.push(e.message));
    await page.addInitScript(f => { window.raisingExtensionManualClock = true; window.raisingExtensionFrameFormats = f; }, formats);
    await page.goto(url); await page.locator('[data-element-id="start"] button').click();
    await page.waitForFunction(() => window.raisingExtensions?.describe().instances.filter(i => i.available).length === 20, null, { timeout: 60000 });
    const measured = await page.evaluate(() => {
      const host = window.raisingExtensions;
      for (let i = 0; i < 5; i++) host.tick(16.7);
      const before = { ...host.describe().timing };
      const t0 = performance.now();
      for (let i = 0; i < 60; i++) host.tick(16.7);
      const wall = (performance.now() - t0) / 60, d = host.describe();
      return { wall, frames: d.timing.frames - before.frames, ms: d.timing.frameMs - before.frameMs, bytes: d.timing.bytes - before.bytes, format: d.plugins[0].frameFormat, heap: d.plugins[0].heap,
        items: d.instances.map(i => i.items.length), meshes: d.instances[0].items.filter(i => i.mesh && i.mesh.indices.length).length };
    });
    assert.equal(measured.format, formats[0]); assert.equal(measured.heap, true, 'init with the large rig went through heap memory');
    assert.equal(measured.frames, 60 * 20);
    assert.ok(measured.items.every(n => n === 149) && measured.meshes === 40, JSON.stringify(measured));
    w[`frame20${label}Ms`] = measured.ms / 60;
    w[`tick20${label}Ms`] = measured.wall;
    w[`reply${label}Bytes`] = Math.round(measured.bytes / measured.frames);
    await page.locator('canvas.visual-plugin-mesh[data-drawn="true"]').first().waitFor();
    assert.deepEqual(errors, []);
    await page.close();
  }
  // ~18 ms measured (64 ms with documented items).
  within(w.frame20CompactMs, 55, 'frame20CompactMs');
  assert.ok(w.frame20CompactMs * 2 < w.frame20DocumentedMs, JSON.stringify(w));
});

test('Qt panel: large-rig repaints, refresh after an edit and gestures (studio-tests)', { timeout: 600000 }, async () => {
  const exe = resolve('dist/cutout/build-win/studio/cutout-studio-tests.exe');
  assert.ok(existsSync(exe), 'build first: node plugins/cutout-animation/tools/build.mjs win studio');
  const out = resolve(OUT, 'qt.json'), log = resolve(OUT, 'qt-tests.txt');
  await mkdir(OUT, { recursive: true });
  const qt = process.env.QT_ROOT ?? 'C:/Qt/6.10.1/mingw_64';
  const r = spawnSync(exe, ['largeRigPerformance', '-o', `${log},txt`], { encoding: 'utf8', timeout: 300000, windowsHide: true,
    env: { ...process.env, CUTOUT_PERF_OUT: out, QT_QPA_PLATFORM: 'offscreen', PATH: `${qt}/bin;${process.env.PATH}` } });
  const text = existsSync(log) ? await readFile(log, 'utf8') : '';
  assert.equal(r.status, 0, text || r.stderr);  // budgets are checked inside the QtTest
  results.qt = JSON.parse(await readFile(out, 'utf8'));
});

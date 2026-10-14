import test from 'node:test';
import assert from 'node:assert/strict';
import { cp, mkdir, mkdtemp, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import { inflateSync } from 'node:zlib';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { exportGame } from '../../dist/packages/export-service/export.js';

async function workspace(name) {
  await mkdir(resolve('.cache/native-render-tests'), { recursive: true });
  return mkdtemp(resolve(`.cache/native-render-tests/${name}-`));
}
async function bundle(root, projectRoot) {
  const built = await buildSnapshot(await loadProject(projectRoot));
  assert.equal(built.ok, true, JSON.stringify(built.diagnostics));
  const destination = resolve(root, 'game');
  await exportGame({ content: built.snapshot, projectRoot, destination, target: 'windows',
    runtimes: { windows: resolve('dist/native-player-app'), web: resolve('dist/web') } });
  return destination;
}
async function run(root, destination, steps) {
  const input = resolve(root, 'steps.json'), reportFile = resolve(root, 'report.json');
  await writeFile(input, JSON.stringify(steps));
  const child = spawnSync(resolve(destination, 'RaisingPlayer.exe'), ['--bundle', destination, '--save', resolve(root, 'save.json'),
    '--hidden', '--automation', input, '--report', reportFile], { encoding: 'utf8', timeout: 60000, windowsHide: true });
  assert.equal(child.status, 0, child.error?.message ?? child.stderr);
  const report = JSON.parse(await readFile(reportFile, 'utf8'));
  assert.equal(report.ok, true); return report;
}

test('native letterboxed pointer conversion and keyboard focus/save/load use real input routes', { timeout: 90000 }, async () => {
  const root = await workspace('input');
  const destination = await bundle(root, resolve('examples/academy-journey'));
  const screenshot = resolve(root, 'square-window.png');
  const report = await run(root, destination, [
    { resize: { width: 800, height: 800 } }, { waitFrames: 30 }, { key: 'Tab', checkpoint: 'focus' },
    { key: 'Enter' }, { waitFrames: 30 }, { expectScreen: 'home' }, { key: 'F5' },
    { clickWindow: 'visit-city' }, { expectScreen: 'city' }, { key: 'F9' },
    { waitFrames: 30 }, { expectScreen: 'home', screenshot },
  ]);
  const focused = report.steps.find(s => s.command.checkpoint === 'focus').ui.focusedId;
  assert.equal(focused, 'start');
  const saved = JSON.parse(await readFile(resolve(root, 'save.json'), 'utf8'));
  assert.deepEqual(report.final.state, saved.state);
  const png = await readFile(screenshot);
  // SDL_ReadPixels captures the logical presentation viewport, excluding bars.
  assert.equal(png.readUInt32BE(16), 800); assert.equal(png.readUInt32BE(20), 450);
  assert.equal(report.final.state.gameplay.locationId, 'home');
});

for (const format of ['wav', 'mp3', 'ogg', 'flac']) test(`native ${format} music decodes and mute changes the live audio engine`, { timeout: 90000 }, async () => {
  const root = await workspace(`audio-${format}`), projectRoot = resolve(root, 'project');
  await cp(resolve('examples/academy-visual'), projectRoot, { recursive: true });
  await cp(resolve(`tests/native-player/fixtures/tone.${format}`), resolve(projectRoot, `assets/test-tone.${format}`));
  const manifestFile = resolve(projectRoot, 'assets/manifest.json');
  const manifest = JSON.parse(await readFile(manifestFile, 'utf8'));
  manifest.assets.push({ id: 'test-tone', kind: 'audio', path: `assets/test-tone.${format}` });
  await writeFile(manifestFile, JSON.stringify(manifest));
  const projectFile = resolve(projectRoot, 'project.json'), project = JSON.parse(await readFile(projectFile, 'utf8'));
  project.visual.defaultScene.musicAssetId = 'test-tone';
  await writeFile(projectFile, JSON.stringify(project));
  const destination = await bundle(root, projectRoot);
  const report = await run(root, destination, [{ click: 'start' }, { waitFrames: 20 }, { checkpoint: 'playing' },
    { click: 'sound' }, { checkpoint: 'muted' }, { click: 'sound' }, { checkpoint: 'unmuted' }]);
  const audio = tag => report.steps.find(s => s.command.checkpoint === tag).ui.audio;
  assert.deepEqual(audio('playing'), { initialized: true, loadedSounds: 1, musicPlaying: true, muted: false });
  assert.equal(audio('muted').muted, true); assert.equal(audio('unmuted').muted, false);
  assert.equal(report.final.message, '소리를 켰습니다.');
});

function pngPixel(bytes, x, y) {
  const width = bytes.readUInt32BE(16), height = bytes.readUInt32BE(20), channels = bytes[25] === 6 ? 4 : 3;
  assert.equal(bytes[24], 8); assert.ok([2, 6].includes(bytes[25])); assert.equal(bytes[28], 0);
  const chunks = []; for (let at = 8; at < bytes.length;) {
    const length = bytes.readUInt32BE(at), type = bytes.toString('ascii', at + 4, at + 8);
    if (type === 'IDAT') chunks.push(bytes.subarray(at + 8, at + 8 + length)); at += length + 12;
  }
  const raw = inflateSync(Buffer.concat(chunks)), stride = width * channels, decoded = Buffer.alloc(stride * height);
  for (let row = 0; row < height; row++) for (let column = 0; column < stride; column++) {
    const filter = raw[row * (stride + 1)], at = row * stride + column;
    const a = column >= channels ? decoded[at - channels] : 0, b = row ? decoded[at - stride] : 0;
    const c = row && column >= channels ? decoded[at - stride - channels] : 0;
    const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
    const predictor = filter === 0 ? 0 : filter === 1 ? a : filter === 2 ? b : filter === 3 ? Math.floor((a + b) / 2) : pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
    decoded[at] = (raw[row * (stride + 1) + 1 + column] + predictor) & 255;
  }
  return [...decoded.subarray((y * width + x) * channels, (y * width + x) * channels + 3)];
}

test('native static WebP decoder renders exact pixels and honors contain versus cover', { timeout: 90000 }, async () => {
  const root = await workspace('webp'), projectRoot = resolve(root, 'project');
  await cp(resolve('examples/academy-visual'), projectRoot, { recursive: true });
  // Lossless 4x2 RGB test fixture, independently encoded by Pillow/libwebp.
  await writeFile(resolve(projectRoot, 'assets/test-pixels.webp'), Buffer.from('UklGRh4AAABXRUJQVlA4TBEAAAAvA0AAAAdQkIIXqP+BiOh/AAA=', 'base64'));
  const manifestFile = resolve(projectRoot, 'assets/manifest.json'), manifest = JSON.parse(await readFile(manifestFile, 'utf8'));
  manifest.assets.push({ id: 'test-webp', kind: 'image', path: 'assets/test-pixels.webp' });
  await writeFile(manifestFile, JSON.stringify(manifest));
  const projectFile = resolve(projectRoot, 'project.json'), project = JSON.parse(await readFile(projectFile, 'utf8'));
  project.visual.screens.find(s => s.id === 'title').elements.push(
    { id: 'fit-background', kind: 'panel', x: 100, y: 100, width: 500, height: 200, fill: '#223344' },
    { id: 'fit-contain', kind: 'image', x: 100, y: 100, width: 200, height: 200, imageAssetId: 'test-webp', fit: 'contain' },
    { id: 'fit-cover', kind: 'image', x: 400, y: 100, width: 200, height: 200, imageAssetId: 'test-webp', fit: 'cover' });
  await writeFile(projectFile, JSON.stringify(project));
  const destination = await bundle(root, projectRoot), screenshot = resolve(root, 'webp-fit.png');
  await run(root, destination, [{ waitFrames: 30 }, { screenshot }]);
  const png = await readFile(screenshot);
  assert.deepEqual(pngPixel(png, 150, 110), [34, 51, 68]);
  assert.deepEqual(pngPixel(png, 150, 190), [224, 32, 64]);
  assert.deepEqual(pngPixel(png, 450, 110), [224, 32, 64]);
});

test('native free HUD text binds individual resources, inventory and relationships through live play', { timeout: 90000 }, async () => {
  const root = await workspace('hud'), projectRoot = resolve(root, 'project');
  await cp(resolve('examples/academy-journey'), projectRoot, { recursive: true });
  const projectFile = resolve(projectRoot, 'project.json'), project = JSON.parse(await readFile(projectFile, 'utf8'));
  const fields = [
    { id: 'hud-money', binding: 'attribute-value', attributeId: 'money', valueFormat: '{label}: {value} / {value}' },
    { id: 'hud-flower', binding: 'item-count', itemId: 'flower', valueFormat: '{label} × {value}' },
    { id: 'hud-trust', binding: 'relationship-value', characterId: 'mentor', relationshipId: 'trust', valueFormat: '{label}: {value}' },
  ];
  for (const screen of project.visual.screens) screen.elements.push(...fields.map((field, index) => ({
    kind: 'text', x: 30, y: 630 + index * 25, width: 650, height: 24, ...field,
  })));
  await writeFile(projectFile, JSON.stringify(project));
  const destination = await bundle(root, projectRoot);
  const report = await run(root, destination, [{ checkpoint: 'title' }, { click: 'start' }, { checkpoint: 'home' },
    { click: 'visit-market' }, { click: 'buy-florist-flower' }, { checkpoint: 'bought' },
    { click: 'visit-city' }, { click: 'interact-gift-yuna' }, { checkpoint: 'gift' }]);
  const texts = tag => report.steps.find(s => s.command.checkpoint === tag).ui.texts;
  assert.equal(texts('home')['hud-money'], '소지금: 60 / 60');
  assert.equal(texts('bought')['hud-money'], '소지금: 20 / 20');
  assert.equal(texts('bought')['hud-flower'], '감사의 꽃 × 1');
  assert.equal(texts('gift')['hud-flower'], '감사의 꽃 × 0');
  assert.equal(texts('gift')['hud-trust'], '유나 · 신뢰: 15');
});

test('native relationship scene plays a sound cue without stopping music using the same asset', { timeout: 90000 }, async () => {
  const root = await workspace('sound-cue'), projectRoot = resolve(root, 'project');
  await cp(resolve('examples/academy-journey'), projectRoot, { recursive: true });
  await cp(resolve('tests/native-player/fixtures/tone.ogg'), resolve(projectRoot, 'assets/test-tone.ogg'));
  const manifestFile = resolve(projectRoot, 'assets/manifest.json'), manifest = JSON.parse(await readFile(manifestFile, 'utf8'));
  manifest.assets.push({ id: 'test-tone', kind: 'audio', path: 'assets/test-tone.ogg' });
  await writeFile(manifestFile, JSON.stringify(manifest));
  const projectFile = resolve(projectRoot, 'project.json'), project = JSON.parse(await readFile(projectFile, 'utf8'));
  project.visual.defaultScene.musicAssetId = 'test-tone'; await writeFile(projectFile, JSON.stringify(project));
  const eventFile = resolve(projectRoot, 'events/flowers-bring-friendship.json'), event = JSON.parse(await readFile(eventFile, 'utf8'));
  event.nodes.find(node => node.type === 'scene').scene.soundAssetId = 'test-tone';
  await writeFile(eventFile, JSON.stringify(event));
  const destination = await bundle(root, projectRoot);
  const report = await run(root, destination, [{ click: 'start' }, { click: 'visit-market' }, { click: 'buy-florist-flower' },
    { click: 'visit-city' }, { click: 'interact-gift-yuna' }, { expectScreen: 'story' }]);
  assert.equal(report.final.audio.loadedSounds, 2);
  assert.equal(report.final.audio.musicPlaying, true);
  assert.equal(report.final.message.trim(), '신뢰 +15');
});

test('native schedule dropdown opens and selects an activity through actual SDL pointer input', { timeout: 90000 }, async () => {
  const root = await workspace('dropdown'), destination = await bundle(root, resolve('examples/academy-journey'));
  const report = await run(root, destination, [{ clickWindow: 'start' }, { clickWindow: 'schedule-link' },
    { waitFrames: 30 }, { checkpoint: 'layout' }, { clickWindow: 'plan-0' }, { waitFrames: 20 },
    { screenshot: resolve(root, 'dropdown-open.png') }, { clickWindow: 'plan-0-option-rest' },
    { checkpoint: 'selected' }, { clickWindow: 'advance' }, { checkpoint: 'planned' }]);
  const ui = tag => report.steps.find(s => s.command.checkpoint === tag).ui;
  assert.equal(ui('layout').controls.length, 3);
  assert.ok(ui('layout').controls.every(control => control.width >= 200));
  assert.equal(new Set(ui('layout').controls.map(control => control.y)).size, 1);
  assert.equal(ui('selected').controls[0].value, 'rest');
  assert.equal(ui('planned').state.schedule[0], 'rest');
  assert.equal(ui('planned').state.phase, 'ready');
});


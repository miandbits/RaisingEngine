// Skin/expression parity: the native core (`cutout-core-tests.exe --look-parity`) and the Wasm
// runtime replay the same host op script (init, mount with props.skin, blink layer over a
// default expression, a portrait binding, setSkin/setExpression, clip expression keys) and
// must produce the same draw lists: image ids exactly, matrices up to libm last-ulp noise.
import test from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { existsSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const engineRoot = fileURLToPath(new URL('../../', import.meta.url));
const exe = path.join(engineRoot, 'dist/cutout/build-win/cutout-core-tests.exe');
const wasm = path.join(engineRoot, 'dist/cutout/build-wasm/cutout-runtime.mjs');

async function runtime() {
  const { default: factory } = await import(pathToFileURL(wasm).href);
  const raw = (await factory()).cwrap('raising_extension_request', 'string', ['string']);
  return value => JSON.parse(raw(JSON.stringify(value)));
}
const reference = () => {
  const native = spawnSync(exe, ['--look-parity'], { encoding: 'utf8' });
  assert.equal(native.status, 0, native.stderr);
  return JSON.parse(native.stdout.trim());
};

test('skins and expressions: Wasm draw lists match the native core', async () => {
  assert.ok(existsSync(exe) && existsSync(wasm), 'build first: node plugins/cutout-animation/tools/build.mjs win wasm test');
  const { script, replies } = reference();
  const request = await runtime();
  let frames = 0;
  script.forEach((op, i) => {
    const got = request(op), want = replies[i];
    assert.equal(got.ok, true, `${op.op}: ${got.error}`);
    if (op.op !== 'frame') { assert.deepStrictEqual(got, want, `reply of ${op.op} #${i}`); return; }
    frames++;
    assert.deepStrictEqual(got.items.map(x => [x.slot, x.imageAssetId, x.width, x.height, x.alpha]), want.items.map(x => [x.slot, x.imageAssetId, x.width, x.height, x.alpha]), `frame #${i} items`);
    got.items.forEach((x, k) => x.matrix.forEach((v, j) => assert.ok(Math.abs(v - want.items[k].matrix[j]) <= 1e-9 * Math.max(1, Math.abs(v)), `frame #${i} ${x.slot} matrix[${j}]`)));
  });
  assert.equal(frames, 8);
  const image = (i, slot) => replies[i].items.find(x => x.slot === slot)?.imageAssetId ?? '-';
  const frameAt = script.map((op, i) => op.op === 'frame' ? i : -1).filter(i => i >= 0);
  // Anchors (core-tests "host protocol" checks the same script): props.skin, blink over the expression, setSkin.
  assert.equal(image(frameAt[0], 'torso'), 'img-torso-winter');
  assert.equal(image(frameAt[0], 'face'), '-', 'defaultExpression hides the face');
  assert.equal(image(frameAt[1], 'eyes'), 'img-eyes-closed-winter', 'blink layer still closes the eyes; the skin resolves the name');
  assert.equal(image(frameAt[3], 'torso'), 'img-torso-summer');
  assert.equal(image(frameAt[7], 'torso'), 'img-torso');
});

test('generic protocol in Wasm: mount props.skin, setSkin/setExpression ops, save/restore of the look', async () => {
  const { script } = reference();
  const request = await runtime();
  assert.equal(request(script[0]).ok, true);  // init with the look rig + controller
  const element = { ...script[1].element, props: { skin: 'summer' } };
  assert.equal(request({ op: 'mount', instance: 'title/hero', element }).ok, true);
  const torso = () => request({ op: 'frame', instance: 'title/hero' }).items.find(x => x.slot === 'torso').imageAssetId;
  assert.equal(torso(), 'img-torso-summer');
  assert.equal(request({ op: 'setSkin', instance: 'title/hero', skin: 'winter' }).ok, true);
  assert.equal(torso(), 'img-torso-winter');
  assert.equal(request({ op: 'setSkin', instance: 'title/hero', skin: 'missing' }).ok, false);
  assert.equal(request({ op: 'setExpression', instance: 'title/hero', expression: 'happy' }).ok, true);
  const saved = request({ op: 'saveAll' }).state;
  assert.deepEqual([saved.instances['title/hero'].state.skin, saved.instances['title/hero'].state.expression], ['winter', 'happy']);
  request({ op: 'setSkin', instance: 'title/hero', skin: '' });
  request({ op: 'setExpression', instance: 'title/hero', expression: null });
  assert.deepEqual(request({ op: 'restoreAll', state: saved }).skipped, []);
  const state = request({ op: 'describe', instance: 'title/hero' }).state;
  assert.deepEqual([state.skin, state.expression], ['winter', 'happy']);
  assert.equal(torso(), 'img-torso-winter');
});

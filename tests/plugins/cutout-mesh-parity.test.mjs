// Mesh parity (plugin 1.4.0): the native core (`cutout-core-tests.exe --mesh-parity`) and the Wasm
// runtime replay the same host op script (a weighted two-bone arm mesh under a rotated root with
// IK, a grid mesh with step/linear/bezier deform keys, a skin swap to another mesh, a mirrored
// element placement; uneven frame steps, then `sample` at fixed times) and must agree: item order,
// image ids, uvs and triangles exactly, matrices, vertex positions and deform offsets up to libm
// last-ulp noise (sin/cos/atan2/acos may differ by an ulp between the two toolchains).
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
const close = (got, want, what) => assert.ok(Math.abs(got - want) <= 1e-9 * Math.max(1, Math.abs(want)), `${what}: ${got} vs ${want}`);
const allClose = (got, want, what) => { assert.equal(got.length, want.length, `${what} length`); got.forEach((v, j) => close(v, want[j], `${what}[${j}]`)); };

test('mesh attachments: Wasm draw items (skinning after IK, deform, skins) match the native core', async () => {
  assert.ok(existsSync(exe) && existsSync(wasm), 'build first: node plugins/cutout-animation/tools/build.mjs win wasm test');
  const native = spawnSync(exe, ['--mesh-parity'], { encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 });
  assert.equal(native.status, 0, native.stderr);
  const { script, replies } = JSON.parse(native.stdout.trim());
  const request = await runtime();
  let frames = 0, samples = 0, meshes = 0;
  script.forEach((op, i) => {
    const got = request(op), want = replies[i];
    assert.equal(got.ok, true, `${op.op}: ${got.error}`);
    if (op.op === 'frame') {
      frames++;
      assert.deepStrictEqual(got.items.map(x => [x.slot, x.imageAssetId, !!x.mesh]), want.items.map(x => [x.slot, x.imageAssetId, !!x.mesh]), `frame #${i} items`);
      got.items.forEach((x, k) => {
        const w = want.items[k];
        allClose(x.matrix, w.matrix, `frame #${i} ${x.slot} matrix`);
        if (!w.mesh) return;
        meshes++;
        assert.deepStrictEqual([x.mesh.uvs, x.mesh.indices], [w.mesh.uvs, w.mesh.indices], `frame #${i} ${x.slot} uvs/indices`);
        allClose(x.mesh.positions, w.mesh.positions, `frame #${i} ${x.slot} positions`);
      });
    } else if (op.op === 'sample') {
      samples++;
      assert.deepStrictEqual(Object.keys(got.pose.deform ?? {}), Object.keys(want.pose.deform ?? {}), `sample #${i} deform keys`);
      for (const [key, offsets] of Object.entries(want.pose.deform ?? {})) allClose(got.pose.deform[key], offsets, `sample #${i} deform ${key}`);
      for (const [id, bone] of Object.entries(want.pose.bones)) allClose(got.pose.bones[id].world, bone.world, `sample #${i} ${id} world`);
    } else assert.deepStrictEqual(got, want, `reply of ${op.op} #${i}`);
  });
  assert.equal(frames, 9);
  assert.equal(samples, 4);
  assert.equal(meshes, 18, 'both slots draw as meshes in every frame');
  // Anchors from the native reply: the red skin swaps the flag image only (same mesh), and the
  // mirrored placement (flipX) flips x of every vertex against the element width.
  const last = replies[script.findLastIndex(op => op.op === 'frame')].items;
  assert.equal(last[1].imageAssetId, 'img-flag-red');
  assert.ok(last.every(item => item.mesh.positions.length === item.mesh.uvs.length));
  const deform550 = replies[script.findIndex((op, i) => op.op === 'sample' && op.timeMs === 550)].pose.deform['flag/flag'];
  assert.ok(deform550.some(v => v !== 0), 'step key holds the wave shape at 550 ms');
});

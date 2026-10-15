// Clipping + image sequence parity (plugin 1.12.0): the native core (`cutout-core-tests.exe
// --clip-parity`) and the Wasm runtime replay the same host op script — a concave clip polygon on a
// turning, moving bone cutting a rotated region and a grid mesh, a sequence part keyed loop then
// pingpong, a mirrored placement, documented and compact frames, the topology table and `sample`
// at frame boundaries — and must agree: item order, images, sequence frames, clipped triangle
// topology (indices) exactly; matrices, positions and uvs up to libm last-ulp noise.
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
  const module = await factory();
  // requestText path (compact frames are written without a JSON tree), like the players.
  const raw = module.cwrap('raising_extension_request', 'string', ['string']);
  return value => JSON.parse(raw(JSON.stringify(value)));
}
const close = (got, want, what) => assert.ok(Math.abs(got - want) <= 1e-9 * Math.max(1, Math.abs(want)), `${what}: ${got} vs ${want}`);
const allClose = (got, want, what) => { assert.equal(got.length, want.length, `${what} length`); got.forEach((v, j) => close(v, want[j], `${what}[${j}]`)); };
/** Compact numbers are fixed-point text (1e-3 px, 1e-6 for matrix scale terms): equal or one unit apart. */
const compactClose = (got, want, what) => assert.ok(Math.abs(got - want) <= 1.0001e-3, `${what}: ${got} vs ${want}`);

test('clipping + sequences: Wasm draw items, compact frames and samples match the native core', async () => {
  assert.ok(existsSync(exe) && existsSync(wasm), 'build first: node plugins/cutout-animation/tools/build.mjs win wasm test');
  const native = spawnSync(exe, ['--clip-parity'], { encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 });
  assert.equal(native.status, 0, native.stderr);
  const { script, replies } = JSON.parse(native.stdout.trim());
  const request = await runtime();
  let frames = 0, compact = 0, clipped = 0, samples = 0;
  const films = new Set();
  script.forEach((op, i) => {
    const got = request(op), want = replies[i];
    assert.equal(got.ok, true, `${op.op}: ${got.error}`);
    if (op.op === 'frame' && op.format !== 2) {
      frames++;
      assert.deepStrictEqual(got.items.map(x => [x.slot, x.imageAssetId, !!x.mesh]), want.items.map(x => [x.slot, x.imageAssetId, !!x.mesh]), `frame #${i} items`);
      got.items.forEach((x, k) => {
        const w = want.items[k];
        allClose(x.matrix, w.matrix, `frame #${i} ${x.slot} matrix`);
        if (x.slot === 'film') films.add(x.imageAssetId);
        if (!w.mesh) return;
        if (x.slot === 'panel') clipped++;
        assert.deepStrictEqual(x.mesh.indices, w.mesh.indices, `frame #${i} ${x.slot} indices`);
        allClose(x.mesh.uvs, w.mesh.uvs, `frame #${i} ${x.slot} uvs`);
        allClose(x.mesh.positions, w.mesh.positions, `frame #${i} ${x.slot} positions`);
      });
    } else if (op.op === 'frame') {
      compact++;
      assert.equal(got.items.length, want.items.length, `compact #${i} count`);
      got.items.forEach((x, k) => {
        const w = want.items[k];
        assert.equal(x[0], w[0], `compact #${i}/${k} image`);
        assert.equal(x.length, w.length, `compact #${i}/${k} shape`);
        if (x.length > 11) assert.equal(x[11], w[11], `compact #${i}/${k} topology id`);
        for (const j of [1, 2, 3, 4, 5, 6, 7, 8, 9]) compactClose(x[j], w[j], `compact #${i}/${k}[${j}]`);
        if (x.length > 12) { assert.equal(x[12].length, w[12].length); x[12].forEach((v, j) => compactClose(v, w[12][j], `compact #${i}/${k} positions[${j}]`)); }
      });
    } else if (op.op === 'meshes') {
      assert.deepStrictEqual(got.meshes.map(m => [m.id, m.indices]), want.meshes.map(m => [m.id, m.indices]), 'topology table ids and indices');
      got.meshes.forEach((m, k) => allClose(m.uvs, want.meshes[k].uvs, `mesh ${m.id} uvs`));
      assert.ok(got.meshes.length > 4, 'clipped topologies were interned');
    } else if (op.op === 'sample') {
      samples++;
      for (const [id, slot] of Object.entries(want.pose.slots)) assert.deepStrictEqual(got.pose.slots[id], slot, `sample #${i} slot ${id}`);
      for (const [id, bone] of Object.entries(want.pose.bones)) allClose(got.pose.bones[id].world, bone.world, `sample #${i} ${id} world`);
    } else assert.deepStrictEqual(got, want, `reply of ${op.op} #${i}`);
  });
  assert.equal(frames, 8);
  assert.equal(compact, 8);
  assert.equal(clipped, 8, 'the panel is cut in every frame');
  assert.ok(films.size >= 3, `the sequence part changes frames: ${[...films]}`);
  assert.equal(samples, 9);
  // Frame boundaries from the native reply: 99 ms setup frame 2, 100 ms the first loop key, 600 ms pingpong from 1.
  const frameAt = ms => replies[script.findIndex(op => op.op === 'sample' && op.timeMs === ms)].pose.slots.film.sequence;
  assert.deepStrictEqual([0, 99, 100, 350, 599, 600, 649, 650, 900].map(frameAt), [2, 2, 0, 2, 0, 1, 1, 2, 1]);
});

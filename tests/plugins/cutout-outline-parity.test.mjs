// Freeform mesh parity (plugin 1.6.0): the native core (`cutout-core-tests.exe --outline-parity`)
// and the Wasm runtime replay the same host op script — `edit` ops building a mesh from a concave
// outline with an interior grid, tracing a skin mesh from an alpha mask, adding/removing vertices,
// pinning an edge, re-triangulating — and must produce identical rig JSON (the triangulation is
// plain IEEE arithmetic, no unordered iteration), then play the result with the same mesh draw
// items (positions up to libm last-ulp noise: skinning uses sin/cos).
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

test('outline meshes: Wasm edit ops and mesh draw items match the native core', async () => {
  assert.ok(existsSync(exe) && existsSync(wasm), 'build first: node plugins/cutout-animation/tools/build.mjs win wasm test');
  const native = spawnSync(exe, ['--outline-parity'], { encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 });
  assert.equal(native.status, 0, native.stderr);
  const { script, replies } = JSON.parse(native.stdout.trim());
  const request = await runtime();
  let edits = 0, frames = 0, meshes = 0;
  script.forEach((op, i) => {
    const got = request(op), want = replies[i];
    assert.equal(want.ok, true, `native ${op.op} #${i}: ${want.error}`);
    assert.equal(got.ok, true, `${op.op} #${i}: ${got.error}`);
    if (op.op === 'edit') {
      edits++;
      assert.deepStrictEqual(got, want, `edit ${op.operation.op} #${i}`);
    } else if (op.op === 'frame') {
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
    } else assert.deepStrictEqual(got, want, `reply of ${op.op} #${i}`);
  });
  assert.equal(edits, 8);
  assert.equal(frames, 5);
  assert.equal(meshes, 5);
  // Anchors from the native replies: the outline meshes really are what the script built.
  const ops = script.map(op => op.operation?.op);
  const at = name => replies[ops.indexOf(name)];
  assert.equal(at('createMeshFromOutline').rig.attachments[0].hull, 6);
  assert.ok(at('autoOutline').outline.length >= 12, 'traced outline');
  assert.equal(at('addMeshVertex').vertex, 39);
  const last = replies[script.findLastIndex(op => op.op === 'frame')].items[0];
  assert.equal(last.mesh.positions.length / 2, replies[script.findIndex(op => op.op === 'edit' && op.operation.op === 'retriangulate')].vertices, 'the skin swap shows the traced mesh');
});

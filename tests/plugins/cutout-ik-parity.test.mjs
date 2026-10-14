// IK parity (plugin 1.3.0): the native core (`cutout-core-tests.exe --ik-parity`) and the Wasm
// runtime replay the same host op script (a two-bone arm under a rotated, scaled root, a mirrored
// aiming head, a looping clip that moves the target and keys mix/bend; uneven frame steps, then
// `sample` at fixed times) and must agree: image ids and IK settings exactly, matrices and solved
// rotations up to libm last-ulp noise (atan2/acos differ by an ulp between the two toolchains).
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

test('IK constraints: Wasm draw lists and sampled poses match the native core', async () => {
  assert.ok(existsSync(exe) && existsSync(wasm), 'build first: node plugins/cutout-animation/tools/build.mjs win wasm test');
  const native = spawnSync(exe, ['--ik-parity'], { encoding: 'utf8' });
  assert.equal(native.status, 0, native.stderr);
  const { script, replies } = JSON.parse(native.stdout.trim());
  const request = await runtime();
  let frames = 0, samples = 0;
  script.forEach((op, i) => {
    const got = request(op), want = replies[i];
    assert.equal(got.ok, true, `${op.op}: ${got.error}`);
    if (op.op === 'frame') {
      frames++;
      assert.deepStrictEqual(got.items.map(x => [x.slot, x.imageAssetId]), want.items.map(x => [x.slot, x.imageAssetId]), `frame #${i} items`);
      got.items.forEach((x, k) => x.matrix.forEach((v, j) => close(v, want.items[k].matrix[j], `frame #${i} ${x.slot} matrix[${j}]`)));
    } else if (op.op === 'sample') {
      samples++;
      assert.deepStrictEqual(got.pose.ik, want.pose.ik, `sample #${i} ik settings`);
      for (const [id, bone] of Object.entries(want.pose.bones)) {
        close(got.pose.bones[id].appliedRotation, bone.appliedRotation, `sample #${i} ${id} appliedRotation`);
        bone.world.forEach((v, j) => close(got.pose.bones[id].world[j], v, `sample #${i} ${id} world[${j}]`));
      }
    } else assert.deepStrictEqual(got, want, `reply of ${op.op} #${i}`);
  });
  assert.equal(frames, 9);
  assert.equal(samples, 4);
  // Anchor: at full mix the wrist sits on the target (both runtimes, from the native reply).
  const at0 = replies[script.findIndex(op => op.op === 'sample')].pose.bones;
  close(at0.hand.world[4], at0.t.world[4], 'wrist x on the target');
  close(at0.hand.world[5], at0.t.world[5], 'wrist y on the target');
  assert.equal(replies.at(-2).pose.ik.arm.bendPositive, false, 'bend key switched at 600 ms');
});

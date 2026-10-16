// Shear, bone inheritance and transform constraints parity (plugin 1.16.0): the native core
// (`cutout-core-tests.exe --transform-parity`) and the Wasm runtime replay the same host op script —
// every inherit mode (a mirrored parent too), shear keys, transform constraints in world / local,
// absolute / relative modes with keyed mixes, interleaved with a two-bone IK by order, physics on a
// noScale tail and a sheared weighted mesh; instance `a` updated at 60 Hz and `b` at irregular pacing
// to the same times, documented and compact frames, `sample` and describeAll — and must agree: item
// order and images exactly, matrices up to libm last-ulp noise. Within each runtime `a` and `b` must
// be identical bit for bit at every shared time (dt partition independence).
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
  const raw = module.cwrap('raising_extension_request', 'string', ['string']);
  return value => JSON.parse(raw(JSON.stringify(value)));
}
const close = (got, want, what) => assert.ok(Math.abs(got - want) <= 1e-9 * Math.max(1, Math.abs(want)), `${what}: ${got} vs ${want}`);
const compactClose = (got, want, what) => assert.ok(Math.abs(got - want) <= 1.0001e-3, `${what}: ${got} vs ${want}`);

/** Frame replies of instance `a` and `b` at the same checkpoint, in script order. */
function pairs(script, replies, format) {
  const out = [];
  let pending = null;
  script.forEach((op, i) => {
    if (op.op !== 'frame' || (op.format === 2) !== (format === 2) || !op.instance) return;
    if (op.instance === 'a') pending = replies[i];
    else if (op.instance === 'b' && pending) { out.push([pending, replies[i]]); pending = null; }
  });
  return out;
}

test('shear + inherit + transform constraints: Wasm matches the native core; pacing never changes a frame', async () => {
  assert.ok(existsSync(exe) && existsSync(wasm), 'build first: node plugins/cutout-animation/tools/build.mjs win wasm test');
  const native = spawnSync(exe, ['--transform-parity'], { encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 });
  assert.equal(native.status, 0, native.stderr);
  const { script, replies } = JSON.parse(native.stdout.trim());
  const request = await runtime();
  const got = [];
  let frames = 0, compact = 0, samples = 0;
  script.forEach((op, i) => {
    const reply = request(op), want = replies[i];
    got.push(reply);
    assert.equal(reply.ok, true, `${op.op}: ${reply.error}`);
    if (op.op === 'frame' && op.format !== 2) {
      frames++;
      assert.deepStrictEqual(reply.items.map(x => [x.slot, x.imageAssetId]), want.items.map(x => [x.slot, x.imageAssetId]), `frame #${i} items`);
      reply.items.forEach((x, k) => {
        x.matrix.forEach((v, j) => close(v, want.items[k].matrix[j], `frame #${i} ${x.slot} matrix[${j}]`));
        (x.mesh?.positions ?? []).forEach((v, j) => close(v, want.items[k].mesh.positions[j], `frame #${i} ${x.slot} position[${j}]`));
      });
    } else if (op.op === 'frame') {
      compact++;
      assert.equal(reply.items.length, want.items.length, `compact #${i} count`);
      reply.items.forEach((x, k) => { assert.equal(x[0], want.items[k][0]); for (let j = 1; j < 10; j++) compactClose(x[j], want.items[k][j], `compact #${i}/${k}[${j}]`); });
    } else if (op.op === 'sample') {
      samples++;
      for (const [id, bone] of Object.entries(want.pose.bones)) {
        bone.world.forEach((v, j) => close(reply.pose.bones[id].world[j], v, `sample #${i} ${id} world[${j}]`));
        if (bone.applied) for (const [k, v] of Object.entries(bone.applied)) close(reply.pose.bones[id].applied[k], v, `sample #${i} ${id} applied.${k}`);
        for (const k of ['shearX', 'shearY']) assert.equal(reply.pose.bones[id][k], bone[k], `sample #${i} ${id} ${k}`);
      }
      assert.deepStrictEqual(reply.pose.transform, want.pose.transform, `sample #${i} transform mixes`);
      assert.deepStrictEqual(Object.keys(reply.pose.transform).sort(), ['copy', 'follow', 'nod', 'sway'], 'every constraint reported');
    } else if (op.op === 'describeAll') {
      for (const id of ['a', 'b']) {
        const p = reply.instances[id].physics, w = want.instances[id].physics;
        assert.deepStrictEqual([p.steps, p.settles, p.teleports], [w.steps, w.settles, w.teleports], `instance ${id} physics counters`);
      }
    } else assert.deepStrictEqual(reply, want, `reply of ${op.op} #${i}`);
  });
  assert.equal(frames, 10);
  assert.equal(compact, 10);
  assert.equal(samples, 4);
  // Pacing independence, in each runtime separately: 60 Hz and irregular frames are identical bits.
  for (const [name, list] of [['native', replies], ['wasm', got]]) {
    const docs = pairs(script, list, 1), packed = pairs(script, list, 2);
    assert.equal(docs.length, 5, `${name}: checkpoints`);
    docs.forEach(([a, b], k) => assert.deepStrictEqual(b.items, a.items, `${name}: 60 Hz vs irregular at ${300 * (k + 1)} ms`));
    packed.forEach(([a, b], k) => assert.deepStrictEqual(b.items, a.items, `${name}: compact at ${300 * (k + 1)} ms`));
  }
  // The constraints really act: the badge follows the hand and the hat turns with the head.
  const sample = replies[script.findIndex(op => op.op === 'sample' && op.timeMs === 333)].pose.bones;
  close(sample.badge.world[4], sample.hand.world[4] + 8 * sample.hand.world[0], 'badge x follows the hand (offset 8 px in its space)');
  assert.ok(Math.abs(sample.hat.applied.rotation - sample.hat.rotation) > 1, 'relative constraint turned the hat');
});

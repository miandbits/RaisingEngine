// IK chains + physics parity (plugin 1.13.0): the native core (`cutout-core-tests.exe
// --physics-parity`) and the Wasm runtime replay the same host op script — a head with three hair
// segments and a bobbing ribbon (physics, one at 120 Hz), a four-bone FABRIK tail that stretches,
// instance `a` updated at 60 Hz and `b` at irregular pacing to the same times, documented and
// compact frames, an environment change, describeAll and `sample` — and must agree: item order and
// images exactly, matrices up to libm last-ulp noise. Within each runtime `a` and `b` must be
// identical bit for bit at every shared time (fixed-step physics on an absolute clock grid).
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

test('physics + IK chains: Wasm matches the native core; pacing never changes a frame', async () => {
  assert.ok(existsSync(exe) && existsSync(wasm), 'build first: node plugins/cutout-animation/tools/build.mjs win wasm test');
  const native = spawnSync(exe, ['--physics-parity'], { encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 });
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
      reply.items.forEach((x, k) => x.matrix.forEach((v, j) => close(v, want.items[k].matrix[j], `frame #${i} ${x.slot} matrix[${j}]`)));
    } else if (op.op === 'frame') {
      compact++;
      assert.equal(reply.items.length, want.items.length, `compact #${i} count`);
      reply.items.forEach((x, k) => { assert.equal(x[0], want.items[k][0]); for (let j = 1; j < 10; j++) compactClose(x[j], want.items[k][j], `compact #${i}/${k}[${j}]`); });
    } else if (op.op === 'sample') {
      samples++;
      for (const [id, bone] of Object.entries(want.pose.bones)) bone.world.forEach((v, j) => close(reply.pose.bones[id].world[j], v, `sample #${i} ${id} world[${j}]`));
      assert.deepStrictEqual(Object.keys(reply.pose.ik.tail).sort(), Object.keys(want.pose.ik.tail).sort(), 'chain report fields');
      assert.equal(reply.pose.ik.tail.iterations, want.pose.ik.tail.iterations, `sample #${i} FABRIK passes`);
      assert.deepStrictEqual(reply.pose.physics, want.pose.physics, `sample #${i} physics mixes`);
    } else if (op.op === 'describeAll') {
      // The same steps, settles and teleports on both runtimes (the clock agrees to float noise).
      for (const id of ['a', 'b']) {
        const p = reply.instances[id].physics, w = want.instances[id].physics;
        assert.deepStrictEqual([p.steps, p.settles, p.teleports, p.wind, p.gravity], [w.steps, w.settles, w.teleports, w.wind, w.gravity], `instance ${id} physics counters`);
        close(p.clockMs, w.clockMs, `instance ${id} clock`);
      }
    } else assert.deepStrictEqual(reply, want, `reply of ${op.op} #${i}`);
  });
  assert.equal(frames, 13);
  assert.equal(compact, 12);
  assert.equal(samples, 3);
  // Pacing independence, in each runtime separately: 60 Hz and irregular frames are identical bits.
  for (const [name, list] of [['native', replies], ['wasm', got]]) {
    const docs = pairs(script, list, 1), packed = pairs(script, list, 2);
    assert.equal(docs.length, 6, `${name}: checkpoints`);
    docs.forEach(([a, b], k) => assert.deepStrictEqual(b.items, a.items, `${name}: 60 Hz vs irregular at ${250 * (k + 1)} ms`));
    packed.forEach(([a, b], k) => assert.deepStrictEqual(b.items, a.items, `${name}: compact at ${250 * (k + 1)} ms`));
  }
  // Both pacings really simulated (hundreds of steps, not a static pose).
  const describe = replies[script.findIndex(op => op.op === 'describeAll')];
  assert.ok(describe.instances.a.physics.steps > 300, `steps taken: ${describe.instances.a.physics.steps}`);
});

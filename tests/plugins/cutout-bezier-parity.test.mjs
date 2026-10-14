// Bezier key sampling parity: the native core and the Wasm runtime run the same C++ curve code.
// Native reference = `cutout-core-tests.exe --bezier-parity` (host `sample` op); the Wasm side is
// the built web runtime module's `sample` op. Sampled channel values (where the curve math
// lives: + - * / only) must match bit for bit; world matrices go through each platform's libm
// sin/cos and may differ in the last ulp, so they get a tiny tolerance. Analytic values pin
// the numbers the core tests assert.
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

test('bezier sampling: Wasm runtime matches the native core bit for bit', async () => {
  assert.ok(existsSync(exe) && existsSync(wasm), 'build first: node plugins/cutout-animation/tools/build.mjs win wasm test');
  const native = spawnSync(exe, ['--bezier-parity'], { encoding: 'utf8' });
  assert.equal(native.status, 0, native.stderr);
  const reference = JSON.parse(native.stdout.trim());
  assert.ok(reference.samples.length >= 10 && reference.rig.clips.some(c => c.tracks.some(t => t.keys.some(k => k.interp === 'bezier'))));
  const request = await runtime();
  const sample = timeMs => {
    const reply = request({ op: 'sample', rig: reference.rig, clip: reference.clip, timeMs });
    assert.equal(reply.ok, true, reply.error);
    return reply.pose;
  };
  const channels = pose => ({
    bones: Object.fromEntries(Object.entries(pose.bones).map(([id, { world, ...local }]) => [id, local])),
    slots: pose.slots,
  });
  for (const { timeMs, pose } of reference.samples) {
    const got = sample(timeMs);
    assert.deepStrictEqual(channels(got), channels(pose), `channel values at ${timeMs} ms`);
    for (const [id, bone] of Object.entries(pose.bones))
      bone.world.forEach((v, i) => assert.ok(Math.abs(got.bones[id].world[i] - v) <= 1e-9 * Math.max(1, Math.abs(v)), `world ${id}[${i}] at ${timeMs} ms`));
  }

  // Analytic anchors (core-tests "bezier keys sample through the runtime").
  const near = (a, b, eps, what) => assert.ok(Math.abs(a - b) <= eps, `${what}: ${a} vs ${b}`);
  near(sample(300).bones.body.y, -106, 1e-9, 'easeInOut midpoint');
  near(sample(250).bones.body.scaleY, 1 + 0.2 * 0.15625, 1e-12, 'flat handles = smoothstep');
  near(sample(550).bones.head.rotation, 20, 0, 'step segment holds');
  near(sample(850).bones.head.rotation, -2.5, 1e-12, 'linear segment unchanged');
  let peak = 0;
  for (let t = 0; t <= 400; t += 5) peak = Math.max(peak, sample(t).bones.head.rotation);
  assert.ok(peak > 21 && peak < 24, `back overshoot peak ${peak}`);
});

test('bezier playback in Wasm is frame-rate independent', async () => {
  const native = JSON.parse(spawnSync(exe, ['--bezier-parity'], { encoding: 'utf8' }).stdout.trim());
  const request = await runtime();
  assert.equal(request({ op: 'loadRig', id: 'parity', data: native.rig }).ok, true);
  const frames = {};
  for (const [id, step] of [['a', 31.25], ['b', 7.8125]]) {
    assert.equal(request({ op: 'create', instance: id, rig: 'parity' }).ok, true);
    request({ op: 'play', instance: id, clip: 'ease', loop: false, then: 'hold', mixMs: 0 });
    frames[id] = [];
    for (let t = step; t <= 1000; t += step) {
      request({ op: 'update', instance: id, dtMs: step });
      if (t % 125 === 0) frames[id].push(request({ op: 'frame', instance: id }).items);  // same build: exact
    }
  }
  assert.equal(frames.a.length, 8);
  assert.deepStrictEqual(frames.a, frames.b);
});

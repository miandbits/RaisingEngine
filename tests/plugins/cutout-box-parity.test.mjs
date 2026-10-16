// Bounding boxes, points and hit tests parity (plugin 1.17.0): the native core
// (`cutout-core-tests.exe --box-parity`) and the Wasm runtime replay the same host op script —
// `points` / `boxes` (a weighted box and a deform key among them) in element space of a plain and
// a mirrored, scaled element, `hitTest` misses, hits and dispatches (`partClicked` starting the
// bound clips), updates, compact frames and describeAll — and must agree: hits, matched bindings
// and playback state exactly, coordinates up to libm last-ulp noise. Rigs with boxes draw the same
// compact frames as before (boxes are never items).
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

test('boxes + points + hit tests: Wasm matches the native core', async () => {
  assert.ok(existsSync(exe) && existsSync(wasm), 'build first: node plugins/cutout-animation/tools/build.mjs win wasm test');
  const native = spawnSync(exe, ['--box-parity'], { encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 });
  assert.equal(native.status, 0, native.stderr);
  const { script, replies } = JSON.parse(native.stdout.trim());
  const request = await runtime();
  const seen = { points: 0, boxes: 0, hitTest: 0, dispatched: 0, frame: 0 };
  script.forEach((op, i) => {
    const reply = request(op), want = replies[i];
    assert.equal(reply.ok, true, `${op.op} #${i}: ${reply.error}`);
    seen[op.op] = (seen[op.op] ?? 0) + 1;
    if (op.op === 'points') {
      assert.deepStrictEqual(reply.points.map(p => [p.slot, p.name]), want.points.map(p => [p.slot, p.name]), `points #${i}`);
      reply.points.forEach((p, k) => { for (const f of ['x', 'y', 'rotation']) close(p[f], want.points[k][f], `points #${i} ${p.slot}.${f}`); });
    } else if (op.op === 'boxes') {
      assert.deepStrictEqual(reply.boxes.map(b => [b.slot, b.name, b.polygon.length]), want.boxes.map(b => [b.slot, b.name, b.polygon.length]), `boxes #${i}`);
      reply.boxes.forEach((b, k) => b.polygon.forEach((v, j) => close(v, want.boxes[k].polygon[j], `boxes #${i} ${b.slot}[${j}]`)));
    } else if (op.op === 'frame') {
      assert.equal(reply.items.length, want.items.length, `frame #${i} count`);
      reply.items.forEach((x, k) => { assert.equal(x[0], want.items[k][0]); for (let j = 1; j < 10; j++) assert.ok(Math.abs(x[j] - want.items[k][j]) <= 1.0001e-3, `frame #${i}/${k}[${j}]`); });
    } else if (op.op === 'describeAll') {
      for (const id of ['a', 'b']) assert.deepStrictEqual(reply.instances[id].tracks.map(t => [t.track, t.clip, t.ended]), want.instances[id].tracks.map(t => [t.track, t.clip, t.ended]), `instance ${id} tracks`);
    } else {
      if (op.op === 'hitTest' && op.dispatch) seen.dispatched++;
      assert.deepStrictEqual(reply, want, `reply of ${op.op} #${i}`);
    }
  });
  // The script really exercises hits, misses and dispatches (see boxes::parityScript in box-tests.hpp).
  const hits = script.map((op, i) => op.op === 'hitTest' ? replies[i] : null).filter(Boolean);
  assert.deepStrictEqual(hits.map(h => h.hit?.slot ?? null), ['head-box', null, 'head-box', 'head-box']);
  assert.deepStrictEqual(hits.map(h => h.matched ?? null), [null, [], ['pat-head'], ['pat-head']]);
  assert.ok(seen.points >= 8 && seen.boxes >= 8 && seen.frame === 6 && seen.dispatched === 3, JSON.stringify(seen));
  const described = replies.at(-1).instances;
  assert.deepStrictEqual(described.a.tracks.map(t => t.clip), ['idle', 'nod'], 'the click started the bound clip on its track');
  // Element space: the mirrored, 1.5x element puts the face point at x = 900 - (225 + 1.5 * (155 - 150 + 150))... spot check.
  const pa = replies[3].points, pb = replies[5].points;
  assert.deepStrictEqual(pa.map(p => p.slot), ['hand', 'face']);
  close(pb[1].x, 900 - (225 + 1.5 * (pa[1].x - 150)), 'mirrored, scaled face x');
  close(pb[1].y, 1.5 * pa[1].y, 'scaled face y');
});

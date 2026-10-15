// Compact frame parity (plugin 1.9.0): the native core (`cutout-core-tests.exe --frame-parity`) and the
// Wasm runtime answer the same compact-frame script (deltas against the previous frame number, the
// mesh topology table, skins/expressions, a mirrored and scaled placement on the large stress rig)
// with the same replies: identical structure, image ids, "unchanged" markers and mesh ids; numbers
// equal up to one unit of the last printed decimal (libm last-ulp noise can flip a rounding tie).
import test from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { existsSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const engineRoot = fileURLToPath(new URL('../../', import.meta.url));
const exe = path.join(engineRoot, 'dist/cutout/build-win/cutout-core-tests.exe');
const wasm = path.join(engineRoot, 'dist/cutout/build-wasm/cutout-runtime.mjs');

/** Recursive comparison: numbers within `tolerance`, everything else exactly. */
function same(got, want, where, tolerance) {
  if (typeof want === 'number') { assert.equal(typeof got, 'number', where); assert.ok(Math.abs(got - want) <= tolerance, `${where}: ${got} vs ${want}`); return; }
  if (Array.isArray(want)) { assert.ok(Array.isArray(got), where); assert.equal(got.length, want.length, `${where} length`); want.forEach((w, i) => same(got[i], w, `${where}/${i}`, tolerance)); return; }
  if (want && typeof want === 'object') { assert.deepEqual(Object.keys(got).sort(), Object.keys(want).sort(), where); for (const k of Object.keys(want)) same(got[k], want[k], `${where}/${k}`, tolerance); return; }
  assert.equal(got, want, where);
}

test('compact frames: the Wasm runtime answers the native core\'s script identically', async () => {
  assert.ok(existsSync(exe) && existsSync(wasm), 'build first: node plugins/cutout-animation/tools/build.mjs win wasm test');
  const native = spawnSync(exe, ['--frame-parity'], { encoding: 'utf8', maxBuffer: 256 * 1024 * 1024 });
  assert.equal(native.status, 0, native.stderr);
  const { script, replies } = JSON.parse(native.stdout.trim());
  const { default: factory } = await import(pathToFileURL(wasm).href);
  const m = await factory();
  const request = value => { const text = JSON.stringify(value), n = m.lengthBytesUTF8(text) + 1, at = m._malloc(n);
    try { m.stringToUTF8(text, at, n); return JSON.parse(m.UTF8ToString(m._raising_extension_request(at))); } finally { m._free(at); } };
  let frames = 0, unchanged = 0, meshes = 0;
  script.forEach((op, i) => {
    const got = request(op), want = replies[i];
    assert.equal(got.ok, true, `${op.op}: ${got.error}`);
    // Mesh epochs count Host instances' table resets: equal per process, not across them.
    if (op.op === 'frame') {
      frames++;
      assert.equal(got.format, 2);
      want.items.forEach(e => { if (e === 0) unchanged++; else if (e.length === 13) meshes++; });
      same(got.items, want.items, `#${i} items`, 1.5e-3);
      assert.equal(got.frame, want.frame, `#${i} frame number`);
    } else if (op.op === 'init') {
      same(got.meshes, want.meshes, 'init mesh table', 0);
      assert.equal(got.frameFormat, 2);
    } else same(got, want, `#${i} ${op.op}`, 0);
  });
  assert.equal(frames, 19);
  assert.ok(unchanged > 0, 'the second frame at dt 0 is all "unchanged"');
  assert.ok(meshes >= 40 * 3, 'every instance sends its meshes');
});

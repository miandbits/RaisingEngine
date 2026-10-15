// Clip event parity (plugin 1.8.0): the native core (`cutout-core-tests.exe --event-parity`) and the
// Wasm runtime replay the same host op script (a looping walk clip at uneven frame steps, a one-shot
// with events at its end) and must report the same notifications, payloads included.
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

test('clip events: Wasm notifications and payloads match the native core', async () => {
  assert.ok(existsSync(exe) && existsSync(wasm), 'build first: node plugins/cutout-animation/tools/build.mjs win wasm test');
  const native = spawnSync(exe, ['--event-parity'], { encoding: 'utf8' });
  assert.equal(native.status, 0, native.stderr);
  const { script, replies } = JSON.parse(native.stdout.trim());
  const request = await runtime();
  const events = [];
  script.forEach((op, i) => {
    const got = request(op);
    assert.equal(got.ok, true, `${op.op}: ${got.error}`);
    assert.deepStrictEqual(got, replies[i], `reply of ${op.op} #${i}`);
    for (const n of got.notifications?.a ?? []) if (n.kind === 'event') events.push(n);
  });
  // Anchors: definition defaults, a key override, audio fields only where the definition has a sound.
  assert.equal(events.length, 14);
  assert.deepEqual(events[0], { kind: 'event', track: 0, clip: 'walk', serial: 1, name: 'step', int: 1, string: 'left', audio: 'sfx-step', volume: 0.5, balance: -0.25 });
  assert.deepEqual(events[1], { kind: 'event', track: 0, clip: 'walk', serial: 1, name: 'hit', float: 2.5, string: 'fist' });
  assert.deepEqual([events[2].int, events[2].string], [2, 'right']);
  assert.deepEqual(events.filter(e => e.clip === 'joy').map(e => Object.keys(e).length), [5, 5], 'joy events without payload keep the 1.7 shape');
});

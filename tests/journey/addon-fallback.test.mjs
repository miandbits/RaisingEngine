import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdir, mkdtemp, cp, readFile, writeFile } from 'node:fs/promises';
import { resolve, dirname, basename } from 'node:path';
import { nativeExecutable, nativeRequest, nativeTransport, closeNativeRuntimes } from '../../dist/packages/native-bridge/index.js';
import { createRun, dispatchCommand } from '../../dist/packages/core/run.js';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';

async function content() { const built = await buildSnapshot(await loadProject(resolve('examples/academy-journey'))); assert.equal(built.ok, true); return built.snapshot; }
function exercise(snapshot, executable) {
  const initial = createRun(snapshot, 42, { mode: 'full' });
  const created = nativeRequest({ operation: 'create', content: snapshot, seed: 42, options: { mode: 'full' } }, executable);
  assert.deepEqual(created, initial);
  const command = { type: 'visit', locationId: 'market' };
  const actual = nativeRequest({ operation: 'command', content: snapshot, state: initial.state, command, expectedRevision: 0 }, executable);
  assert.deepEqual(actual, dispatchCommand(snapshot, initial.state, command, 0));
  const rejected = nativeRequest({ operation: 'command', content: snapshot, state: actual.state, command: { type: 'buy', shopId: 'florist', itemId: 'flower', quantity: 1 }, expectedRevision: 0 }, executable);
  assert.equal(rejected.ok, false); assert.equal(rejected.error.code, 'REVISION_CONFLICT'); assert.deepEqual(rejected.state, actual.state);
}

test('matching addon and explicitly selected subprocess preserve reference gameplay transactions', async () => {
  const snapshot = await content(), previous = process.env.RAISING_NATIVE_TRANSPORT;
  try {
    closeNativeRuntimes(); process.env.RAISING_NATIVE_TRANSPORT = 'auto';
    assert.equal(nativeTransport().transport, 'addon'); exercise(snapshot);
    closeNativeRuntimes(); process.env.RAISING_NATIVE_TRANSPORT = 'process';
    assert.deepEqual(nativeTransport(), { transport: 'process' }); exercise(snapshot);
  } finally { closeNativeRuntimes(); if (previous === undefined) delete process.env.RAISING_NATIVE_TRANSPORT; else process.env.RAISING_NATIVE_TRANSPORT = previous; }
});

test('packaged addon host mismatch or missing binary visibly falls back to real C++ subprocess', async () => {
  const snapshot = await content(), executable = nativeExecutable();
  const original = JSON.parse(await readFile(resolve(dirname(executable), 'addon.json'), 'utf8'));
  const output = resolve('.cache/journey-addon-fallback'); await mkdir(output, { recursive: true });
  const previous = process.env.RAISING_NATIVE_TRANSPORT; process.env.RAISING_NATIVE_TRANSPORT = 'auto';
  try {
    for (const patch of [{ executable: 'different-runtime.exe' }, { arch: 'unsupported-cpu' }, { napi: 999 }, { filename: 'raising-addon-0000000000000000.node' }]) {
      const root = await mkdtemp(`${output}/case-`), copied = resolve(root, basename(executable));
      await cp(executable, copied); await writeFile(resolve(root, 'addon.json'), JSON.stringify({ ...original, ...patch }));
      const transport = nativeTransport(copied); assert.equal(transport.transport, 'process'); assert.ok(transport.fallbackReason);
      assert.equal(nativeRequest({ operation: 'info' }, copied).implementation, 'C++17'); exercise(snapshot, copied);
      closeNativeRuntimes();
    }
  } finally { closeNativeRuntimes(); if (previous === undefined) delete process.env.RAISING_NATIVE_TRANSPORT; else process.env.RAISING_NATIVE_TRANSPORT = previous; }
});

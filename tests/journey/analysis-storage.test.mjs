import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdir, mkdtemp, readFile, readdir } from 'node:fs/promises';
import { spawn } from 'node:child_process';
import { once } from 'node:events';
import { resolve } from 'node:path';
import { setTimeout as delay } from 'node:timers/promises';
import { atomicJson } from '../../dist/packages/analysis-service/jobs.js';

async function fixture() {
  const output = resolve('.cache/journey-analysis-storage'); await mkdir(output, { recursive: true });
  const root = await mkdtemp(`${output}/run-`), file = resolve(root, 'progress.json');
  await atomicJson(file, { completed: 287 }); return { root, file };
}
async function lock(file) {
  const script = `$stream=[System.IO.File]::Open('${file.replaceAll("'", "''")}',[System.IO.FileMode]::Open,[System.IO.FileAccess]::Read,[System.IO.FileShare]::Read); [Console]::WriteLine('LOCKED'); try { [Console]::ReadLine() | Out-Null } finally { $stream.Dispose() }`;
  const child = spawn('powershell.exe', ['-NoProfile', '-NonInteractive', '-EncodedCommand', Buffer.from(script, 'utf16le').toString('base64')], { stdio: ['pipe', 'pipe', 'pipe'], windowsHide: true });
  let stderr = ''; child.stderr.on('data', bytes => { stderr += bytes; });
  await new Promise((resolve, reject) => {
    child.stdout.on('data', bytes => { if (String(bytes).includes('LOCKED')) resolve(); });
    child.once('exit', code => reject(new Error(`lock helper exited ${code}: ${stderr}`)));
    child.once('error', reject);
  });
  return async () => { const exited = once(child, 'exit'); child.stdin.end('release\n'); await exited; };
}

test('Windows progress JSON survives an actual temporary FileShare.Read replacement lock', { skip: process.platform !== 'win32', timeout: 20000 }, async () => {
  const { root, file } = await fixture(); const release = await lock(file);
  let settled = false;
  const pending = atomicJson(file, { completed: 288 }).finally(() => { settled = true; });
  try {
    await delay(125); assert.equal(settled, false);
    assert.deepEqual(JSON.parse(await readFile(file, 'utf8')), { completed: 287 });
  } finally { await release(); }
  await pending;
  assert.deepEqual(JSON.parse(await readFile(file, 'utf8')), { completed: 288 });
  assert.deepEqual(await readdir(root), ['progress.json']);
});

test('Windows persistent replacement lock reports failure, preserves old progress and cleans staging', { skip: process.platform !== 'win32', timeout: 30000 }, async () => {
  const { root, file } = await fixture(); const release = await lock(file);
  try {
    await assert.rejects(atomicJson(file, { completed: 999 }), error => ['EPERM', 'EACCES', 'EBUSY'].includes(error.code));
    assert.deepEqual(JSON.parse(await readFile(file, 'utf8')), { completed: 287 });
    assert.deepEqual(await readdir(root), ['progress.json']);
  } finally { await release(); }
});

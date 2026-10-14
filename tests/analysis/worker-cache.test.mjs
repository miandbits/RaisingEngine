import test from 'node:test';
import assert from 'node:assert/strict';
import { Worker } from 'node:worker_threads';
import { once } from 'node:events';
import { randomUUID } from 'node:crypto';
import { mkdir, mkdtemp, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { emptyResult } from '../../dist/packages/analysis-service/model.js';
import { runAnalysis } from '../../dist/packages/analysis-service/runner.js';
import { closeNativeRuntimes } from '../../dist/packages/native-bridge/index.js';

test('worker restores immutable snapshot caching without freezing caller data or changing seeded traces', { timeout: 30000 }, async () => {
  const built = await buildSnapshot(await loadProject(resolve('examples/academy-journey')));
  assert.ok(built.ok);
  const content = structuredClone(built.snapshot);
  assert.equal(Object.isFrozen(content), false);
  assert.equal(Object.isFrozen(content.project), false);
  const options = { runs: 2, seed: 42000, strategy: 'random', maxCommands: 2000, timeoutMs: 60000 };
  const reference = await runAnalysis(content, options);
  assert.equal(Object.isFrozen(content), false, 'direct caller data remains editable');
  await mkdir('.cache/analysis-worker-cache', { recursive: true });
  const dir = await mkdtemp(resolve('.cache/analysis-worker-cache/run-'));
  const manifest = JSON.parse(await readFile('dist/native/addon.json', 'utf8'));
  const realAddon = resolve('dist/native', manifest.filename);
  const wrapper = resolve(dir, 'observe-addon.cjs');
  const boot = resolve(dir, 'worker.mjs');
  const report = resolve(dir, 'cache-report.json');
  await writeFile(wrapper, `const real = require(${JSON.stringify(realAddon)});
const stats = { registered: 0, referenced: 0, uncached: 0 };
module.exports = { stats, request(raw) {
  const r = JSON.parse(raw);
  if (r.operation === 'registerContent') stats.registered++;
  if (['create','command'].includes(r.operation)) { if(r.contentId) stats.referenced++; else stats.uncached++; }
  return real.request(raw);
} };`);
  await writeFile(boot, `import { workerData } from 'node:worker_threads';
import { createRequire } from 'node:module';
import { writeFileSync } from 'node:fs';
const before = Object.isFrozen(workerData.content);
await import(${JSON.stringify(pathToFileURL(resolve('dist/packages/analysis-service/worker.js')).href)});
const { stats } = createRequire(import.meta.url)(${JSON.stringify(wrapper)});
writeFileSync(${JSON.stringify(report)}, JSON.stringify({ before, after: Object.isFrozen(workerData.content), nestedAfter: Object.isFrozen(workerData.content.project.gameplay.locations), ...stats }));`);
  const job = { id: randomUUID(), root: dir, sourceVersion: 'regression', revision: null,
    fingerprint: content.contentFingerprint, createdAt: new Date().toISOString(), status: 'running',
    options, result: emptyResult(options.runs), error: null };
  const worker = new Worker(pathToFileURL(boot), { workerData: { content, job, dir }, execArgv: [],
    env: { ...process.env, NODE_OPTIONS: '', RAISING_RUNTIME: 'native', RAISING_NATIVE_TRANSPORT: 'addon', RAISING_NATIVE_ADDON: wrapper } });
  try {
    const [code] = await once(worker, 'exit');
    assert.equal(code, 0);
    const saved = JSON.parse(await readFile(resolve(dir, `${job.id}.json`), 'utf8'));
    assert.equal(saved.status, 'completed', saved.error);
    assert.deepEqual(saved.result, reference, 'worker cache preserves exact traces, coverage, and state');
    const cache = JSON.parse(await readFile(report, 'utf8'));
    assert.equal(cache.before, false, 'worker transfer removes freezing');
    assert.equal(cache.after, true);
    assert.equal(cache.nestedAfter, true);
    assert.equal(cache.registered, 1, 'one snapshot registration for the whole job');
    assert.equal(cache.uncached, 0, 'commands must not resend the entire project');
    assert.ok(cache.referenced > 500, 'real simulation commands used cached native content');
    assert.equal(Object.isFrozen(content), false);
    content.project.title = 'Still editable outside worker';
    assert.equal(content.project.title, 'Still editable outside worker');
  } finally { await worker.terminate(); closeNativeRuntimes(); }
});

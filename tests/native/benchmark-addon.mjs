// Manual timing evidence; assertions compare results, not noisy timing thresholds.
import assert from 'node:assert/strict';
import { performance } from 'node:perf_hooks';
import { writeFile } from 'node:fs/promises';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { runAnalysis } from '../../dist/packages/analysis-service/runner.js';
import { nativeRequest, nativeTransport, closeNativeRuntimes } from '../../dist/packages/native-bridge/index.js';
const root = process.argv[2] ?? 'examples/academy-journey';
const built = await buildSnapshot(await loadProject(root)); assert.equal(built.ok, true, JSON.stringify(built.diagnostics));
const content = built.snapshot;
const options = { runs:Number(process.argv[3] ?? 30), seed:42000, strategy:'random', maxCommands:2000, timeoutMs:600000 };
const report = { root, options, measurements:[] };
let expected;
for (const transport of ['process','addon']) {
  process.env.RAISING_NATIVE_TRANSPORT = transport; closeNativeRuntimes();
  assert.equal(nativeTransport().transport, transport);
  const start = performance.now();
  for (let i=0;i<1000;i++) nativeRequest({ operation:'info' });
  const infoMs = (performance.now()-start)/1000;
  const started = performance.now();
  // A fixed hook removes wall-clock early stopping from the parity comparison.
  const result = await runAnalysis(content, options, { now:()=>0 });
  const elapsedMs = performance.now()-started;
  if (expected) assert.deepEqual(result, expected); else expected = result;
  report.measurements.push({ transport, infoMs, elapsedMs, completed:result.completed, outcomes:result.outcomes });
  console.log(JSON.stringify(report.measurements.at(-1)));
}
report.speedup = report.measurements[0].elapsedMs/report.measurements[1].elapsedMs;
await writeFile('.cache/addon-benchmark.json',JSON.stringify(report,null,2));
closeNativeRuntimes(); console.log(JSON.stringify({speedup:report.speedup,exactAnalysisParity:true}));

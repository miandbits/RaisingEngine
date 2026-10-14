// Manual diagnostic benchmark, not a timing-sensitive regression assertion.
import { performance } from 'node:perf_hooks';
import { readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { createRun, dispatchCommand } from '../../dist/packages/core/run.js';
import { runScenario } from '../../dist/packages/runtime-service/scenario.js';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';
import { nativeRequest, closeNativeRuntimes } from '../../dist/packages/native-bridge/index.js';

const root = resolve('examples/academy-year');
const { snapshot } = await buildSnapshot(await loadProject(root));
const scenario = JSON.parse(await readFile(resolve(root, 'scenarios/wanderer-year.json'), 'utf8'));
const report = { contentBytes: Buffer.byteLength(JSON.stringify(snapshot)), measurements: [] };
async function measure(label, repetitions, fn) {
  const begun = performance.now();
  let last;
  for (let index = 0; index < repetitions; index++) last = await fn();
  const elapsedMs = performance.now() - begun;
  const value = { label, repetitions, elapsedMs: Math.round(elapsedMs * 100) / 100, perOperationMs: Math.round(elapsedMs / repetitions * 1000) / 1000 };
  report.measurements.push(value);
  if (last?.ok === false) throw Error(`${label}: ${JSON.stringify(last)}`);
  return last;
}

await measure('native cold worker + info', 1, () => nativeRequest({ operation: 'info' }));
await measure('native warm info roundtrip', 500, () => nativeRequest({ operation: 'info' }));
const options = { mode: 'full' }, state = createRun(snapshot, 42, options).state;
const command = { type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] };
await measure('TS pure schedule transaction', 500, () => dispatchCommand(snapshot, state, command, 0));
await measure('native schedule incl full JSON boundary', 500, () => nativeRequest({ operation: 'command', content: snapshot, state, command, expectedRevision: 0 }));
for (const backend of ['typescript', 'native']) {
  process.env.RAISING_RUNTIME = backend;
  const direct = await measure(`${backend} complete 12-month scenario`, 3, () => runScenario(snapshot, scenario));
  report.measurements.at(-1).commands = direct.steps;
  let saves = 0;
  await measure(`${backend} scenario save+decode after every command`, 1, () => runScenario(snapshot, scenario, {
    checkpoint: async session => {
      saves++;
      const encoded = await encodeSave(snapshot, session);
      if (!encoded.ok) return encoded;
      return decodeSave(snapshot, encoded.text);
    },
  }));
  report.measurements.at(-1).saveCheckpoints = saves;
}
closeNativeRuntimes();
await writeFile(resolve('.cache/journey-native-benchmark.json'), JSON.stringify(report, null, 2));
console.log(JSON.stringify(report));

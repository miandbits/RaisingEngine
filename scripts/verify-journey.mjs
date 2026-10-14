import { writeFile, mkdir } from 'node:fs/promises';
import { StudioEditor } from '../dist/packages/studio-service/editor.js';
import { AnalysisJobs } from '../dist/packages/analysis-service/jobs.js';
const root = process.argv[2] ?? 'examples/academy-journey';
const editor = new StudioEditor();
await editor.open(root, true);
const built = await editor.snapshot(editor.revision);
if (!built.ok) throw new Error(JSON.stringify(built.diagnostics));
const jobs = new AnalysisJobs();
const variants = [
  { name: 'random-1000', runs: 1000, seed: 42000, strategy: 'random', maxCommands: 2000, timeoutMs: 3600000 },
  { name: 'relationship-reachability', runs: 50, seed: 42, strategy: 'explore', maxCommands: 2000, timeoutMs: 300000,
    target: { kind: 'event', id: 'flowers-bring-friendship' }, beamWidth: 16, maxStates: 10000 },
];
await mkdir('.cache/journey-analysis', { recursive: true });
for (const { name, ...options } of variants) {
  let job = await jobs.start(root, built.snapshot, editor.diskVersion(), null, options);
  console.log(JSON.stringify({ name, id: job.id, status: job.status }));
  while (job.status === 'running') {
    await new Promise(resolve => setTimeout(resolve, 1000)); job = await jobs.get(root, job.id);
  }
  await writeFile(`.cache/journey-analysis/${name}.json`, JSON.stringify(job, null, 2));
  console.log(JSON.stringify({ name, id: job.id, status: job.status, completed: job.result.completed,
    outcomes: job.result.outcomes, targetStatus: job.result.targetStatus, events: job.result.events,
    endings: job.result.endings, stopReason: job.result.stopReason, error: job.error }));
}

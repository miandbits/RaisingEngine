import { mkdir,mkdtemp,cp,writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import assert from 'node:assert/strict';
import { StudioEditor } from '../dist/packages/studio-service/editor.js';
import { AnalysisJobs } from '../dist/packages/analysis-service/jobs.js';
import { replayTrace } from '../dist/packages/analysis-service/runner.js';
await mkdir('.cache/analysis-scale',{recursive:true});const root=await mkdtemp(resolve('.cache/analysis-scale/year-'));await cp('examples/academy-year',root,{recursive:true});
const editor=new StudioEditor();await editor.open(root);const snapshot=await editor.snapshot(editor.revision);assert.ok(snapshot.ok);
const jobs=new AnalysisJobs();const start=Date.now();let job=await jobs.start(root,snapshot.snapshot,editor.diskVersion(),editor.revision,{runs:2000,seed:42,strategy:'random',maxCommands:2000,timeoutMs:600000});
while(job.status==='running'){await new Promise(r=>setTimeout(r,1000));job=await jobs.get(root,job.id);}
assert.equal(job.status,'completed',job.error);assert.equal(job.result.completed,2000);assert.equal(job.result.outcomes.finished,2000);for(const trace of job.result.traces)replayTrace(snapshot.snapshot,trace);
const summary={root,id:job.id,elapsedMs:Date.now()-start,completed:job.result.completed,outcomes:job.result.outcomes,endings:job.result.endings,eventsVisited:Object.keys(job.result.events).length,choicesVisited:Object.keys(job.result.choices).length,retainedReplays:job.result.traces.length};
await writeFile('.cache/analysis-scale/summary.json',JSON.stringify(summary,null,2));console.log(JSON.stringify(summary));

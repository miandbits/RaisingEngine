import {createHash} from 'node:crypto';
import { workerData } from 'node:worker_threads';
import { existsSync } from 'node:fs';
import {mkdir,writeFile} from 'node:fs/promises';
import path from 'node:path';
import { atomicJson } from './jobs.js';
import { runAnalysisPool } from './pool.js';
import type { AnalysisJob } from './model.js';
import type { ContentSnapshot } from '../core/model.js';
import { freeze } from '../core/util.js';
import {summarizeRun} from './routes.js';
import {ArchiveWriter} from './archive.js';
const { job, content, dir } = workerData as { job: AnalysisJob; content: ContentSnapshot; dir: string };
const file=path.join(dir,`${job.id}.json`),prefix=path.join(dir,job.id),archive=new ArchiveWriter(prefix);let last=0,lastCompleted=-1;
const saveFull=()=>atomicJson(file,{...job,ownerPid:process.pid});
const saveLight=()=>atomicJson(`${prefix}.progress.json`,{...job,result:{...job.result,routeGraph:undefined,traces:[]},ownerPid:process.pid});
try {
  // Worker structured cloning removes Object.freeze. Restore the snapshot's
  // read-only contract once so native commands can reuse registered content.
  // This is the worker-owned copy; the caller's editable data stays mutable.
  freeze(content);
  const traces=path.join(dir,`${job.id}.runs`);await mkdir(traces,{recursive:true});
  job.result = await runAnalysisPool(content, job.options, { trace:async trace=>{
    const text=JSON.stringify(trace);await writeFile(path.join(traces,`${trace.run}.json`),text,{flag:'wx'});
    await archive.append({...summarizeRun(trace),traceHash:createHash('sha256').update(text).digest('hex')});
  },cancelled: () => existsSync(`${file}.cancel`), progress: async result => {
    job.result=result;if(result.completed!==lastCompleted){lastCompleted=result.completed;await saveLight();}
    if(Date.now()-last>5000){last=Date.now();await saveFull();}
  } });
  job.status = job.result.stopReason === 'cancelled' ? 'cancelled' : 'completed';
} catch (e) { job.status = 'error'; job.error = e instanceof Error ? e.message : String(e); }
await saveFull();await saveLight();

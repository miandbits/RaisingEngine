import { parentPort, workerData, threadId } from 'node:worker_threads';
import { freeze } from '../core/util.js';
import type { ContentSnapshot } from '../core/model.js';
import type { AnalysisOptions, Trace } from './model.js';
import { runAnalysis } from './runner.js';
const { content, options, control } = workerData as {content:ContentSnapshot;options:AnalysisOptions;control:SharedArrayBuffer};
const flag=new Int32Array(control);
freeze(content);
parentPort!.on('message',async ({run}:{run:number})=>{
  try{
    let trace:Trace|null=null;
    const result=await runAnalysis(content,{...options,runs:1,workers:1,seed:(options.seed+run)>>>0},{cancelled:()=>Atomics.load(flag,0)!==0,trace:t=>{t.run=run;trace=t;}});
    parentPort!.postMessage({run,threadId,trace,blocked:result.blocked,exploredStates:result.exploredStates});
  }catch(error){parentPort!.postMessage({run,threadId,error:error instanceof Error?error.message:String(error)});}
});

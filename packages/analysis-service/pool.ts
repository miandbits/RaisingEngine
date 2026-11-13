import { Worker } from 'node:worker_threads';
import type { ContentSnapshot } from '../core/model.js';
import { emptyResult, validateOptions, type AnalysisOptions, type Trace } from './model.js';
import { record, runAnalysis, type RunHooks } from './runner.js';
type Message={run:number;threadId:number;trace:Trace|null;blocked:Record<string,number>;exploredStates:number;error?:string};
/** Ordered, bounded merge: scheduling never affects seeds, coverage, or sampled traces. */
export async function runAnalysisPool(content:ContentSnapshot,input:AnalysisOptions,hooks:RunHooks={}){
  const options=validateOptions(input,content), count=options.target||options.strategy==='explore'?1:Math.min(options.workers??1,options.runs);
  if(count===1)return runAnalysis(content,options,hooks);
  const result=emptyResult(options.runs),control=new SharedArrayBuffer(4),flag=new Int32Array(control),started=Date.now();
  const threads:Worker[]=[],idle=new Set<Worker>(),inflight=new Map<Worker,number>(),pending=new Map<number,Message>();
  const queue:{worker:Worker;message:Message}[]=[];
  let wake:(()=>void)|undefined,nextAssign=0,nextCommit=0,failure:Error|undefined,closing=false;
  const stopped=()=>{if(hooks.cancelled?.())Atomics.store(flag,0,1);else if(Date.now()-started>=options.timeoutMs)Atomics.compareExchange(flag,0,0,2);return Atomics.load(flag,0)!==0;};
  const timer=setInterval(()=>{if(stopped())wake?.();},25);
  const schedule=()=>{for(const worker of idle){if(stopped()||nextAssign>=options.runs||nextAssign>=nextCommit+count)break;idle.delete(worker);inflight.set(worker,nextAssign);worker.postMessage({run:nextAssign++});}};
  try{
    for(let i=0;i<count;i++){
      const worker=new Worker(new URL('./run-worker.js',import.meta.url),{workerData:{content,options,control},execArgv:[],env:{...process.env,NODE_OPTIONS:''}});threads.push(worker);idle.add(worker);
      worker.on('message',(message:Message)=>{queue.push({worker,message});wake?.();});
      worker.on('error',error=>{failure=error;wake?.();});
      worker.on('exit',code=>{if(!closing){failure=new Error(`분석 스레드가 예기치 않게 종료되었습니다: ${code}`);wake?.();}});
    }
    schedule();
    while(inflight.size||pending.size){
      if(failure)throw failure;
      if(!queue.length){await new Promise<void>(resolve=>{wake=resolve;});wake=undefined;if(failure)throw failure;if(!queue.length)continue;}
      for(const {worker,message}of queue.splice(0)){
        if(message.error)throw new Error(message.error);
        if(inflight.get(worker)!==message.run)throw new Error('분석 스레드 회차가 일치하지 않습니다.');
        inflight.delete(worker);idle.add(worker);pending.set(message.run,message);
      }
      while(pending.has(nextCommit)){
        const message=pending.get(nextCommit)!;pending.delete(nextCommit++);
        result.exploredStates+=message.exploredStates;
        for(const [key,value]of Object.entries(message.blocked))result.blocked[key]=(result.blocked[key]??0)+value;
        if(message.trace){
          if(Atomics.load(flag,0)===2&&message.trace.outcome==='limit'){message.trace.reason='분석 시간 제한';if(message.trace.balance?.termination==='cancelled')message.trace.balance.termination='time-limit';}
          await hooks.trace?.(message.trace);record(result,message.trace,undefined);
        }
        await hooks.progress?.(result);
      }
      schedule();
    }
    result.stopReason=Atomics.load(flag,0)===1?'cancelled':Atomics.load(flag,0)===2?'time-limit':'completed';
    return result;
  }finally{closing=true;clearInterval(timer);Atomics.store(flag,0,1);await Promise.all(threads.map(worker=>worker.terminate()));}
}

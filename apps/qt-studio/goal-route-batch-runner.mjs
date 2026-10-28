import {parentPort,workerData} from 'node:worker_threads';
import {freeze} from '../../dist/packages/core/util.js';
import {checkGoalImpacts,goalBaselineInputStamp} from './goal-impact.mjs';
import {runGoalRoute} from '../../dist/packages/analysis-service/goal-routes.js';
import {captureGoalBaseline} from '../../dist/packages/analysis-service/goal-history.js';
import {stageGoalBaseline} from './goal-baseline-publication.mjs';
import {runtimeIdentity} from './goal-baselines.mjs';
const {content,entries,limits,cancelBuffer,impact,baseline}=workerData,cancel=new Int32Array(cancelBuffer),started=Date.now();
freeze(content); // IPC cloning must not disable native immutable-content registration.
async function captureBaseline(entry,index,remaining){
  const checkRuntime=async()=>{if(await runtimeIdentity()!==baseline.runtime)throw Object.assign(Error('The runtime changed during baseline capture. Restart with the current runtime.'),{code:'STALE_RUNTIME'});};
  await checkRuntime();
  const captured=await captureGoalBaseline(content,entry.route,baseline.names[entry.route.id],baseline.runtime,{
    timeoutMs:Math.min(limits.routeTimeoutMs,remaining),cancelled:()=>!!Atomics.load(cancel,0),progress:value=>parentPort.postMessage({index,progress:value})});
  if(!captured.trace.reached||captured.trace.failure||captured.trace.unexecuted)throw Object.assign(Error('The registered route did not reach its target. No baseline was published.'),{code:'TARGET_NOT_REACHED'});
  if(Atomics.load(cancel,0))throw Object.assign(Error('Baseline capture cancelled.'),{code:'CANCELLED'});
  await checkRuntime();
  const metadata=await stageGoalBaseline(baseline.stagingPrefix+'.baseline-'+index+'.pending',captured);
  const published=await new Promise(resolve=>{const receive=value=>{if(value.index!==index)return;parentPort.off('message',receive);resolve(value);};parentPort.on('message',receive);parentPort.postMessage({publishBaseline:{index,metadata}});});
  return {id:entry.route.id,name:entry.route.name,commands:entry.route.commands.length,executed:captured.trace.steps.length-1,ok:published.ok,...(published.ok?{baseline:published.baseline}:{failure:published.failure})};
}
for(let index=0;index<entries.length;index++){
  if(Atomics.load(cancel,0))break;
  const remaining=limits.batchTimeoutMs-(Date.now()-started);
  if(remaining<=0){parentPort.postMessage({done:true,reason:'timed-out'});break;}
  const entry=entries[index];parentPort.postMessage({active:index});
  let report;
  try{report=entry.error?{ok:false,status:'failed',failure:{step:0,code:'INVALID_ROUTE',message:entry.error}}:baseline?await captureBaseline(entry,index,remaining):await runGoalRoute(content,entry.route,{timeoutMs:Math.min(limits.routeTimeoutMs,remaining),cancelled:()=>!!Atomics.load(cancel,0),progress:value=>parentPort.postMessage({index,progress:value})});}
  catch(error){report={ok:false,status:'failed',failure:{step:0,code:error.code??(/64 MiB/.test(error.message)?'TRACE_TOO_LARGE':'ERROR'),message:String(error.message).slice(0,4096)}};}
  parentPort.postMessage({index,report:{...report,status:report.ok?'passed':report.failure?.code==='TIMEOUT'?'timed-out':report.failure?.code==='CANCELLED'?'cancelled':'failed'}});
  if(baseline&&['STALE_INPUT','STALE_RUNTIME','CANCELLED'].includes(report.failure?.code))break;
}
if(impact&&!Atomics.load(cancel,0)&&Date.now()-started<limits.batchTimeoutMs){
  parentPort.postMessage({stage:'comparison'});
  let goalImpact;
  try{
    if(await goalBaselineInputStamp(impact.root)!==impact.baselineStamp)throw Error('Baseline records changed after save. Save again to compare the new selection.');
    goalImpact=await checkGoalImpacts(impact.root,content,impact.revision,{capturedRoutes:entries,timeoutMs:Math.max(1,limits.batchTimeoutMs-(Date.now()-started))});
    if(await goalBaselineInputStamp(impact.root)!==impact.baselineStamp)Object.assign(goalImpact,{stale:true,complete:false,ok:false,error:'Baseline records changed during comparison. Save again to recheck.'});
  }catch(error){goalImpact={rows:[],complete:false,ok:false,stale:true,error:String(error.message).slice(0,4096)};}
  parentPort.postMessage({goalImpact});
}
parentPort.postMessage({done:true,reason:Atomics.load(cancel,0)?'cancelled':Date.now()-started>=limits.batchTimeoutMs?'timed-out':null});

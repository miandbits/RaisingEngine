import {parentPort,workerData} from 'node:worker_threads';
import {freeze} from '../../dist/packages/core/util.js';
import {checkGoalImpacts,goalBaselineInputStamp} from './goal-impact.mjs';
import {runGoalRoute} from '../../dist/packages/analysis-service/goal-routes.js';
const {content,entries,limits,cancelBuffer,impact}=workerData,cancel=new Int32Array(cancelBuffer),started=Date.now();
freeze(content); // IPC cloning must not disable native immutable-content registration.
for(let index=0;index<entries.length;index++){
  if(Atomics.load(cancel,0))break;
  const remaining=limits.batchTimeoutMs-(Date.now()-started);
  if(remaining<=0){parentPort.postMessage({done:true,reason:'timed-out'});break;}
  const entry=entries[index];parentPort.postMessage({active:index});
  let report;
  try{report=entry.error?{ok:false,status:'failed',failure:{step:0,code:'INVALID_ROUTE',message:entry.error}}:await runGoalRoute(content,entry.route,{timeoutMs:Math.min(limits.routeTimeoutMs,remaining),cancelled:()=>!!Atomics.load(cancel,0),progress:value=>parentPort.postMessage({index,progress:value})});}
  catch(error){report={ok:false,status:'failed',failure:{step:0,code:'ERROR',message:String(error.message).slice(0,4096)}};}
  parentPort.postMessage({index,report:{...report,status:report.ok?'passed':report.failure?.code==='TIMEOUT'?'timed-out':report.failure?.code==='CANCELLED'?'cancelled':'failed'}});
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

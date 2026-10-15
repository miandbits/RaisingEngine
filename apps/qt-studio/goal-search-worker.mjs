import {parentPort,workerData} from 'node:worker_threads';
import {searchGoalRoute} from '../../dist/packages/analysis-service/goal-search.js';
try {
  const result=await searchGoalRoute(workerData.content,workerData.options,{progress:progress=>parentPort.postMessage({progress})});
  parentPort.postMessage({result});
} catch(error) {parentPort.postMessage({error:String(error?.message??error)});}

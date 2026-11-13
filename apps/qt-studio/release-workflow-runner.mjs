import {parentPort,workerData} from 'node:worker_threads';
import {runReleaseWorkflowJob} from './release-workflow-checks.mjs';
import {closeNativeRuntimes} from '../../dist/packages/native-bridge/index.js';
try{const result=await runReleaseWorkflowJob(workerData,progress=>parentPort.postMessage({progress}));parentPort.postMessage({result});}
catch(error){parentPort.postMessage({error:{message:error.message,code:error.code??null}});}
finally{closeNativeRuntimes();}

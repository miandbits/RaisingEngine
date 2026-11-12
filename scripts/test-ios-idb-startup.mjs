import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {mkdir,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {cleanUpIOSSimulator,runBoundedAppleTool,selectIOSSimulator} from './lib/ios-simulator-test.mjs';
import {idbExperimentConfiguration,startIDBCompanion} from './lib/ios-idb-contact-test.mjs';
import {observeIDBStartup} from './lib/ios-idb-startup-observer.mjs';

const configuration=idbExperimentConfiguration();
assert.equal(configuration?.backend,'startup');
const out=path.resolve('.cache/ios-idb-startup');
await mkdir(out);
const receipt={socketObserved:false,guestRegistration:'unobserved',rpcCalls:0,primitiveEvents:0};
const xcrun='/usr/bin/xcrun';
const tool=(command,args,log,timeout)=>runBoundedAppleTool(command,args,{cwd:process.cwd(),logFile:path.join(out,log),timeout});
const sim=(...args)=>execFileSync(xcrun,['simctl',...args],{encoding:'utf8',timeout:30000}).trim();
let device,companion,saveStartupObservation;
try {
  const {runtime,type}=selectIOSSimulator(JSON.parse(sim('list','runtimes','--json')).runtimes,
    {runtimeId:process.env.RAISING_IOS_SIMULATOR_RUNTIME,deviceType:process.env.RAISING_IOS_SIMULATOR_DEVICE_TYPE});
  device=sim('create','RaisingEngine idb no-input compatibility',type,runtime);
  Object.assign(receipt,{device,runtime,type});
  await tool(xcrun,['simctl','boot',device],'boot-start.log',60000);
  await tool(xcrun,['simctl','bootstatus',device,'-b'],'boot.log',180000);
  receipt.bootCompletedAtUTC=new Date().toISOString();
  companion=await startIDBCompanion(configuration.companion,device,out,{observeStartup:observeIDBStartup});
  receipt.socketObserved=true;
} catch(error) {
  receipt.error=String(error);
  saveStartupObservation=error.saveStartupObservation;
  throw error;
} finally {
  if(companion) {
    try {
      const cleanup=await companion.stop();
      receipt.companionCleanup=cleanup;
      saveStartupObservation=()=>companion.saveObservation(cleanup);
      assert.ok(cleanup.exitCode===0&&cleanup.signal===null&&!cleanup.forced,'idb companion did not exit cleanly');
    } catch(error) {receipt.companionCleanupError=String(error);}
  }
  if(device)receipt.cleanupErrors=await cleanUpIOSSimulator(device,(action,id)=>tool(xcrun,['simctl',action,id],action+'.log',60000));
  if(saveStartupObservation)try {await saveStartupObservation();}
  catch(error) {receipt.observationError=String(error);}
  await writeFile(path.join(out,'result.json'),JSON.stringify(receipt,null,2));
  if(!receipt.error&&(receipt.cleanupErrors?.delete||receipt.companionCleanupError))throw Error('idb startup observation cleanup failed');
}

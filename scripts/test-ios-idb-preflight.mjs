import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {mkdir,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {cleanUpIOSSimulator,runBoundedAppleTool,selectIOSSimulator} from './lib/ios-simulator-test.mjs';
import {idbExperimentConfiguration,startIDBCompanion,verifyIDBPreflight} from './lib/ios-idb-contact-test.mjs';

const configuration=idbExperimentConfiguration();
assert.equal(configuration?.backend,'preflight');
const out=path.resolve('.cache/ios-idb-preflight');
await mkdir(out,{recursive:false});
const receipt={ok:false,guestRegistration:'unobserved',primitiveEvents:0};
const xcrun='/usr/bin/xcrun';
const tool=(command,args,log,timeout)=>runBoundedAppleTool(command,args,{cwd:process.cwd(),logFile:path.join(out,log),timeout});
const sim=(...args)=>execFileSync(xcrun,['simctl',...args],{encoding:'utf8',timeout:30000}).trim();
let device,companion;
try {
  const {runtime,type}=selectIOSSimulator(JSON.parse(sim('list','runtimes','--json')).runtimes,
    {runtimeId:process.env.RAISING_IOS_SIMULATOR_RUNTIME,deviceType:process.env.RAISING_IOS_SIMULATOR_DEVICE_TYPE});
  device=sim('create','RaisingEngine idb no-input compatibility',type,runtime);
  Object.assign(receipt,{device,runtime,type});
  await tool(xcrun,['simctl','boot',device],'boot-start.log',60000);
  await tool(xcrun,['simctl','bootstatus',device,'-b'],'boot.log',180000);
  companion=await startIDBCompanion(configuration.companion,device,out);
  receipt.screen=await verifyIDBPreflight({companion,python:configuration.python,
    coordinator:path.resolve('scripts/lib/ios-idb-contact.py'),device,out,tool});
  receipt.ok=true;
} catch(error) {receipt.error=String(error);throw error;}
finally {
  if(companion) {
    try {
      const cleanup=await companion.stop();
      receipt.companionCleanup=cleanup;
      assert.ok(cleanup.exitCode===0&&cleanup.signal===null&&!cleanup.forced,'idb companion did not exit cleanly');
    } catch(error) {receipt.ok=false;receipt.companionCleanupError=String(error);}
  }
  if(device)receipt.cleanupErrors=await cleanUpIOSSimulator(device,(action,id)=>tool(xcrun,['simctl',action,id],action+'.log',60000));
  if(receipt.cleanupErrors?.delete)receipt.ok=false;
  await writeFile(path.join(out,'result.json'),JSON.stringify(receipt,null,2));
  if(!receipt.error&&!receipt.ok)throw Error('idb preflight cleanup failed');
}

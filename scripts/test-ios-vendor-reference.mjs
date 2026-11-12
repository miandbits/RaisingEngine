import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {createHash,randomBytes} from 'node:crypto';
import {cp,mkdir,mkdtemp,readFile,stat,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
import {cleanUpIOSSimulator,runBoundedAppleTool,selectIOSSimulator} from './lib/ios-simulator-test.mjs';
import {captureVendorTouchLog,vendorIsolationConfiguration,verifyVendorContactMarker} from './lib/ios-vendor-observation.mjs';

const configuration=vendorIsolationConfiguration();
assert.ok(configuration&&['U','S'].includes(configuration.arm),'Select the UIKit or SDL vendor reference');
const {arm,output}=configuration;
assert.ok(process.argv[2],'Pass the verified reference app bundle');
const source=path.resolve(process.argv[2]);
const root=process.cwd(),xcrun='/usr/bin/xcrun';
const hash=async file=>createHash('sha256').update(await readFile(file)).digest('hex');
const sim=(...args)=>execFileSync(xcrun,['simctl',...args],{encoding:'utf8',timeout:30000}).trim();
await mkdir(output,{recursive:true});
const out=await mkdtemp(path.join(output,'run-'));
const app=path.join(out,path.basename(source)),receipt={arm,ok:false,productAcceptance:false,
  requestedContact:{x:191,y:572.5000305,durationSeconds:0.20,count:1}};
const tool=(command,args,log,timeout,env)=>runBoundedAppleTool(command,args,{cwd:root,logFile:path.join(out,log),timeout,env});
let device,launchTime,executable;
try {
  await cp(source,app,{recursive:true});
  const plist=path.join(app,'Info.plist');
  const info=JSON.parse(execFileSync('/usr/bin/plutil',['-convert','json','-o','-',plist],{encoding:'utf8'}));
  executable=info.CFBundleExecutable;
  assert.match(executable,/^[A-Za-z0-9_-]+$/,'Invalid app executable name');
  const appId=info.CFBundleIdentifier;
  assert.match(appId,/^org\.raisingengine\.[a-zA-Z0-9._-]+$/,'Unexpected reference bundle identifier');
  const appExecutableSha256=await hash(path.join(app,executable));
  assert.equal(appExecutableSha256,await hash(path.join(source,executable)));
  Object.assign(receipt,{appId,executable,appExecutableSha256});

  const harness=path.join(out,'harness');
  await mkdir(harness);
  await cp(path.join(root,'tests/ios-system-ui/SystemUI.xcodeproj'),path.join(harness,'SystemUI.xcodeproj'),{recursive:true});
  const template=path.join(root,'.github/diagnostics/ios-vendor-isolation',arm+'-ReferenceTests.swift');
  receipt.harnessSourceSha256=await hash(template);
  const swift=(await readFile(template,'utf8')).replaceAll('__APP_ID__',appId);
  await writeFile(path.join(harness,'SystemUITests.swift'),swift);
  const build=['-project',path.join(harness,'SystemUI.xcodeproj'),'-scheme','SystemUITests',
    '-derivedDataPath',path.join(out,'DerivedData'),'CODE_SIGNING_ALLOWED=NO'];
  await tool('/usr/bin/xcodebuild',['build-for-testing',...build,'-destination','generic/platform=iOS Simulator'],'build.log');

  const {runtime,type}=selectIOSSimulator(JSON.parse(sim('list','runtimes','--json')).runtimes,
    {runtimeId:process.env.RAISING_IOS_SIMULATOR_RUNTIME,deviceType:process.env.RAISING_IOS_SIMULATOR_DEVICE_TYPE});
  receipt.createAttempted=true;
  const created=sim('create','RaisingEngine vendor '+arm+' '+path.basename(out),type,runtime);
  assert.match(created,/^[0-9a-f]{8}-(?:[0-9a-f]{4}-){3}[0-9a-f]{12}$/i,'Simulator create must return one owned UUID');
  device=created;
  receipt.device={id:device,runtime,type};
  await tool(xcrun,['simctl','boot',device],'boot-start.log',60000);
  await tool(xcrun,['simctl','bootstatus',device,'-b'],'boot.log',180000);
  await tool(xcrun,['simctl','install',device,app],'install.log',120000);
  const stdout=path.join(out,'reference.stdout');
  await writeFile(stdout,'',{flag:'wx',mode:0o600});
  const nonce=randomBytes(32).toString('hex');
  launchTime=Math.floor(Date.now()/1000);
  await tool(xcrun,['simctl','launch','--stdout='+stdout,'--stderr='+path.join(out,'reference.stderr'),
    device,appId,'--input-proof-nonce',nonce],'launch.log',90000);
  const launched=await readFile(path.join(out,'launch.log'),'utf8');
  const pidLines=launched.split('\n').filter(line=>line.startsWith(appId+':'));
  assert.equal(pidLines.length,1,'Simulator launch must identify one owned app PID');
  const pid=Number(pidLines[0].slice(appId.length+1).trim());
  assert.ok(Number.isInteger(pid)&&pid>0);
  process.kill(pid,0);
  const {dev,ino}=await stat(stdout,{bigint:true});
  const input={version:1,arm,appId,pid,nonce,stdout,stdoutDevice:String(dev),stdoutInode:String(ino)};
  receipt.contact=input;
  // Match the existing engine runner's delay between launch binding and XCTest.
  await delay(1000);
  await tool('/usr/bin/xcodebuild',['test-without-building',...build,'-destination','platform=iOS Simulator,id='+device,
    '-parallel-testing-enabled','NO','-resultBundlePath',path.join(out,'reference.xcresult'),
    '-only-testing:SystemUITests/SystemUITests/testSingleContact'],'test.log',undefined,
  {...process.env,TEST_RUNNER_RAISING_VENDOR_REFERENCE:JSON.stringify(input)});
  receipt.proofAccepted=verifyVendorContactMarker(await readFile(path.join(out,'test.log'),'utf8'),input);
  receipt.ok=true;
} catch(error) {
  receipt.error=String(error);
  throw error;
} finally {
  if(device&&receipt.device?.id===device) {
    if(launchTime!==undefined)receipt.osObservation=await captureVendorTouchLog({device,start:launchTime,executable,out,tool});
    receipt.cleanupErrors=await cleanUpIOSSimulator(device,(action,id)=>tool(xcrun,['simctl',action,id],action+'.log',60000));
    receipt.deletedDevice=!receipt.cleanupErrors.delete;
    if(!receipt.deletedDevice)receipt.ok=false;
  }
  receipt.finishedAt=new Date().toISOString();
  await writeFile(path.join(out,'result.json'),JSON.stringify(receipt,null,2));
  await writeFile(path.join(output,'latest.json'),JSON.stringify({ok:receipt.ok,out},null,2));
  if(!receipt.error&&!receipt.ok)throw Error(`Reference cleanup failed; inspect ${out}`);
}

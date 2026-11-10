import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {cp, mkdir, mkdtemp, readFile, writeFile} from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {cleanUpIOSSimulator,runBoundedAppleTool,selectIOSSimulator} from './lib/ios-simulator-test.mjs';

// This is a focused VoiceOver smoke for the checked-in public starter.
const args = process.argv.slice(2);
const buildOnly = args.includes('--build-only');
const [appArg, evidenceArg] = args.filter(arg => arg !== '--build-only');
assert.ok(process.platform === 'darwin' && appArg,
  'Usage: node scripts/test-ios-voiceover.mjs STARTER_APP [EVIDENCE_DIRECTORY] [--build-only]');
const repo = fileURLToPath(new URL('..', import.meta.url));
const app = path.resolve(appArg);
const evidence = path.resolve(evidenceArg ?? path.join(repo, '.cache/ios-voiceover'));
await mkdir(evidence, {recursive:true});
const out = await mkdtemp(path.join(evidence,'run-'));
const tool = (name,args) => execFileSync(name,args,{encoding:'utf8',timeout:45000}).trim();
const sim = (...args) => tool('/usr/bin/xcrun',['simctl',...args]);
const simInfo = (...args) => JSON.parse(sim('list','-j',...args));
const plist = JSON.parse(tool('/usr/bin/plutil',['-convert','json','-o','-',path.join(app,'Info.plist')]));
const bundleId = plist.CFBundleIdentifier;
assert.match(bundleId,/^[A-Za-z0-9.-]+$/);
const version = tool('/usr/bin/xcodebuild',['-version']);
assert.ok(Number(/Xcode (\d+)/.exec(version)?.[1]) >= 27,'The VoiceOver service requires Xcode 27 or later.');
const hash = bytes => createHash('sha256').update(bytes).digest('hex');
const source = path.join(repo,'tests/ios-system-ui');
const project = path.join(out,'SystemUI.xcodeproj');
const sourceFile = await readFile(path.join(source,'VoiceOverTests.m'),'utf8');
const gameJson = await readFile(path.join(app,'game/game.json'));
const receipt = {
  passed:false, harnessBuilt:false, buildOnly, app, bundleId, version,
  executableSha256:hash(await readFile(path.join(app,plist.CFBundleExecutable))),
  gameJsonSha256:hash(gameJson), contentFingerprint:JSON.parse(gameJson).content.contentFingerprint,
  runnerSha256:hash(await readFile(new URL(import.meta.url))),
  testSourceSha256:hash(sourceFile), resultBundle:path.join(out,'result.xcresult'),
  scope:buildOnly
    ? 'Compile the public starter VoiceOver XCTest harness without running it.'
    : 'Real VoiceOver focus and utterances on the public starter title, menu and first schedule.',
  limits:[
    'Ordinary taps with VoiceOver disabled open the menu and start the game.',
    'Gesture activation and physical-device acceptance remain separate.'
  ]
};
const persist = () => writeFile(path.join(out,'result.json'),JSON.stringify(receipt,null,2)+'\n');
const logTool=(command,args,logName,timeout)=>runBoundedAppleTool(command,args,
  {logFile:path.join(out,logName),timeout});
try {
  await cp(path.join(source,'SystemUI.xcodeproj'),project,{recursive:true});
  const pbx=path.join(project,'project.pbxproj');
  await writeFile(pbx,(await readFile(pbx,'utf8')).replace('sourcecode.swift','sourcecode.c.objc')
    .replace('path = SystemUITests.swift','path = VoiceOverTests.m'));
  await writeFile(path.join(out,'VoiceOverTests.m'),sourceFile.replace('__APP_ID__',bundleId));
  const buildArgs=['-project',project,'-scheme','SystemUITests','-derivedDataPath',path.join(out,'DerivedData'),
    '-parallel-testing-enabled','NO','IPHONEOS_DEPLOYMENT_TARGET=27.0','CODE_SIGNING_ALLOWED=NO'];
  if(buildOnly) {
    await logTool('/usr/bin/xcodebuild',['build-for-testing',...buildArgs,
      '-destination','generic/platform=iOS Simulator'],'build.log');
    receipt.harnessBuilt=true;
  } else {
    const {runtime,type}=selectIOSSimulator(simInfo('runtimes').runtimes,{minimumMajor:27,
      runtimeId:process.env.RAISING_IOS_SIMULATOR_RUNTIME,deviceType:process.env.RAISING_IOS_SIMULATOR_DEVICE_TYPE});
    receipt.runtime=runtime;
    receipt.deviceType=type;
    await persist();
    receipt.device=sim('create','RaisingEngine VoiceOver '+path.basename(out),type,runtime);
    await persist();
    await logTool('/usr/bin/xcrun',['simctl','boot',receipt.device],'boot-start.log',60000);
    await logTool('/usr/bin/xcrun',['simctl','bootstatus',receipt.device,'-b'],'boot.log',180000);
    await logTool('/usr/bin/xcrun',['simctl','install',receipt.device,app],'install.log',120000);
    await logTool('/usr/bin/xcodebuild',['test',...buildArgs,'-destination',
      'platform=iOS Simulator,id='+receipt.device,'-resultBundlePath',receipt.resultBundle],'test.log');
    receipt.harnessBuilt=true;
    receipt.passed=true;
  }
} catch(error) {
  receipt.error=String(error);
  throw error;
} finally {
  if(receipt.device) {
    receipt.cleanupErrors=await cleanUpIOSSimulator(receipt.device,(action,id)=>
      logTool('/usr/bin/xcrun',['simctl',action,id],action+'.log',60000));
    receipt.deletedDevice=!receipt.cleanupErrors.delete;
    if(!receipt.deletedDevice)receipt.passed=false;
  }
  await persist();
  console.log('iOS VoiceOver evidence: '+out);
  if(!receipt.error&&!buildOnly&&!receipt.passed)throw Error('Simulator cleanup failed; inspect '+out);
}

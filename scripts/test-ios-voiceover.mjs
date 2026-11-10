import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {cp, mkdir, mkdtemp, readFile, writeFile} from 'node:fs/promises';
import {openSync, closeSync} from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

// This is a focused VoiceOver smoke for the checked-in public starter.
const [appArg, evidenceArg] = process.argv.slice(2);
assert.ok(process.platform === 'darwin' && appArg,
  'Usage: node scripts/test-ios-voiceover.mjs STARTER_APP [EVIDENCE_DIRECTORY]');
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
const runtime = simInfo('runtimes').runtimes.filter(r=>r.isAvailable && r.identifier.includes('.iOS-') &&
  Number(r.version.split('.')[0])>=27).reverse().find(r=>r.supportedDeviceTypes.some(ordinaryPhone));
function ordinaryPhone(type) {
  return type.identifier.includes('.iPhone-') && !type.identifier.endsWith('.iPhone-Duo');
}
assert.ok(runtime,'Install an iOS 27+ simulator runtime with a nonfolding iPhone in Xcode.');
const type = runtime.supportedDeviceTypes.find(type=>type.identifier.endsWith('.iPhone-17e')) ??
  runtime.supportedDeviceTypes.find(ordinaryPhone);
const source = path.join(repo,'tests/ios-system-ui');
const project = path.join(out,'SystemUI.xcodeproj');
const sourceFile = await readFile(path.join(source,'VoiceOverTests.m'),'utf8');
const gameJson = await readFile(path.join(app,'game/game.json'));
const receipt = {
  passed:false, app, bundleId, version, runtime:runtime.identifier, deviceType:type.identifier,
  executableSha256:hash(await readFile(path.join(app,plist.CFBundleExecutable))),
  gameJsonSha256:hash(gameJson), contentFingerprint:JSON.parse(gameJson).content.contentFingerprint,
  runnerSha256:hash(await readFile(new URL(import.meta.url))),
  testSourceSha256:hash(sourceFile), resultBundle:path.join(out,'result.xcresult'),
  scope:'Real VoiceOver focus and utterances on the public starter title and menu.',
  limits:[
    'An ordinary tap with VoiceOver disabled opens the menu.',
    'Gesture activation and physical-device acceptance remain separate.'
  ]
};
const persist = () => writeFile(path.join(out,'result.json'),JSON.stringify(receipt,null,2)+'\n');
function logTool(command,args,log,timeout=600000) {
  const fd=openSync(path.join(out,log),'w');
  try { execFileSync(command,args,{stdio:['ignore',fd,fd],timeout}); }
  finally {closeSync(fd);}
}
try {
  await cp(path.join(source,'SystemUI.xcodeproj'),project,{recursive:true});
  const pbx=path.join(project,'project.pbxproj');
  await writeFile(pbx,(await readFile(pbx,'utf8')).replace('sourcecode.swift','sourcecode.c.objc')
    .replace('path = SystemUITests.swift','path = VoiceOverTests.m'));
  await writeFile(path.join(out,'VoiceOverTests.m'),sourceFile.replace('__APP_ID__',bundleId));
  receipt.device=sim('create','RaisingEngine VoiceOver '+path.basename(out),type.identifier,runtime.identifier);
  await persist();
  sim('boot',receipt.device);
  logTool('/usr/bin/xcrun',['simctl','bootstatus',receipt.device,'-b'],'boot.log',180000);
  logTool('/usr/bin/xcrun',['simctl','install',receipt.device,app],'install.log',120000);
  logTool('/usr/bin/xcodebuild',['test','-project',project,'-scheme','SystemUITests','-destination',
    'platform=iOS Simulator,id='+receipt.device,'-derivedDataPath',path.join(out,'DerivedData'),
    '-parallel-testing-enabled','NO','-resultBundlePath',receipt.resultBundle,'CODE_SIGNING_ALLOWED=NO'],'test.log');
  receipt.passed=true;
} catch(error) {
  receipt.error=String(error);
  throw error;
} finally {
  try {
    if(receipt.device) {
      try {sim('shutdown',receipt.device);} finally {sim('delete',receipt.device);}
      receipt.deletedDevice=true;
    }
  } finally {
    await persist();
    console.log('iOS VoiceOver evidence: '+out);
  }
}

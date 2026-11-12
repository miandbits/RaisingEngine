import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
import {readFileSync} from 'node:fs';
import {createHash,randomBytes} from 'node:crypto';
import {cp, mkdir, mkdtemp, readFile, rename, stat, writeFile} from 'node:fs/promises';
import path from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
import {loadProject} from '../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../dist/packages/project-service/snapshot.js';
import {analyzeInputIdleMemory} from './lib/ios-input-memory.mjs';
import {cleanUpIOSSimulator,runBoundedAppleTool,selectIOSSimulator,ownedIOSSimulatorDataPath,readIOSPortableStaging,validateIOSSandboxReceipt,waitForIOSSandboxReceipt} from './lib/ios-simulator-test.mjs';

const args=process.argv.slice(2), root=process.cwd();
const option=name=>{const i=args.indexOf(name);return i<0?undefined:args[i+1];};
if(process.platform!=='darwin'||!args[0]||!option('--project')) {
  throw Error('Usage: node scripts/test-ios-system-ui.mjs IOS_EXPORT --project PROJECT [--family iphone|ipad] [--test all|files|files-roundtrip|input|input-idle|selection|composition|japanese|composition-probe] [--text-scale 1|2] [--keep-failed-device]');
}
const selectedCases={
  files:['testFilesKeyboardBackgroundRotationCancel'],
  'files-roundtrip':['testFilesExportBackup','testFilesImportBackupAfterRelaunch'],
  input:['testOwnedTextInputRotation'],
  'input-idle':['testOwnedTextInputIdle'],
  selection:['testNativeSelectionAndFieldSwitching'],
  composition:['testKoreanCompositionRotation'],
  japanese:['testJapaneseMarkedComposition'],
  'composition-probe':['testKoreanMidSyllableRotation']
};
const selection=option('--test')??'all';
assert.ok(selection==='all'||Object.hasOwn(selectedCases,selection),'Unknown system UI test selection');
const cases=selection==='all'?[
  ...selectedCases.files,...selectedCases.input,...selectedCases.selection,...selectedCases.composition,...selectedCases.japanese
]:selectedCases[selection];
const textScale=option('--text-scale')??'1';assert.ok(['1','2'].includes(textScale),'Use --text-scale 1 or 2');
const family=option('--family')??'iphone';assert.ok(['iphone','ipad'].includes(family));
const keepFailed=args.includes('--keep-failed-device'), source=path.resolve(args[0]), project=path.resolve(option('--project'));
const originalApp=path.join(source,'build-iphonesimulator/native-player/Release-iphonesimulator/RaisingPlayer.app');
const hash=bytes=>createHash('sha256').update(bytes).digest('hex');
const sim=(...values)=>execFileSync('/usr/bin/xcrun',['simctl',...values],{encoding:'utf8',timeout:30000}).trim();
const originalGame=JSON.parse(await readFile(path.join(originalApp,'game/game.json'),'utf8'));
const loaded=await loadProject(project), original=await buildSnapshot(loaded);
assert.ok(original.ok,JSON.stringify(original.diagnostics));
assert.equal(original.snapshot.contentFingerprint,originalGame.content.contentFingerprint,'Project must match the supplied simulator app');
const doc=loaded.documents.find(d=>d.kind==='project'), authored=doc.data;
const title=authored.visual?.screens.find(s=>s.id==='title');assert.ok(title,'The probe requires an authored title screen');
assert.ok(!authored.visual.screens.some(s=>s.id==='system-records'),'Reserved probe screen already exists');
assert.ok(!(authored.userFields??[]).some(f=>['system-ui-name','system-ui-note'].includes(f.id)),'Reserved probe field already exists');
authored.gameId='ios-system-ui-proof';
authored.userFields=[...(authored.userFields??[]),{id:'system-ui-name',type:'text',label:'Probe name',initial:'',minLength:0,maxLength:40},{id:'system-ui-note',type:'text',label:'Probe note',initial:'',minLength:0,maxLength:40}];
authored.visual.readableLayout={maxShortEdge:600,fontSize:18,minTargetSize:44,gap:12,padding:16,maxWidth:760,background:'#172C29'};
authored.visual.saveSystem={manualSlots:1,autoSlots:0};
const text=(id,y,value)=>({id,kind:'text',x:30,y,width:1000,height:70,text:value,color:'#F5EFDF',fontSize:24});
const button=(id,y,label,action)=>({id,kind:'button',x:30,y,width:400,height:52,text:label,action,color:'#263C36',fill:'#F5EFDF',fontSize:20});
title.elements=[text('instructions',30,'Type a name, rotate, and continue typing.'),{id:'name-input',kind:'input',userFieldId:'system-ui-name',x:30,y:130,width:1000,height:70,color:'#263C36',fill:'#FFFDF6',fontSize:24},{id:'note-input',kind:'input',userFieldId:'system-ui-note',x:30,y:240,width:1000,height:70,color:'#263C36',fill:'#FFFDF6',fontSize:24},button('start',580,'Start','start'),{...button('load',640,'Files tools','openModal'),screenId:'system-records'}];
title.readableLayout={items:[{elementId:'instructions'},{elementId:'name-input'},{elementId:'note-input'}],actions:['start','load']};
authored.visual.screens.push({id:'system-records',label:'System UI probe',modal:true,elements:[{id:'slots',kind:'loadSlots',x:30,y:30,width:1000,height:540,color:'#263C36',fill:'#F5EFDF',fontSize:18},button('close',640,'Close','closeModal')],readableLayout:{items:[{elementId:'slots'}],actions:['close']}});
if(selection==='files-roundtrip')for(const screen of authored.visual.screens) {
  if(screen===title||screen.modal||!screen.readableLayout)continue;
  screen.elements.push({...button('system-ui-files',640,'Files tools','openModal'),screenId:'system-records'});
  screen.readableLayout={
    items:screen.readableLayout.items.filter(item=>item.planningGuidance!=='first-use'),
    actions:['system-ui-files']
  };
}
const fixture=await buildSnapshot(loaded);assert.ok(fixture.ok,JSON.stringify(fixture.diagnostics));
await mkdir(path.join(root,'.cache/ios-system-ui'),{recursive:true});
const out=await mkdtemp(path.join(root,'.cache/ios-system-ui/run-')),app=path.join(out,'SystemUI.app');
await cp(originalApp,app,{recursive:true});
const plist=JSON.parse(execFileSync('/usr/bin/plutil',['-convert','json','-o','-',path.join(app,'Info.plist')],{encoding:'utf8'}));
const executable=plist.CFBundleExecutable, nativeHash=hash(await readFile(path.join(app,executable)));
const appId='org.raisingengine.systemui.'+path.basename(out).replaceAll('-','').toLowerCase();
execFileSync('/usr/bin/plutil',['-replace','CFBundleIdentifier','-string',appId,path.join(app,'Info.plist')]);
await writeFile(path.join(app,'game/game.json'),JSON.stringify({...originalGame,content:fixture.snapshot}));
await writeFile(path.join(out,'fixture-project.json'),JSON.stringify(authored,null,2));
await writeFile(path.join(out,'fixture-content.json'),JSON.stringify(fixture.snapshot));
assert.equal(hash(await readFile(path.join(app,executable))),nativeHash,'Probe must not modify the native executable');
let device,exportedPID,firstLaunchTimestamp,passed=false;
const results=[],receipt={ok:false,actualSystemPicker:false,actualFilesRoundTrip:false,sourceExport:source,sourceProject:project,sourceContentFingerprint:original.snapshot.contentFingerprint,fixtureContentFingerprint:fixture.snapshot.contentFingerprint,appExecutableSha256:nativeHash,appId,family,selection,textScale,results,diagnosticsTransport:'simulator-stdout',limits:['Simulator only; no physical device or external provider acceptance.','Cache-only authored fixture with unchanged native executable; not the caller game release.','Selected focused regressions, not the complete malformed-save or provider matrix.','XCTest performs all interactions and reads diagnostics from per-launch player stdout.']};
const exportCheckpoint=path.join(out,'files-export-checkpoint.json');
const tool=(command,values,logName,timeout,env)=>runBoundedAppleTool(command,values,
  {cwd:root,logFile:path.join(out,logName),timeout,env});
try {
  const {runtimes}=JSON.parse(sim('list','runtimes','--json'));
  const selected=selectIOSSimulator(runtimes,{family,runtimeId:process.env.RAISING_IOS_SIMULATOR_RUNTIME,
    deviceType:process.env.RAISING_IOS_SIMULATOR_DEVICE_TYPE});
  receipt.device=selected;await writeFile(path.join(out,'result.json'),JSON.stringify(receipt,null,2));
  const harness=path.join(root,'tests/ios-system-ui'),copied=path.join(out,'harness');await cp(harness,copied,{recursive:true});
  const swift=path.join(copied,'SystemUITests.swift');
  const template=await readFile(swift,'utf8');receipt.harnessSourceSha256=hash(template);
  await writeFile(swift,template.replaceAll('__APP_ID__',appId));
  const buildArgs=['-project',path.join(copied,'SystemUI.xcodeproj'),'-scheme','SystemUITests','-derivedDataPath',path.join(out,'DerivedData'),'CODE_SIGNING_ALLOWED=NO'];
  await tool('/usr/bin/xcodebuild',['build-for-testing',...buildArgs,'-destination','generic/platform=iOS Simulator'],'build.log');
  const deviceName='RaisingEngine system UI '+path.basename(out);
  device=sim('create',deviceName,selected.type,selected.runtime);
  receipt.device={id:device,...selected};await writeFile(path.join(out,'result.json'),JSON.stringify(receipt,null,2));
  const prebootInventory=selection==='files-roundtrip'?sim('list','devices','--json'):undefined;
  const deviceDataPath=prebootInventory?ownedIOSSimulatorDataPath(JSON.parse(prebootInventory),device):undefined;
  let guiPrebootInventory;
  if(prebootInventory&&process.env.RAISING_IOS_GUI_ATTACHMENT_TOOL) {
    guiPrebootInventory=path.join(out,'simulator-gui-attachment-preboot-inventory.json');
    await writeFile(guiPrebootInventory,prebootInventory+'\n',{flag:'wx',mode:0o600});
  }
  await tool('/usr/bin/xcrun',['simctl','boot',device],'boot-start.log',60000);
  await tool('/usr/bin/xcrun',['simctl','bootstatus',device,'-b'],'boot.log',180000);
  await tool('/usr/bin/xcrun',['simctl','install',device,app],'install.log',120000);
  let filesSaveDirectory;
  const filesRoundTrip=selection==='files-roundtrip';
  const testDestination='platform=iOS Simulator,id='+device;
  for(const name of cases) {
    const currentLog=path.join(out,name+'.stdout');
    await writeFile(currentLog,'',{flag:'wx',mode:0o600});
    const sandboxNonce=filesRoundTrip?randomBytes(32).toString('hex'):undefined;
    const saveArgs=filesRoundTrip?['--sandbox-receipt',sandboxNonce]:[];
    const launchLog=name+'-launch.log';
    if(process.env.RAISING_IOS_CAPTURE_TOUCH_LOG==='1'&&firstLaunchTimestamp===undefined)
      firstLaunchTimestamp=Math.floor(Date.now()/1000);
    await tool('/usr/bin/xcrun',['simctl','launch','--stdout='+currentLog,
      '--stderr='+path.join(out,name+'.stderr'),device,appId,'--editor-events','--locale','en',
      '--reading-layout','on','--text-scale',textScale,...saveArgs],launchLog,90000);
    const launched=await readFile(path.join(out,launchLog),'utf8');
    const pidLine=launched.split('\n').find(line=>line.startsWith(appId+':'));
    const memory=[],pid=Number(pidLine?.slice(appId.length+1).trim());
    assert.ok(Number.isInteger(pid)&&pid>0,'Simulator launch must report its app PID');
    process.kill(pid,0);
    const {dev,ino}=await stat(currentLog,{bigint:true});
    if(filesRoundTrip) {
      const sandboxReceipt=await waitForIOSSandboxReceipt(currentLog,{alive:()=>process.kill(pid,0)});
      const receiptIdentity=await stat(currentLog,{bigint:true});
      assert.ok(receiptIdentity.dev===dev&&receiptIdentity.ino===ino,'Player stdout was replaced');
      const sandbox=await validateIOSSandboxReceipt(sandboxReceipt,
        {nonce:sandboxNonce,pid,contentFingerprint:fixture.snapshot.contentFingerprint,gameId:authored.gameId,deviceDataPath});
      if(filesSaveDirectory)assert.equal(sandbox.directory,filesSaveDirectory,'Relaunch must use the same app sandbox');
      filesSaveDirectory=sandbox.directory;
      receipt.sandbox={home:sandbox.home,saveFile:sandbox.saveFile};
      await writeFile(path.join(out,'result.json'),JSON.stringify(receipt,null,2));
    }
    if(name==='testFilesExportBackup'&&process.env.RAISING_IOS_GUI_ATTACHMENT_TOOL) {
      assert.ok(guiPrebootInventory,'GUI attachment requires the saved preboot ownership inventory');
      const simulatorApp=path.join(process.env.DEVELOPER_DIR,'Applications','Simulator.app');
      const guiReceipt=path.join(out,'simulator-gui-attachment.json');
      await tool(process.env.RAISING_IOS_GUI_ATTACHMENT_TOOL,
        [simulatorApp,device,deviceName,selected.runtime,appId,String(pid),guiReceipt,guiPrebootInventory],
        'simulator-gui-attachment.log',120000);
      const attachment=JSON.parse(await readFile(guiReceipt,'utf8'));
      assert.equal(attachment.deviceUDID,device,'GUI attachment must belong to the owned Simulator');
      assert.equal(attachment.playerPID,pid,'GUI attachment must retain the live player');
      assert.ok(attachment.interventionPerformed===true&&attachment.ready===true&&attachment.noInputPosted===true,
        'The GUI attachment intervention must complete without posting input');
      receipt.guiAttachment={file:path.basename(guiReceipt),device,playerPID:pid};
      await writeFile(path.join(out,'result.json'),JSON.stringify(receipt,null,2));
      process.kill(pid,0);
    }
    if(name==='testFilesImportBackupAfterRelaunch') {
      assert.ok(exportedPID,'The import requires a successful export process');
      assert.notEqual(pid,exportedPID,'The simulator must start a different player process to import the backup');
    }
    const diagnostics={session:appId,test:name,pid,stdout:currentLog,device:String(dev),inode:String(ino),exportedPID};
    const testEnvironment={...process.env,TEST_RUNNER_RAISING_SYSTEM_UI_DIAGNOSTICS:JSON.stringify(diagnostics)};
    const idleProbe=name==='testOwnedTextInputIdle';let memoryTimer;
    const resultBundle=path.join(out,name+'.xcresult');
    const selectedTests=['-only-testing:SystemUITests/SystemUITests/'+name];
    if(name==='testFilesExportBackup')selectedTests.push('-only-testing:SystemUITests/SystemUIDiagnosticsTests');
    const sample=()=>{
      try {
        const [rssKiB,cpuPercent]=execFileSync('/bin/ps',['-o','rss=,%cpu=','-p',String(pid)],{encoding:'utf8',timeout:2000}).trim().split(/\s+/).map(Number);
        const footprintFile=path.join(out,'input-footprint-latest.json');
        execFileSync('/usr/bin/footprint',['-p',String(pid),'--noCategories','-f','bytes','--json',footprintFile],{encoding:'utf8',timeout:10000});
        const report=JSON.parse(readFileSync(footprintFile,'utf8')),process=report.processes?.find(value=>value.pid===pid);
        assert.ok(process?.footprint>0,'The app physical footprint must be available');
        memory.push({at:Date.now(),rssKiB,cpuPercent,footprintKiB:process.footprint/1024});
      } catch(error) {memory.push({at:Date.now(),error:String(error)});}
    };
    if(idleProbe){sample();memoryTimer=setInterval(sample,15000);}
    try {
      await delay(1000);
      await tool('/usr/bin/xcodebuild',['test-without-building',...buildArgs,'-destination',testDestination,'-parallel-testing-enabled','NO','-resultBundlePath',resultBundle,...selectedTests],name+'.log',undefined,testEnvironment);
    } finally {
      clearInterval(memoryTimer);
      if(idleProbe){sample();await writeFile(path.join(out,'input-memory-samples.json'),JSON.stringify(memory,null,2));}
    }
    if(idleProbe) {
      const transcript=await readFile(path.join(out,name+'.log'),'utf8');
      const start=Number(/INPUT_IDLE_START ([0-9.]+)/.exec(transcript)?.[1])*1000,end=Number(/INPUT_IDLE_END ([0-9.]+)/.exec(transcript)?.[1])*1000;
      receipt.inputIdleMemory=analyzeInputIdleMemory(memory,start,end);
      await writeFile(path.join(out,'result.json'),JSON.stringify(receipt,null,2));
      assert.ok(receipt.inputIdleMemory.diagnosticPass,'Focused idle physical footprint exceeded 8 MiB overall or a median 1 MiB/minute growth rate; inspect input-memory-samples.json');
    }
    const result={name,ok:true,actualUI:true,pid,xcresult:resultBundle};
    if(filesSaveDirectory) {
      process.kill(pid,0);
      const stagingEntries=await readIOSPortableStaging(filesSaveDirectory);
      await writeFile(path.join(out,name+'-staging.json'),JSON.stringify({pid,stagingEntries},null,2));
      assert.deepEqual(stagingEntries,[],'Files must release owned staging files before restarting the player');
      result.portableStagingEntries=stagingEntries.length;
    }
    if(name==='testFilesExportBackup') {
      const transcript=await readFile(path.join(out,name+'.log'),'utf8');
      const faultTests=[...transcript.matchAll(/^Test Case '-\[(?:[\w.]+\.)?SystemUIDiagnosticsTests (test\w+)\]' (passed|failed|skipped) /gm)];
      assert.ok(faultTests.length,'Files XCTest must execute the diagnostics fault tests');
      assert.ok(faultTests.every(test=>test[2]==='passed'),'All diagnostics fault tests must pass');
      receipt.diagnosticsFaultTests=faultTests.map(test=>test[1]);
      const checkpoint=/FILES_EXPORT_STATE ([A-Za-z0-9+/=]+)/.exec(transcript);
      assert.ok(checkpoint,'The export test must supply its canonical state checkpoint');
      const state=JSON.parse(Buffer.from(checkpoint[1],'base64').toString('utf8'));
      assert.ok(state.state,'The export must contain started game state');
      const pending=exportCheckpoint+'.pending';
      await writeFile(pending,JSON.stringify({session:appId,pid,state}),{flag:'wx',mode:0o600});
      await rename(pending,exportCheckpoint);
      exportedPID=pid;
    }
    results.push(result);
    receipt.actualSystemPicker=results.some(r=>r.name==='testFilesKeyboardBackgroundRotationCancel'||r.name==='testFilesExportBackup');
    receipt.actualFilesRoundTrip=selectedCases['files-roundtrip'].every(name=>results.some(r=>r.name===name));
    await writeFile(path.join(out,'result.json'),JSON.stringify(receipt,null,2));
    sim('terminate',device,appId);
  }
  passed=true;receipt.ok=true;
} catch(error) {receipt.error=String(error);throw error;}
finally {
  if(device&&firstLaunchTimestamp!==undefined) {
    const predicate='(process == "backboardd" AND category == "TouchEvents") OR '+
      '(process == "testmanagerd" AND (subsystem == "com.apple.dt.xctest" OR subsystem == "com.apple.recap")) OR '+
      '(process == "RaisingPlayer" AND subsystem == "com.apple.UIKit" AND category == "EventDispatch")';
    try {
      await tool('/usr/bin/xcrun',['simctl','spawn',device,'log','show','--start','@'+firstLaunchTimestamp,
        '--timezone','UTC','--style','compact','--info','--debug','--predicate',predicate],'simulator-touch.log',30000);
      receipt.touchLog={captured:true,file:'simulator-touch.log',firstLaunchTimestamp};
    } catch(error) {
      receipt.touchLog={captured:false,error:String(error),firstLaunchTimestamp};
    }
  }
  receipt.keptDevice=!!device&&!passed&&keepFailed;
  if(device&&!receipt.keptDevice) {
    receipt.cleanupErrors=await cleanUpIOSSimulator(device,(action,id)=>
      tool('/usr/bin/xcrun',['simctl',action,id],action+'.log',60000));
    receipt.deletedDevice=!receipt.cleanupErrors.delete;
    if(!receipt.deletedDevice){passed=false;receipt.ok=false;}
  }
  receipt.finishedAt=new Date().toISOString();
  await writeFile(path.join(out,'result.json'),JSON.stringify(receipt,null,2));
  await writeFile(path.join(root,'.cache/ios-system-ui/latest.json'),JSON.stringify({ok:passed,out},null,2));
  console.log(`iOS system UI evidence: ${out}`);
  if(!receipt.error&&!passed)throw Error(`Simulator cleanup failed; inspect ${out}`);
}

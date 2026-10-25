import assert from 'node:assert/strict';
import {spawn, execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {createServer} from 'node:http';
import {cp, mkdir, mkdtemp, open, readFile, writeFile} from 'node:fs/promises';
import path from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
import {loadProject} from '../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../dist/packages/project-service/snapshot.js';

const args=process.argv.slice(2), root=process.cwd();
const option=name=>{const i=args.indexOf(name);return i<0?undefined:args[i+1];};
if(process.platform!=='darwin'||!args[0]||!option('--project')) {
  throw Error('Usage: node scripts/test-ios-system-ui.mjs IOS_EXPORT --project PROJECT [--family iphone|ipad] [--keep-failed-device]');
}
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
assert.ok(!(authored.userFields??[]).some(f=>f.id==='system-ui-name'),'Reserved probe field already exists');
authored.gameId='ios-system-ui-proof';
authored.userFields=[...(authored.userFields??[]),{id:'system-ui-name',type:'text',label:'Probe name',initial:'',minLength:0,maxLength:40}];
authored.visual.readableLayout={maxShortEdge:600,fontSize:18,minTargetSize:44,gap:12,padding:16,maxWidth:760,background:'#172C29'};
authored.visual.saveSystem={manualSlots:1,autoSlots:0};
const text=(id,y,value)=>({id,kind:'text',x:30,y,width:1000,height:70,text:value,color:'#F5EFDF',fontSize:24});
const button=(id,y,label,action)=>({id,kind:'button',x:30,y,width:400,height:52,text:label,action,color:'#263C36',fill:'#F5EFDF',fontSize:20});
title.elements=[text('instructions',30,'Type a name, rotate, and continue typing.'),{id:'name-input',kind:'input',userFieldId:'system-ui-name',x:30,y:130,width:1000,height:70,color:'#263C36',fill:'#FFFDF6',fontSize:24},button('start',580,'Start','start'),{...button('load',640,'Files tools','openModal'),screenId:'system-records'}];
title.readableLayout={items:[{elementId:'instructions'},{elementId:'name-input'}],actions:['start','load']};
authored.visual.screens.push({id:'system-records',label:'System UI probe',modal:true,elements:[{id:'slots',kind:'loadSlots',x:30,y:30,width:1000,height:540,color:'#263C36',fill:'#F5EFDF',fontSize:18},button('close',640,'Close','closeModal')],readableLayout:{items:[{elementId:'slots'}],actions:['close']}});
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
let currentLog,device,passed=false;
const results=[],receipt={ok:false,actualSystemPicker:false,sourceExport:source,sourceProject:project,sourceContentFingerprint:original.snapshot.contentFingerprint,fixtureContentFingerprint:fixture.snapshot.contentFingerprint,appExecutableSha256:nativeHash,appId,family,results,limits:['Simulator only; no physical device or external provider acceptance.','Cache-only authored fixture with unchanged native executable; not the caller game release.','Two focused regressions; not the complete import/export or malformed-save matrix.','All interactions use XCTest input; HTTP only exposes read-only editor-view diagnostics.']};
const server=createServer(async(req,res)=>{
  try {
    assert.equal(req.method,'GET');assert.equal(req.url,'/state');
    const lines=(await readFile(currentLog,'utf8')).split('\n');let view;
    for(let i=lines.length-1;i>=0;i--) {try{const row=JSON.parse(lines[i]);if(row.event==='editor-view'){view=row.view;break;}}catch{}}
    assert.ok(view,'No player diagnostics yet');res.setHeader('Content-Type','application/json');res.end(JSON.stringify(view));
  } catch(error) {res.statusCode=503;res.end(JSON.stringify({error:String(error)}));}
});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
async function tool(command,values,logName,timeout=600000) {
  const logfile=path.join(out,logName),fd=await open(logfile,'w');let timer,forceTimer;
  try {
    const child=spawn(command,values,{cwd:root,stdio:['ignore',fd.fd,fd.fd]});
    let expired=false;timer=setTimeout(()=>{expired=true;child.kill('SIGTERM');forceTimer=setTimeout(()=>child.kill('SIGKILL'),10000);},timeout);
    const result=await new Promise((resolve,reject)=>{child.once('error',reject);child.once('close',(status,signal)=>resolve({status,signal}));});
    if(result.status!==0||expired)throw Error(`${logName}: ${expired?'tool timeout':`exit ${result.status}, signal ${result.signal??'none'}`}; inspect ${logfile}`);
  } finally {clearTimeout(timer);clearTimeout(forceTimer);await fd.close();}
}
try {
  const available=JSON.parse(sim('list','devices','available','--json')).devices;
  const candidates=Object.entries(available).filter(([runtime])=>runtime.includes('.iOS-')).flatMap(([runtime,devices])=>devices.filter(d=>d.name.toLowerCase().includes(family)).map(d=>({runtime,type:d.deviceTypeIdentifier})));
  assert.ok(candidates.length,'Install a matching iOS simulator runtime in Xcode first');
  const selected=candidates.at(-1);device=sim('create','RaisingEngine system UI '+path.basename(out),selected.type,selected.runtime);
  receipt.device={id:device,...selected};await writeFile(path.join(out,'result.json'),JSON.stringify(receipt,null,2));
  sim('boot',device);await tool('/usr/bin/xcrun',['simctl','bootstatus',device,'-b'],'boot.log');sim('install',device,app);
  const harness=path.join(root,'tests/ios-system-ui'),copied=path.join(out,'harness');await cp(harness,copied,{recursive:true});
  const swift=path.join(copied,'SystemUITests.swift');
  const template=await readFile(swift,'utf8');receipt.harnessSourceSha256=hash(template);
  await writeFile(swift,template.replaceAll('__APP_ID__',appId).replaceAll('__READ_ONLY_URL__',`http://127.0.0.1:${server.address().port}/state`));
  const buildArgs=['-project',path.join(copied,'SystemUI.xcodeproj'),'-scheme','SystemUITests','-destination','platform=iOS Simulator,id='+device,'-derivedDataPath',path.join(out,'DerivedData'),'CODE_SIGNING_ALLOWED=NO'];
  await tool('/usr/bin/xcodebuild',['build-for-testing',...buildArgs],'build.log');
  for(const name of ['testFilesKeyboardBackgroundRotationCancel','testOwnedTextInputRotation']) {
    currentLog=path.join(out,name+'.stdout');
    sim('launch','--stdout='+currentLog,'--stderr='+path.join(out,name+'.stderr'),device,appId,'--editor-events','--locale','en','--reading-layout','on','--text-scale','1');
    await delay(1000);
    await tool('/usr/bin/xcodebuild',['test-without-building',...buildArgs,'-parallel-testing-enabled','NO','-resultBundlePath',path.join(out,name+'.xcresult'),'-only-testing:SystemUITests/SystemUITests/'+name],name+'.log');
    results.push({name,ok:true,actualUI:true,xcresult:path.join(out,name+'.xcresult')});sim('terminate',device,appId);
  }
  passed=true;receipt.ok=true;receipt.actualSystemPicker=true;
} catch(error) {receipt.error=String(error);throw error;}
finally {
  receipt.finishedAt=new Date().toISOString();receipt.keptDevice=!!device&&!passed&&keepFailed;
  await writeFile(path.join(out,'result.json'),JSON.stringify(receipt,null,2));
  await writeFile(path.join(root,'.cache/ios-system-ui/latest.json'),JSON.stringify({ok:passed,out},null,2));
  server.closeAllConnections();await new Promise(resolve=>server.close(resolve));
  if(device&&!receipt.keptDevice) {try{sim('shutdown',device);}finally{sim('delete',device);}}
  console.log(`iOS system UI evidence: ${out}`);
}

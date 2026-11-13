import {execFileSync} from 'node:child_process';
import {mkdir,readFile,writeFile,copyFile,mkdtemp} from 'node:fs/promises';
import {setTimeout as delay} from 'node:timers/promises';
import path from 'node:path';
import assert from 'node:assert/strict';
import {runAppleTool} from './lib/ios-project.mjs';

// Run against any built iOS export with default music and visible Start/Sound
// actions. This exercises the real device backend, not a silent/mock engine.
if(process.platform!=='darwin'||!process.argv[2])throw Error('Usage: node scripts/test-ios-audio.mjs PATH_TO_BUILT_IOS_EXPORT');
const project=path.resolve(process.argv[2]),app=path.join(project,'build-iphonesimulator/native-player/Release-iphonesimulator/RaisingPlayer.app');
const metadata=JSON.parse(await readFile(path.join(project,'ios-build.json'),'utf8'));
const content=JSON.parse(await readFile(path.join(app,'game/game.json'),'utf8')).content;
const visual=content.project.visual;
assert.ok(visual.defaultScene?.musicAssetId,'The audio regression requires default scene music.');
const start=visual.screens.flatMap(s=>s.elements).find(e=>e.kind==='button'&&e.action==='start');
const sound=visual.screens.flatMap(s=>s.elements).find(e=>e.kind==='button'&&e.action==='sound');
assert.ok(start&&sound,'The audio regression requires Start and Sound buttons.');
const sim=(...args)=>execFileSync('/usr/bin/xcrun',['simctl',...args],{encoding:'utf8',timeout:30000}).trim();
const available=JSON.parse(sim('list','devices','available','--json')).devices;let selected;
for(const [runtime,devices] of Object.entries(available))if(runtime.includes('.iOS-'))for(const device of devices)if(device.name.includes('iPhone')&&(!selected||device.name.includes('Pro')))selected={runtime,type:device.deviceTypeIdentifier};
assert.ok(selected,'Install an iPhone simulator runtime in Xcode first.');
await mkdir('.cache/ios-audio-proof',{recursive:true});const out=await mkdtemp(path.resolve('.cache/ios-audio-proof/run-'));
const device=sim('create','RaisingEngine isolated audio test',selected.type,selected.runtime);
await writeFile(path.join(out,'device.json'),JSON.stringify({device,...selected}));
try{
 sim('boot',device);await runAppleTool('/usr/bin/xcrun',['simctl','bootstatus',device,'-b'],out,{logFile:path.join(out,'boot.log')});sim('install',device,app);
 const data=sim('get_app_container',device,metadata.applicationId,'data'),dir=path.join(data,'Documents','audio-proof');await mkdir(dir,{recursive:true});
 const script=path.join(dir,'steps.json'),report=path.join(dir,'report.json');
 await writeFile(script,JSON.stringify([
  {touchWindow:start.id},{waitFrames:15},{checkpoint:'playing'},
  {waitFrames:120},{checkpoint:'advanced'},
  {touchWindow:sound.id},{checkpoint:'muted'},
  {touchWindow:sound.id},{checkpoint:'unmuted'},
  {lifecycle:['background','foreground']},{waitFrames:120},{checkpoint:'resumed'},
  {screenshot:path.join(dir,'resumed.png')}
 ]));
 sim('launch',device,metadata.applicationId,'--automation',script,'--report',report,'--save',path.join(dir,'save.json'));
 let proof;const started=Date.now(),deadline=started+45000;
 while(Date.now()<deadline){try{proof=JSON.parse(await readFile(report,'utf8'));break;}catch(e){if(e.code!=='ENOENT'&&!(e instanceof SyntaxError))throw e;}await delay(250);}
 assert.ok(proof,'iOS audio did not produce a report within 45 seconds; inspect its crash report and simulator audio logs.');
 await copyFile(report,path.join(out,'report.json'));assert.equal(proof.ok,true,JSON.stringify(proof.error));
 const at=name=>proof.steps.find(s=>s.command.checkpoint===name).ui;
 for(const name of ['playing','advanced','muted','unmuted','resumed'])assert.ok(at(name).audio.initialized&&at(name).audio.musicPlaying&&at(name).audio.loadedSounds>=1,name+' music backend is live');
 assert.notEqual(at('advanced').audio.musicCursor,at('playing').audio.musicCursor,'The audio device must consume decoded music frames (allowing loop wrap).');
 assert.equal(at('muted').audio.muted,true);assert.equal(at('unmuted').audio.muted,false);
 assert.equal(at('resumed').backgrounded,false);
 assert.equal(at('resumed').lifecycleSaves,at('unmuted').lifecycleSaves+1,'The background transition must save once before resuming.');
 assert.notEqual(at('resumed').audio.musicCursor,at('unmuted').audio.musicCursor,'Music must continue after returning to the foreground (allowing loop wrap).');
 await copyFile(path.join(dir,'resumed.png'),path.join(out,'resumed.png'));
 const result={ok:true,out,project,contentFingerprint:metadata.contentFingerprint,...selected,elapsedMs:Date.now()-started,audio:Object.fromEntries(['playing','advanced','muted','unmuted','resumed'].map(name=>[name,at(name).audio]))};
 await writeFile(path.join(out,'result.json'),JSON.stringify(result,null,2));await writeFile('.cache/ios-audio-proof/latest.json',JSON.stringify(result,null,2));console.log(JSON.stringify(result));
}finally{
 // A crash may already have stopped our app. Cleanup must retain the original
 // test failure instead of replacing it with simctl's "not running" error.
 try{sim('terminate',device,metadata.applicationId);}catch{}
 try{sim('shutdown',device);}finally{sim('delete',device);}
}

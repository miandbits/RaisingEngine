import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {mkdir,readFile,writeFile,copyFile,mkdtemp} from 'node:fs/promises';
import {setTimeout as delay} from 'node:timers/promises';
import path from 'node:path';
import assert from 'node:assert/strict';
import {firstGameRoute} from '../tests/qt-studio/first-game-route.mjs';
import {runAppleTool} from './lib/ios-project.mjs';

if(process.platform!=='darwin'||!process.argv[2])throw Error('Usage: node scripts/test-ios-game.mjs PATH_TO_IOS_EXPORT (built simulator starter)');
const project=path.resolve(process.argv[2]),app=path.join(project,'build-iphonesimulator/native-player/Release-iphonesimulator/RaisingPlayer.app');
const metadata=JSON.parse(await readFile(path.join(project,'ios-build.json'),'utf8'));
const content=JSON.parse(await readFile(path.join(app,'game/game.json'),'utf8')).content;
const sim=(...args)=>execFileSync('/usr/bin/xcrun',['simctl',...args],{encoding:'utf8',timeout:30000}).trim();
const available=JSON.parse(sim('list','devices','available','--json')).devices;
let selected;
for(const [runtime,devices] of Object.entries(available))if(runtime.includes('.iOS-'))for(const device of devices)if(device.name.includes('iPhone')&&(!selected||device.name.includes('Pro')))selected={runtime,type:device.deviceTypeIdentifier};
if(!selected)throw Error('Install an iPhone simulator runtime in Xcode first.');
await mkdir('.cache/first-game-ios-proof',{recursive:true});const out=await mkdtemp(path.resolve('.cache/first-game-ios-proof/run-'));
const device=sim('create','RaisingEngine isolated game test',selected.type,selected.runtime);
const hash=bytes=>createHash('sha256').update(bytes).digest('hex');
const results=[],receipt={ok:false,project,...selected,device,appExecutableSha256:hash(await readFile(path.join(app,'RaisingPlayer'))),contentFingerprint:content.contentFingerprint,textScale:2,readingLayout:'on',harnessSha256:hash(await readFile(new URL(import.meta.url))),results,limits:['Isolated iOS Simulator with native automated touch input; not physical-device or human usability acceptance.','Local authored Save/Load controls; actual Files-picker coverage belongs to the separate system-UI tests.']};
await writeFile(path.join(out,'device.json'),JSON.stringify({device,...selected}));
try {
  sim('boot',device);
  await runAppleTool('/usr/bin/xcrun',['simctl','bootstatus',device,'-b'],out,{logFile:path.join(out,'boot.log')});
  sim('install',device,app);
  const data=sim('get_app_container',device,metadata.applicationId,'data');
  // UIKit retains its process after SDL_main returns. Wait for the completed
  // report, then terminate our own app instead of waiting on --console forever.
  const launch=async(script,report,save)=>{
    sim('launch',device,metadata.applicationId,'--automation',script,'--report',report,'--save',save,'--locale','en','--reading-layout','on','--text-scale','2');
    try {
      const deadline=Date.now()+90000;
      while(Date.now()<deadline) {
        try {return JSON.parse(await readFile(report,'utf8'));}
        catch(error) {if(error.code!=='ENOENT'&&!(error instanceof SyntaxError))throw error;}
        await delay(250);
      }
      throw Error('Timed out waiting for iOS automation report: '+report);
    } finally {sim('terminate',device,metadata.applicationId);}
  };
  for(const accept of [true,false]) {
    const route=firstGameRoute(content,accept),dir=path.join(data,'Documents',route.id);await mkdir(dir,{recursive:true});
    const touch=id=>({touchWindow:id}),steps=[];let finished=false;
    for(const step of route.steps){
      if(step.checkpoint==='ending')finished=true;
      if(step.select)steps.push(touch('board-slot-'+step.select.slice(5)),touch('board-activity-'+step.value),touch('board-apply'));
      else if(step.key==='F5'){if(!finished)steps.push(touch('engine-reading-menu'));steps.push(touch('save-controls__save'));}
      else if(step.key==='F9')steps.push(touch('engine-reading-menu'),touch('save-controls__load'));
      else if(step.click){
        steps.push(touch(step.click));
        // Each route has a separate save directory and player-preference profile.
        // Traverse its first-use guide through the same controls as a player.
        if(step.click==='start')steps.push({checkpoint:'guide-open',screenshot:path.join(dir,'guide.png')},touch('engine-planning-guide-next'),touch('engine-planning-guide-next'),touch('engine-planning-guide-done'),{checkpoint:'guide-closed'});
      }else steps.push(step);
    }
    const script=path.join(dir,'route.json'),report=path.join(dir,'report.json'),save=path.join(dir,'save.json');
    await writeFile(script,JSON.stringify([...steps,{screenshot:path.join(dir,'ending.png')} ]));
    await copyFile(script,path.join(out,route.id+'-input.json'));
    const proof=await launch(script,report,save);
    // Keep the native diagnostic even if a control or checkpoint assertion fails.
    await copyFile(report,path.join(out,route.id+'-report.json'));
    assert.equal(proof.ok,true,JSON.stringify(proof.error));
    const at=label=>proof.steps.find(s=>s.command.checkpoint===label)?.ui;
    {assert.equal(at('guide-open').readingLayout.guidanceOpen,true);assert.equal(at('guide-closed').readingLayout.guidanceOpen,false);assert.deepEqual(at('guide-closed').state,at('guide-open').state);assert.deepEqual(at('guide-closed').commands,at('guide-open').commands);await copyFile(path.join(dir,'guide.png'),path.join(out,route.id+'-guide.png'));}
    assert.equal(at('start').readingLayout.active,true);assert.equal(at('start').readingLayout.textScale,2);assert.equal(at('start').readingLayout.guidanceAcknowledged,true);
    for(const [label,state] of Object.entries(route.expected))assert.deepEqual(proof.steps.find(s=>s.command.checkpoint===label)?.ui.state,state,route.id+'/'+label);
    await copyFile(path.join(dir,'ending.png'),path.join(out,route.id+'.png'));await copyFile(save,path.join(out,route.id+'-save.json'));
    await writeFile(script,JSON.stringify([touch('engine-reading-menu'),touch('load'),{checkpoint:'restored',screenshot:path.join(dir,'restored.png')}]));
    const restored=await launch(script,path.join(dir,'restored-report.json'),save);await copyFile(path.join(dir,'restored-report.json'),path.join(out,route.id+'-restored-report.json'));assert.equal(restored.ok,true,JSON.stringify(restored.error));
    const restoredUI=restored.steps.find(s=>s.command.checkpoint==='restored').ui;assert.deepEqual(restoredUI.state,route.expected.ending);assert.deepEqual(restoredUI.commands,at('ending').commands);await copyFile(path.join(dir,'restored.png'),path.join(out,route.id+'-restored.png'));
    results.push({route:route.id,ending:route.ending,checkpoints:Object.keys(route.expected).length,touch:true,compactPlanning:true,textScale:2,firstUseGuideChecked:true,freshSaveRestore:true,exactRestoredCommands:restoredUI.commands.length});
  }
  receipt.ok=true;await writeFile(path.join(out,'result.json'),JSON.stringify(receipt,null,2));
  await writeFile('.cache/first-game-ios-proof/latest.json',JSON.stringify({out,ok:true},null,2));console.log('iOS simulator proof: '+out);
} catch(error){receipt.error=String(error);throw error;} finally {
  receipt.finishedAt=new Date().toISOString();await writeFile(path.join(out,'result.json'),JSON.stringify(receipt,null,2));
  try{sim('shutdown',device);}finally{sim('delete',device);}
}

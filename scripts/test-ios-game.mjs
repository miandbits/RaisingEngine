import {execFileSync} from 'node:child_process';
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
await writeFile(path.join(out,'device.json'),JSON.stringify({device,...selected}));
try {
  sim('boot',device);
  await runAppleTool('/usr/bin/xcrun',['simctl','bootstatus',device,'-b'],out,{logFile:path.join(out,'boot.log')});
  sim('install',device,app);
  const data=sim('get_app_container',device,metadata.applicationId,'data');
  // UIKit retains its process after SDL_main returns. Wait for the completed
  // report, then terminate our own app instead of waiting on --console forever.
  const launch=async(script,report,save)=>{
    sim('launch',device,metadata.applicationId,'--automation',script,'--report',report,'--save',save);
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
  const results=[];
  for(const accept of [true,false]) {
    const route=firstGameRoute(content,accept),dir=path.join(data,'Documents',route.id);await mkdir(dir,{recursive:true});
    const steps=route.steps.map(s=>s.click?{touchWindow:s.click}:s.key==='F5'?{touchWindow:'save-controls__save'}:s.key==='F9'?{touchWindow:'save-controls__load'}:s);
    const script=path.join(dir,'route.json'),report=path.join(dir,'report.json'),save=path.join(dir,'save.json');
    await writeFile(script,JSON.stringify([...steps,{screenshot:path.join(dir,'ending.png')} ]));
    const proof=await launch(script,report,save);assert.equal(proof.ok,true,JSON.stringify(proof.error));
    for(const [label,state] of Object.entries(route.expected))assert.deepEqual(proof.steps.find(s=>s.command.checkpoint===label)?.ui.state,state,route.id+'/'+label);
    await copyFile(report,path.join(out,route.id+'-report.json'));await copyFile(path.join(dir,'ending.png'),path.join(out,route.id+'.png'));
    await writeFile(script,JSON.stringify([{key:'F9'},{checkpoint:'restored'}]));
    const restored=await launch(script,path.join(dir,'restored-report.json'),save);assert.deepEqual(restored.steps.find(s=>s.command.checkpoint==='restored').ui.state,route.expected.ending);
    results.push({route:route.id,ending:route.ending,checkpoints:Object.keys(route.expected).length,touch:true,freshSaveRestore:true});
  }
  await writeFile(path.join(out,'result.json'),JSON.stringify({ok:true,project,...selected,results},null,2));
  await writeFile('.cache/first-game-ios-proof/latest.json',JSON.stringify({out,ok:true},null,2));console.log('iOS simulator proof: '+out);
} finally {
  try{sim('shutdown',device);}finally{sim('delete',device);}
}

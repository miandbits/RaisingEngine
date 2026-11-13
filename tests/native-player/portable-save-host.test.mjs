import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile,rm} from 'node:fs/promises';
import {writeFileSync} from 'node:fs';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';
import {decodePng,encodeRgbaPng} from '../../dist/packages/studio-service/png.js';
const run=(command,args,options={})=>{
 const {evidenceFile,...spawnOptions}=options,started=Date.now();
 const result=spawnSync(command,args,{encoding:'utf8',timeout:240000,windowsHide:true,maxBuffer:8*1024*1024,...spawnOptions});
 const diagnostic={command,args,status:result.status,signal:result.signal,error:result.error?{code:result.error.code,message:result.error.message}:null,elapsedMs:Date.now()-started,stdout:result.stdout,stderr:result.stderr};
 if(evidenceFile)writeFileSync(evidenceFile,JSON.stringify(diagnostic,null,2)+'\n');
 assert.equal(result.status,0,JSON.stringify({status:result.status,signal:result.signal,error:diagnostic.error,elapsedMs:diagnostic.elapsedMs})+'\n'+(result.stderr??'')+(result.stdout??''));return result;
};

test('portable backup host uses isolated review, strict acceptance and real queue/lifecycle/touch guards',{timeout:360000},async()=>{
 await mkdir('.cache/native-portable-host',{recursive:true});const out=await mkdtemp(resolve('.cache/native-portable-host/run-')),project=join(out,'project');await cp(resolve('examples/my-first-raising-game'),project,{recursive:true});
 await rm(join(project,'events'),{recursive:true,force:true});await mkdir(join(project,'events'));await rm(join(project,'scenarios'),{recursive:true,force:true});
 const file=join(project,'project.json'),p=JSON.parse(await readFile(file,'utf8'));p.periodsPerMonth=3;p.durationMonths=3;p.schedulePlayback="continuous";if(!p.localization.locales.some(l=>l.id==="ja"))p.localization.locales.push({id:"ja",label:"日本語"});
 const element=(id,kind,extra={})=>({id,kind,x:24,y:40,width:1200,height:530,...extra}),button=(id,action)=>element(id,'button',{text:id,action,autoLabel:false,y:630,width:210,height:48});
 const layout=(items,actions)=>({items:items.map(x=>typeof x==='string'?{elementId:x}:x),actions,actionLayout:'grouped',primaryAction:actions[0]});
 p.visual={width:1280,height:720,readableLayout:{background:'#f5efdf'},readingHistory:{},saveSystem:{manualSlots:3,autoSlots:2},screens:[
  {id:'title',label:'Title',elements:[button('start','start'),button('load','load')],readableLayout:layout([],['start','load'])},
  {id:'schedule',label:'Plan',elements:[element('intro','text',{text:'Keep a careful plan. The backup copies progress and does not use the selected record.'}),element('planner','schedule',{scheduleLayout:'board'}),element('base-slots','saveSlots'),button('advance','advance'),button('save','save'),button('load','load')],readableLayout:layout(['intro',{elementId:'planner',schedulePresentation:'compact'}],['advance','save','load'])},
  {id:'story',label:'Story',elements:[element('story','dialogue'),element('choices','choices'),button('next','advance')],readableLayout:layout(['story','choices'],['next'])},
  {id:'ending',label:'Ending',elements:[element('ending-text','text',{binding:'ending'}),button('start','start'),button('save','save'),button('load','load')],readableLayout:layout(['ending-text'],['start','save','load'])},
  ...[false,true].map(load=>({id:load?'load-records':'records',label:'Records',modal:true,elements:[element('slots',load?'loadSlots':'saveSlots'),button('close','closeModal')],readableLayout:layout(['slots'],['close'])}))
 ]};
 p.gameplay={startLocationId:'home',locations:[{id:'home',name:'Quiet garden',connections:[]}],items:[],shops:[],interactions:[],actions:[{id:'unlock-memory',name:'Remember',requirements:{op:'always'},costs:[],effects:[{op:'setFlag',flagId:'garden-visited',value:true}],confirmation:{enabled:false}}]};
 p.gallery={title:'Memories',entries:[{id:'memory',kind:'cg',sourceId:'fixture-cg',title:'Garden',category:'Memories',hint:'Remember the garden.',unlockCondition:{op:'flag',flagId:'garden-visited',equals:true}}]};
 await writeFile(join(project,'assets/fixture.png'),encodeRgbaPng({width:2,height:2,pixels:Buffer.from(Array(4).fill([30,160,100,255]).flat())}));await writeFile(join(project,'assets/manifest.json'),JSON.stringify({formatVersion:'0.1.0',assets:[{id:'fixture-cg',kind:'image',path:'assets/fixture.png'}]}));await writeFile(file,JSON.stringify(p));
 const built=await buildSnapshot(await loadProject(project));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
 const target=desktopTarget(),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??'dist/native-player-app'),destination=join(out,'game');await exportGame({projectRoot:project,content:built.snapshot,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
 // The real shipping desktop executable does not expose a fake platform picker.
 const script=join(out,'shipping.json'),shippingReport=join(out,'shipping-report.json');await writeFile(script,JSON.stringify([{resize:{width:390,height:844}},{waitFrames:3},{key:'F9'},{waitFrames:3},{checkpoint:'records'}]));
 run(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',join(out,'shipping/save.json'),'--locale','en','--hidden','--automation',script,'--report',shippingReport,'--report-checkpoints-only']);const shipping=JSON.parse(await readFile(shippingReport));assert.equal(shipping.final.portableSave.available,false);assert.ok(!shipping.final.readingLayout.controls.some(c=>c.id.endsWith('-portable-import')));
 const cmake=process.env.CMAKE??'cmake',build=resolve(process.env.NATIVE_PLAYER_TEST_BUILD??'dist/native-player');
 const configured=run(cmake,['-S','native/player','-B',build,'-DRAISING_PORTABLE_SAVE_TESTING=ON']);await writeFile(join(out,'configure.log'),configured.stdout+configured.stderr);
 const compiled=run(cmake,['--build',build,'--target','raising-portable-host-test','--parallel',process.env.BUILD_JOBS??'3']);await writeFile(join(out,'build.log'),compiled.stdout+compiled.stderr);
 const exe=join(build,process.platform==='win32'?'raising-portable-host-test.exe':'raising-portable-host-test');for(const locale of ['en','ko','ja']){const folder=locale==='en'?'host':'host-'+locale;const result=run(exe,[gameDirectory(target,destination),join(out,folder),resolve('native/player/assets'),locale],{timeout:120000,evidenceFile:join(out,folder+'-process.json')});await writeFile(join(out,folder+'-process.log'),result.stdout+result.stderr);const local=JSON.parse(await readFile(join(out,folder,'host-report.json')));assert.equal(local.ok,true);assert.ok(local.checks.length>=16);} const report=JSON.parse(await readFile(join(out,'host/host-report.json')));assert.equal(report.ok,true);assert.ok(report.checks.length>=16);
 for(const width of [320,390,844]){const png=decodePng(await readFile(join(out,`host/review-${width}.png`)));let light=0;for(let i=0;i<png.pixels.length;i+=4)if(png.pixels[i]>150&&png.pixels[i+1]>150&&png.pixels[i+2]>150)light++;assert.ok(light>png.width*png.height*.3,`Review ${width} screenshot must contain the rendered reading surface`);}
 // A new, uninjected shipping player loads the resulting ordinary canonical save.
 const restoredPath=join(out,'fresh/save.json');await mkdir(join(out,'fresh'));await cp(join(out,'host/accepted.json'),restoredPath);const freshScript=join(out,'fresh.json'),freshReport=join(out,'fresh-report.json');await writeFile(freshScript,JSON.stringify([{resize:{width:390,height:844}},{waitFrames:3},{key:'F9'},{waitFrames:3},{scrollIntoView:'load-records__slots-request',touchWindow:'load-records__slots-request'},{waitFrames:3},{scrollIntoView:'load-records__slots-confirm',touchWindow:'load-records__slots-confirm'},{waitFrames:3},{checkpoint:'restored'}]));run(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',restoredPath,'--locale','en','--hidden','--automation',freshScript,'--report',freshReport,'--report-checkpoints-only']);const fresh=JSON.parse(await readFile(freshReport)),accepted=JSON.parse(await readFile(join(out,'host/accepted.json')));assert.deepEqual(fresh.final.state,accepted.state);assert.deepEqual(fresh.final.commands,accepted.commands);assert.equal(fresh.final.readingHistory.snapshot.reason,'restored');assert.deepEqual(await readFile(restoredPath),await readFile(join(out,'host/accepted.json')));
 await writeFile(resolve('.cache/native-portable-host/latest.json'),JSON.stringify({ok:true,out,checks:report.checks.length,platformClaim:'Injected desktop host only; genuine UIKit Files acceptance is separate.'},null,2));console.log('Portable host evidence: '+out);
});

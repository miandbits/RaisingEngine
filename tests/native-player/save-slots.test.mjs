import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,readFile,writeFile,readdir,mkdir,copyFile} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('manual slots, checkpoint ring, thumbnails, confirm/cancel and exact restoration',{timeout:180000},async()=>{
 const source=resolve('tests/fixtures/projects/last-grandson-polished');
 const result=await buildSnapshot(await loadProject(source));assert.equal(result.ok,true);
 const content=structuredClone(result.snapshot),root=await mkdtemp(resolve('.cache/save-slots-'));
 const runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??'dist/native-player-slots');
 const e=(id,kind,x,y,width,height,extra={})=>({id,kind,x,y,width,height,...extra});
 const b=(id,action,y,extra={})=>e(id,'button',30,y,270,45,{text:id,action,autoLabel:false,...extra});
 content.project.visual.screens=[
  {id:'title',label:'Title',elements:[b('start','start',20),b('load','openModal',80,{screenId:'load-records'})]},
  {id:'home',label:'Quiet room',elements:[e('bg','panel',0,0,1280,720,{fill:'#365449'}),b('change','executeAction',20,{actionId:'raise-skill'}),b('save','openModal',80,{screenId:'records'}),b('load','openModal',140,{screenId:'load-records'})]},
  {id:'schedule',label:'Schedule',elements:[]},{id:'story',label:'Story',elements:[]},{id:'ending',label:'Ending',elements:[]},
  ...[false,true].map(load=>({id:load?'load-records':'records',label:'Records',modal:true,elements:[b('close','closeModal',650),e('slots',load?'loadSlots':'saveSlots',40,30,1200,590,{color:'#263C36',fill:'#F5EFDF'})]}))
 ];
 content.project.visual.saveSystem={manualSlots:6,autoSlots:3};
 content.project.visual.routing={defaultScreenId:'home',useLocationScreens:false,phases:[{phase:'title',screenId:'title'},{phase:'planning',screenId:'home'}]};
 content.project.gameplay??={startLocationId:'home',locations:[{id:'home',name:'Home',connections:[]}],items:[],shops:[],interactions:[]};
 content.project.gameplay.actions??=[];
 content.project.gameplay.actions.push({id:'raise-skill',name:'Raise skill',costs:[],effects:[{op:'add',target:{kind:'attribute',attributeId:'skill'},value:{op:'literal',value:1}}],confirmation:{enabled:false}});
 const bundle=join(root,'game');await exportGame({projectRoot:source,content,target:'windows',destination:bundle,runtimes:{windows:runtime,web:resolve('dist/web-player')}});
 const save=join(root,'slot.json'),dir=join(root,'slot.slots');
 const run=async(name,steps)=>{
  const script=join(root,name+'.json'),report=join(root,name+'-report.json');await writeFile(script,JSON.stringify(steps));
  const r=spawnSync(join(bundle,'RaisingPlayer.exe'),['--bundle',bundle,'--save',save,'--automation',script,'--report',report,'--hidden'],{windowsHide:true,encoding:'utf8',timeout:60000});
  assert.equal(r.status,0,r.stderr||String(r.error));const data=JSON.parse(await readFile(report,'utf8'));assert.equal(data.ok,true);return data;
 };
 const steps=[],click=id=>steps.push({click:id},{waitFrames:2}),mark=id=>steps.push({checkpoint:id});
 click('start');mark('initial');click('save');click('records__slots-request');mark('first');click('records__close');
 click('change');click('save');click('records__slots-slot-1');mark('selected-second');click('records__slots-request');mark('second');
 click('records__slots-slot-0');click('records__slots-request');mark('overwrite-dialog');click('records__slots-cancel');mark('cancelled');click('records__close');
 click('load');click('load-records__slots-slot-0');click('load-records__slots-request');mark('load-dialog');click('load-records__slots-cancel');mark('load-cancelled');
 click('load-records__slots-request');click('load-records__slots-confirm');mark('restored-first');
 for(let i=0;i<5;i++)click('change');
 click('save');click('records__slots-slot-0');click('records__slots-request');click('records__slots-confirm');mark('overwritten');click('records__close');
 click('load');click('load-records__slots-slot-1');click('load-records__slots-request');click('load-records__slots-confirm');mark('restored-second');
 click('save');click('records__slots-auto');mark('protected-auto');click('records__close');
 click('load');click('load-records__slots-auto');mark('auto-list');click('load-records__slots-slot-0');click('load-records__slots-request');click('load-records__slots-confirm');mark('auto-restored');
 const data=await run('flow',steps),at=id=>data.steps.find(s=>s.command.checkpoint===id).ui;
 assert.equal(at('selected-second').focusedId,'records__slots-slot-1');
 assert.equal(at('overwrite-dialog').focusedId,'records__slots-cancel');
 assert.equal(at('load-dialog').focusedId,'load-records__slots-cancel');
 assert.equal(at('overwritten').focusedId,'records__slots-slot-0');
 assert.equal(at('cancelled').focusedId,'records__slots-slot-0');
 assert.deepEqual(at('initial').state,at('restored-first').state);
 assert.deepEqual(at('second').state,at('restored-second').state);
 assert.deepEqual(at('cancelled').state,at('second').state);assert.deepEqual(at('load-cancelled').state,at('second').state);
 assert.equal(at('protected-auto').elements.find(e=>e.id==='records__slots-request').enabled,false);
 const first=JSON.parse(await readFile(save)),second=JSON.parse(await readFile(join(dir,'manual-2.json'))),backup=JSON.parse(await readFile(save+'.bak'));
 assert.deepEqual(first.state,at('overwritten').state);assert.deepEqual(second.state,at('second').state);assert.deepEqual(backup.state,at('initial').state);
 const autos=await Promise.all([1,2,3].map(async i=>JSON.parse(await readFile(join(dir,`auto-${i}.json`)))));
 assert.deepEqual(autos[0].state,at('auto-restored').state);
 assert.equal(new Set(autos.map(a=>a.playerPresentation.saveRecord.sequence)).size,3);
 for(const record of [first,second,backup,...autos]){
  const m=record.playerPresentation.saveRecord;assert.equal(m.place,'Quiet room');assert.ok(m.date);assert.ok(m.savedAt);
  const png=await readFile(join(dir,m.thumbnail));assert.equal(png.subarray(1,4).toString(),'PNG');assert.equal(png.readUInt32BE(16),480);
 }
 const files=await readdir(dir);assert.ok(files.filter(f=>f.endsWith('.png')).length<=10,'Prune unreferenced captures; retain backups');
 // A corrupt record must not poison the other five manual slots or be overwritten by browsing.
 await writeFile(join(dir,'manual-3.json'),'{broken');
 const broken=await run('damaged',[{click:'load'},{waitFrames:2},{click:'load-records__slots-slot-2'},{waitFrames:2}]);
 assert.equal(broken.final.elements.find(e=>e.id==='load-records__slots-request').enabled,false);
 assert.equal(await readFile(join(dir,'manual-3.json'),'utf8'),'{broken');
 // Existing save envelope remains accepted without a screenshot/metadata field.
 delete first.playerPresentation.saveRecord;await writeFile(save,JSON.stringify(first));
 const legacy=await run('legacy',[{click:'load'},{waitFrames:2},{click:'load-records__slots-request'},{waitFrames:2},{click:'load-records__slots-confirm'},{waitFrames:2}]);
 assert.deepEqual(legacy.final.state,first.state);
 const old=join(root,'old-runtime');await mkdir(old);const caps=JSON.parse(await readFile(join(runtime,'capabilities.json')));caps.visualFeatures=caps.visualFeatures.filter(x=>x!=='save-slots-v1');await writeFile(join(old,'capabilities.json'),JSON.stringify(caps));await copyFile(join(runtime,'RaisingPlayer.exe'),join(old,'RaisingPlayer.exe'));
 await assert.rejects(exportGame({projectRoot:source,content,target:'windows',destination:join(root,'old'),runtimes:{windows:old,web:resolve('dist/web-player')}}),/save-slots-v1/);
 console.log(JSON.stringify({evidence:root,steps:data.steps.length,autos:autos.length}));
});

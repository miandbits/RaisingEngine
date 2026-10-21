import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,readFile,writeFile,mkdir,rmdir} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('player preferences survive restart/load, mix buses, reveal UTF-8 and stop auto reading at choices/modals',{timeout:180000},async()=>{
 const source=resolve('tests/fixtures/projects/last-grandson-polished');
 const snapshot=await buildSnapshot(await loadProject(source));assert.equal(snapshot.ok,true);
 const content=structuredClone(snapshot.snapshot),root=await mkdtemp(resolve('.cache/player-settings-'));
 const runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app');
 const e=(id,kind,x,y,width,height,extra={})=>({id,kind,x,y,width,height,...extra});
 const b=(id,action,y,extra={})=>e(id,'button',30,y,270,44,{text:id,action,autoLabel:false,...extra});
 content.project.visual.screens=[
  {id:'title',label:'Title',elements:[b('start','start',20),b('settings','openModal',80,{screenId:'prefs'})]},
  {id:'home',label:'Home',elements:[b('talk','executeAction',20,{actionId:'read-story'}),b('settings','openModal',80,{screenId:'prefs'}),b('save','save',140),b('load','load',200)]},
  {id:'schedule',label:'Schedule',elements:[]},{id:'ending',label:'Ending',elements:[]},
  {id:'story',label:'Story',elements:[e('text','dialogue',40,100,1100,140),e('choices','choices',40,300,900,200),b('next','advance',560,{visibleWhen:'dialogue',autoLabel:true}),b('settings','openModal',620,{screenId:'prefs'}),b('save','save',20),e('status','text',400,620,700,40,{binding:'reading-status'})]},
  {id:'prefs',label:'Preferences',modal:true,elements:[b('close','closeModal',660),e('controls','playerSettings',40,40,1200,540)]}
 ];
 content.project.visual.defaultScene={...content.project.visual.defaultScene,musicAssetId:'winter-motif'};
 content.project.visual.routing={defaultScreenId:'home',useLocationScreens:false,phases:[{phase:'title',screenId:'title'},{phase:'planning',screenId:'home'},{phase:'dialogue',screenId:'story'},{phase:'choice',screenId:'story'}]};
 content.project.gameplay??={startLocationId:'home',locations:[{id:'home',name:'Home',requirements:{op:'always'},connections:[]}],items:[],shops:[],interactions:[],actions:[]};
 content.project.gameplay.actions??=[];
 content.project.gameplay.actions.push({id:'read-story',name:'Read',requirements:{op:'always'},costs:[],effects:[],eventId:'read-story',confirmation:{enabled:false}});
 content.events.push({formatVersion:'0.1.0',id:'read-story',label:'Read',window:{fromPeriod:0,throughPeriod:0},requirements:{op:'always'},priority:100,trigger:'interaction',repeatable:true,entryNodeId:'one',nodes:[
  {id:'one',type:'dialogue',speakerId:null,text:'눈을 건너는 아이는 길고 긴 이야기를 천천히 읽습니다.\n한국어 日本語 & < >도 온전히 표시합니다.',next:'two'},
  {id:'two',type:'dialogue',speakerId:null,text:'두 번째 이야기입니다.',next:'choice'},
  {id:'choice',type:'choice',prompt:'Your choice',choices:[{id:'answer',text:'Continue',requirements:{op:'always'},effects:[],next:'three'}]},
  {id:'three',type:'dialogue',speakerId:null,text:'선택 후에 이어지는 문장입니다.',next:'end'},
  {id:'end',type:'end'}
 ]});
 const bundle=join(root,'game');await exportGame({projectRoot:source,content,target:'windows',destination:bundle,runtimes:{windows:runtime,web:runtime}});
 const save=join(root,'save.json'),prefs=join(root,`settings-${content.project.gameId}.json`);
 const run=async(name,steps)=>{const script=join(root,name+'.json'),report=join(root,name+'-report.json');await writeFile(script,JSON.stringify(steps));const r=spawnSync(join(bundle,'RaisingPlayer.exe'),['--bundle',bundle,'--save',save,'--automation',script,'--report',report,'--hidden'],{windowsHide:true,encoding:'utf8',timeout:60000});assert.equal(r.status,0,r.stderr||String(r.error));return JSON.parse(await readFile(report,'utf8'));};
 const steps=[],click=id=>steps.push({click:id},{waitFrames:2}),mark=checkpoint=>steps.push({checkpoint});
 click('settings');click('prefs__controls-musicVolume-down');click('prefs__controls-effectsVolume-down');click('prefs__controls-effectsVolume-down');click('prefs__controls-speed-20');click('prefs__controls-auto-on');click('prefs__controls-delay-down');mark('configured');click('prefs__close');click('start');click('talk');mark('initial');steps.push({dialogueAdvanceMs:500});mark('partial');click('settings');const clock=()=>steps.push({dialogueAdvanceMs:10000});clock();mark('modal-paused');click('prefs__close');click('next');mark('revealed');click('next');mark('second');clock();mark('choice');clock();mark('still-choice');click('choose-answer');mark('third');click('save');click('settings');click('prefs__controls-auto-off');click('prefs__controls-speed-70');mark('changed-after-save');click('prefs__close');clock();mark('manual-stopped');
 const result=await run('flow',steps),at=id=>result.steps.find(s=>s.command.checkpoint===id).ui;
 assert.equal(at('configured').settings.values.musicVolume,70);assert.equal(at('configured').settings.values.effectsVolume,60);
 assert.ok(Math.abs(at('configured').audio.musicGain-.7)<.001);assert.ok(Math.abs(at('configured').audio.effectsGain-.6)<.001);
 assert.equal(at('configured').audio.musicPlaying,true);
 assert.equal(at('initial').dialogueReading.visibleBytes,0);assert.ok(at('partial').dialogueReading.visibleBytes>0);assert.equal(at('partial').dialogueReading.complete,false);
 assert.deepEqual(at('partial').state,at('modal-paused').state);assert.equal(at('partial').dialogueReading.visibleBytes,at('modal-paused').dialogueReading.visibleBytes);
 assert.deepEqual(at('partial').state,at('revealed').state);assert.equal(at('revealed').dialogueReading.complete,true);
 assert.equal(at('partial').elements.find(e=>e.id==='next').label,'끝까지 보기');assert.equal(at('revealed').elements.find(e=>e.id==='next').label,'다음 문장');
 assert.notDeepEqual(at('revealed').state,at('second').state);assert.deepEqual(at('choice').state,at('still-choice').state);assert.ok(at('choice').elements.some(e=>e.id==='choose-answer'));
 assert.deepEqual(at('third').state,at('manual-stopped').state);
 const persisted=JSON.parse(await readFile(prefs,'utf8'));assert.equal(persisted.textSpeed,70);assert.equal(persisted.autoAdvance,false);
 const restarted=await run('restarted',[{checkpoint:'startup'},{click:'start'},{waitFrames:2},{click:'load'},{waitFrames:2},{checkpoint:'loaded'}]);
 assert.deepEqual(restarted.steps[0].ui.settings.values,persisted);assert.deepEqual(restarted.final.settings.values,persisted);assert.deepEqual(restarted.final.state,at('third').state);assert.equal(restarted.final.dialogueReading.complete,false);
 // A failed settings write must roll back live audio and keep the prior file.
 await mkdir(prefs+'.tmp');const failed=await run('write-failed',[{click:'settings'},{waitFrames:2},{click:'prefs__controls-musicVolume-down'},{waitFrames:2}]);
 assert.deepEqual(failed.final.settings.values,persisted);assert.ok(failed.final.settings.notice);assert.ok(Math.abs(failed.final.audio.musicGain-.7)<.001);assert.deepEqual(JSON.parse(await readFile(prefs,'utf8')),persisted);await rmdir(prefs+'.tmp');
 // A malformed preferences file is preserved while defaults are usable.
 await writeFile(prefs,'{broken');const damaged=await run('damaged',[{click:'settings'},{waitFrames:2}]);assert.equal(await readFile(prefs,'utf8'),'{broken');assert.ok(damaged.final.settings.notice);assert.equal(damaged.final.settings.values.musicVolume,80);
 // Widgets fail explicitly when exported through a runtime without this capability.
 const old=join(root,'old');await mkdir(old);for(const f of ['RaisingPlayer.exe','index.html'])await writeFile(join(old,f),'');
 const caps=JSON.parse(await readFile(join(runtime,'capabilities.json'),'utf8'));caps.visualFeatures=caps.visualFeatures.filter(f=>f!=='player-settings-v1');await writeFile(join(old,'capabilities.json'),JSON.stringify(caps));
 for(const target of ['windows','web'])await assert.rejects(exportGame({projectRoot:source,content,target,destination:join(root,`old-${target}`),runtimes:{windows:old,web:old}}),/player-settings-v1/);
});

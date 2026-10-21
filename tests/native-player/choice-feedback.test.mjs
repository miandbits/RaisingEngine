import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('dialogue keeps unchanged backgrounds opaque; choices disclose and restore actual clamped results',{timeout:180000},async()=>{
 const projectRoot=resolve('tests/fixtures/projects/last-grandson-polished');
 const loaded=await buildSnapshot(await loadProject(projectRoot));assert.equal(loaded.ok,true,JSON.stringify(loaded.diagnostics));
 const content=structuredClone(loaded.snapshot),root=await mkdtemp(resolve('.cache/choice-feedback-'));
 const runtime=resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-dialogue');
 const el=(id,kind,x,y,width,height,extra={})=>({id,kind,x,y,width,height,...extra});
 const button=(id,action,x,y,extra={})=>el(id,'button',x,y,220,48,{action,text:id,autoLabel:false,...extra});
 content.attributes.find(a=>a.id==='skill').max=100;
 content.project.initialState.attributes.skill=99;
 content.characters.find(c=>c.id==='apprentice').portraits.face='seol-neutral';
 content.project.visual.screens=[
  {id:'title',label:'Title',elements:[button('start','start',10,10)]},
  {id:'home',label:'Home',elements:[button('open','executeAction',10,10,{actionId:'feedback'})]},
  {id:'story',label:'Story',elements:[
   el('background','image',0,0,1280,720,{imageSource:'background',fit:'cover',motion:'fade',durationMs:1000}),
   el('dialogue','dialogue',40,40,1100,130),
   el('choices','choices',40,220,900,330,{choiceLayout:'cards',showChoiceEffects:true,rowHeight:110,gap:12,fontSize:20,fill:'#f5efdf',color:'#263c36',padding:0}),
   el('feedback','text',40,560,1000,90,{binding:'choice-result',fontSize:18}),
   button('next','advance',1010,650,{visibleWhen:'dialogue'})
  ]},
  {id:'schedule',label:'Schedule',elements:[]},{id:'ending',label:'Ending',elements:[]}
 ];
 content.project.visual.routing={defaultScreenId:'home',useLocationScreens:false,phases:[{phase:'title',screenId:'title'},{phase:'planning',screenId:'home'},{phase:'dialogue',screenId:'story'},{phase:'choice',screenId:'story'}]};
 content.project.gameplay??={startLocationId:'home',locations:[{id:'home',name:'Home',requirements:{op:'always'},connections:[]}],items:[],shops:[],interactions:[],actions:[]};content.project.gameplay.actions??=[];
 content.project.gameplay.actions.push({id:'feedback',name:'Feedback',requirements:{op:'always'},costs:[],effects:[],eventId:'feedback',confirmation:{enabled:false}});
 content.events.push({formatVersion:'0.1.0',id:'feedback',label:'Feedback',window:{fromPeriod:0,throughPeriod:0},requirements:{op:'always'},priority:100,trigger:'interaction',repeatable:false,entryNodeId:'scene',nodes:[
  {id:'scene',type:'scene',scene:{backgroundAssetId:'winter-courtyard',portraits:[]},next:'one'},
  {id:'one',type:'dialogue',speakerId:null,text:'First line.',next:'portrait'},
  {id:'portrait',type:'scene',scene:{portraits:[{characterId:'apprentice',portraitId:'face',position:'right'}]},next:'two'},
  {id:'two',type:'dialogue',speakerId:'apprentice',text:'A new speaker, same background.',next:'three'},
  {id:'three',type:'dialogue',speakerId:null,text:'One more line.',next:'question'},
  {id:'question',type:'choice',prompt:'Choose.',choices:[
   {id:'accept',text:'Keep the promise',consequenceText:'Meet again at the library.',requirements:{op:'always'},effects:[{op:'add',target:{kind:'attribute',attributeId:'skill'},value:{op:'literal',value:10}},{op:'add',target:{kind:'relationship',characterId:'mentor',relationshipId:'trust'},value:{op:'literal',value:6}}],next:'reply'},
   {id:'later',text:'Not yet',consequenceText:'No promise made.',requirements:{op:'always'},effects:[],next:'reply'}
  ]},
  {id:'reply',type:'dialogue',speakerId:null,text:'The answer is remembered.',next:'changed'},
  {id:'changed',type:'scene',scene:{backgroundAssetId:'apothecary'},next:'four'},
  {id:'four',type:'dialogue',speakerId:null,text:'A genuinely different background.',next:'end'},
  {id:'end',type:'end'}
 ]});
 const game=resolve(root,'game');await exportGame({content,projectRoot,target:'windows',destination:game,runtimes:{windows:runtime}});
 const legacyRuntime=resolve(root,'legacy-runtime');await mkdir(legacyRuntime);
 const capabilities=JSON.parse(await readFile(resolve(runtime,'capabilities.json'),'utf8'));
 capabilities.visualFeatures=capabilities.visualFeatures.filter(f=>f!=='choice-feedback-v1');
 await writeFile(resolve(legacyRuntime,'index.html'),'<!doctype html>');
 await writeFile(resolve(legacyRuntime,'capabilities.json'),JSON.stringify(capabilities));
 await assert.rejects(exportGame({content,projectRoot,target:'web',destination:resolve(root,'unsupported'),runtimes:{web:legacyRuntime}}),/choice-feedback-v1/);
 const steps=[{click:'start'},{click:'open'},{waitFrames:120},{checkpoint:'stable'},{screenshot:resolve(root,'stable.png')},
  {click:'next'},{checkpoint:'speaker-changed'},{screenshot:resolve(root,'speaker-changed.png')},
  {click:'next'},{checkpoint:'line-changed'},{click:'next'},{checkpoint:'question'},
  {key:'F5'},{checkpoint:'choice-save'},{click:'choose-accept'},{checkpoint:'accepted'},
  {key:'F9'},{checkpoint:'choice-load'},{click:'choose-later'},{checkpoint:'no-change'},
  {key:'F9'},{click:'choose-accept'},
  {key:'F5'},{checkpoint:'result-save'},{click:'next'},{checkpoint:'background-changed'},
  {key:'F9'},{checkpoint:'result-load'},{click:'next'},{click:'next'},{checkpoint:'ended'}];
 const script=resolve(root,'steps.json'),reportPath=resolve(root,'report.json');await writeFile(script,JSON.stringify(steps.flatMap(s=>s.click||s.key?[s,{waitFrames:2}]:[s])));
 const run=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',resolve(root,'save.json'),'--automation',script,'--report',reportPath,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:120000});
 const report=JSON.parse(await readFile(reportPath));assert.equal(run.status,0,JSON.stringify(report.error??run.error));
 const at=id=>report.steps.find(s=>s.command.checkpoint===id).ui;
 const alpha=id=>at(id).nodes.find(n=>n.id==='background').opacity;
 for(const id of ['stable','speaker-changed','line-changed','question','accepted'])assert.ok(alpha(id)>.99,`${id}: unchanged background disappeared (${alpha(id)})`);
 assert.ok(alpha('background-changed')<.9,'A real background replacement still animates');
 assert.equal(at('accepted').state.attributes.skill,100);
 assert.match(at('accepted').texts.feedback,/99 → 100 \(\+1\)/);
 assert.match(at('accepted').texts.feedback,/\+6/);
 assert.match(at('accepted').texts.feedback,/Meet again at the library/);
 assert.deepEqual(at('choice-save').state,at('choice-load').state);
 assert.equal(at('no-change').state.attributes.skill,99,'Another branch does not retain the previous choice effect');
 assert.match(at('no-change').texts.feedback,/No promise made/);
 assert.doesNotMatch(at('no-change').texts.feedback,/Meet again|\+6/);
 assert.deepEqual(at('result-save').state,at('result-load').state);
 assert.equal(at('result-save').texts.feedback,at('result-load').texts.feedback);
 assert.equal(at('ended').texts.feedback,undefined,'Feedback does not leak into other screens');
 console.log('Choice feedback evidence: '+root);
});

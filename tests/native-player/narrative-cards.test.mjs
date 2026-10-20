import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('authored choice cards retain availability, branches and saves; unsupported exports are rejected',{timeout:120000},async()=>{
  const projectRoot=resolve('tests/fixtures/projects/last-grandson-polished');
  const snapshot=await buildSnapshot(await loadProject(projectRoot));
  assert.equal(snapshot.ok,true,JSON.stringify(snapshot.diagnostics));
  const content=structuredClone(snapshot.snapshot),root=await mkdtemp(resolve('.cache/narrative-cards-'));
  const el=(id,kind,x,y,width,height,extra={})=>({id,kind,x,y,width,height,...extra});
  const button=(id,action,x,y,extra={})=>el(id,'button',x,y,220,48,{action,text:id,autoLabel:false,...extra});
  content.project.visual.screens=[
    {id:'title',label:'Title',elements:[button('start','start',80,80)]},
    {id:'schedule',label:'Schedule',elements:[]},
    {id:'ending',label:'Ending',elements:[]},
    {id:'home',label:'Home',elements:[button('open','executeAction',80,80,{actionId:'test-choice'})]},
    {id:'story',label:'Story',elements:[
      el('dialogue','dialogue',60,80,1100,120),
      el('choices','choices',60,240,720,300,{choiceLayout:'cards',rowHeight:64,gap:12,fontSize:24,fill:'#f5efdf',color:'#263c36',borderWidth:1,borderColor:'#c7b99a',hoverFill:'#dde6da',padding:0}),
      button('next','advance',1000,640,{visibleWhen:'dialogue'})
    ]}
  ];
  content.project.visual.routing={defaultScreenId:'home',useLocationScreens:false,phases:[
    {phase:'title',screenId:'title'},{phase:'planning',screenId:'home'},{phase:'dialogue',screenId:'story'},{phase:'choice',screenId:'story'}
  ]};
  content.project.gameplay??={startLocationId:'home',locations:[{id:'home',name:'Home',requirements:{op:'always'},connections:[]}],items:[],shops:[],interactions:[],actions:[]};
  content.project.gameplay.actions??=[];
  content.project.gameplay.actions.push({id:'test-choice',name:'Test choice',requirements:{op:'always'},costs:[],effects:[],eventId:'test-choice',confirmation:{enabled:false}});
  content.events.push({formatVersion:'0.1.0',id:'test-choice',label:'Test choice',window:{fromPeriod:0,throughPeriod:0},requirements:{op:'always'},priority:100,trigger:'interaction',repeatable:false,entryNodeId:'question',nodes:[
    {id:'question',type:'choice',prompt:'Choose one path.',choices:[
      {id:'first',text:'The first path',requirements:{op:'always'},effects:[{op:'add',target:{kind:'attribute',attributeId:'skill'},value:{op:'literal',value:1}}],next:'first'},
      {id:'locked',text:'A locked path',requirements:{op:'compare',comparator:'eq',left:{op:'literal',value:0},right:{op:'literal',value:1}},effects:[],next:'third'},
      {id:'third',text:'The third path',requirements:{op:'always'},effects:[],next:'third'}
    ]},
    {id:'first',type:'dialogue',speakerId:null,text:'First response.',next:'end'},
    {id:'third',type:'dialogue',speakerId:null,text:'Third response.',next:'end'},
    {id:'end',type:'end'}
  ]});
  const runtime=resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-narrative'),game=resolve(root,'game');
  await exportGame({content,projectRoot,target:'windows',destination:game,runtimes:{windows:runtime,web:runtime}});
  const old=resolve(root,'old-runtime');await mkdir(old);
  for(const entry of ['RaisingPlayer.exe','index.html'])await writeFile(resolve(old,entry),'');
  const caps=JSON.parse(await readFile(resolve(runtime,'capabilities.json'),'utf8'));
  caps.visualFeatures=caps.visualFeatures.filter(f=>f!=='narrative-cards-v1');
  await writeFile(resolve(old,'capabilities.json'),JSON.stringify(caps));
  for(const target of ['windows','web'])await assert.rejects(exportGame({content,projectRoot,target,destination:resolve(root,`old-${target}`),runtimes:{windows:old,web:old}}),/narrative-cards-v1/);
  const steps=[
    {click:'start'},{waitFrames:2},{click:'open'},{waitFrames:2},{checkpoint:'question'},
    {resize:{width:960,height:540}},{checkpoint:'small'},
    {key:'F5'},{waitFrames:2},{checkpoint:'saved'},
    {click:'choose-first'},{waitFrames:2},{checkpoint:'first'},
    {key:'F9'},{waitFrames:2},{checkpoint:'restored'},
    {click:'choose-third'},{waitFrames:2},{checkpoint:'third'}
  ];
  const script=resolve(root,'steps.json'),reportPath=resolve(root,'report.json');
  await writeFile(script,JSON.stringify(steps));
  const result=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',resolve(root,'save.json'),'--automation',script,'--report',reportPath,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:60000});
  const report=JSON.parse(await readFile(reportPath,'utf8'));
  assert.equal(result.status,0,JSON.stringify(report.error??result.error));
  const at=id=>report.steps.find(s=>s.command.checkpoint===id).ui;
  assert.equal(at('question').elements.filter(e=>e.id.startsWith('choose-')).length,3);
  assert.equal(at('question').elements.find(e=>e.id==='choose-locked').enabled,false);
  const questionRows=at('question').elements.filter(e=>e.id.startsWith('choose-'));
  assert.deepEqual(questionRows.map(e=>e.label),['The first path','A locked path','The third path']);
  for(const row of questionRows){
    assert.equal(row.color,'#263c36');
    assert.equal(row.fontSize,'24px');
    assert.equal(row.height+26,64,'card content plus vertical padding and borders follows authored row height');
  }
  const rows=at('small').elements.filter(e=>e.id.startsWith('choose-'));
  for(let i=1;i<rows.length;i++)assert.ok(rows[i].y>=rows[i-1].y+rows[i-1].height);
  assert.equal(at('first').state.activeNarrative.nodeId,'first');
  assert.deepEqual(at('restored').state,at('saved').state);
  assert.deepEqual(at('restored').texts,at('saved').texts);
  assert.equal(at('third').state.activeNarrative.nodeId,'third');
  assert.equal(at('third').state.attributes.skill,at('saved').state.attributes.skill,'restored choice must discard the first branch effect');
  // Projects without the new option still render and restore the original list.
  delete content.project.visual.screens.find(s=>s.id==='story').elements.find(e=>e.kind==='choices').choiceLayout;
  const legacyGame=resolve(root,'legacy-game'),legacyReport=resolve(root,'legacy-report.json');
  await exportGame({content,projectRoot,target:'windows',destination:legacyGame,runtimes:{windows:runtime,web:runtime}});
  const legacyRun=spawnSync(resolve(legacyGame,'RaisingPlayer.exe'),['--bundle',legacyGame,'--save',resolve(root,'legacy-save.json'),'--automation',script,'--report',legacyReport,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:60000});
  const legacy=JSON.parse(await readFile(legacyReport,'utf8'));
  assert.equal(legacyRun.status,0,JSON.stringify(legacy.error??legacyRun.error));
  const legacyAt=id=>legacy.steps.find(s=>s.command.checkpoint===id).ui;
  assert.equal(legacyAt('question').elements.find(e=>e.id==='choose-locked').enabled,false);
  assert.deepEqual(legacyAt('restored').state,legacyAt('saved').state);
  assert.equal(legacyAt('third').state.activeNarrative.nodeId,'third');
  assert.deepEqual(legacyAt('third').state.attributes,at('third').state.attributes);
});

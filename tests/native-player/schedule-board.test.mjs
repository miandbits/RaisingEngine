import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('schedule board drafts survive navigation/save/load; menu freezes playback; old saves remain valid',{timeout:180000},async()=>{
  const project=resolve('tests/fixtures/projects/last-grandson-polished');
  const snapshot=await buildSnapshot(await loadProject(project));
  assert.equal(snapshot.ok,true,JSON.stringify(snapshot.diagnostics));
  const content=structuredClone(snapshot.snapshot);
  const el=(id,kind,x,y,width,height,props={})=>({id,kind,x,y,width,height,...props});
  const button=(id,action,x,y,extra={})=>el(id,'button',x,y,180,44,{text:id,action,autoLabel:false,...extra});
  content.project.visual.screens=content.project.visual.screens.filter(s=>['title','schedule','story','ending'].includes(s.id));
  content.project.visual.routing={defaultScreenId:'schedule',useLocationScreens:false,phases:[
    {phase:'title',screenId:'title'},{phase:'dialogue',screenId:'story'},{phase:'choice',screenId:'story'},{phase:'finished',screenId:'ending'}
  ]};
  // No plugin is needed for this generic presentation-state contract test.
  for(const s of content.project.visual.screens)s.elements=s.elements.filter(e=>e.kind!=='plugin'&&e.kind!=='component');
  const schedule=content.project.visual.screens.find(s=>s.id==='schedule');
  schedule.elements=[
    el('plan','schedule',40,100,1200,480,{scheduleLayout:'board',fontSize:20,attributeIds:['skill','money','stamina','stress'],decreasingAttributeIds:['stress'],visibleWhen:'planning',activityIcons:[{activityId:'rest',imageAssetId:content.assets.find(a=>a.kind==='image').id}]}),
    button('back','navigate',40,650,{screenId:'home',visibleWhen:'planning'}),
    button('commit','commitPlan',1020,650,{visibleWhen:'planning'}),
    button('menu','openModal',1060,20,{screenId:'pause'}),
    el('result','result',100,180,600,250,{resultLayout:'changes',visibleWhen:'result'})
  ];
  content.project.visual.screens.push(
    {id:'home',label:'Home',elements:[button('schedule','navigate',100,200,{screenId:'schedule'})]},
    {id:'pause',label:'Pause',modal:true,elements:[button('close','closeModal',500,300)]});
  const runtime=resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player');
  const root=await mkdtemp(resolve('.cache/schedule-board-')),game=resolve(root,'game'),save=resolve(root,'save.json');
  await exportGame({content,projectRoot:project,destination:game,target:'windows',runtimes:{windows:runtime,web:resolve('dist/web')}});
  const unsupported=resolve(root,'old-runtime');await mkdir(unsupported);
  await writeFile(resolve(unsupported,'RaisingPlayer.exe'),'');
  await writeFile(resolve(unsupported,'index.html'),'');
  await writeFile(resolve(unsupported,'capabilities.json'),JSON.stringify({formatVersion:'1',imageExtensions:['.png'],audioExtensions:['.wav'],visualFeatures:[]}));
  for(const target of ['windows','web'])await assert.rejects(
    exportGame({content,projectRoot:project,destination:resolve(root,'unsupported-'+target),target,runtimes:{windows:unsupported,web:unsupported}}),
    /schedule-board-icons-v1/,'a runtime without authored icons must not silently export broken UI');
  const run=async(name,steps)=>{
    const script=resolve(root,name+'.json'),report=resolve(root,name+'-report.json');
    await writeFile(script,JSON.stringify(steps));
    const child=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',save,'--seed','42','--automation',script,'--report',report,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:60000});
    const result=JSON.parse(await readFile(report,'utf8'));
    assert.equal(child.status,0,JSON.stringify(result.error??child.error));
    assert.equal(result.ok,true);assert.equal(result.steps.length,steps.length);
    return result;
  };
  const report=await run('draft',[
    {click:'start'},{checkpoint:'initial'},
    {click:'board-slot-0'},{click:'board-activity-rest'},{key:'F5'},{checkpoint:'saved'},
    {click:'back'},{click:'schedule'},{checkpoint:'returned'},
    {click:'board-clear'},{checkpoint:'cleared'},{key:'F9'},{checkpoint:'restored'},
    {click:'board-slot-1'},{click:'board-activity-rest'},{click:'board-slot-2'},{click:'board-activity-rest'},
    {checkpoint:'complete'},{resize:{width:960,height:540}},{checkpoint:'small'},
    {click:'commit'},{checkpoint:'lead'},{timelineAdvanceMs:1200},{checkpoint:'mid'},
    {click:'menu'},{checkpoint:'menu'},{timelineAdvanceMs:5000},{scheduleAdvanceMs:10000},{extensionAdvanceMs:5000},{checkpoint:'held'},
    {key:'Escape'},{key:'F5'},{checkpoint:'mid-saved'},
    {timelineAdvanceMs:1000},{key:'F9'},{checkpoint:'mid-restored'}
  ]);
  const ui=id=>report.steps.find(s=>s.command.checkpoint===id).ui;
  assert.equal(ui('saved').scheduleDraft.selected,1);
  assert.deepEqual(ui('returned').scheduleDraft,ui('saved').scheduleDraft);
  assert.equal(ui('cleared').scheduleDraft.selected,0);
  assert.deepEqual(ui('restored').state,ui('saved').state);
  assert.deepEqual(ui('restored').scheduleDraft,ui('saved').scheduleDraft);
  assert.deepEqual(ui('complete').state,ui('restored').state,'forecasts and assignments do not consume RNG, revisions or commands');
  assert.match(ui('complete').texts.plan,/현재.*활동 후 예상/);
  assert.ok(ui('complete').elements.find(e=>e.id==='commit').enabled);
  assert.equal(ui('lead').state.phase,'activity-result');
  assert.ok(!ui('lead').texts.result.includes('+0'),'unrevealed effects are not shown as zero changes');
  assert.deepEqual(ui('held').state,ui('menu').state);
  assert.deepEqual(ui('held').timeline,ui('menu').timeline);
  assert.deepEqual(ui('mid-restored').timeline,ui('mid-saved').timeline);
  assert.deepEqual(ui('mid-restored').state,ui('mid-saved').state);
  const saved=JSON.parse(await readFile(save,'utf8'));
  const legacy=structuredClone(saved);delete legacy.playerPresentation.scheduleDraft;
  await writeFile(save,JSON.stringify(legacy));
  const old=await run('legacy',[{click:'load'}]);
  assert.deepEqual(old.final.state,ui('mid-saved').state);
  const invalid=structuredClone(saved);invalid.playerPresentation.scheduleDraft.activities[0]='unknown-activity';
  await writeFile(save,JSON.stringify(invalid));
  const rejected=await run('invalid',[{click:'start'},{checkpoint:'before'},{key:'F9'}]);
  assert.deepEqual(rejected.final.state,rejected.steps.find(s=>s.command.checkpoint==='before').ui.state,'invalid draft must not partially load the simulation');
  assert.ok(rejected.final.message);
});

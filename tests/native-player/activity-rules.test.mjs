import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,readFile,writeFile,mkdir} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('Windows activity rule summaries scope each actor, preserve opt-out and exact saves',{timeout:90000},async()=>{
  const projectRoot=resolve('examples/genre-suite/roster');const built=await buildSnapshot(await loadProject(projectRoot));assert.ok(built.ok);
  const content=structuredClone(built.snapshot);content.events=[];
  const [first,second]=content.project.roster.members;const [work,rest]=content.activities;
  const local=content.project.roster.localAttributeIds[0],shared=content.attributes.find(a=>!content.project.roster.localAttributeIds.includes(a.id)).id;
  content.characters.find(c=>c.id===first.characterId).name='One';content.characters.find(c=>c.id===second.characterId).name='Two';
  content.attributes.find(a=>a.id===local).label='Craft';content.attributes.find(a=>a.id===shared).label='Funds';
  work.costs=[{attributeId:shared,amount:4}];work.baseEffects=[{op:'add',target:{kind:'attribute',attributeId:local,owner:{kind:'actor'}},value:{op:'literal',value:4}}];
  work.requirements={op:'all',conditions:[]};delete work.successCheck;delete work.successEffects;delete work.failureEffects;
  rest.costs=[];rest.baseEffects=[];first.activityId=work.id;second.activityId=rest.id;
  const schedule=enabled=>({id:'schedule-widget',kind:'schedule',x:40,y:40,width:1100,height:500,showActivityRules:enabled});
  const go=(id,screenId)=>({id,kind:'button',x:40,y:600,width:300,height:60,text:id,action:'navigate',screenId});
  content.project.visual={...content.project.visual,routing:{defaultScreenId:'schedule',useLocationScreens:false,phases:[{phase:'title',screenId:'title'}]},screens:[
    {id:'title',label:'Title',elements:[{id:'start',kind:'button',x:40,y:40,width:200,height:60,action:'start'}]},
    {id:'schedule',label:'Rules',elements:[schedule(true),go('hide','plain')]},
    {id:'plain',label:'Without rules',elements:[schedule(false)]},{id:'story',label:'Story',elements:[]},{id:'ending',label:'Ending',elements:[]} ]};
  const root=await mkdtemp(resolve('.cache/activity-rules-')),game=resolve(root,'game');
  const runtime=resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app');
  await exportGame({content,projectRoot,target:'windows',destination:game,runtimes:{windows:runtime}});
  const steps=[{click:'start'},{checkpoint:'initial'},{select:'roster-'+second.characterId,value:work.id},{checkpoint:'draft'},
    {click:'roster-assign'},{checkpoint:'assigned'},{key:'F5'},{click:'hide'},{checkpoint:'hidden'},{key:'F9'},{checkpoint:'restored'}];
  const script=resolve(root,'steps.json'),reportFile=resolve(root,'report.json');await writeFile(script,JSON.stringify(steps.flatMap(s=>s.click||s.key||s.select?[s,{waitFrames:2}]:[s])));
  const child=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',resolve(root,'save.json'),'--automation',script,'--report',reportFile,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:60000});
  const report=JSON.parse(await readFile(reportFile));assert.equal(child.status,0,JSON.stringify(report.error??child.error));assert.ok(report.ok);
  const at=id=>report.steps.find(s=>s.command.checkpoint===id).ui;const body=id=>JSON.stringify(at(id).texts);
  assert.match(body('initial'),/공용 · Funds 4/);assert.match(body('initial'),/One · Craft \+4/);assert.doesNotMatch(body('initial'),/Two · Craft \+4/);
  assert.match(body('draft'),/Two · Craft \+4/);assert.deepEqual(at('initial').state,at('draft').state);assert.doesNotMatch(body('hidden'),/규칙 요약/);
  assert.deepEqual(at('assigned').state,at('restored').state);
  const old=resolve(root,'old-runtime');await mkdir(old);const capabilities=JSON.parse(await readFile(resolve(runtime,'capabilities.json')));capabilities.visualFeatures=capabilities.visualFeatures.filter(f=>f!=='activity-rules-summary-v1');await writeFile(resolve(old,'capabilities.json'),JSON.stringify(capabilities));await writeFile(resolve(old,'RaisingPlayer.exe'),'unused test entry');
  await assert.rejects(()=>exportGame({content,projectRoot,target:'windows',destination:resolve(root,'rejected'),runtimes:{windows:old}}),/activity-rules-summary-v1/);
  console.log('Activity rules evidence: '+root);
});

test('Windows legacy single-character columns show changed rules without changing state',{timeout:90000},async()=>{
  const projectRoot=resolve('examples/genre-suite/roster');const built=await buildSnapshot(await loadProject(projectRoot));assert.ok(built.ok);
  const content=structuredClone(built.snapshot);content.events=[];delete content.project.roster;
  const [work,rest]=content.activities;const attribute=content.attributes[0];attribute.label='Craft';
  work.costs=[];work.baseEffects=[{op:'add',target:{kind:'attribute',attributeId:attribute.id},value:{op:'literal',value:4}}];
  rest.costs=[];rest.baseEffects=[];
  content.project.visual={...content.project.visual,routing:{defaultScreenId:'schedule',useLocationScreens:false,phases:[{phase:'title',screenId:'title'}]},screens:[
    {id:'title',label:'Title',elements:[{id:'start',kind:'button',x:40,y:40,width:200,height:60,action:'start'}]},
    {id:'schedule',label:'Columns',elements:[{id:'schedule-widget',kind:'schedule',x:40,y:40,width:1100,height:500,showActivityRules:true}]},
    {id:'story',label:'Story',elements:[]},{id:'ending',label:'Ending',elements:[]}]};
  const root=await mkdtemp(resolve('.cache/activity-rules-columns-')),game=resolve(root,'game');
  await exportGame({content,projectRoot,target:'windows',destination:game,runtimes:{windows:resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app')}});
  const script=resolve(root,'steps.json'),reportFile=resolve(root,'report.json');
  await writeFile(script,JSON.stringify([{click:'start'},{waitFrames:2},{select:'plan-0',value:work.id},{waitFrames:2},{checkpoint:'work'},{select:'plan-0',value:rest.id},{waitFrames:2},{checkpoint:'rest'}]));
  const child=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',resolve(root,'save.json'),'--automation',script,'--report',reportFile,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:60000});
  const report=JSON.parse(await readFile(reportFile));assert.equal(child.status,0,JSON.stringify(report.error??child.error));assert.ok(report.ok);
  const at=id=>report.steps.find(s=>s.command.checkpoint===id).ui;
  assert.match(JSON.stringify(at('work').texts),/Craft \+4/);assert.match(JSON.stringify(at('rest').texts),/수치 변화 없음/);
  assert.deepEqual(at('work').state,at('rest').state);
});

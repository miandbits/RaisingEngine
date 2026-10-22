import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('Windows roster descriptions follow draft selection and preserve the opt-out layout',{timeout:90000},async()=>{
  const projectRoot=resolve('examples/genre-suite/roster');
  const built=await buildSnapshot(await loadProject(projectRoot));assert.equal(built.ok,true);
  const content=structuredClone(built.snapshot);content.events=[];
  const [first,second]=content.project.roster.members;
  const [work,rest]=content.activities;assert.ok(work&&rest);
  work.description='WORK: shared cost and personal growth. <b>Literal</b>';
  rest.description='REST: personal energy recovery. '+ 'Long explanation. '.repeat(60);
  first.activityId=work.id;second.activityId=rest.id;
  const schedule=enabled=>({id:'schedule-widget',kind:'schedule',x:40,y:40,width:1100,height:500,showActivityDescription:enabled});
  const go=(id,screenId)=>({id,kind:'button',x:40,y:600,width:300,height:60,text:id,action:'navigate',screenId});
  content.project.visual={...content.project.visual,routing:{defaultScreenId:'schedule',useLocationScreens:false,phases:[{phase:'title',screenId:'title'}]},screens:[
    {id:'title',label:'Title',elements:[{id:'start',kind:'button',x:40,y:40,width:200,height:60,action:'start'}]},
    {id:'schedule',label:'With descriptions',elements:[schedule(true),go('hide','plain')]},
    {id:'plain',label:'Without descriptions',elements:[schedule(false),go('show','schedule')]},
    {id:'story',label:'Story',elements:[]},{id:'ending',label:'Ending',elements:[]} ]};
  const root=await mkdtemp(resolve('.cache/roster-description-')),game=resolve(root,'game');
  await exportGame({content,projectRoot,target:'windows',destination:game,runtimes:{windows:resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app')}});
  const steps=[{click:'start'},{checkpoint:'initial'},
    {select:'roster-'+first.characterId,value:rest.id},{checkpoint:'draft'},
    {click:'roster-assign'},{checkpoint:'assigned'},{key:'F5'},
    {click:'hide'},{checkpoint:'hidden'},{key:'F9'},{checkpoint:'restored'}];
  const script=resolve(root,'steps.json'),reportFile=resolve(root,'report.json');
  await writeFile(script,JSON.stringify(steps.flatMap(s=>s.click||s.key||s.select?[s,{waitFrames:2}]:[s])));
  const child=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',resolve(root,'save.json'),'--automation',script,'--report',reportFile,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:60000});
  const report=JSON.parse(await readFile(reportFile));assert.equal(child.status,0,JSON.stringify(report.error??child.error));assert.equal(report.ok,true);
  const at=id=>report.steps.find(s=>s.command.checkpoint===id).ui;
  const body=id=>JSON.stringify(at(id).texts);
  assert.match(body('initial'),/WORK: shared cost/);assert.match(body('initial'),/REST: personal energy/);
  assert.doesNotMatch(body('draft'),/WORK: shared cost/);assert.match(body('draft'),/REST: personal energy/);
  assert.equal(at('assigned').state.roster.assignments[first.characterId],rest.id);
  assert.doesNotMatch(body('hidden'),/WORK: shared cost|REST: personal energy/);
  assert.deepEqual(at('assigned').state,at('restored').state);
  console.log('Roster description evidence: '+root);
});

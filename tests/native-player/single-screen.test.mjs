import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('explicit routing runs one freely named screen while legacy and corrupt routes remain checked',{timeout:90000},async()=>{
  const projectRoot=resolve('examples/genre-suite/roster');
  const loaded=await loadProject(projectRoot);
  const built=await buildSnapshot(loaded);assert.equal(built.ok,true);
  const content=structuredClone(built.snapshot);content.events=[];
  content.project.visual={...content.project.visual,routing:{defaultScreenId:'workshop',phases:[],useLocationScreens:false},screens:[
    {id:'workshop',label:'Workshop',elements:[
      {id:'start',kind:'button',x:20,y:20,width:200,height:50,action:'start',visibleWhen:'title'},
      {id:'schedule',kind:'schedule',x:20,y:90,width:1100,height:290},
      {id:'result',kind:'result',x:20,y:400,width:900,height:180,visibleWhen:'result'},
      {id:'advance',kind:'button',x:940,y:600,width:240,height:50,action:'advance'}]}]};
  const root=await mkdtemp(resolve('.cache/single-screen-')),game=resolve(root,'game'),runtime=resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app');
  const authored=resolve(root,'authored');await cp(projectRoot,authored,{recursive:true});
  const projectFile=resolve(authored,'project.json'),project=JSON.parse(await readFile(projectFile));project.visual=content.project.visual;
  for(const location of project.gameplay.locations)if(location.screenId)location.screenId='workshop';content.project.gameplay=project.gameplay;
  await writeFile(projectFile,JSON.stringify(project));
  const validated=await buildSnapshot(await loadProject(authored));assert.equal(validated.ok,true,JSON.stringify(validated.diagnostics));
  await exportGame({content,projectRoot,target:'windows',destination:game,runtimes:{windows:runtime}});
  const script=resolve(root,'steps.json'),reportFile=resolve(root,'report.json'),save=resolve(root,'save.json');
  const steps=[{checkpoint:'title'},{click:'start'},{checkpoint:'planning'},{click:'roster-assign'},
    {click:'roster-execute'},{checkpoint:'result'},{key:'F5'},{click:'advance'},{key:'F9'},{checkpoint:'restored'}];
  await writeFile(script,JSON.stringify(steps.flatMap(s=>s.click||s.key?[s,{waitFrames:2}]:[s])));
  const run=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',save,'--automation',script,'--report',reportFile,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:60000});
  const report=JSON.parse(await readFile(reportFile));assert.equal(run.status,0,JSON.stringify(report.error));assert.equal(report.ok,true);
  const at=id=>report.steps.find(s=>s.command.checkpoint===id).ui;
  for(const id of ['title','planning','result','restored'])assert.equal(at(id).screenId,'workshop');
  assert.equal(at('result').state.phase,'activity-result');assert.deepEqual(at('result').state,at('restored').state);
  const gameFile=resolve(game,'game.json'),original=JSON.parse(await readFile(gameFile));
  const cases=[
    ['legacy',v=>delete v.routing,'visual.screens'],
    ['missing-default',v=>v.routing.defaultScreenId='absent','visual.routing.defaultScreenId'],
    ['missing-phase',v=>v.routing.phases=[{phase:'planning',screenId:'absent'}],'visual.routing.phases.screenId'],
    ['missing-condition',v=>v.routing.conditions=[{label:'Missing',when:{op:'always'},screenId:'absent'}],'visual.routing.conditions.screenId'],
    ['not-object',v=>v.routing=true,'visual.routing']
  ];
  for(const [name,mutate,error]of cases){
    const variant=structuredClone(original);mutate(variant.content.project.visual);await writeFile(gameFile,JSON.stringify(variant));
    const path=resolve(root,name+'.json');
    const child=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',save,'--frames','2','--report',path,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:10000});
    assert.equal(child.status,1,name);assert.ok(JSON.parse(await readFile(path)).error.includes(error),name);
  }
  await writeFile(gameFile,JSON.stringify(original));
  console.log('Single screen evidence: '+root);
});

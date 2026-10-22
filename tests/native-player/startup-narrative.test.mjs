import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp, readFile, writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('new game settles setup nodes, stops at the first choice, and restores the opening', {timeout:90000}, async()=>{
  const projectRoot=resolve('examples/genre-suite/roster');
  const built=await buildSnapshot(await loadProject(projectRoot));
  assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const content=structuredClone(built.snapshot);
  const protagonist=content.project.protagonistId;
  content.events=[{formatVersion:'0.1.0',id:'opening',label:'Opening',trigger:'period-start',
    window:{fromPeriod:0,throughPeriod:0},requirements:{op:'always'},priority:100,entryNodeId:'scene',nodes:[
      {id:'scene',type:'scene',scene:{portraits:[]},next:'question'},
      {id:'question',type:'choice',prompt:'Choose a beginning.',choices:[
        {id:'begin',text:'Begin together',requirements:{op:'always'},effects:[],next:'first'}]},
      {id:'first',type:'dialogue',speakerId:protagonist,text:'The first spoken line.',next:'end'},
      {id:'end',type:'end'}]}];
  content.project.visual={...content.project.visual,routing:{defaultScreenId:'home',useLocationScreens:false,phases:[
    {phase:'title',screenId:'title'},{phase:'dialogue',screenId:'story'},{phase:'choice',screenId:'story'}]},screens:[
    {id:'title',label:'Title',elements:[{id:'start',kind:'button',x:40,y:40,width:220,height:60,text:'Start',action:'start'}]},
    {id:'home',label:'Home',elements:[]},{id:'schedule',label:'Schedule',elements:[]},{id:'ending',label:'Ending',elements:[]},
    {id:'story',label:'Story',elements:[
      {id:'dialogue',kind:'dialogue',x:40,y:40,width:1100,height:150},
      {id:'choices',kind:'choices',x:40,y:220,width:1100,height:260},
      {id:'next',kind:'button',x:40,y:540,width:220,height:60,action:'advance',text:'Next'}]}]};
  const root=await mkdtemp(resolve('.cache/startup-narrative-')),game=resolve(root,'game');
  const runtime=resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app');
  await exportGame({content,projectRoot,destination:game,target:'windows',runtimes:{windows:runtime}});
  const save=resolve(root,'save.json');
  async function play(name,steps){
    const script=resolve(root,name+'-steps.json'),reportFile=resolve(root,name+'-report.json');
    await writeFile(script,JSON.stringify(steps.flatMap(s=>s.click||s.key?[s,{waitFrames:2}]:[s])));
    const child=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',save,'--automation',script,'--report',reportFile,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:60000});
    const report=JSON.parse(await readFile(reportFile));assert.equal(child.status,0,JSON.stringify(report.error??child.error));
    assert.equal(report.ok,true);return id=>report.steps.find(s=>s.command.checkpoint===id).ui;
  }
  const at=await play('first',[{click:'start'},{checkpoint:'choice'},{key:'F5'},{checkpoint:'saved'},
    {click:'choose-begin'},{checkpoint:'dialogue'},{key:'F9'},{checkpoint:'loaded'}]);
  assert.equal(at('choice').state.activeNarrative.nodeId,'question');
  assert.equal(at('choice').state.stateRevision,1,'Only the noninteractive scene is advanced');
  assert.equal(at('choice').screenId,'story');
  assert.equal(at('dialogue').state.activeNarrative.nodeId,'first');
  assert.deepEqual(at('loaded').state,at('saved').state);
  const fresh=await play('fresh',[{key:'F9'},{checkpoint:'loaded'}]);
  assert.deepEqual(fresh('loaded').state,at('saved').state);
  console.log('Opening narrative evidence: '+root);
});

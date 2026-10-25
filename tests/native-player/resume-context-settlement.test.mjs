import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,rm,writeFile} from 'node:fs/promises';
import {join,resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject,buildSnapshot} from '../../dist/packages/project-service/index.js';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/index.js';
import {encodeSave} from '../../dist/packages/runtime-service/save.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';

const flag='garden-visited';
const text=(id,extra={})=>({id,kind:'text',x:32,y:48,width:1100,height:180,text:id,...extra});
const button=(id,action,x)=>text(id,{kind:'button',x,y:620,width:230,height:60,action,autoLabel:false});
const at=(report,label)=>report.steps.find(step=>step.command.checkpoint===label)?.ui;

test('automatic reminders use settled restore conditions when an effect adds or removes a selected note',{timeout:180000},async()=>{
  await mkdir('.cache/native-resume-context',{recursive:true});
  const out=await mkdtemp(resolve('.cache/native-resume-context/settlement-')),results=[];
  for(const targetValue of [true,false]){
    const directory=join(out,targetValue?'added':'removed'),root=join(directory,'project');
    await cp(resolve('examples/my-first-raising-game'),root,{recursive:true});
    await rm(join(root,'events'),{recursive:true,force:true});await mkdir(join(root,'events'));
    await rm(join(root,'scenarios'),{recursive:true,force:true});
    const project=JSON.parse(await readFile(join(root,'project.json'),'utf8'));
    project.initialState.flags[flag]=!targetValue;
    project.visual={width:1280,height:720,readingHistory:{},resumeContext:{screenId:'notes',elementIds:['current'],showAfterLoad:true},routing:{defaultScreenId:'schedule',phases:[{phase:'title',screenId:'title'},{phase:'dialogue',screenId:'story'},{phase:'choice',screenId:'story'},{phase:'finished',screenId:'ending'}]},screens:[
      {id:'title',label:'Title',elements:[text('intro'),button('load','load',32)]},
      {id:'schedule',label:'Plan',elements:[text('schedule',{kind:'schedule'})]},
      {id:'story',label:'Story',elements:[text('story',{kind:'dialogue'}),button('reminders','resumeContext',32),button('next','advance',280)]},
      {id:'ending',label:'Ending',elements:[text('ending',{binding:'ending'})]},
      {id:'notes',label:'Current notes',elements:[text('current',{text:'You have a current garden commitment.',visibleCondition:{op:'flag',flagId:flag,equals:true}})]}
    ]};
    const event={formatVersion:'0.1.0',id:'settlement',label:'Restore settlement',trigger:'period-start',priority:100,window:{fromPeriod:0,throughPeriod:0},requirements:{op:'always'},entryNodeId:'update',nodes:[
      {id:'update',type:'effect',effects:[{op:'setFlag',flagId:flag,value:targetValue}],next:'line'},
      {id:'line',type:'dialogue',speakerId:project.protagonistId,text:'This line follows automatic settlement.',next:'end'},{id:'end',type:'end'}]};
    await writeFile(join(root,'project.json'),JSON.stringify(project));await writeFile(join(root,'events/settlement.json'),JSON.stringify(event));
    const built=await buildSnapshot(await loadProject(root));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
    const content=built.snapshot,initial=createSession(content,42,{mode:'full'});assert.equal(initial.ok,true);
    assert.equal(initial.session.state.activeNarrative.nodeId,'update');assert.equal(initial.session.state.flags[flag],!targetValue);
    // A valid portable save can stop before automatic narrative work. Restore must
    // decide whether to show notes from the state actually presented afterward.
    const saved=await encodeSave(content,initial.session);assert.equal(saved.ok,true);
    const settled=dispatchSession(content,initial.session,{type:'advanceNarrative'},initial.session.state.stateRevision);assert.equal(settled.ok,true);
    assert.equal(settled.session.state.activeNarrative.nodeId,'line');assert.equal(settled.session.state.flags[flag],targetValue);
    const save=join(directory,'save.json');await writeFile(save,saved.text);
    const target=desktopTarget(),destination=join(directory,'export'),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??'dist/native-player-app');
    await exportGame({projectRoot:root,content,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
    const steps=[{key:'F9'},{waitFrames:4},{checkpoint:'restored'}];
    if(targetValue)steps.push({click:'engine-resume-context-close'},{waitFrames:3},{checkpoint:'returned'});
    else steps.push({click:'reminders'},{waitFrames:3},{checkpoint:'manual'},{click:'engine-resume-context-close'},{waitFrames:3},{checkpoint:'returned'});
    const script=join(directory,'steps.json'),reportFile=join(directory,'report.json');await writeFile(script,JSON.stringify(steps));
    const run=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',save,'--automation',script,'--report',reportFile,'--report-checkpoints-only','--locale','en','--hidden'],{encoding:'utf8',windowsHide:true,timeout:60000,killSignal:'SIGKILL'});
    await writeFile(join(directory,'process.txt'),(run.stdout??'')+'\n'+(run.stderr??''));
    assert.equal(run.status,0,run.error?.message??run.stderr);assert.doesNotMatch((run.stdout??'')+'\n'+(run.stderr??''),/Syntax error parsing property declaration|Invalid property declaration/);
    const report=JSON.parse(await readFile(reportFile,'utf8'));assert.equal(report.ok,true,JSON.stringify(report.error));
    const restored=at(report,'restored');assert.deepEqual(restored.state,settled.session.state);assert.deepEqual(restored.commands,settled.session.commands);
    assert.equal(restored.resumeContext.open,targetValue,'Automatic visibility follows the settled selected note, not its saved pre-effect value');
    if(targetValue){assert.equal(restored.resumeContext.projection.entries.length,1);assert.equal(restored.resumeContext.projection.entries[0].elementId,'current');}
    else {const manual=at(report,'manual');assert.equal(manual.resumeContext.open,true);assert.deepEqual(manual.resumeContext.projection.entries,[]);assert.match(manual.texts['engine-resume-context-panel'],/no reminders/);}
    const returned=at(report,'returned');assert.equal(returned.resumeContext.open,false);assert.deepEqual(returned.state,settled.session.state);assert.deepEqual(returned.commands,settled.session.commands);assert.equal(await readFile(save,'utf8'),saved.text);
    results.push({targetValue,automaticOpen:restored.resumeContext.open,exactSettledState:true,savedBytesUnchanged:true});
  }
  await writeFile(join(out,'result.json'),JSON.stringify({ok:true,results},null,2));
});

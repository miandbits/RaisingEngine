import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve,basename} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {createRun,dispatchCommand} from '../../dist/packages/core/run.js';

// Opt in with the current authored production project. Copy it before loading/exporting;
// no test command, generated bundle, or save is written into the designer's project.
test('production expedition movement, battle and completed return survive native process restarts',
  {skip:!process.env.RAISING_EXPEDITION_PROJECT,timeout:180000},async t=>{
  await mkdir(resolve('.cache/native-expedition-production'),{recursive:true});
  const root=await mkdtemp(resolve('.cache/native-expedition-production/run-'));
  const projectRoot=resolve(root,'project'),destination=resolve(root,'game'),save=resolve(root,'save.json');
  await cp(resolve(process.env.RAISING_EXPEDITION_PROJECT),projectRoot,{recursive:true,
    filter:file=>!['.studio','.playtest','saves'].includes(basename(file))});
  const built=await buildSnapshot(await loadProject(projectRoot));
  assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const content=built.snapshot;
  await exportGame({content,projectRoot,destination,target:'windows',runtimes:{
    windows:resolve(process.env.NATIVE_PLAYER_OUT??'dist/native-player-app'),web:resolve('dist/web')}});
  t.diagnostic(`Evidence: ${root}`);
  const origin={seed:42,mode:'full',initialOverrides:{attributes:{},flags:{},relationships:{}}};
  let state=createRun(content,origin.seed,origin).state,commands=[];
  const expected=new Map();
  function send(command){
    const result=dispatchCommand(content,state,command,state.stateRevision);
    assert.equal(result.ok,true,JSON.stringify(result.error));state=result.state;commands.push(command);
  }
  function settle(){
    for(let i=0;i<100;i++){
      if(state.phase==='period-end'){send({type:'resolvePeriodEnd'});continue;}
      const cursor=state.activeNarrative;
      if(cursor){
        const story=(cursor.kind==='event'?content.events:content.endings).find(x=>x.id===cursor.id);
        const node=story.nodes.find(x=>x.id===cursor.nodeId);
        if(!['dialogue','choice'].includes(node.type)){send({type:'advanceNarrative'});continue;}
      }
      return;
    }
    assert.fail('automatic narrative settling exceeded its test limit');
  }
  const mark=(steps,name)=>{expected.set(name,structuredClone(state));steps.push({checkpoint:name});};
  const branch=()=>structuredClone({state,commands});
  const restore=value=>{state=structuredClone(value.state);commands=structuredClone(value.commands);};
  const move=(steps,nodeId)=>{steps.push({click:`move-expedition-${nodeId}`});send({type:'moveExpedition',nodeId});};
  const attack=steps=>{steps.push({click:'battle-attack'});send({type:'battleAction',action:'attack'});};
  async function run(name,steps){
    const input=resolve(root,`${name}-steps.json`),output=resolve(root,`${name}-report.json`);
    await writeFile(input,JSON.stringify(steps));
    const child=spawnSync(resolve(destination,'RaisingPlayer.exe'),['--bundle',destination,'--save',save,
      '--seed','42','--hidden','--automation',input,'--report',output],{encoding:'utf8',timeout:120000,windowsHide:true});
    assert.equal(child.status,0,`${child.error?.message??child.stderr}\n${output}`);
    const report=JSON.parse(await readFile(output,'utf8'));assert.equal(report.ok,true);
    for(const step of report.steps)if(step.command.checkpoint){
      assert.deepEqual(step.ui.state,expected.get(step.command.checkpoint),`${name}: ${step.command.checkpoint}`);
      if(['expedition','battle'].includes(step.ui.state.phase))assert.equal(step.ui.screenId,step.ui.state.phase);
    }
    const saved=JSON.parse(await readFile(save,'utf8'));
    assert.deepEqual(saved.state,state,`${name}: saved reference state`);
    assert.deepEqual(saved.commands,commands,`${name}: exact command history`);
    await writeFile(resolve(root,`${name}-save.json`),JSON.stringify(saved,null,2));
    return report;
  }

  const first=[{click:'start'},{click:'go-schedule'},
    {click:'plan-group-adventure'},{click:'plan-activity-adventure-east'},
    {click:'plan-group-rest'},{click:'plan-activity-rest'},
    {click:'plan-group-rest'},{click:'plan-activity-rest'},{click:'plan-start'}];
  send({type:'navigate',screenId:'schedule'});
  send({type:'setSchedule',activityIds:['adventure-east','rest','rest']});send({type:'executeActivity'});
  assert.equal(state.phase,'expedition');mark(first,'entrance');
  move(first,'east-crossroads');mark(first,'crossroads');first.push({key:'F5'});const crossroads=branch();
  move(first,'east-lair');mark(first,'battle-entry');first.push({key:'F9'});restore(crossroads);mark(first,'crossroads-loaded');
  move(first,'east-lair');attack(first);assert.equal(state.phase,'battle');
  mark(first,'battle-after-hit');first.push({key:'F5'});
  await run('movement-and-battle',first);

  const second=[{key:'F9'}];mark(second,'battle-restarted');
  for(let turns=0;state.phase==='battle'&&turns<100;turns++)attack(second);
  assert.equal(state.phase,'expedition');assert.equal(state.systems.lastBattle.outcome,'victory');
  const victory=branch();mark(second,'victory');second.push({key:'F5'});
  move(second,'east-crossroads');move(second,'east-lair');mark(second,'cleared-revisit');
  assert.deepEqual(state.gameplay.inventory,victory.state.gameplay.inventory,'cleared encounter grants no second item');
  assert.deepEqual(state.attributes,victory.state.attributes,'cleared encounter grants no second numeric reward');
  second.push({key:'F9'});restore(victory);mark(second,'victory-loaded');
  move(second,'east-crossroads');move(second,'east-entrance');
  second.push({click:'return-expedition'});send({type:'returnFromExpedition'});
  assert.equal(state.phase,'activity-result');assert.equal(state.periodIndex,0);
  assert.deepEqual(state.systems.lastExpedition,{expeditionId:'east',outcome:'completed',clearedNodes:['east-lair']});
  assert.equal(state.systems.expedition,null);assert.equal(state.systems.battle,null);
  assert.deepEqual(state.presentation,victory.state.systems.expedition.returnPresentation);
  mark(second,'returned');second.push({key:'F5'});const returned=branch();
  second.push({click:'advance'});send({type:'acknowledgeResult'});settle();mark(second,'acknowledged');
  second.push({key:'F9'});restore(returned);mark(second,'returned-loaded');
  second.push({click:'advance'});send({type:'acknowledgeResult'});settle();mark(second,'acknowledged-again');second.push({key:'F5'});
  await run('victory-and-return',second);
  const third=[{key:'F9'}];mark(third,'post-return-restarted');
  for(let i=0;state.phase==='narrative'&&i<100;i++){
    const cursor=state.activeNarrative,story=(cursor.kind==='event'?content.events:content.endings).find(x=>x.id===cursor.id);
    const node=story.nodes.find(x=>x.id===cursor.nodeId);
    assert.equal(node.type,'dialogue','this production fixture has no player choice before its next period');
    third.push({click:'advance'});send({type:'advanceNarrative'});settle();
  }
  assert.equal(state.periodIndex,1);assert.equal(state.phase,'ready');mark(third,'ready-after-return');third.push({key:'F5'});
  await run('ready-restart',third);
  await writeFile(resolve(root,'coverage.json'),JSON.stringify({sourceProject:resolve(process.env.RAISING_EXPEDITION_PROJECT),
    contentFingerprint:content.contentFingerprint,mode:'hidden-native-rml-automation',desktopInput:false,
    checkpoints:[...expected.keys()],reference:'TypeScript core state and full command history',
    finalPeriodIndex:state.periodIndex,finalOutcome:state.systems.lastExpedition.outcome},null,2));
});

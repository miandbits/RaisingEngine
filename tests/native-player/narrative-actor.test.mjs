import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {nativeCreateRun,nativeDispatchCommand} from '../../dist/packages/native-bridge/index.js';

test('story participant follows actor-scoped events, differs from speaker and survives save/load',{timeout:90000},async()=>{
  const projectRoot=resolve('examples/genre-suite/roster'),built=await buildSnapshot(await loadProject(projectRoot));assert.equal(built.ok,true);
  const content=structuredClone(built.snapshot),activity=content.activities[0],members=content.project.roster.members;
  activity.requirements={op:'always'};activity.costs=[];activity.baseEffects=[];delete activity.successCheck;
  for(const member of members)member.activityId=activity.id;
  content.events=[{formatVersion:'0.1.0',id:'meeting',label:'Meeting',scope:'actor',trigger:'after-activity',activityId:activity.id,window:{fromPeriod:0,throughPeriod:0},priority:1,requirements:{op:'always'},entryNodeId:'first',nodes:[
    {id:'first',type:'dialogue',speakerId:members[0].characterId,text:'A shared lesson.',next:'question'},
    {id:'question',type:'choice',prompt:'Listen?',choices:[{id:'ok',text:'Yes',requirements:{op:'always'},effects:[],next:'end'}]},
    {id:'end',type:'end'}]}];
  content.project.visual={...content.project.visual,routing:{defaultScreenId:'lesson',phases:[],useLocationScreens:false},screens:[{id:'lesson',label:'Lesson',elements:[
    {id:'start',kind:'button',x:20,y:20,width:200,height:50,action:'start',visibleWhen:'title'},
    {id:'actor-name',kind:'text',x:20,y:90,width:400,height:45,binding:'narrative-actor'},
    {id:'speaker-name',kind:'text',x:460,y:90,width:400,height:45,binding:'speaker'},
    {id:'schedule',kind:'schedule',x:20,y:160,width:1100,height:220,visibleWhen:'planning'},
    {id:'dialogue',kind:'dialogue',x:20,y:400,width:1100,height:70},
    {id:'choices',kind:'choices',x:20,y:480,width:900,height:80},
    {id:'advance',kind:'button',x:940,y:600,width:240,height:50,action:'advance'}]}]};
  for(const location of content.project.gameplay.locations)if(location.screenId)location.screenId='lesson';
  const root=await mkdtemp(resolve('.cache/narrative-actor-')),game=resolve(root,'game'),runtime=resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app');
  const old=resolve(root,'unsupported');await mkdir(old);const caps=JSON.parse(await readFile(resolve(runtime,'capabilities.json')));caps.visualFeatures=caps.visualFeatures.filter(f=>f!=='narrative-actor-binding-v1');await writeFile(resolve(old,'capabilities.json'),JSON.stringify(caps));await writeFile(resolve(old,'RaisingPlayer.exe'),'fixture');
  await assert.rejects(exportGame({content,projectRoot,target:'windows',destination:resolve(root,'rejected'),runtimes:{windows:old}}),/narrative-actor-binding-v1/);
  await exportGame({content,projectRoot,target:'windows',destination:game,runtimes:{windows:runtime}});
  let state=nativeCreateRun(content,42,{mode:'full'}).state;const steps=[{click:'start'},{waitFrames:2},{checkpoint:'initial'}],expected=[];
  for(let guard=0;guard<30&&state.periodIndex===0;++guard){
    let command;
    if(state.phase==='planning'){command={type:'assignActivities',assignments:Object.fromEntries(members.map(m=>[m.characterId,activity.id]))};steps.push({click:'roster-assign'});}
    else if(state.phase==='ready'){command={type:'executeActivity'};steps.push({click:'advance'});}
    else if(state.phase==='narrative'){
      const node=content.events[0].nodes.find(n=>n.id===state.activeNarrative.nodeId),id='actor-'+guard;
      if(node.type==='dialogue'||node.type==='choice'){
        const actorId=state.activeNarrative.actorId;expected.push({id,actorId,node:node.id});steps.push({checkpoint:id},{key:'F5'},{key:'F9'},{checkpoint:id+'-restored'});
        if(node.type==='choice'){command={type:'choose',choiceId:'ok'};steps.push({click:'choose-ok'});}else{command={type:'advanceNarrative'};steps.push({click:'advance'});}
      }else command={type:'advanceNarrative'};
    }else if(state.phase==='activity-result'){command={type:'acknowledgeResult'};steps.push({click:'advance'});}
    else if(state.phase==='period-end')command={type:'resolvePeriodEnd'};
    else throw Error(state.phase);
    const next=nativeDispatchCommand(content,state,command,state.stateRevision);assert.equal(next.ok,true,JSON.stringify(next.error));state=next.state;
    if(steps.at(-1)?.click)steps.push({waitFrames:2});
  }
  steps.push({checkpoint:'outside'});const script=resolve(root,'steps.json'),reportFile=resolve(root,'report.json');await writeFile(script,JSON.stringify(steps));
  const child=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',resolve(root,'save.json'),'--automation',script,'--report',reportFile,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:60000});
  const report=JSON.parse(await readFile(reportFile));assert.equal(child.status,0,JSON.stringify(report.error));
  const at=id=>report.steps.find(s=>s.command.checkpoint===id).ui;
  assert.equal(at('initial').texts['actor-name'],'');assert.equal(at('outside').texts['actor-name'],'');
  assert.equal(new Set(expected.map(e=>e.actorId)).size,2);
  for(const e of expected){assert.equal(at(e.id).texts['actor-name'],content.characters.find(c=>c.id===e.actorId).name);assert.deepEqual(at(e.id).state,at(e.id+'-restored').state);assert.equal(at(e.id+'-restored').texts['actor-name'],at(e.id).texts['actor-name']);
    if(e.node==='first')assert.equal(at(e.id).texts['speaker-name'],content.characters.find(c=>c.id===members[0].characterId).name);
  }
  console.log('Story participant evidence: '+root);
});

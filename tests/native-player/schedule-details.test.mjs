import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';

const at=(r,name)=>r.steps.find(s=>s.command.checkpoint===name).ui;
test('native detail planner keeps complete authored clauses in one region and preserves draft/command semantics',{timeout:120000},async()=>{
  const project=resolve('examples/my-first-raising-game'),built=await buildSnapshot(await loadProject(project));assert.equal(built.ok,true);
  const content=structuredClone(built.snapshot);content.project.periodsPerMonth=4;
  const screen=content.project.visual.screens.find(s=>s.id==='schedule'),planner=screen.elements.find(e=>e.kind==='schedule');
  Object.assign(planner,{x:40,y:100,width:1180,height:490,showActivityDescription:true,showActivityRules:true,color:'#273f3b'});
  screen.elements=screen.elements.filter(e=>e.kind==='schedule'||e.id==='advance');
  screen.elements.push({...screen.elements.find(e=>e.id==='advance'),id:'authored',x:640,width:310,autoLabel:false,text:'Authored continue'});
  const study=content.activities.find(a=>a.id==='study');study.description='Requirements: Keep enough tuition money, then complete the recovery requirement. '.repeat(45)+'FINAL REQUIREMENT CLAUSE.';
  await mkdir('.cache/native-schedule-details',{recursive:true});
  const out=await mkdtemp(resolve('.cache/native-schedule-details/run-')),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app'),target=desktopTarget(),destination=join(out,'game');
  await exportGame({content,projectRoot:project,destination,target,runtimes:{windows:runtime,macos:runtime,web:runtime}});
  const steps=[{clickWindow:'start'},{select:'plan-0',value:'study'},{select:'plan-1',value:'work'},{select:'plan-2',value:'rest'},{select:'plan-3',value:'walk'},{clickWindow:'plan-review-0'},{checkpoint:'before',screenshot:join(out,'details.png')},{wheelWindow:{x:600,y:430,deltaY:-8}},{checkpoint:'scrolled'},{clickWindow:'plan-review-0'},...Array.from({length:8},()=>({key:'Tab'})),{checkpoint:'detail-focus'},{key:'ArrowDown'},{checkpoint:'keyboard-scrolled'},{clickWindow:'plan-review-2'},{checkpoint:'reviewed'},{key:'Tab'},{checkpoint:'keyboard'},{clickWindow:'plan-1'},{checkpoint:'dropdown'},{clickWindow:'plan-1-option-rest'},{checkpoint:'changed'},{clickWindow:'advance'},{checkpoint:'confirmed'},{clickWindow:'plan-1'},{clickWindow:'plan-1-option-study'},{checkpoint:'revised'},{clickWindow:'advance'},{checkpoint:'reconfirmed'},{clickWindow:'advance'},{checkpoint:'executed'}];
  const input=join(out,'steps.json'),report=join(out,'report.json');await writeFile(input,JSON.stringify(steps));
  const child=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',join(out,'save.json'),'--automation',input,'--report',report,'--report-checkpoints-only','--hidden'],{encoding:'utf8',timeout:90000});
  assert.equal(child.status,0,child.stderr||child.error?.message);const result=JSON.parse(await readFile(report,'utf8'));assert.equal(result.ok,true,result.error);
  const before=at(result,'before'),details=before.nodes.filter(n=>n.kind==='schedule-details');assert.equal(details.length,1);
  assert.match(before.texts.schedule,/FINAL REQUIREMENT CLAUSE/);assert.match(before.texts.schedule,/Costs|Cost|비용/);
  assert.equal((before.texts.schedule.match(/id="plan-description-/g)??[]).length,1,'Only one activity description');
  assert.ok(details[0].clientWidth>1100);assert.ok(details[0].scrollHeight>details[0].clientHeight);assert.ok(details[0].scrollWidth<=details[0].clientWidth+1);
  assert.ok(at(result,'scrolled').nodes.find(n=>n.kind==='schedule-details').scrollTop>0,'Wheel reaches the whole detail');
  assert.equal(at(result,'detail-focus').focusId,'plan-details-schedule');assert.ok(at(result,'keyboard-scrolled').nodes.find(n=>n.kind==='schedule-details').scrollTop>at(result,'detail-focus').nodes.find(n=>n.kind==='schedule-details').scrollTop,'Keyboard scrolls the focused pane without changing periods');
  for(const name of ['reviewed','changed']){assert.deepEqual(at(result,name).state,before.state);assert.deepEqual(at(result,name).commands,before.commands);}
  assert.deepEqual(at(result,'reviewed').scheduleDraft.activities,before.scheduleDraft.activities);assert.equal(at(result,'keyboard').focusId,'plan-2','Review button leads to that period dropdown on Tab');
  assert.ok(at(result,'dropdown').options.filter(o=>o.id.startsWith('plan-1-option-')).every(o=>o.visible),'Dropdown remains visible above the detail pane');
  assert.equal(at(result,'changed').scheduleDraft.activities[1],'rest');assert.equal(at(result,'changed').scheduleDraft.slot,1);
  assert.deepEqual(at(result,'confirmed').state.schedule,['study','rest','rest','walk']);assert.deepEqual(at(result,'confirmed').commands.slice(before.commands.length),[{type:'setSchedule',activityIds:['study','rest','rest','walk']}]);
  const label=name=>at(result,name).elements.find(e=>e.id==='advance').label;
  assert.equal(label('before'),'Confirm schedule');assert.equal(label('confirmed'),'Start activity');assert.equal(label('revised'),'Confirm schedule');assert.equal(label('reconfirmed'),'Start activity');assert.equal(label('executed'),'Continue');
  assert.deepEqual(at(result,'revised').commands,at(result,'confirmed').commands);assert.deepEqual(at(result,'revised').state,at(result,'confirmed').state);
  assert.deepEqual(at(result,'reconfirmed').commands.slice(at(result,'confirmed').commands.length),[{type:'setSchedule',activityIds:['study','study','rest','walk']}]);
  assert.deepEqual(at(result,'executed').commands.slice(at(result,'reconfirmed').commands.length),[{type:'executeActivity'}]);
  for(const name of ['before','confirmed','revised','reconfirmed','executed'])assert.equal(at(result,name).elements.find(e=>e.id==='authored').label,'Authored continue','Explicit author label stays unchanged');
  await writeFile(resolve('.cache/native-schedule-details/latest.json'),JSON.stringify({ok:true,out},null,2));console.log('Schedule details evidence: '+out);
});

test('native detail planner supports one and twelve periods, horizontal review and disabled execution state',{timeout:120000},async()=>{
  await mkdir('.cache/native-schedule-details',{recursive:true});
  for(const count of [1,12]){
    const project=resolve('examples/my-first-raising-game'),built=await buildSnapshot(await loadProject(project));assert.equal(built.ok,true);
    const content=structuredClone(built.snapshot);content.project.periodsPerMonth=count;
    const screen=content.project.visual.screens.find(s=>s.id==='schedule'),planner=screen.elements.find(e=>e.kind==='schedule');
    Object.assign(planner,{x:40,y:100,width:540,height:490,showActivityDescription:true,showActivityRules:true,color:'#273f3b'});delete planner.visibleWhen;
    screen.elements=screen.elements.filter(e=>e.kind==='schedule'||e.id==='advance');
    const out=await mkdtemp(resolve('.cache/native-schedule-details/run-'+count+'-')),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app'),target=desktopTarget(),destination=join(out,'game');
    await exportGame({content,projectRoot:project,destination,target,runtimes:{windows:runtime,macos:runtime,web:runtime}});
    const last=count-1,steps=[{clickWindow:'start'},{checkpoint:'initial'},
      {scrollIntoView:'plan-review-'+last},{clickWindow:'plan-review-'+last},{checkpoint:'last'},
      {clickWindow:'plan-'+last},{checkpoint:'dropdown'},{clickWindow:'plan-'+last+'-option-rest'},{checkpoint:'edited'},
      {clickWindow:'advance'},{checkpoint:'confirmed'},{clickWindow:'advance'},{checkpoint:'result'},
      {scrollIntoView:'plan-review-'+last},{clickWindow:'plan-review-'+last},{clickWindow:'plan-'+last},{checkpoint:'disabled',screenshot:join(out,'disabled.png')}];
    const input=join(out,'steps.json'),report=join(out,'report.json');await writeFile(input,JSON.stringify(steps));
    const child=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',join(out,'save.json'),'--automation',input,'--report',report,'--report-checkpoints-only','--hidden'],{encoding:'utf8',timeout:60000});
    assert.equal(child.status,0,child.stderr||child.error?.message);const result=JSON.parse(await readFile(report,'utf8'));assert.equal(result.ok,true,result.error);
    const initial=at(result,'initial'),lastView=at(result,'last'),review=lastView.nodes.find(n=>n.id==='plan-review-'+last);
    assert.equal(review.clipped,false);assert.equal(review.clickableAtCenter,true);assert.equal(lastView.scheduleDraft.slot,last);assert.deepEqual(lastView.commands,initial.commands);assert.deepEqual(lastView.scheduleDraft.activities,initial.scheduleDraft.activities);
    assert.ok(at(result,'dropdown').options.filter(o=>o.id.startsWith('plan-'+last+'-option-')).every(o=>o.visible),'All dropdown options remain reachable after horizontal scrolling');
    assert.deepEqual(at(result,'confirmed').state.schedule,Array(count).fill('rest'));
    assert.deepEqual(at(result,'confirmed').commands,[{type:'setSchedule',activityIds:Array(count).fill('rest')}]);
    assert.equal(at(result,'result').state.phase,'activity-result');assert.deepEqual(at(result,'disabled').state,at(result,'result').state);assert.deepEqual(at(result,'disabled').commands,at(result,'result').commands);
    assert.ok(at(result,'disabled').options.filter(o=>o.id.startsWith('plan-'+last+'-option-')).every(o=>!o.visible),'Execution phase keeps activity dropdown disabled');
  }
});

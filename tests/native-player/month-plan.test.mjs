import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,readdir,rm,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('real native UI selects four activities and completes the month without further clicks, with save/load', {timeout:120000},async()=>{
 await mkdir('.cache/native-month-plan',{recursive:true});const root=await mkdtemp(resolve('.cache/native-month-plan/run-')),project=resolve(root,'project');
 await cp(resolve('examples/academy-journey'),project,{recursive:true});
 for(const dir of ['events','scenarios'])for(const name of await readdir(resolve(project,dir)))if(name.endsWith('.json'))await rm(resolve(project,dir,name));
 // Fixture excludes story events so only automatic monthly scheduling is under test.
 const p=JSON.parse(await readFile(resolve(project,'project.json'),'utf8'));p.periodsPerMonth=4;p.schedulePlayback='continuous';
 p.gameplay.ruleCalendar={daysPerMonth:28};p.gameplay.interactions=[];
 p.visual.timelines=[{id:'week',name:'Week',clips:[{id:'day',durationMs:50,text:'{activity} · {day}일'}]}];
 const ids=['study','work','rest','expedition'];p.visual.activityTimelines=ids.map(activityId=>({activityId,timelineId:'week',repeat:'logicalDays'}));
 const screen=p.visual.screens.find(x=>x.id==='schedule');screen.elements=screen.elements.filter(e=>e.kind!=='schedule');
 screen.elements.push({id:'month-calendar',kind:'schedule',scheduleLayout:'calendar',x:20,y:100,width:500,height:430},{id:'menu',kind:'schedule',scheduleLayout:'menu',x:550,y:100,width:500,height:430,scheduleGroups:[{id:'all',label:'활동',activityIds:ids}]});
 await writeFile(resolve(project,'project.json'),JSON.stringify(p));
 const built=await buildSnapshot(await loadProject(project));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
 const game=resolve(root,'game');await exportGame({content:built.snapshot,projectRoot:project,destination:game,target:'windows',runtimes:{windows:resolve(process.env.NATIVE_PLAYER_OUT??'dist/native-player-month-plan'),web:resolve('dist/web')}});
 const steps=[{click:'start'},{click:'schedule-link'}];for(const id of ids)steps.push({click:'plan-group-all'},{click:'plan-activity-'+id});
 steps.push({checkpoint:'four-selected',screenshot:resolve(root,'four-selected.png')},{click:'plan-start'},{checkpoint:'started'},
  {click:'timeline-pause'},{key:'F5'},{checkpoint:'saved'},{timelineRealtime:true},{waitFrames:50},{checkpoint:'paused'},
  {click:'timeline-pause'},{waitFrames:1400},{checkpoint:'month-done',screenshot:resolve(root,'month-done.png')},
  {key:'F9'},{checkpoint:'restored'},{waitFrames:50},{checkpoint:'restored-paused'});
 await writeFile(resolve(root,'steps.json'),JSON.stringify(steps));const save=resolve(root,'save.json'),report=resolve(root,'report.json');
 const child=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',save,'--automation',resolve(root,'steps.json'),'--report',report,'--hidden'],{encoding:'utf8',timeout:90000,windowsHide:true});assert.equal(child.status,0,child.stderr);
 const result=JSON.parse(await readFile(report,'utf8')),ui=tag=>result.steps.find(x=>x.command.checkpoint===tag).ui;
 assert.equal(result.ok,true);assert.deepEqual(ui('started').state.schedule,ids);assert.equal(ui('started').timeline.plan.clips.length,7);
 assert.deepEqual(ui('paused').state,ui('saved').state);assert.deepEqual(ui('paused').timeline.cursor,ui('saved').timeline.cursor);
 assert.equal(ui('month-done').state.periodIndex,4);assert.equal(ui('month-done').state.phase,'planning');assert.equal(ui('month-done').state.schedule,null);
 assert.equal(ui('month-done').state.lastActivityResult,null);
 assert.deepEqual(ui('restored').state,ui('saved').state);assert.deepEqual(ui('restored-paused').timeline.cursor,ui('saved').timeline.cursor);
 console.log('Native monthly GUI evidence: '+root);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdtemp,readFile,writeFile,mkdir} from 'node:fs/promises';
import {resolve} from 'node:path';
import {PlayerController} from '../../dist/packages/player-service/controller.js';
import {WebPlayerController} from '../../dist/packages/web-player/controller.js';
import {WasmRuntime} from '../../dist/packages/native-bridge/wasm.js';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import {createRun,dispatchCommand} from '../../dist/packages/core/run.js';
import {nativeCreateRun,nativeDispatchCommand} from '../../dist/packages/native-bridge/index.js';
import {scheduleStep} from '../../dist/packages/player-service/schedule.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

const version=c=>({generation:c.generation,revision:c.session.state.stateRevision});
async function fixture(timeline=false){
 await mkdir('.cache/month-plan-tests',{recursive:true});const root=await mkdtemp(resolve('.cache/month-plan-tests/run-')),project=resolve(root,'project');
 await cp(resolve(timeline?'examples/academy-journey':'examples/academy-demo'),project,{recursive:true});
 const file=resolve(project,'project.json'),p=JSON.parse(await readFile(file,'utf8'));p.periodsPerMonth=4;p.schedulePlayback='continuous';
 if(timeline){p.gameplay.ruleCalendar={daysPerMonth:28};p.visual.timelines=[{id:'week',name:'Week',clips:[{id:'day',durationMs:50,text:'{day}일'}]}];p.visual.activityTimelines=[{activityId:'rest',timelineId:'week',repeat:'logicalDays'}];}
 await writeFile(file,JSON.stringify(p));const desktop=new PlayerController();assert.equal((await desktop.openProject(project)).error,null);
 let saved=null;const web=new WebPlayerController(desktop.content,new WasmRuntime(await createModule()),{read:async()=>saved,write:async text=>{saved=text}},()=>true);
 for(const c of [desktop,web])assert.equal((await c.newRun(null)).error,null);
 return {root,project,desktop,web,content:desktop.content};
}

test('four distinct weekly slots run to next month without per-period confirmation; events wait and saves replay',async()=>{
 const f=await fixture();const plan=['study','work','rest','work'];let eventWaits=0;
 for(const c of [f.desktop,f.web]){
  const original=structuredClone(c.session);assert.ok((await c.act({type:'commitPlan',activityIds:plan.slice(0,3)},version(c))).error);assert.deepEqual(c.session,original);
  assert.equal((await c.act({type:'commitPlan',activityIds:plan},version(c))).error,null);
  for(let n=0;n<60&&c.session.state.periodIndex<4;n++){
   const before=structuredClone(c.session);const auto=scheduleStep(c.session.state,'continuous',c.presentation.view(f.content,c.session.state));
   if(!auto){assert.equal(c.session.state.phase,'narrative');assert.equal((await c.act({type:'scheduleStep'},version(c))).error,null);assert.deepEqual(c.session,before);eventWaits++;const reply=await c.view();const narrative=reply.view?reply.view.narrative:reply.narrative;
    const action=narrative?.node.type==='choice'?{type:'choose',choiceId:narrative.choices.find(x=>x.available!==false&&x.enabled!==false)?.id??narrative.node.choices[0].id}:{type:'continue'};
    assert.equal((await c.act(action,version(c))).error,null);
   }else assert.equal((await c.act({type:'scheduleStep'},version(c))).error,null);
  }
  assert.equal(c.session.state.periodIndex,4);assert.equal(c.session.state.phase,'planning');assert.equal(c.session.state.schedule,null);
  assert.equal(c.session.commands.filter(x=>x.type==='executeActivity').length,4);assert.equal(c.session.commands.filter(x=>x.type==='setSchedule').length,1);
  const stopped=structuredClone(c.session);await c.act({type:'scheduleStep'},version(c));assert.deepEqual(c.session,stopped);
 }
 assert.deepEqual(f.web.session,f.desktop.session);
 assert.ok(eventWaits>0,'the month must exercise a narrative interruption');
 // Replay each native command through the independent TypeScript rules too.
 let reference=createRun(f.content,42,{mode:'full'}).state;for(const command of f.desktop.session.commands){const r=dispatchCommand(f.content,reference,command,reference.stateRevision);assert.equal(r.ok,true,JSON.stringify(r));reference=r.state;}assert.deepEqual(reference,f.desktop.session.state);
 const expected=structuredClone(f.web.session);assert.equal((await f.web.save(version(f.web))).error,null);await f.web.newRun(version(f.web));assert.equal((await f.web.load(version(f.web))).error,null);assert.deepEqual(f.web.session,expected);
});

test('weekly timeline waits while paused, restores cursor, then auto steps; failed costs preserve state',async()=>{
 const f=await fixture(true);
 for(const c of [f.desktop,f.web]){
  await c.act({type:'commitPlan',activityIds:['rest','rest','rest','rest']},version(c));
  assert.equal(c.presentation.view(f.content,c.session.state).plan.clips.length,7);
  await c.act({type:'timelinePause'},version(c));const paused=structuredClone(c.session);await c.act({type:'timelineTick',deltaMs:1000},version(c));await c.act({type:'scheduleStep'},version(c));assert.deepEqual(c.session,paused);
  const file=resolve(f.root,'desktop-save.json');const saved=c===f.desktop?await c.save(file,version(c)):await c.save(version(c));assert.equal(saved.error,null);
  await c.act({type:'timelineSkip'},version(c));
  const loaded=c===f.desktop?await c.load(file,version(c)):await c.load(version(c));assert.equal(loaded.error,null);assert.equal(c.presentation.view(f.content,c.session.state).cursor.paused,true);assert.deepEqual(c.session,paused);
  await c.act({type:'timelineSkip'},version(c));assert.equal((await c.act({type:'scheduleStep'},version(c))).error,null);
 }
 assert.deepEqual(f.desktop.session,f.web.session);
 const created=nativeCreateRun(f.content,42,{mode:'full',initialOverrides:{attributes:{money:0},flags:{},relationships:{}}});assert.equal(created.ok,true);
 const ready=nativeDispatchCommand(f.content,created.state,{type:'setSchedule',activityIds:['study','rest','rest','rest']},created.state.stateRevision);assert.equal(ready.ok,true);
 const fail=nativeDispatchCommand(f.content,ready.state,{type:'executeActivity'},ready.state.stateRevision);assert.equal(fail.ok,false);assert.equal(ready.state.periodIndex,0);
});

test('four-slot content refuses old renderer capability before exporting',async()=>{
 const f=await fixture(true),runtime=resolve(f.root,'old-runtime');await mkdir(runtime);await writeFile(resolve(runtime,'RaisingPlayer.exe'),'test fixture');await writeFile(resolve(runtime,'capabilities.json'),JSON.stringify({formatVersion:'1',imageExtensions:[],audioExtensions:[],visualFeatures:[]}));
 await assert.rejects(exportGame({content:f.content,projectRoot:f.project,destination:resolve(f.root,'export'),target:'windows',runtimes:{windows:runtime,web:runtime}}),/monthly-schedule-v1/);
});

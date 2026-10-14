import test from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import {readFile,writeFile} from 'node:fs/promises';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import {WasmRuntime} from '../../dist/packages/native-bridge/wasm.js';
import {WebPlayerController} from '../../dist/packages/web-player/controller.js';
import {PlayerController} from '../../dist/packages/player-service/controller.js';
import {fixture} from '../gameplay/fixture.mjs';

for(const backend of ['native','wasm'])test(`${backend} timeline pause/save/load and normal/fast/skip leave the committed seed state identical`,async t=>{
  const {root,built}=await fixture(t,p=>{
    p.visual.timelines=[{id:'rest-film',name:'Rest film',clips:[{id:'begin',durationMs:250,text:'{activity} {outcome}',transition:'none'},{id:'result',durationMs:500,text:'{changes}',transition:'fade'}]}];
    p.visual.activityTimelines=[{activityId:'rest',timelineId:'rest-film',repeat:'once'}];
  });assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const runtime=backend==='wasm'?new WasmRuntime(await createModule()):null;const file=path.join(root,'mid-clip-save.json');let saved='';
  const make=async()=>{const c=runtime?new WebPlayerController(built.snapshot,runtime,{read:async()=>saved,write:async text=>{saved=text;}},()=>true):new PlayerController();if(!runtime)assert.equal((await c.openProject(root)).error,null);assert.equal((await c.newRun(null)).error,null);return c;};
  const version=c=>({generation:c.generation,revision:c.session.state.stateRevision});
  const act=async(c,action)=>{const reply=await c.act(action,version(c));assert.equal(reply.error,null);return reply.view;};
  const prepare=async()=>{const c=await make();await act(c,{type:'schedule',activityIds:['rest','rest','rest']});await act(c,{type:'execute'});return c;};
  const c=await prepare(),committed=structuredClone(c.session);let v=await act(c,{type:'timelineTick',deltaMs:100});assert.equal(v.timeline.cursor.elapsedMs,100);assert.deepEqual(c.session,committed);
  v=await act(c,{type:'timelinePause'});assert.equal(v.timeline.cursor.paused,true);await act(c,{type:'timelineTick',deltaMs:600});assert.equal(c.presentation.cursor.elapsedMs,100);
  await act(c,{type:'timelineSpeed'});assert.equal(c.presentation.cursor.speed,4);
  assert.ok((await c.act({type:'continue'},version(c))).error);assert.ok((await c.act({type:'visit',locationId:'city'},version(c))).error);assert.deepEqual(c.session,committed);
  const cursor=structuredClone(c.presentation.cursor);
  assert.equal((await(runtime?c.save(version(c)):c.save(file,version(c)))).error,null);if(!runtime)saved=await readFile(file,'utf8');
  const serialized=JSON.parse(saved);assert.deepEqual(serialized.playerPresentation,{version:1,timeline:cursor});
  await act(c,{type:'timelineSkip'});assert.equal(c.presentation.cursor.completed,true);
  assert.equal((await(runtime?c.load(version(c)):c.load(file,version(c)))).error,null);assert.deepEqual(c.presentation.cursor,cursor);assert.deepEqual(c.session,committed);
  const valid=saved;
  for(const mutate of [s=>{s.playerPresentation.timeline.clipIndex=999;},s=>{s.playerPresentation.timeline.resultRevision++;},s=>{s.playerPresentation.timeline=null;}]){
    const bad=JSON.parse(valid);mutate(bad);saved=JSON.stringify(bad);if(!runtime)await writeFile(file,saved);
    assert.ok((await(runtime?c.load(version(c)):c.load(file,version(c)))).error);assert.deepEqual(c.session,committed);assert.deepEqual(c.presentation.cursor,cursor);
  }
  const old=JSON.parse(valid);delete old.playerPresentation;saved=JSON.stringify(old);if(!runtime)await writeFile(file,saved);
  assert.equal((await(runtime?c.load(version(c)):c.load(file,version(c)))).error,null);assert.equal(c.presentation.cursor.clipIndex,0);assert.equal(c.presentation.cursor.elapsedMs,0);assert.deepEqual(c.session,committed);
  const results=[];
  for(const strategy of ['normal','fast','skip']){
    const run=await prepare(),before=structuredClone(run.session);
    if(strategy==='skip')await act(run,{type:'timelineSkip'});
    else{if(strategy==='fast')await act(run,{type:'timelineSpeed'});for(let i=0;i<10&&!run.presentation.cursor.completed;i++)await act(run,{type:'timelineTick',deltaMs:100});}
    assert.equal(run.presentation.cursor.completed,true);assert.deepEqual(run.session,before,'presentation cannot alter seed, effects or command history');
    await act(run,{type:'continue'});assert.equal(run.presentation.cursor,null);results.push(structuredClone(run.session));
  }
  assert.deepEqual(results[0],results[1]);assert.deepEqual(results[0],results[2]);
});

for(const backend of ['native','wasm'])test(`${backend} reading and pausing across a logical-day boundary preserves the playback cursor`,async t=>{
  const {root,built}=await fixture(t,p=>{
    p.gameplay.ruleCalendar={daysPerMonth:30};
    p.visual.timelines=[{id:'days',name:'Days',clips:[{id:'first',durationMs:250,text:'{day}'},{id:'second',durationMs:500,text:'{day}'}]}];
    p.visual.activityTimelines=[{activityId:'rest',timelineId:'days',repeat:'logicalDays'}];
  });assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const runtime=backend==='wasm'?new WasmRuntime(await createModule()):null;
  const c=runtime?new WebPlayerController(built.snapshot,runtime,{read:async()=>null,write:async()=>{}},()=>true):new PlayerController();
  if(!runtime)assert.equal((await c.openProject(root)).error,null);
  assert.equal((await c.newRun(null)).error,null);
  const act=async action=>{const reply=await c.act(action,{generation:c.generation,revision:c.session.state.stateRevision});assert.equal(reply.error,null);return reply.view;};
  await act({type:'schedule',activityIds:['rest','rest','rest']});await act({type:'execute'});
  const committed=structuredClone(c.session);
  let v=await act({type:'timelineTick',deltaMs:850});assert.equal(v.timeline.cursor.clipIndex,2);assert.equal(v.timeline.cursor.elapsedMs,100);assert.equal(v.timeline.plan.clips[2].day,2);
  const cursor=structuredClone(v.timeline.cursor);
  const current=async()=>runtime?(await c.view()).view:c.view();
  for(let i=0;i<3;i++)assert.deepEqual((await current()).timeline.cursor,cursor);
  v=await act({type:'timelinePause'});assert.deepEqual(v.timeline.cursor,{...cursor,paused:true});
  await act({type:'timelineTick',deltaMs:1000});assert.deepEqual((await current()).timeline.cursor,{...cursor,paused:true});assert.deepEqual(c.session,committed);
});

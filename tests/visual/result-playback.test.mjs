import test from 'node:test';
import assert from 'node:assert/strict';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/session.js';
import {planActivityTimeline,createTimelineCursor,advanceTimeline,resultPlaybackText,validateTimelineCursor} from '../../dist/packages/visual/timeline.js';
import {PlayerPresentation} from '../../dist/packages/player-service/presentation.js';
import {scheduleStep} from '../../dist/packages/player-service/schedule.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';

async function setup(){
  const built=await buildSnapshot(await loadProject('tests/fixtures/projects/last-grandson-polished'));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const content=built.snapshot;let session=createSession(content,42,{mode:'activities-only'}).session;
  for(const command of [{type:'setSchedule',activityIds:['study','rest','work']},{type:'executeActivity'}]){const reply=dispatchSession(content,session,command,session.state.stateRevision);assert.equal(reply.ok,true);session=reply.session;}
  return {content,state:session.state,session};
}

test('inline result interpolates committed costs then effects without changing simulation or RNG',async()=>{
  const {content,state}=await setup(),original=JSON.stringify(state),plan=planActivityTimeline(content,state),start=createTimelineCursor(plan);
  assert.equal(plan.totalDurationMs,5000);assert.match(resultPlaybackText(plan,start),/30 → 30  \(\+0\)/);
  const half=advanceTimeline(plan,start,1200),text=resultPlaybackText(plan,half);
  assert.match(text,/30 → 27  \(-3\)/);assert.match(text,/50 → 50  \(\+0\)/);assert.match(text,/0 → 0  \(\+0\)/);
  const next=advanceTimeline(plan,start,2000);assert.match(resultPlaybackText(plan,next),/30 → 24  \(-6\)/);assert.match(resultPlaybackText(plan,next),/50 → 45  \(-5\)/);
  const done=advanceTimeline(plan,next,60000);assert.equal(done.completed,true);assert.match(resultPlaybackText(plan,done),/0 → 12  \(\+12\)/);
  assert.equal(JSON.stringify(state),original);
});

test('saved partial display restores exactly and blocks auto continuation until finished',async()=>{
  const {content,state}=await setup(),p=new PlayerPresentation();p.view(content,state);assert.equal(p.act(content,state,{type:'timelineTick',deltaMs:1200}),null);
  const saved=JSON.parse(JSON.stringify(p.envelope(content,state))),loaded=new PlayerPresentation();assert.equal(loaded.restore(content,state,saved),null);
  assert.deepEqual(loaded.view(content,state),p.view(content,state));assert.equal(scheduleStep(state,'continuous',loaded.view(content,state)),null);
  loaded.act(content,state,{type:'timelineTick',deltaMs:60000});assert.equal(scheduleStep(state,'continuous',loaded.view(content,state)),'continue');
  assert.equal(scheduleStep({...state,phase:'narrative'},'continuous',null),null);assert.equal(scheduleStep({...state,phase:'planning',schedule:null},'continuous',null),null);
  assert.equal(scheduleStep(state,'manual',loaded.view(content,state)),null);
  const plan=planActivityTimeline(content,state);assert.equal(validateTimelineCursor(plan,{...saved.timeline,elapsedMs:NaN}),false);
});

test('legacy projects stay manual; authored activity timelines retain priority; zero changes still finish',async()=>{
  const {content,state}=await setup(),copy=structuredClone(content);
  delete copy.project.visual.resultPlayback;assert.equal(planActivityTimeline(copy,state),null);
  copy.project.visual.resultPlayback=content.project.visual.resultPlayback;copy.project.visual.timelines=[{id:'custom',name:'Custom',clips:[{id:'one',durationMs:100,text:'custom'}]}];copy.project.visual.activityTimelines=[{activityId:'study',timelineId:'custom'}];assert.equal(planActivityTimeline(copy,state).timelineId,'custom');
  const empty={...state,lastActivityResult:{...state.lastActivityResult,changes:[]}},plan=planActivityTimeline(content,empty);
  assert.equal(plan.totalDurationMs,1800);assert.match(resultPlaybackText(plan,createTimelineCursor(plan)),/능력 변화 없음/);assert.equal(advanceTimeline(plan,createTimelineCursor(plan),1800).completed,true);
});

test('auto-only navigation allowance does not weaken manual navigation validation',async()=>{
  const loaded=await loadProject('tests/fixtures/projects/last-grandson-polished'),project=loaded.documents.find(d=>d.kind==='project');project.data.schedulePlayback='manual';
  const result=await buildSnapshot(loaded);assert.equal(result.ok,false);assert.ok(result.diagnostics.some(d=>d.code==='UI_NAVIGATION'));
});

test('pause and 4x are presentation-only, survive encoded save, and carry across events',async()=>{
  const {content,state,session}=await setup(),original=JSON.stringify(state),p=new PlayerPresentation();p.view(content,state);
  p.act(content,state,{type:'timelineTick',deltaMs:400});p.act(content,state,{type:'timelineSpeed'});p.act(content,state,{type:'timelinePause'});
  const before=p.view(content,state);p.act(content,state,{type:'timelineTick',deltaMs:30000});assert.deepEqual(p.view(content,state),before);
  const saved=await encodeSave(content,session,p.envelope(content,state));assert.equal(saved.ok,true,JSON.stringify(saved));
  const loaded=await decodeSave(content,saved.text);assert.equal(loaded.ok,true,JSON.stringify(loaded));
  const q=new PlayerPresentation();assert.equal(q.restore(content,loaded.session.state,loaded.playerPresentation),null);assert.deepEqual(q.view(content,state),before);
  q.act(content,state,{type:'timelinePause'});q.act(content,state,{type:'timelineTick',deltaMs:100});assert.equal(q.view(content,state).cursor.clipIndex,1);assert.equal(q.view(content,state).cursor.elapsedMs,0);
  q.act(content,state,{type:'timelineSkip'});assert.equal(q.view(content,state).cursor.completed,true);assert.equal(q.view(content,state).cursor.paused,false);
  const narrative={...state,phase:'narrative'};assert.equal(q.view(content,narrative),null);assert.equal(scheduleStep(narrative,'continuous',null),null);
  const next=new PlayerPresentation();assert.equal(next.restore(content,narrative,q.envelope(content,narrative)),null);assert.equal(next.view(content,{...state,stateRevision:state.stateRevision+1}).cursor.speed,4);
  assert.equal(JSON.stringify(state),original);
  const corrupt=JSON.parse(saved.text);corrupt.playerPresentation.scheduleSpeed=3;assert.equal((await decodeSave(content,JSON.stringify(corrupt))).ok,false);
  corrupt.playerPresentation.scheduleSpeed=1;assert.equal((await decodeSave(content,JSON.stringify(corrupt))).ok,false,'inconsistent speed must not silently override the saved cursor');
  const legacy=JSON.parse(saved.text);delete legacy.playerPresentation.scheduleSpeed;assert.equal((await decodeSave(content,JSON.stringify(legacy))).ok,true,'old cursor-only saves remain valid');
  next.clear();assert.equal(next.scheduleSpeed,1);
});

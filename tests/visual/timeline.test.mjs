import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {fixture} from '../gameplay/fixture.mjs';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/session.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';
import {planActivityTimeline,createTimelineCursor,advanceTimeline,skipTimeline,validateTimelineCursor,timelineChangesText} from '../../dist/packages/visual/timeline.js';

function configure(p){
 p.gameplay.ruleCalendar={daysPerMonth:10};
 p.visual.timelines=[{id:'rest-scene',name:'Rest',clips:[{id:'opening',durationMs:100,text:'{activity} {outcome} {day}',transition:'fade'},{id:'summary',durationMs:200,text:'{changes}'}]},{id:'won',name:'Success',clips:[{id:'win',durationMs:50,text:'Success'}]},{id:'lost',name:'Failure',clips:[{id:'lose',durationMs:75,text:'Failure'}]}];
 p.visual.activityTimelines=[{activityId:'rest',timelineId:'rest-scene',successTimelineId:'won',failureTimelineId:'lost'}];
}
async function setup(t,mutate=()=>{}){const f=await fixture(t,p=>{configure(p);mutate(p)});assert.equal(f.built.ok,true,JSON.stringify(f.built.diagnostics));return f.built.snapshot;}
function command(content,session,cmd){const next=dispatchSession(content,session,cmd,session.state.stateRevision);assert.equal(next.ok,true,JSON.stringify(next));return next.session;}
function resultSession(content){let s=createSession(content,42,{mode:'activities-only'}).session;s=command(content,s,{type:'setSchedule',activityIds:['rest','rest','rest']});return command(content,s,{type:'executeActivity'});}
test('planner selects branches only from committed activity results and resolves literal tokens',async t=>{
 const content=await setup(t),session=resultSession(content),plan=planActivityTimeline(content,session.state),before=JSON.stringify(session);
 assert.equal(plan.timelineId,'rest-scene');assert.equal(plan.clips[0].id,'once__opening');assert.equal(plan.clips[0].day,null);assert.match(plan.clips[0].text,/완료/);assert.equal(plan.clips[0].transition,'fade');assert.equal(plan.clips[1].transition,'none');assert.equal(plan.totalDurationMs,300);
 for(const passed of [true,false]){const state=structuredClone(session.state);state.lastActivityResult.success={passed,requestedChance:5000,chance:5000,roll:1000,rawDraws:1};assert.equal(planActivityTimeline(content,state).timelineId,passed?'won':'lost');}
 assert.equal(planActivityTimeline(content,{...session.state,phase:'ready'}),null);assert.equal(JSON.stringify(session),before);
});
test('logical days use existing period partitions, with committed summary only on first visual day',async t=>{
 const content=await setup(t,p=>p.visual.activityTimelines[0].repeat='logicalDays'),s=resultSession(content);
 const expected=[[1,2,3],[4,5,6],[7,8,9,10]];
 for(let period=0;period<3;period++){const state=structuredClone(s.state);state.lastActivityResult.time.periodIndex=period;state.lastActivityResult.changes=[{kind:'numeric',source:'/already-committed',target:{kind:'attribute',attributeId:'money'},before:10,operand:7,requested:17,after:17,delta:7,clamped:false}];const plan=planActivityTimeline(content,state);assert.deepEqual([...new Set(plan.clips.map(x=>x.day))],expected[period]);assert.match(plan.clips[1].text,/\+7/);assert.equal(plan.clips[3].text,'');assert.equal(plan.clips.length,expected[period].length*2);}
});
test('normal/fast/pause/skip and exact clip boundaries preserve core state and random draws',async t=>{
 const content=await setup(t),s=resultSession(content),plan=planActivityTimeline(content,s.state),before=JSON.stringify(s),initial=createTimelineCursor(plan);
 const paused={...initial,paused:true};assert.deepEqual(advanceTimeline(plan,paused,9999),paused);
 const boundary=advanceTimeline(plan,initial,100);assert.equal(boundary.clipIndex,1);assert.equal(boundary.elapsedMs,0);
 const normal=advanceTimeline(plan,initial,300),fast=advanceTimeline(plan,{...initial,speed:4},75),skip=skipTimeline(plan,initial);
 for(const c of [normal,fast,skip]){assert.equal(c.completed,true);assert.equal(c.clipIndex,2);assert.equal(c.elapsedMs,0);assert.equal(validateTimelineCursor(plan,c),true);}
 assert.deepEqual(advanceTimeline(plan,normal,1234),normal);assert.throws(()=>advanceTimeline(plan,initial,-1));assert.throws(()=>advanceTimeline(plan,initial,NaN));assert.equal(JSON.stringify(s),before);
 let a=command(content,s,{type:'acknowledgeResult'}),b=command(content,structuredClone(s),{type:'acknowledgeResult'});a=command(content,a,{type:'completePeriodWithoutEvents'});b=command(content,b,{type:'completePeriodWithoutEvents'});assert.deepEqual(a,b);
});
test('midclip cursor survives JSON save roundtrip; malformed/stale cursors reject',async t=>{
 const content=await setup(t),s=resultSession(content),plan=planActivityTimeline(content,s.state),cursor={...advanceTimeline(plan,createTimelineCursor(plan),125),paused:true,speed:4};
 const encoded=await encodeSave(content,s);assert.equal(encoded.ok,true);const envelope=JSON.parse(encoded.text);envelope.playerPresentation={version:1,timeline:cursor};
 const loaded=await decodeSave(content,JSON.stringify(envelope));assert.equal(loaded.ok,true,JSON.stringify(loaded));assert.deepEqual(loaded.session.state,s.state);assert.deepEqual(loaded.playerPresentation,envelope.playerPresentation);assert.equal(validateTimelineCursor(plan,JSON.parse(JSON.stringify(cursor))),true);
 const resume={...cursor,paused:false};assert.deepEqual(advanceTimeline(plan,resume,43.75),skipTimeline(plan,resume));
 for(const bad of [{...cursor,resultRevision:999},{...cursor,timelineId:'wrong'},{...cursor,clipIndex:2},{...cursor,elapsedMs:200},{...cursor,elapsedMs:-1},{...cursor,speed:2},{...cursor,extra:true}]){assert.equal(validateTimelineCursor(plan,bad),false);const rejected=await decodeSave(content,JSON.stringify({...envelope,playerPresentation:{version:1,timeline:bad}}));assert.equal(rejected.ok,false);}
 assert.equal(validateTimelineCursor(null,cursor),false);assert.equal(validateTimelineCursor(plan,{...skipTimeline(plan,cursor),paused:true}),false);
});
test('multiple random activities produce identical final core state across normal/fast/skip and saved midclip',async t=>{
 const f=await fixture(t,configure,async root=>{const file=path.join(root,'activities/rest.json'),a=JSON.parse(await readFile(file,'utf8'));a.successCheck={chance:{op:'literal',value:5000}};a.successEffects=[{op:'add',target:{kind:'attribute',attributeId:'money'},value:{op:'literal',value:2}}];a.failureEffects=[];await writeFile(file,JSON.stringify(a));});assert.equal(f.built.ok,true,JSON.stringify(f.built.diagnostics));const content=f.built.snapshot,final=[];
 for(const mode of ['normal','fast','skip']){let session=createSession(content,77,{mode:'activities-only'}).session;
  for(let i=0;i<6;i++){if(i%3===0)session=command(content,session,{type:'setSchedule',activityIds:['rest','rest','rest']});session=command(content,session,{type:'executeActivity'});const plan=planActivityTimeline(content,session.state);assert.equal(plan.timelineId,session.state.lastActivityResult.success.passed?'won':'lost');let cursor=createTimelineCursor(plan);
   if(mode==='skip')cursor=skipTimeline(plan,cursor);else{cursor={...cursor,speed:mode==='fast'?4:1};cursor=advanceTimeline(plan,cursor,5);const saved=await encodeSave(content,session,{version:1,timeline:cursor});assert.equal(saved.ok,true);const restored=await decodeSave(content,saved.text);assert.equal(restored.ok,true);session=restored.session;cursor=advanceTimeline(plan,restored.playerPresentation.timeline,10000);}assert.equal(cursor.completed,true);
   session=command(content,session,{type:'acknowledgeResult'});session=command(content,session,{type:'completePeriodWithoutEvents'});
  }assert.equal(session.state.rng.draws,6);final.push(session);
 }assert.deepEqual(final[0],final[1]);assert.deepEqual(final[0],final[2]);
});
test('change summary combines costs and effects without generating day-specific state',async t=>{
 const content=await setup(t),change=delta=>({kind:'numeric',source:'/effect',target:{kind:'attribute',attributeId:'money'},before:0,operand:delta,requested:delta,after:delta,delta,clamped:false});
 assert.match(timelineChangesText(content,[change(-10),change(3)]),/-7$/);assert.equal(timelineChangesText(content,[change(-10),change(10)]),'');
});
test('semantic validation reports timeline/clip duplicates, invalid refs, unsupported tokens and missing day calendar',async t=>{
 const f=await fixture(t,p=>{configure(p);p.visual.timelines[0].clips.push({id:'opening',durationMs:30,text:'{invented}'},{id:'empty',durationMs:10},{id:'bad-image',durationMs:10,imageAssetId:'missing-image'});p.visual.activityTimelines.push({...p.visual.activityTimelines[0],timelineId:'missing-timeline'});p.visual.activityTimelines[0].repeat='logicalDays';delete p.gameplay.ruleCalendar;});
 assert.equal(f.built.ok,false);for(const code of ['TIMELINE_TEXT_TOKEN','TIMELINE_EMPTY_CLIP','TIMELINE_DUPLICATE_ACTIVITY','TIMELINE_DAY_CALENDAR'])assert.ok(f.built.diagnostics.some(x=>x.code===code),code);assert.ok(f.built.diagnostics.some(x=>x.pointer.endsWith('/imageAssetId')));assert.ok(f.built.diagnostics.some(x=>x.pointer.endsWith('/timelineId')));
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {fixture} from '../gameplay/fixture.mjs';
import {createRun,dispatchCommand} from '../../dist/packages/core/run.js';
import {nativeCreateRun,nativeDispatchCommand,nativeReplay} from '../../dist/packages/native-bridge/index.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';
import {commandAttempts,applyCommands} from '../../dist/packages/analysis-service/runner.js';
const add=(id,value)=>({op:'add',target:{kind:'attribute',attributeId:id,owner:{kind:'actor'}},value:{op:'literal',value}});
async function setup(t,mutate=()=>{}){return fixture(t,p=>{
 p.roster={stateModel:'character-v1',localAttributeIds:['skill','stamina','stress'],members:[{characterId:'apprentice',initialAttributes:{skill:10},activityId:'rest',actionIds:['learn','work']},{characterId:'mentor',initialAttributes:{skill:40},activityId:'rest',actionIds:['learn','work']}]};
 p.gameplay.actions=[{id:'learn',name:'Learn',costs:[],effects:[add('skill',5)],maxQuantity:3},{id:'work',name:'Work',costs:[],effects:[add('skill',7)]}];
 p.progression={model:'unified-v2',ticksPerPeriod:10,parallel:true,unitLabel:'시간',actionPoints:{max:8,initial:6,recoveryPerTick:1},actorPools:[{actorId:'mentor',max:4,initial:3,recoveryPerTick:0}],activities:[{activityId:'rest',actionPointCost:1}],actions:[{actionId:'learn',duration:4,actionPointCost:3},{actionId:'work',duration:6,actionPointCost:2}],appointments:[{id:'lesson',eventId:'gift-friendship',actorId:'mentor',atTick:2,repeatEvery:2,repeatCount:3}]};mutate(p);
 },async root=>{await writeFile(resolve(root,'events/gift-friendship.json'),JSON.stringify({formatVersion:'0.1.0',id:'gift-friendship',label:'예약 수업',priority:1,trigger:'interaction',scope:'actor',window:{fromPeriod:0,throughPeriod:35},requirements:{op:'always'},entryNodeId:'hello',nodes:[{id:'hello',type:'dialogue',speakerId:'mentor',text:'시간이 되었다.',next:'effect'},{id:'effect',type:'effect',effects:[add('skill',1)],next:'end'},{id:'end',type:'end'}]}));});}
function session(c){const origin={seed:42,mode:'full',initialOverrides:{attributes:{},flags:{},relationships:{}}},commands=[];let state=nativeCreateRun(c,42,origin).state;assert.deepEqual(state,createRun(c,42,origin).state);
 return {get state(){return state;},send(command,ok=true){const n=nativeDispatchCommand(c,state,command,state.stateRevision),ts=dispatchCommand(c,state,command,state.stateRevision);assert.equal(n.ok,ok,JSON.stringify(n.error));assert.equal(ts.ok,ok,JSON.stringify(ts.error));if(ok){assert.deepEqual(n,ts);state=n.state;commands.push(command);}else{assert.equal(n.error.code,ts.error.code);assert.deepEqual(n.state,state);assert.deepEqual(ts.state,state);}return n;},async save(){const e=await encodeSave(c,{origin,state,commands});assert.ok(e.ok,JSON.stringify(e));const d=await decodeSave(c,e.text);assert.ok(d.ok,JSON.stringify(d));assert.deepEqual(d.session.state,state);assert.deepEqual(nativeReplay(c,origin,commands),{ok:true,state});return e.text;}};
}
const action=(actorId,actionId)=>({type:'executeActorAction',actorId,actionId,quantity:1}),advance={type:'advanceSession'};
function story(s,c){let n=0;while(s.state.phase==='narrative'){assert.ok(++n<50);const cursor=s.state.activeNarrative;const node=c?(cursor.kind==='event'?c.events:c.endings).find(x=>x.id===cursor.id).nodes.find(x=>x.id===cursor.nodeId):null;if(node?.type==='choice'){const choice=node.choices.find(x=>nativeDispatchCommand(c,s.state,{type:'choose',choiceId:x.id},s.state.stateRevision).ok);assert.ok(choice);s.send({type:'choose',choiceId:choice.id});}else s.send({type:'advanceNarrative'});}}
test('parallel actor clocks, repeated personal appointments, calendar budget and exact save replay agree in C++/TS',async t=>{
 const {built}=await setup(t);assert.ok(built.ok,JSON.stringify(built.diagnostics));const s=session(built.snapshot);
 s.send(action('apprentice','learn'));s.send(action('mentor','work'));assert.equal(s.state.progression.sessions.length,2);assert.equal(s.state.progression.actors.apprentice.actionPoints,3);assert.equal(s.state.progression.actors.mentor.actionPoints,1);await s.save();
 s.send(action('apprentice','learn'),false);s.send({type:'assignActivities',assignments:{apprentice:'rest',mentor:'rest'}},false);
 s.send(advance);assert.equal(s.state.progression.tick,2);assert.equal(s.state.activeNarrative.actorId,'mentor');await s.save();story(s);
 s.send(advance);assert.equal(s.state.progression.tick,4);story(s);s.send(advance);assert.equal(s.state.progression.sessions.length,1);assert.equal(s.state.roster.attributes.apprentice.skill,15);await s.save();
 s.send(advance);assert.equal(s.state.progression.tick,6);story(s);s.send(advance);assert.equal(s.state.progression.pending,null);assert.equal(s.state.roster.attributes.mentor.skill,50);assert.deepEqual(s.state.progression.handled.map(x=>x.atTick),[2,4,6]);
 assert.equal(s.state.progression.actors.apprentice.elapsed,4);assert.equal(s.state.progression.actors.mentor.elapsed,6);s.send({type:'waitTime',ticks:5},false);
 s.send({type:'assignActivities',assignments:{apprentice:'rest',mentor:'rest'}});s.send({type:'executeActivity'});assert.equal(s.state.progression.pending.remaining,4);await s.save();s.send(advance);assert.equal(s.state.progression.tick,10);assert.equal(s.state.phase,'activity-result');await s.save();
 s.send({type:'acknowledgeResult'});s.send({type:'resolvePeriodEnd'});story(s);assert.equal(s.state.periodIndex,1);assert.equal(s.state.progression.tick,10);assert.equal(s.state.progression.actors.apprentice.elapsed,8);assert.equal(s.state.progression.actors.mentor.elapsed,10);
 const corrupt=JSON.parse(await s.save());corrupt.state.progression.actors.mentor.elapsed++;assert.equal((await decodeSave(built.snapshot,JSON.stringify(corrupt))).ok,false);
});
test('individual cancellation preserves other session, spent AP and elapsed time; all-cancel and zero-time calendar settlement',async t=>{
 const {built}=await setup(t,p=>{p.progression.appointments=[];});const s=session(built.snapshot);
 s.send(action('apprentice','learn'));s.send(action('mentor','work'));s.send({type:'cancelActorSession',actorId:'apprentice'});assert.equal(s.state.progression.sessions[0].id,'mentor');assert.equal(s.state.progression.actors.apprentice.actionPoints,3);s.send({type:'cancelActorSession',actorId:'apprentice'},false);s.send(advance);assert.equal(s.state.roster.attributes.apprentice.skill,10);assert.equal(s.state.progression.actors.apprentice.elapsed,0);await s.save();
 s.send(action('apprentice','learn'));s.send({type:'cancelSession'});assert.equal(s.state.progression.pending,null);s.send({type:'waitTime',ticks:4});s.send(advance);assert.equal(s.state.progression.tick,10);s.send(action('apprentice','learn'),false);
 s.send({type:'assignActivities',assignments:{apprentice:'rest',mentor:'rest'}});s.send({type:'executeActivity'});assert.equal(s.state.progression.pending.remaining,0);s.send(advance);assert.equal(s.state.progression.tick,10);await s.save();
});
test('parallel off, unbound actors, oversized duration and insufficient personal AP reject atomically',async t=>{
 const {built}=await setup(t,p=>{p.progression.parallel=false;p.progression.appointments=[];});const s=session(built.snapshot);
 s.send(action('nobody','learn'),false);s.send({...action('apprentice','learn'),quantity:3},false);s.send(action('mentor','learn'));s.send(action('apprentice','learn'),false);s.send(advance);s.send(action('mentor','learn'),false);await s.save();
});
test('validation locates bad repeats, owner pools, duplicate activities and missing calendar settings',async t=>{
 const {built}=await setup(t,p=>{delete p.progression.ticksPerPeriod;p.progression.actorPools.push({actorId:'missing',max:1,initial:2,recoveryPerTick:0});p.progression.activities.push({activityId:'rest',actionPointCost:99});delete p.progression.appointments[0].repeatCount;});assert.equal(built.ok,false);
 for(const code of ['PERIOD_TIME_REQUIRED','UNKNOWN_ACTOR','ACTION_POINTS_RANGE','DUPLICATE_TIMING','REPEAT_APPOINTMENT_CONFIG'])assert.ok(built.diagnostics.some(d=>d.code===code),code);
});
test('analysis offers feasible parallel starts and individual cancellation',async t=>{
 const {built}=await setup(t);const s=session(built.snapshot);s.send(action('apprentice','learn'));
 const candidates=commandAttempts(built.snapshot,s.state).filter(commands=>applyCommands(built.snapshot,s.state,commands));
 assert.ok(candidates.some(c=>c[0].type==='executeActorAction'&&c[0].actorId==='mentor'));
 assert.ok(candidates.some(c=>c[0].type==='cancelActorSession'&&c[0].actorId==='apprentice'));
});
test('same-tick settlements are ordered and shared-cost races roll back without losing other sessions',async t=>{
 const {built}=await setup(t,p=>{p.progression.appointments=[];p.initialState.attributes.money=5;p.progression.actions[1].duration=4;for(const a of p.gameplay.actions)a.costs=[{attributeId:'money',amount:5}];});const s=session(built.snapshot);
 s.send(action('mentor','work'));s.send(action('apprentice','learn'));s.send(advance);assert.equal(s.state.progression.tick,4);assert.equal(s.state.attributes.money,0);assert.equal(s.state.roster.attributes.apprentice.skill,15);assert.equal(s.state.roster.attributes.mentor.skill,40);await s.save();
 const before=structuredClone(s.state);s.send(advance,false);assert.deepEqual(s.state,before);s.send({type:'cancelActorSession',actorId:'mentor'});assert.equal(s.state.progression.pending,null);await s.save();
});
test('unified monthly schedule completes a full year through the existing ending rules',async t=>{
 const {built}=await setup(t,p=>{p.progression.appointments=[];p.progression.activities=[];p.progression.actorPools=[];});const s=session(built.snapshot);
 for(let i=0;i<36;i++){s.send({type:'assignActivities',assignments:{apprentice:'rest',mentor:'rest'}});s.send({type:'executeActivity'});s.send(advance);assert.equal(s.state.progression.tick,(i+1)*10);s.send({type:'acknowledgeResult'});s.send({type:'resolvePeriodEnd'});story(s,built.snapshot);}
 assert.equal(s.state.phase,'finished');assert.equal(s.state.progression.tick,360);await s.save();
});

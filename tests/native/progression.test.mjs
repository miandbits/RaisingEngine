import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {fixture} from '../gameplay/fixture.mjs';
import {createRun,dispatchCommand} from '../../dist/packages/core/run.js';
import {nativeCreateRun,nativeDispatchCommand,nativeReplay} from '../../dist/packages/native-bridge/index.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {runAnalysis,replayTrace} from '../../dist/packages/analysis-service/runner.js';
const add=(id,value)=>({op:'add',target:{kind:'attribute',attributeId:id},value:{op:'literal',value}});
async function setup(t,mutate=()=>{}){return fixture(t,p=>{
 p.gameplay.actions=[{id:'study-time',name:'Study',costs:[{attributeId:'money',amount:5}],effects:[add('skill',5)],maxQuantity:3}];
 p.progression={unitLabel:'시간',actionPoints:{max:6,initial:4,recoveryPerTick:1},actions:[{actionId:'study-time',duration:5,actionPointCost:3}],appointments:[{id:'visit',eventId:'gift-friendship',atTick:2}]};mutate(p);
},async root=>{const file=resolve(root,'events/gift-friendship.json'),e=JSON.parse(await readFile(file,'utf8'));e.requirements={op:'always'};await writeFile(file,JSON.stringify(e));});}
function session(c){const origin={seed:42,mode:'full',initialOverrides:{attributes:{},flags:{},relationships:{}}},commands=[];
 let created=nativeCreateRun(c,42,origin);assert.deepEqual(created,createRun(c,42,origin));assert.ok(created.ok);let state=created.state;
 return {origin,commands,get state(){return state;},send(command,ok=true){const n=nativeDispatchCommand(c,state,command,state.stateRevision),ts=dispatchCommand(c,state,command,state.stateRevision);assert.equal(n.ok,ok,JSON.stringify(n));if(ok){assert.deepEqual(n,ts);state=n.state;commands.push(command);}else{assert.equal(ts.ok,false);assert.equal(n.error.code,ts.error.code);assert.deepEqual(n.state,state);assert.deepEqual(ts.state,state);}return n;},async save(){const e=await encodeSave(c,{origin,state,commands});assert.ok(e.ok,JSON.stringify(e));const d=await decodeSave(c,e.text);assert.ok(d.ok,JSON.stringify(d));assert.deepEqual(d.session.state,state);assert.deepEqual(nativeReplay(c,origin,commands),{ok:true,state});return e.text;}};
}
function finishStory(s){let guard=0;while(s.state.phase==='narrative'){assert.ok(++guard<20);s.send({type:'advanceNarrative'});}}
const action={type:'executeAction',actionId:'study-time',quantity:1},advance={type:'advanceSession'};
test('timed action interrupts at appointment; pending and narrative saves replay identically in C++ and TS',async t=>{
 const {built}=await setup(t);assert.ok(built.ok,JSON.stringify(built));const s=session(built.snapshot),initial=s.state;
 s.send(action);assert.equal(s.state.progression.actionPoints,1);assert.equal(s.state.progression.pending.remaining,5);assert.deepEqual(s.state.attributes,initial.attributes);await s.save();
 s.send(advance);assert.equal(s.state.progression.tick,2);assert.equal(s.state.progression.pending.remaining,3);assert.equal(s.state.phase,'narrative');await s.save();
 s.send(advance,false);finishStory(s);s.send(advance);assert.equal(s.state.progression.tick,5);assert.equal(s.state.progression.pending,null);assert.equal(s.state.attributes.skill,initial.attributes.skill+5);assert.equal(s.state.attributes.money,initial.attributes.money-5);await s.save();
 const tampered=JSON.parse(await s.save());tampered.state.progression.tick++;assert.equal((await decodeSave(built.snapshot,JSON.stringify(tampered))).ok,false);
});
test('insufficient AP, busy commands, invalid wait, revision, cancel and wait recovery are atomic',async t=>{
 const {built}=await setup(t);const s=session(built.snapshot);assert.equal(s.send({...action,quantity:2},false).error.code,'INSUFFICIENT_ACTION_POINTS');
 s.send(action);assert.equal(s.send({type:'waitTime',ticks:1},false).error.code,'SESSION_BUSY');s.send(action,false);s.send({type:'setSchedule',activityIds:['rest','rest','rest']},false);
 const conflict=nativeDispatchCommand(built.snapshot,s.state,advance,0);assert.equal(conflict.error.code,'REVISION_CONFLICT');
 s.send({type:'cancelSession'});assert.equal(s.state.progression.actionPoints,1);assert.equal(s.state.progression.tick,0);s.send({type:'waitTime',ticks:0},false);
 s.send({type:'waitTime',ticks:1});s.send(advance);assert.equal(s.state.progression.actionPoints,2);await s.save();
});
test('same-tick reservations use stable IDs; already completed events skip without duplicate narrative',async t=>{
 const {built}=await setup(t,p=>p.progression.appointments=[{id:'z',eventId:'gift-friendship',atTick:2},{id:'a',eventId:'gift-friendship',atTick:2}]);const s=session(built.snapshot);
 s.send(action);s.send(advance);assert.deepEqual(s.state.progression.handled,[{id:'a',outcome:'fired'}]);finishStory(s);s.send(advance);assert.equal(s.state.progression.tick,2);assert.deepEqual(s.state.progression.handled,[{id:'a',outcome:'fired'},{id:'z',outcome:'skipped'}]);s.send(advance);await s.save();
});
test('reservation actors survive pending action context and saves',async t=>{
 const {root,project}=await setup(t,p=>{p.roster={stateModel:'character-v1',localAttributeIds:['skill'],members:[{characterId:'apprentice',initialAttributes:{skill:10},activityId:'study',actionIds:['study-time']},{characterId:'mentor',initialAttributes:{skill:30},activityId:'rest',actionIds:[]}]};p.progression.appointments[0].actorId='mentor';p.gameplay.actions[0].effects[0].target.owner={kind:'actor'};});
 const file=resolve(root,'events/gift-friendship.json'),e=JSON.parse(await readFile(file,'utf8'));e.scope='actor';await writeFile(file,JSON.stringify(e));await writeFile(resolve(root,'project.json'),JSON.stringify(project));const built=await buildSnapshot(await loadProject(root));assert.ok(built.ok,JSON.stringify(built));const s=session(built.snapshot);
 s.send({...action,type:'executeActorAction',actorId:'apprentice'});s.send(advance);assert.equal(s.state.activeNarrative.actorId,'mentor');await s.save();finishStory(s);s.send(advance);assert.equal(s.state.roster.attributes.apprentice.skill,15);assert.equal(s.state.roster.attributes.mentor.skill,30);await s.save();
});
test('completion failure preserves session and cancellation does not refund spent time/AP',async t=>{
 const {root,project}=await setup(t);const file=resolve(root,'events/gift-friendship.json'),e=JSON.parse(await readFile(file,'utf8'));e.nodes[0].next='drain';e.nodes.splice(1,0,{id:'drain',type:'effect',effects:[{op:'set',target:{kind:'attribute',attributeId:'money'},value:{op:'literal',value:0}}],next:'end'});await writeFile(file,JSON.stringify(e));const built=await buildSnapshot(await loadProject(root));assert.ok(built.ok);const s=session(built.snapshot);
 s.send(action);s.send(advance);finishStory(s);const before=s.state;s.send(advance,false);assert.deepEqual(s.state,before);s.send({type:'cancelSession'});assert.equal(s.state.progression.tick,2);assert.equal(s.state.progression.actionPoints,3);await s.save();
});
test('invalid timing ranges, missing references, duplicate reservations and invalid actors are located',async t=>{
 const {root,project}=await setup(t);project.progression.actionPoints.initial=7;project.progression.actions.push({...project.progression.actions[0]});project.progression.appointments.push({...project.progression.appointments[0],actorId:'missing'});project.progression.actions.push({actionId:'missing',duration:1,actionPointCost:1});await writeFile(resolve(root,'project.json'),JSON.stringify(project));let b=await buildSnapshot(await loadProject(root));assert.equal(b.ok,false);for(const code of ['ACTION_POINTS_RANGE','DUPLICATE_TIMING','DUPLICATE_APPOINTMENT','APPOINTMENT_ACTOR','UNKNOWN_ACTION'])assert.ok(b.diagnostics.some(d=>d.code===code),JSON.stringify(b.diagnostics));
 project.progression.actions[0].duration=0;await writeFile(resolve(root,'project.json'),JSON.stringify(project));b=await buildSnapshot(await loadProject(root));assert.equal(b.ok,false);
});
test('absent config leaves legacy state unchanged and rejects progression commands',async t=>{
 const {built}=await fixture(t);const s=session(built.snapshot);assert.equal(s.state.progression,undefined);assert.equal(s.send({type:'waitTime',ticks:1},false).error.code,'PROGRESSION_DISABLED');await s.save();
});
test('automatic analysis continues time sessions and replays scheduled-event routes',async t=>{
 const {built}=await setup(t);const c=built.snapshot;
 const result=await runAnalysis(c,{runs:1,seed:42,strategy:'random',maxCommands:150,timeoutMs:10000,target:{kind:'event',id:'gift-friendship'},beamWidth:12,maxStates:200});
 assert.equal(result.targetStatus,'found',JSON.stringify(result));assert.ok(result.traces.some(t=>t.commands.some(c=>c.type==='advanceSession')));
 for(const trace of result.traces)assert.deepEqual(replayTrace(c,trace).state,trace.state);
});
test('eligibility probes do not spend RNG; probabilistic completion is deterministic across saves',async t=>{
 const {built}=await setup(t,p=>{p.progression.appointments=[];p.gameplay.actions.push({id:'bonus',name:'Bonus',costs:[],effects:[add('skill',7)]});p.gameplay.actions[0].flow=[{id:'roll',chance:{op:'literal',value:5000},thenActionId:'bonus'}];});
 assert.ok(built.ok,JSON.stringify(built));const s=session(built.snapshot),rng=s.state.rng;
 s.send(action);assert.deepEqual(s.state.rng,rng);await s.save();s.send(advance);assert.ok(s.state.rng.draws>rng.draws);await s.save();
 const second=session(built.snapshot);second.send(action);second.send(advance);assert.deepEqual(second.state,s.state);
});
test('an appointment exactly at completion runs before settlement, with zero remaining time saved',async t=>{
 const {built}=await setup(t,p=>p.progression.appointments[0].atTick=5);const s=session(built.snapshot),skill=s.state.attributes.skill;
 s.send(action);s.send(advance);assert.equal(s.state.phase,'narrative');assert.equal(s.state.progression.pending.remaining,0);assert.equal(s.state.attributes.skill,skill);await s.save();finishStory(s);s.send(advance);assert.equal(s.state.attributes.skill,skill+5);assert.equal(s.state.progression.tick,5);await s.save();
});

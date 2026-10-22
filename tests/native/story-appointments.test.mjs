import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {appointmentFixture} from './story-appointment-fixture.mjs';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {createRun,dispatchCommand} from '../../dist/packages/core/run.js';
import {nativeCreateRun,nativeDispatchCommand,nativeReplay} from '../../dist/packages/native-bridge/index.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';

function session(c){const origin={seed:42,mode:'full',initialOverrides:{attributes:{},flags:{},relationships:{}}},commands=[],seen=[];
 let state=nativeCreateRun(c,42,origin).state;assert.deepEqual(state,createRun(c,42,origin).state);
 return {get state(){return state;},seen,send(command){const n=nativeDispatchCommand(c,state,command,state.stateRevision),ts=dispatchCommand(c,state,command,state.stateRevision);assert.deepEqual(n,ts,JSON.stringify(command));assert.ok(n.ok,JSON.stringify(n));state=n.state;commands.push(command);if(state.activeNarrative)seen.push([state.activeNarrative.id,state.activeNarrative.actorId]);},async save(){const e=await encodeSave(c,{origin,state,commands});assert.ok(e.ok,JSON.stringify(e));const d=await decodeSave(c,e.text);assert.ok(d.ok,JSON.stringify(d));assert.deepEqual(d.session.state,state);assert.deepEqual(nativeReplay(c,origin,commands),{ok:true,state});state=d.session.state;return e;}};
}
const advance=s=>s.send({type:'advanceNarrative'});
async function drain(s){for(let i=0;s.state.phase==='narrative';++i){assert.ok(i<100);await s.save();advance(s);}}
async function promise(s,id,choice='yes'){s.send({type:'executeActorAction',actorId:id,actionId:'talk',quantity:1});advance(s);s.send({type:'choose',choiceId:choice});await s.save();advance(s);assert.notEqual(s.state.phase,'narrative','promise does not start a delayed follow-up immediately');}
async function activity(s,assignments){if(assignments||s.state.phase==='planning')s.send({type:'assignActivities',assignments:assignments??s.state.roster.assignments});s.send({type:'executeActivity'});await s.save();s.send({type:'acknowledgeResult'});}
async function boundary(s){const before=s.state.periodIndex;for(let i=0;s.state.periodIndex===before;i++){assert.ok(i<100);if(s.state.phase==='narrative')advance(s);else s.send({type:'resolvePeriodEnd'});}}
for(const order of [['apprentice','mentor'],['mentor','apprentice']])test(`date appointments queue each actor and retain return/chain context across saves (${order[0]} first)`,async t=>{
 const {content:c}=await appointmentFixture(t,'period-start',{fromPeriod:1,throughPeriod:1}),s=session(c);
 for(const id of order)await promise(s,id);
 await activity(s);await boundary(s);assert.equal(s.state.periodIndex,1);assert.equal(s.state.activeNarrative.id,'personal-follow');
 assert.equal(s.state.activeNarrative.actorId,'apprentice','queue uses stable roster order, not choice order');
 await drain(s);assert.equal(s.state.phase,'ready');
 for(const id of order)assert.deepEqual(s.state.roster.completedEvents[id],['personal-choice','personal-follow','personal-coda']);
 await s.save();await activity(s);await boundary(s);await drain(s);assert.equal(s.state.periodIndex,2);
 for(const id of order)assert.equal(s.state.roster.completedEvents[id].filter(x=>x==='personal-follow').length,1);
});
test('a shared true promise and another actor exact choice never unlock a date appointment; expired appointments stay closed',async t=>{
 const {content:c}=await appointmentFixture(t,'period-start',{fromPeriod:1,throughPeriod:1}),s=session(c);
 await promise(s,'apprentice');await promise(s,'mentor','other');await activity(s);await boundary(s);await drain(s);
 assert.deepEqual(s.state.roster.completedEvents.mentor,['personal-choice']);
 await promise(s,'mentor');await activity(s);await boundary(s);await drain(s);assert.equal(s.state.periodIndex,2);assert.deepEqual(s.state.roster.completedEvents.mentor,['personal-choice']);await s.save();
});
test('activity appointment uses each executed result, not the protagonist schedule; one reward per actor',async t=>{
 const {content:c}=await appointmentFixture(t,'after-activity'),s=session(c);await promise(s,'apprentice');await promise(s,'mentor');
 await activity(s,{apprentice:'study',mentor:'rest'});assert.equal(s.state.activeNarrative.actorId,'apprentice');await drain(s);assert.equal(s.state.phase,'period-end');assert.deepEqual(s.state.roster.completedEvents.mentor,['personal-choice']);
 await boundary(s);await drain(s);await activity(s,{apprentice:'rest',mentor:'study'});assert.equal(s.state.activeNarrative.actorId,'mentor');await drain(s);await s.save();
 for(const id of ['apprentice','mentor'])assert.ok(s.state.roster.completedEvents[id].includes('personal-coda'));
});
test('location appointments wait for an actual entry, restore that location and isolate both actor records',async t=>{
 const {content:c}=await appointmentFixture(t,'location-enter'),s=session(c);await promise(s,'mentor');await promise(s,'apprentice');
 s.send({type:'visit',locationId:'city'});assert.equal(s.state.phase,'planning');await s.save();s.send({type:'visit',locationId:'market'});assert.equal(s.state.activeNarrative.id,'personal-follow');await drain(s);
 assert.equal(s.state.gameplay.locationId,'market');assert.equal(s.state.gameplay.screenId,'market');assert.equal(s.state.phase,'planning');await s.save();
 for(const id of ['apprentice','mentor'])assert.equal(s.state.roster.attributes[id].skill,17);
 s.send({type:'visit',locationId:'market'});assert.equal(s.state.phase,'planning');s.send({type:'visit',locationId:'city'});s.send({type:'visit',locationId:'market'});assert.equal(s.state.phase,'planning');
});
test('location entry rules and appointments share an actual movement boundary',async t=>{
 const {root}=await appointmentFixture(t,'location-enter'),file=resolve(root,'project.json'),project=JSON.parse(await readFile(file,'utf8'));
 project.gameplay.actions.push({id:'entry-reward',name:'Entry reward',costs:[],effects:[{op:'add',target:{kind:'attribute',attributeId:'skill'},value:{op:'literal',value:1}}]});
 project.gameplay.rules=[{id:'entry-rule',name:'On arrival',trigger:{type:'locationEnter',locationId:'market'},actionId:'entry-reward'}];
 await writeFile(file,JSON.stringify(project));const built=await buildSnapshot(await loadProject(root));assert.ok(built.ok,JSON.stringify(built));const s=session(built.snapshot);
 const initial=s.state.attributes.skill;s.send({type:'visit',locationId:'city'});s.send({type:'visit',locationId:'market'});assert.equal(s.state.attributes.skill,initial+1);
 s.send({type:'visit',locationId:'market'});assert.equal(s.state.attributes.skill,initial+1,'visiting the current place is not another entry');
 s.send({type:'visit',locationId:'city'});s.send({type:'visit',locationId:'market'});assert.equal(s.state.attributes.skill,initial+2);await s.save();
});
test('appointment schemas require a valid activity/place and reject stale or repeatable trigger configuration',async t=>{
 const {root}=await appointmentFixture(t,'after-activity'),file=resolve(root,'events/personal-follow.json'),original=JSON.parse(await readFile(file,'utf8'));
 for(const bad of [{...original,activityId:'missing'}, {...original,trigger:'period-start'}, {...original,repeatable:true}, {...original,trigger:'location-enter',activityId:undefined,locationId:'missing'}]){
  await writeFile(file,JSON.stringify(bad));const b=await buildSnapshot(await loadProject(root));assert.equal(b.ok,false,JSON.stringify(bad));
 }
});

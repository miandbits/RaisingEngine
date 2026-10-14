import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdtemp,readFile,writeFile,mkdir} from 'node:fs/promises';
import path from 'node:path';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {createRun,dispatchCommand} from '../../dist/packages/core/run.js';
import {nativeCreateRun,nativeDispatchCommand,nativeReplay} from '../../dist/packages/native-bridge/index.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';
import {StudioEditor} from '../../dist/packages/studio-service/editor.js';
import {runAnalysis,replayTrace} from '../../dist/packages/analysis-service/runner.js';
import {gameplayDefinition} from '../gameplay/fixture.mjs';

async function fixture(change=()=>{}){
  await mkdir('.cache/roster-tests',{recursive:true});
  const root=await mkdtemp(path.resolve('.cache/roster-tests/run-'));
  await cp('examples/academy-demo',root,{recursive:true});
  const file=path.join(root,'project.json'),project=JSON.parse(await readFile(file,'utf8'));
  project.roster={localAttributeIds:['skill','stamina','stress'],members:[
    {characterId:'apprentice',initialAttributes:{skill:10,stamina:80,stress:0},activityId:'study'},
    {characterId:'mentor',initialAttributes:{skill:40,stamina:30,stress:20},activityId:'rest'}]};
  change(project);await writeFile(file,JSON.stringify(project,null,2));
  return {root,project,built:await buildSnapshot(await loadProject(root))};
}
function run(content,overrides={},mode='activities-only'){
 const origin={seed:42,mode,initialOverrides:{attributes:overrides,flags:{},relationships:{}}};
 let c=nativeCreateRun(content,origin.seed,origin);assert.deepEqual(c,createRun(content,origin.seed,origin));assert.equal(c.ok,true);
 let state=c.state;const commands=[];
 function send(command,expected=true){const n=nativeDispatchCommand(content,state,command,state.stateRevision),t=dispatchCommand(content,state,command,state.stateRevision);assert.equal(n.ok,expected,JSON.stringify(n));if(expected)assert.deepEqual(n,t);else {assert.equal(t.ok,false);assert.equal(n.error.code,t.error.code);assert.deepEqual(n.state,state);assert.deepEqual(t.state,state);}if(n.ok){state=n.state;commands.push(command);}return n;}
 return {origin,commands,send,get state(){return state;},async save(){const e=await encodeSave(content,{origin,state,commands});assert.equal(e.ok,true,JSON.stringify(e));const d=await decodeSave(content,e.text);assert.equal(d.ok,true,JSON.stringify(d));assert.deepEqual(d.session.state,state);assert.deepEqual(nativeReplay(content,origin,commands),{ok:true,state});return e.text;}};
}

test('each trainee rolls with their own skill and deterministic RNG survives save/replay',async()=>{
 const {built}=await fixture();const c=structuredClone(built.snapshot);
 const activity=c.activities.find(a=>a.id==='study');
 activity.successCheck={chance:{op:'multiply',left:{op:'read',target:{kind:'attribute',attributeId:'skill'}},right:{op:'literal',value:100}}};
 activity.successEffects=[];activity.failureEffects=[];
 const s=run(c);s.send({type:'assignActivities',assignments:{apprentice:'study',mentor:'study'}});s.send({type:'executeActivity'});
 assert.deepEqual(s.state.lastActivityResult.actorResults.map(r=>r.result.success.chance),[1000,4000]);
 assert.notDeepEqual(s.state.rng,nativeCreateRun(c,42,s.origin).state.rng);await s.save();
 const again=run(c);for(const cmd of s.commands)again.send(cmd);assert.deepEqual(again.state,s.state);
});

test('roster automatic analysis produces real assign commands and replayable complete runs',async()=>{
 const {built}=await fixture();const c=built.snapshot;
 const result=await runAnalysis(c,{runs:3,seed:42,strategy:'random',maxCommands:200,timeoutMs:10000});
 assert.equal(result.completed,3);assert.equal(result.outcomes.finished,3,JSON.stringify(result.outcomes));
 for(const trace of result.traces){assert.ok(trace.commands.some(c=>c.type==='assignActivities'));assert.deepEqual(replayTrace(c,trace).state,trace.state);}
});

test('shared policies apply once to protagonist and keep actor-only result changes separate',async()=>{
 const {built}=await fixture();const c=structuredClone(built.snapshot),g=gameplayDefinition();g.interactions=[];
 const effect=value=>({op:'add',target:{kind:'attribute',attributeId:'stamina'},value:{op:'literal',value}});
 g.actions=[{id:'before',name:'Before',costs:[{attributeId:'money',amount:1}],effects:[effect(1)]},{id:'after',name:'After',costs:[{attributeId:'money',amount:1}],effects:[effect(2)]}];
 g.policyGroups=[{id:'before',name:'Before',trigger:{type:'beforeActivity'},options:[{id:'on',name:'On',actionId:'before'}]},{id:'after',name:'After',trigger:{type:'afterActivity'},options:[{id:'on',name:'On',actionId:'after'}]}];c.project.gameplay=g;
 const s=run(c,{},'full');s.send({type:'selectPolicy',groupId:'before',optionId:'on'});s.send({type:'selectPolicy',groupId:'after',optionId:'on'});
 s.send({type:'assignActivities',assignments:{apprentice:'study',mentor:'rest'}});s.send({type:'executeActivity'});
 assert.equal(s.state.roster.attributes.apprentice.stamina,73);assert.equal(s.state.roster.attributes.mentor.stamina,55);
 assert.equal(s.state.attributes.money,nativeCreateRun(c,42,s.origin).state.attributes.money-7);
 assert.equal(s.state.lastActivityResult.actorResults[0].result.changes.length,4);
 assert.equal(s.state.lastActivityResult.changes.length,8);await s.save();
});
test('two trainees: distinct activity effects, shared resource, deterministic journal and save in each phase',async()=>{
 const {built}=await fixture();assert.equal(built.ok,true,JSON.stringify(built));const c=built.snapshot,s=run(c);
 await s.save();s.send({type:'assignActivities',assignments:{mentor:'rest',apprentice:'study'}});await s.save();
 const before=structuredClone(s.state);s.send({type:'executeActivity'});
 assert.equal(s.state.lastActivityResult.actorResults.length,2);assert.equal(s.state.periodIndex,0);
 assert.equal(s.state.roster.attributes.mentor.skill,40);assert.ok(s.state.roster.attributes.mentor.stamina>before.roster.attributes.mentor.stamina);
 assert.ok(s.state.roster.attributes.apprentice.stamina<80);assert.deepEqual(s.state.roster.attributes.apprentice,{skill:s.state.attributes.skill,stamina:s.state.attributes.stamina,stress:s.state.attributes.stress});
 await s.save();s.send({type:'acknowledgeResult'});await s.save();s.send({type:'completePeriodWithoutEvents'});assert.equal(s.state.periodIndex,1);
 s.send({type:'assignActivities',assignments:{apprentice:'rest',mentor:'study'}});s.send({type:'executeActivity'});await s.save();
 const again=run(c);for(const cmd of s.commands)again.send(cmd);assert.deepEqual(again.state,s.state);
});
test('second trainee failure rolls back first trainee costs, effects and RNG; assignments and versions are checked',async()=>{
 const {built}=await fixture();assert.equal(built.ok,true);const c=built.snapshot,s=run(c,{money:5});
 s.send({type:'assignActivities',assignments:{apprentice:'study',mentor:'study'}});
 const before=structuredClone(s.state);s.send({type:'executeActivity'},false);assert.deepEqual(s.state,before);
 s.send({type:'assignActivities',assignments:{apprentice:'study'}},false);
 s.send({type:'assignActivities',assignments:{apprentice:'study',outsider:'rest'}},false);
 s.send({type:'assignActivities',assignments:{apprentice:'study',mentor:'missing'}},false);
 s.send({type:'setSchedule',activityIds:['rest','rest','rest']},false);
 assert.equal(nativeDispatchCommand(c,s.state,{type:'executeActivity'},0).error.code,'REVISION_CONFLICT');
 await s.save();
});
test('roster validation reports duplicate targets, missing refs and initial ranges',async()=>{
 const {built}=await fixture(p=>{p.roster.members[1].characterId='apprentice';p.roster.members[1].activityId='missing';p.roster.members[0].initialAttributes.skill=999;p.roster.members[0].initialAttributes.money=1;});
 assert.equal(built.ok,false);for(const code of ['ROSTER_DUPLICATE','REFERENCE_MISSING','ROSTER_LOCAL_ATTRIBUTE'])assert.ok(built.diagnostics.some(d=>d.code===code&&d.pointer),code);
 assert.ok(built.diagnostics.some(d=>d.pointer.includes('initialAttributes/skill')));
});
test('tampered secondary trainee state is rejected by journal replay',async()=>{
 const {built}=await fixture(),s=run(built.snapshot);s.send({type:'assignActivities',assignments:{apprentice:'study',mentor:'rest'}});s.send({type:'executeActivity'});
 const value=JSON.parse(await s.save());value.state.roster.attributes.mentor.skill++;
 const result=await decodeSave(built.snapshot,JSON.stringify(value));assert.equal(result.ok,false);
});
test('legacy project state has no roster extension and keeps native/reference save parity',async()=>{
 const built=await buildSnapshot(await loadProject(path.resolve('examples/academy-demo')));assert.equal(built.ok,true);const s=run(built.snapshot);
 assert.equal(Object.hasOwn(s.state,'roster'),false);s.send({type:'setSchedule',activityIds:['rest','study','work']});s.send({type:'executeActivity'});assert.equal(Object.hasOwn(s.state.lastActivityResult,'actorResults'),false);await s.save();
});
test('shared editor service retains batch undo/redo and rejects stale roster edits',async()=>{
 const f=await fixture(),editor=new StudioEditor();await editor.open(f.root);const initial=editor.view();
 const next=structuredClone(f.project);next.roster.members[1].activityId='work';
 const edited=editor.edit({type:'replace',file:'project.json',data:next},initial.revision);assert.equal(edited.error,null);
 assert.throws(()=>editor.edit({type:'replace',file:'project.json',data:f.project},initial.revision));
 assert.equal(editor.edit({type:'undo'},editor.view().revision).error,null);assert.equal(editor.edit({type:'redo'},editor.view().revision).error,null);
 assert.equal((await editor.save(editor.view().revision)).error,null);
 assert.equal(JSON.parse(await readFile(path.join(f.root,'project.json'),'utf8')).roster.members[1].activityId,'work');
});

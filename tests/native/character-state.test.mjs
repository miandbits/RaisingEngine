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
const actor={kind:'actor'},fixed=characterId=>({kind:'character',characterId});
const item=itemId=>({kind:'inventory',itemId,owner:actor});
const trust={kind:'relationship',characterId:'mentor',relationshipId:'trust',owner:actor};
const add=(target,value)=>({op:'add',target,value:{op:'literal',value}});
const compare=(target,value)=>({op:'compare',comparator:'gte',left:{op:'read',target},right:{op:'literal',value}});
async function setup(t){return fixture(t,p=>{
 p.roster={stateModel:'character-v1',localAttributeIds:['skill','stamina','stress'],members:[
 {characterId:'apprentice',initialAttributes:{skill:10},activityId:'study',initialInventory:{flower:2},initialRelationships:{mentor:{trust:3}},actionIds:['gift','learn']},
 {characterId:'mentor',initialAttributes:{skill:40},activityId:'rest',initialInventory:{flower:1},initialRelationships:{mentor:{trust:7}},actionIds:['learn','gift']} ]};
 p.gameplay.actions=[{id:'gift',name:'Gift',costs:[],requirements:compare(item('flower'),1),effects:[add(item('flower'),-1),add(trust,15)],eventId:'personal'},
 {id:'learn',name:'Learn',costs:[],effects:[add({kind:'attribute',attributeId:'skill',owner:actor},5)],eventId:'personal-learn'}];
},async root=>{
 const base={formatVersion:'0.1.0',priority:90,trigger:'interaction',scope:'actor',window:{fromPeriod:0,throughPeriod:35},entryNodeId:'hello',requirements:compare(trust,15),nodes:[{id:'hello',type:'dialogue',speakerId:'mentor',text:'Personal story',next:'effect'},{id:'effect',type:'effect',effects:[add(item('book'),1)],next:'end'},{id:'end',type:'end'}]};
 await writeFile(resolve(root,'events/personal.json'),JSON.stringify({...base,id:'personal',label:'Personal gift'}));
 await writeFile(resolve(root,'events/personal-learn.json'),JSON.stringify({...base,id:'personal-learn',label:'Personal learning',requirements:{op:'always'}}));
});}
function session(c){const origin={seed:42,mode:'full',initialOverrides:{attributes:{},flags:{},relationships:{}}},commands=[];
 let a=nativeCreateRun(c,42,origin);assert.deepEqual(a,createRun(c,42,origin));assert.ok(a.ok,JSON.stringify(a));let state=a.state;
 return {origin,commands,get state(){return state;},send(command,ok=true){const n=nativeDispatchCommand(c,state,command,state.stateRevision),ts=dispatchCommand(c,state,command,state.stateRevision);assert.equal(n.ok,ok,JSON.stringify(n));if(ok){assert.deepEqual(n,ts);state=n.state;commands.push(command);}else{assert.equal(ts.ok,false);assert.equal(n.error.code,ts.error.code);assert.deepEqual(n.state,state);assert.deepEqual(ts.state,state);}return n;},async save(){const e=await encodeSave(c,{origin,state,commands});assert.ok(e.ok,JSON.stringify(e));const d=await decodeSave(c,e.text);assert.ok(d.ok,JSON.stringify(d));assert.deepEqual(d.session.state,state);assert.deepEqual(nativeReplay(c,origin,commands),{ok:true,state});return e.text;}};
}
const action=(actorId,actionId)=>({type:'executeActorAction',actorId,actionId,quantity:1});
const finish=s=>{while(s.state.phase==='narrative')s.send({type:'advanceNarrative'});};

test('actor inventory/relationships and personal story cursor survive deterministic save and independent completion',async t=>{
 const f=await setup(t);assert.equal(f.built.ok,true,JSON.stringify(f.built.diagnostics));const c=f.built.snapshot,s=session(c),shared=structuredClone(s.state.relationships);
 s.send(action('apprentice','gift'));assert.equal(s.state.roster.inventory.apprentice.flower,1);assert.equal(s.state.roster.relationships.apprentice.mentor.trust,18);assert.equal(s.state.roster.relationships.mentor.mentor.trust,7);
 assert.equal(s.state.activeNarrative.actorId,'apprentice');assert.equal(s.state.roster.actorId,undefined);await s.save();finish(s);
 assert.deepEqual(s.state.roster.completedEvents.apprentice,['personal']);assert.deepEqual(s.state.completedEvents,[]);assert.equal(s.state.roster.inventory.apprentice.book,1);
 s.send(action('mentor','gift'));await s.save();finish(s);assert.deepEqual(s.state.roster.completedEvents.mentor,['personal']);assert.equal(s.state.roster.inventory.mentor.book,1);assert.deepEqual(s.state.relationships,shared);
 s.send(action('mentor','learn'));assert.equal(s.state.roster.attributes.mentor.skill,45);assert.equal(s.state.attributes.skill,10);finish(s);await s.save();
 const again=session(c);for(const cmd of s.commands)again.send(cmd);assert.deepEqual(again.state,s.state);
});

test('scoped failures rollback all targets and reject unbound actor, invalid owner, missing context and stale version',async t=>{
 const {built}=await setup(t);const c=structuredClone(built.snapshot),s=session(c);
 s.send(action('missing','gift'),false);c.project.roster.members[1].actionIds=['learn'];s.send(action('mentor','gift'),false);
 s.send({type:'executeAction',actionId:'gift',quantity:1},false);
 c.project.gameplay.actions.find(a=>a.id==='learn').effects=[add({kind:'attribute',attributeId:'skill',owner:fixed('apprentice')},10),add(item('flower'),-20)];
 s.send(action('mentor','learn'),false);assert.equal(s.state.attributes.skill,10);
 const stale=nativeDispatchCommand(c,s.state,action('apprentice','gift'),99);assert.equal(stale.ok,false);assert.deepEqual(stale.state,s.state);
});

test('fixed owners and actor activities share one numeric resolver without overwriting another trainee',async t=>{
 const {built}=await setup(t);const c=structuredClone(built.snapshot),s=session(c);
 const activity=c.activities.find(a=>a.id==='rest');activity.baseEffects=[add({kind:'attribute',attributeId:'skill',owner:fixed('apprentice')},2),add({kind:'attribute',attributeId:'skill',owner:actor},3),add(item('flower'),1)];
 s.send({type:'assignActivities',assignments:{apprentice:'rest',mentor:'rest'}});s.send({type:'executeActivity'});
 assert.equal(s.state.roster.attributes.apprentice.skill,17);assert.equal(s.state.roster.attributes.mentor.skill,43);
 assert.equal(s.state.roster.inventory.apprentice.flower,3);assert.equal(s.state.roster.inventory.mentor.flower,2);await s.save();
});

test('owner/member/schema semantics reject missing references, shared attributes and unsupported automatic actor events',async t=>{
 const f=await setup(t),p=JSON.parse(await readFile(resolve(f.root,'project.json'),'utf8'));
 p.roster.members[0].initialInventory.flower=999;p.roster.members[1].initialRelationships.mentor.trust=999;
 p.roster.members[0].actionIds.push('missing');p.gameplay.actions[0].effects.push(add({kind:'attribute',attributeId:'money',owner:fixed('absent')},1));
 await writeFile(resolve(f.root,'project.json'),JSON.stringify(p));const e=JSON.parse(await readFile(resolve(f.root,'events/personal.json'),'utf8'));e.trigger='period-end';await writeFile(resolve(f.root,'events/personal.json'),JSON.stringify(e));
 const result=await buildSnapshot(await loadProject(f.root));assert.equal(result.ok,false);for(const code of ['INITIAL_RANGE','REFERENCE_MISSING','ACTOR_REQUIRED','SHARED_ATTRIBUTE','ACTOR_EVENT_TRIGGER'])assert.ok(result.diagnostics.some(d=>d.code===code),code+JSON.stringify(result.diagnostics));
});

test('actor cursor and individual inventory tampering are rejected; analysis reaches personal stories with actor commands',async t=>{
 const {built}=await setup(t);assert.equal(built.ok,true,JSON.stringify(built.diagnostics));const c=built.snapshot,s=session(c);s.send(action('apprentice','gift'));const text=await s.save();
 for(const mutate of [save=>{save.state.activeNarrative.actorId='mentor';},save=>{save.state.roster.inventory.mentor.flower=9;}]){const bad=JSON.parse(text);mutate(bad);const decoded=await decodeSave(c,JSON.stringify(bad));assert.equal(decoded.ok,false);}
 const result=await runAnalysis(c,{runs:1,seed:42,strategy:'random',maxCommands:120,timeoutMs:10000,target:{kind:'event',id:'personal'},beamWidth:12,maxStates:200});
 assert.equal(result.targetStatus,'found',JSON.stringify(result));assert.ok(result.traces.some(t=>t.commands.some(c=>c.type==='executeActorAction')));
 for(const trace of result.traces)assert.deepEqual(replayTrace(c,trace).state,trace.state);
});

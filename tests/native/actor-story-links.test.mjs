import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {fixture} from '../gameplay/fixture.mjs';
import {createRun,dispatchCommand} from '../../dist/packages/core/run.js';
import {nativeCreateRun,nativeDispatchCommand,nativeReplay} from '../../dist/packages/native-bridge/index.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';
import {evaluateCondition} from '../../dist/packages/core/evaluation.js';
const actor={kind:'actor'},fixed=characterId=>({kind:'character',characterId});
const flag=(flagId,owner=actor)=>({op:'flag',flagId,equals:true,...(owner?{owner}:{})});
const set=(flagId,value,owner=actor)=>({op:'setFlag',flagId,value,...(owner?{owner}:{})});
async function setup(t){return fixture(t,p=>{
 p.roster={stateModel:'character-v1',localAttributeIds:['skill'],members:['apprentice','mentor'].map(characterId=>({characterId,initialAttributes:{skill:10},activityId:'study',actionIds:['talk','bad']}))};
 p.gameplay.actions=[{id:'talk',name:'Talk',costs:[],effects:[],eventId:'personal-choice'},{id:'bad',name:'Bad',costs:[],effects:[set('personal-promise',true),set('missing-flag',true)]}];
 p.initialState.flags['personal-promise']=true; p.initialState.flags['personal-choice-record']=false;
 // The invalid action is used only in a detached snapshot below.
 p.gameplay.actions[1].effects=[];
},async root=>{
 const flagsPath=resolve(root,'flags.json'),defs=JSON.parse(await readFile(flagsPath,'utf8'));
 defs.flags.push(...['personal-promise','personal-choice-record'].map(id=>({id,label:id,initial:false})));await writeFile(flagsPath,JSON.stringify(defs));
 const common={formatVersion:'0.1.0',scope:'actor',priority:80,window:{fromPeriod:0,throughPeriod:35},entryNodeId:'intro',requirements:{op:'always'}};
 const source={...common,id:'personal-choice',label:'Personal choice',trigger:'interaction',repeatable:true,nodes:[
 {id:'intro',type:'dialogue',speakerId:null,text:'Will you return?',next:'ask'},
 {id:'ask',type:'choice',prompt:'Your promise',choices:[
 {id:'yes',text:'Return',requirements:{op:'always'},effects:[set('personal-promise',true),set('personal-choice-record',true)],next:'end'},
 {id:'other',text:'Another promise',requirements:{op:'always'},effects:[set('personal-promise',true)],next:'end'}]},
 {id:'end',type:'end'}]};
 const follow={...common,id:'personal-follow',label:'Follow',trigger:'after-event',requirements:{op:'all',conditions:[{op:'eventCompleted',eventId:source.id,owner:actor},flag('personal-promise'),flag('personal-choice-record')]},nodes:[{id:'intro',type:'dialogue',speakerId:null,text:'Your own promise.',next:'reward'},{id:'reward',type:'effect',effects:[{op:'add',target:{kind:'attribute',attributeId:'skill',owner:actor},value:{op:'literal',value:7}}],next:'end'},{id:'end',type:'end'}]};
 const chained={...common,id:'personal-coda',label:'Coda',trigger:'after-event',requirements:{op:'eventCompleted',eventId:follow.id,owner:actor},nodes:[{id:'intro',type:'dialogue',speakerId:null,text:'Remembered.',next:'end'},{id:'end',type:'end'}]};
 for(const e of [source,follow,chained])await writeFile(resolve(root,`events/${e.id}.json`),JSON.stringify(e));
});}
function session(c){const origin={seed:42,mode:'full',initialOverrides:{attributes:{},flags:{},relationships:{}}},commands=[];
 const initial=nativeCreateRun(c,42,origin);assert.deepEqual(initial,createRun(c,42,origin));assert.ok(initial.ok);let state=initial.state;
 return {get state(){return state;},send(command){const n=nativeDispatchCommand(c,state,command,state.stateRevision),ts=dispatchCommand(c,state,command,state.stateRevision);assert.deepEqual(n,ts);assert.ok(n.ok,JSON.stringify(n));state=n.state;commands.push(command);},async save(){const e=await encodeSave(c,{origin,state,commands});assert.ok(e.ok,JSON.stringify(e));const d=await decodeSave(c,e.text);assert.ok(d.ok,JSON.stringify(d));assert.deepEqual(d.session.state,state);assert.deepEqual(nativeReplay(c,origin,commands),{ok:true,state});state=d.session.state;}};
}
const talk=(s,id)=>s.send({type:'executeActorAction',actorId:id,actionId:'talk',quantity:1});
const advance=s=>s.send({type:'advanceNarrative'});
for(const first of ['apprentice','mentor'])test(`personal story chain isolates promises and survives every cursor restore (${first} first)`,async t=>{
 const {built}=await setup(t);assert.ok(built.ok,JSON.stringify(built.diagnostics));const c=built.snapshot,s=session(c),second=first==='apprentice'?'mentor':'apprentice';
 assert.equal(s.state.flags['personal-promise'],true);
 assert.equal(evaluateCondition(c,s.state,flag('personal-promise',fixed(first))),false,'shared true never initializes personal flag');
 assert.equal(s.state.roster.flags,undefined,'legacy state shape remains unchanged until a personal write');
 talk(s,first);await s.save();advance(s);s.send({type:'choose',choiceId:'yes'});await s.save();advance(s);
 assert.equal(s.state.activeNarrative.id,'personal-follow');assert.equal(s.state.activeNarrative.actorId,first);await s.save();advance(s);advance(s);advance(s);
 assert.equal(s.state.activeNarrative.id,'personal-coda');assert.equal(s.state.activeNarrative.actorId,first);await s.save();advance(s);advance(s);
 assert.equal(s.state.phase,'planning');assert.equal(s.state.roster.attributes[first].skill,17);assert.equal(s.state.roster.attributes[second].skill,10);
 assert.deepEqual(s.state.roster.completedEvents[second],[]);assert.equal(evaluateCondition(c,s.state,flag('personal-choice-record',fixed(second))),false);
 talk(s,second);advance(s);s.send({type:'choose',choiceId:'other'});await s.save();advance(s);
 assert.equal(s.state.phase,'planning');assert.equal(s.state.roster.flags[second]['personal-promise'],true);assert.equal(s.state.roster.flags[second]['personal-choice-record'],undefined);
 assert.deepEqual(s.state.roster.completedEvents[second],['personal-choice']);
 talk(s,second);advance(s);s.send({type:'choose',choiceId:'yes'});advance(s);await s.save();while(s.state.phase==='narrative')advance(s);
 assert.equal(s.state.roster.attributes[second].skill,17);assert.equal(s.state.flags['personal-choice-record'],false);await s.save();
 talk(s,first);advance(s);s.send({type:'choose',choiceId:'yes'});advance(s);assert.equal(s.state.phase,'planning');assert.equal(s.state.roster.attributes[first].skill,17,'follow-up reward once per actor');
 const bad=structuredClone(c);bad.project.gameplay.actions.find(x=>x.id==='bad').effects=[set('personal-promise',false),set('missing-flag',true)];
 const cmd={type:'executeActorAction',actorId:first,actionId:'bad',quantity:1};const n=nativeDispatchCommand(bad,s.state,cmd,s.state.stateRevision),ts=dispatchCommand(bad,s.state,cmd,s.state.stateRevision);
 assert.equal(n.ok,false);assert.equal(ts.ok,false);assert.equal(n.error.code,'UNKNOWN_FLAG');assert.deepEqual(n.state,s.state);assert.deepEqual(ts.state,s.state);
});

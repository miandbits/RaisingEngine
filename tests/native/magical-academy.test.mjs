import test from 'node:test';
import assert from 'node:assert/strict';
import path from 'node:path';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {createRun,dispatchCommand} from '../../dist/packages/core/run.js';
import {nativeCreateRun,nativeDispatchCommand,nativeReplay} from '../../dist/packages/native-bridge/index.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';
const built=await buildSnapshot(await loadProject(path.resolve('projects/magical-academy-reference')));
assert.equal(built.ok,true,JSON.stringify(built.diagnostics));const content=built.snapshot;
function session(){
 const origin={seed:71,mode:'full',initialOverrides:{attributes:{},flags:{},relationships:{}}};
 const first=nativeCreateRun(content,origin.seed,origin);assert.equal(first.ok,true,JSON.stringify(first));assert.deepEqual(first,createRun(content,origin.seed,origin));
 let state=first.state;const commands=[];
 const send=(command,ok=true)=>{const result=nativeDispatchCommand(content,state,command,state.stateRevision);assert.deepEqual(result,dispatchCommand(content,state,command,state.stateRevision));assert.equal(result.ok,ok,JSON.stringify(result));if(ok){state=result.state;commands.push(command);}else assert.deepEqual(result.state,state);return result;};
 return {origin,commands,send,get state(){return state;},act:(id,ok=true)=>send({type:'executeAction',actionId:id,quantity:1},ok)};
}
const choose=s=>['magic','magic','literature','literature','mathematics'].forEach(id=>s.act(`classes-add-${id}`));
const classes=s=>{choose(s);s.act('classes-start');for(let i=0;i<4;i++)s.act('classes-next');s.act('classes-finish');};
const sleep=s=>{s.send({type:'setSchedule',activityIds:['rest']});s.send({type:'executeActivity'});s.send({type:'acknowledgeResult'});s.send({type:'resolvePeriodEnd'});};

test('ordered class queue, guarded helpers, edit/undo, exact mid-class restore and day/night rules',async()=>{
 const s=session();s.act('classes-start',false);s.act('classes-run-magic',false);s.act('study-high',false);s.act('rest-night',false);
 s.act('classes-add-magic');s.act('classes-add-magic');s.act('classes-add-magic',false);s.act('classes-pop');
 assert.equal(s.state.attributes['classes-selected-magic'],1);assert.equal(s.state.attributes['classes-slot-2'],0);
 s.act('classes-clear');choose(s);s.act('classes-add-mathematics',false);
 assert.deepEqual([1,2,3,4,5].map(i=>s.state.attributes[`classes-slot-${i}`]),[1,1,2,2,3]);
 s.act('classes-start');assert.equal(s.state.attributes['magic-mastery'],61);assert.equal(s.state.attributes.int,32);
 s.act('classes-pop',false);s.act('classes-finish',false);
 const savedState=structuredClone(s.state),savedCommands=[...s.commands];
 const saved=await encodeSave(content,{origin:s.origin,state:s.state,commands:savedCommands});assert.equal(saved.ok,true);
 s.act('classes-next');assert.equal(s.state.attributes.int,52);assert.equal(s.state.attributes['magic-mastery'],100);
 const loaded=await decodeSave(content,saved.text);assert.equal(loaded.ok,true);assert.deepEqual(loaded.session.state,savedState);
 assert.deepEqual(nativeReplay(content,s.origin,savedCommands),{ok:true,state:savedState});
 s.act('classes-next');s.act('classes-next');s.act('classes-next');assert.equal(s.state.attributes.int,72);assert.equal(s.state.attributes.stress,10);
 assert.equal(s.state.attributes.energy,10);assert.equal(s.state.attributes.money,500);assert.equal(s.state.periodIndex,0);
 s.act('classes-next',false);s.act('classes-finish');s.act('classes-finish',false);
 s.act('open-academy');const beforeStudy=s.state.attributes.int;s.act('self-study');assert.ok([6,12].includes(s.state.attributes.int-beforeStudy));assert.equal(s.state.attributes.energy,7);
 s.act('rest-night');assert.equal(s.state.attributes.energy,10);assert.equal(s.state.attributes['day-stage'],2);assert.equal(s.state.periodIndex,0);s.act('rest-night',false);
 sleep(s);assert.equal(s.state.periodIndex,1);assert.equal(s.state.attributes.stress,0);assert.equal(s.state.attributes.morality,51);assert.equal(s.state.attributes['classes-count'],0);assert.equal(s.state.attributes['day-stage'],0);
 s.act('self-study',false);assert.deepEqual(nativeReplay(content,s.origin,s.commands),{ok:true,state:s.state});
});

test('36 months complete deterministically without bypassing five classes or finishing early',async()=>{
 const s=session();
 for(let month=0;month<36;month++){
   assert.equal(s.state.periodIndex,month);classes(s);s.act('open-relations');s.act('talk-noah');s.act('talk-noah',false);s.act('rest-night');sleep(s);
   assert.equal(s.state.attributes['classes-latch'],0);assert.equal(s.state.attributes['study-latch'],0);
 }
 assert.equal(s.state.endingId,'observation-boundary');
 while(s.state.phase==='narrative')s.send({type:'advanceNarrative'});
 assert.equal(s.state.phase,'finished');assert.deepEqual(nativeReplay(content,s.origin,s.commands),{ok:true,state:s.state});
 const save=await encodeSave(content,{origin:s.origin,state:s.state,commands:s.commands});assert.equal(save.ok,true);assert.equal((await decodeSave(content,save.text)).ok,true);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import Ajv2020 from 'ajv/dist/2020.js';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import {WasmRuntime} from '../../dist/packages/native-bridge/wasm.js';
import {WebPlayerController} from '../../dist/packages/web-player/controller.js';
import {nativeReplay} from '../../dist/packages/native-bridge/index.js';
import {appointmentFixture} from './story-appointment-fixture.mjs';
const ajv=new Ajv2020({strict:true,allErrors:true});ajv.addSchema(JSON.parse(await readFile('packages/schema/common.schema.json','utf8')));
const validate=ajv.compile(JSON.parse(await readFile('packages/runtime-service/save.schema.json','utf8')));
for(const trigger of ['period-start','after-activity','location-enter'])test(`web appointments: ${trigger}, personal queue and fresh save replay`,async t=>{
 const {content}=await appointmentFixture(t,trigger,{fromPeriod:trigger==='period-start'?1:0,throughPeriod:35});
 const runtime=new WasmRuntime(await createModule());let saved;
 const make=()=>new WebPlayerController(content,runtime,{read:async()=>saved,write:async text=>{saved=text;}},validate);
 let player=make();assert.equal((await player.newRun(null)).error,null);
 const state=()=>player.session.state,version=()=>({generation:player.generation,revision:state().stateRevision});
 const parity=()=>assert.deepEqual(nativeReplay(content,player.session.origin,player.session.commands),{ok:true,state:state()});
 const send=async action=>{assert.equal((await player.act(action,version())).error,null);parity();};
 const restore=async()=>{const before=structuredClone(state());assert.equal((await player.save(version())).error,null);assert.ok(validate(JSON.parse(saved)),JSON.stringify(validate.errors));player=make();assert.equal((await player.load(null)).error,null);assert.deepEqual(state(),before);parity();};
 for(const actorId of ['mentor','apprentice']){await send({type:'executeActorAction',actorId,actionId:'talk',quantity:1});await send({type:'continue'});await send({type:'choose',choiceId:'yes'});await restore();assert.equal(state().phase,'planning');}
 if(trigger==='location-enter'){await send({type:'visit',locationId:'city'});assert.equal(state().phase,'planning');await send({type:'visit',locationId:'market'});}
 else{await send({type:'assignActivities',assignments:{apprentice:'study',mentor:'rest'}});await send({type:'execute'});await restore();await send({type:'continue'});if(trigger==='period-start'){for(let i=0;state().periodIndex===0;i++){assert.ok(i<20);await send({type:'continue'});}}}
 assert.equal(state().activeNarrative.id,'personal-follow');assert.equal(state().activeNarrative.actorId,'apprentice');assert.equal(state().activeNarrative.appointmentTrigger,trigger);
 for(let i=0;state().phase==='narrative';i++){assert.ok(i<20);await restore();await send({type:'continue'});}
 assert.ok(state().roster.completedEvents.apprentice.includes('personal-coda'));
 assert.equal(state().roster.completedEvents.mentor.includes('personal-coda'),trigger!=='after-activity');await restore();
 const before=structuredClone(state()),tampered=JSON.parse(saved);tampered.state.roster.flags.mentor['personal-choice-record']=false;saved=JSON.stringify(tampered);assert.ok((await player.load(version())).error);assert.deepEqual(state(),before);
});

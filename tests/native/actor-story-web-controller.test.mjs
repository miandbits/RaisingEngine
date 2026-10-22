import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import Ajv2020 from 'ajv/dist/2020.js';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import {WasmRuntime} from '../../dist/packages/native-bridge/wasm.js';
import {WebPlayerController} from '../../dist/packages/web-player/controller.js';
import {nativeReplay} from '../../dist/packages/native-bridge/index.js';
import {actorStoryFixture} from './actor-story-fixture.mjs';

const ajv=new Ajv2020({strict:true,allErrors:true});
ajv.addSchema(JSON.parse(await readFile('packages/schema/common.schema.json','utf8')));
const validate=ajv.compile(JSON.parse(await readFile('packages/runtime-service/save.schema.json','utf8')));

for(const first of ['apprentice','mentor'])test(`web actor story: independent records, chained events and fresh save replay (${first} first)`,async t=>{
  const {built}=await actorStoryFixture(t);assert.ok(built.ok,JSON.stringify(built.diagnostics));
  const content=built.snapshot,second=first==='apprentice'?'mentor':'apprentice';
  const runtime=new WasmRuntime(await createModule());let saved;
  const make=()=>new WebPlayerController(content,runtime,{read:async()=>saved,write:async text=>{saved=text;}},validate);
  let player=make();assert.equal((await player.newRun(null)).error,null);
  const version=()=>({generation:player.generation,revision:player.session.state.stateRevision});
  const state=()=>player.session.state;
  const parity=()=>{
    const expected=nativeReplay(content,player.session.origin,player.session.commands);
    assert.equal(expected.ok,true);assert.deepEqual(state(),expected.state);
  };
  const send=async action=>{const reply=await player.act(action,version());assert.equal(reply.error,null);parity();return reply.view;};
  const restore=async()=>{
    const before=structuredClone(state());
    assert.equal((await player.save(version())).error,null);
    assert.equal(validate(JSON.parse(saved)),true,JSON.stringify(validate.errors));
    player=make();assert.equal((await player.load(null)).error,null);
    assert.deepEqual(state(),before);parity();
  };
  const talk=actorId=>send({type:'executeActorAction',actorId,actionId:'talk',quantity:1});
  const next=()=>send({type:'continue'});
  // Pre-feature saves still have no personal flag map and retain their exact shape.
  assert.equal(state().roster.flags,undefined);await restore();assert.equal(state().roster.flags,undefined);
  assert.equal(state().flags['personal-promise'],true);
  await talk(first);await restore();await next();await restore();
  await send({type:'choose',choiceId:'yes'});
  assert.equal(state().activeNarrative.id,'personal-follow');assert.equal(state().activeNarrative.actorId,first);
  await restore();await next();
  assert.equal(state().activeNarrative.id,'personal-coda');assert.equal(state().activeNarrative.actorId,first);
  await restore();await next();
  assert.equal(state().phase,'planning');assert.equal(state().roster.attributes[first].skill,17);
  assert.equal(state().roster.flags[second],undefined);
  await talk(second);await next();await send({type:'choose',choiceId:'other'});
  assert.equal(state().phase,'planning');assert.equal(state().roster.flags[second]['personal-promise'],true);
  assert.equal(state().roster.flags[second]['personal-choice-record'],undefined);
  assert.deepEqual(state().roster.completedEvents[second],['personal-choice']);await restore();
  // Matching the same promise is insufficient; the exact choice belongs to its actor.
  await talk(second);await next();await send({type:'choose',choiceId:'yes'});
  assert.equal(state().activeNarrative.actorId,second);await restore();await next();await next();
  assert.equal(state().roster.attributes[second].skill,17);
  await talk(first);await next();await send({type:'choose',choiceId:'yes'});
  assert.equal(state().phase,'planning');assert.equal(state().roster.attributes[first].skill,17,'no duplicate reward');
  assert.equal(state().flags['personal-choice-record'],false);await restore();
  const before=structuredClone(state()),tampered=JSON.parse(saved);
  tampered.state.roster.flags[second]['personal-choice-record']=false;saved=JSON.stringify(tampered);
  assert.match((await player.load(version())).error,/재현 결과/);assert.deepEqual(state(),before);
});

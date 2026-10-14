import test from 'node:test';
import assert from 'node:assert/strict';
import { fixture } from '../gameplay/fixture.mjs';
import { createRun,dispatchCommand } from '../../dist/packages/core/run.js';
import { calendarTime } from '../../dist/packages/core/evaluation.js';
import { getNarrativeView } from '../../dist/packages/core/narrative.js';
import { getGameplayView } from '../../dist/packages/gameplay/service.js';
import { nativeRequest,nativeCreateRun,nativeDispatchCommand,nativeReplay,closeNativeRuntimes } from '../../dist/packages/native-bridge/index.js';
import { freeze } from '../../dist/packages/core/util.js';

test('C++ view projection exactly matches shared read rules across location, shop and immediate narrative',async t=>{
  const {built}=await fixture(t);assert.equal(built.ok,true);const content=built.snapshot;
  assert.deepEqual(nativeRequest({operation:'view',content,state:null}),{ok:true,time:null,narrative:null,gameplay:null,systems:null});
  let state=createRun(content,42,{mode:'full'}).state;
  const commands=[{type:'visit',locationId:'city'},{type:'visit',locationId:'market'},{type:'buy',shopId:'florist',itemId:'flower',quantity:2},{type:'visit',locationId:'city'},{type:'interact',interactionId:'gift-mentor'},{type:'advanceNarrative'},{type:'advanceNarrative'}];
  for(const command of commands){
    assert.deepEqual(nativeRequest({operation:'view',content,state}),{ok:true,time:calendarTime(content,state),narrative:getNarrativeView(content,state),gameplay:getGameplayView(content,state),systems:null});
    const result=dispatchCommand(content,state,command,state.stateRevision);assert.equal(result.ok,true);state=result.state;
  }
  assert.deepEqual(nativeRequest({operation:'view',content,state}),{ok:true,time:calendarTime(content,state),narrative:getNarrativeView(content,state),gameplay:getGameplayView(content,state),systems:null});
});

test('native adapter preserves immutable state and rejected transaction identity',async t=>{
  const {built}=await fixture(t);const content=built.snapshot;
  const created=nativeCreateRun(content,42,{mode:'full'});assert.equal(created.ok,true);
  assert.equal(Object.isFrozen(created.state.gameplay.inventory),true);
  const rejected=nativeDispatchCommand(content,created.state,{type:'executeActivity'},0);
  assert.equal(rejected.ok,false);assert.equal(rejected.state,created.state);
  for(const command of [undefined,{type:'setSchedule',activityIds:['rest','rest','rest'],extra:undefined},{type:'buy',shopId:'florist',itemId:'flower',quantity:1n}]){
    const expected=dispatchCommand(content,created.state,command,0);
    const actual=nativeDispatchCommand(content,created.state,command,0);
    assert.equal(actual.ok,false);assert.equal(actual.error.code,expected.error.code);assert.equal(actual.error.path,expected.error.path);assert.equal(actual.state,created.state);
  }
});

test('native malformed protocol recovery and missing executable fail without fallback',()=>{
  for(const request of [null,{},[],{operation:123},{operation:'random',seed:1,count:1.5},{operation:'random',seed:1,bound:-1},{operation:'random',seed:1e308}])assert.equal(nativeRequest(request).ok,false);
  assert.equal(nativeRequest({operation:'info'}).implementation,'C++17');
  assert.throws(()=>nativeRequest({operation:'info'},'definitely-not-an-installed-native-runtime.exe'),/ENOENT/);
  closeNativeRuntimes();
});

test('batch replay has the same successful state and addresses the first failing command',async t=>{
  const {built}=await fixture(t);const content=built.snapshot;const origin={seed:42,mode:'full'};
  const commands=[{type:'visit',locationId:'city'},{type:'visit',locationId:'market'},{type:'buy',shopId:'florist',itemId:'flower',quantity:1}];
  let state=createRun(content,42,origin).state;for(const command of commands)state=dispatchCommand(content,state,command,state.stateRevision).state;
  assert.deepEqual(nativeReplay(content,origin,commands),{ok:true,state});
  const failed=nativeReplay(content,origin,[...commands,{type:'visit',locationId:'home'}]);
  assert.equal(failed.ok,false);assert.equal(failed.error.code,'SAVE_REPLAY');assert.equal(failed.error.path,'/commands/3');
  assert.equal(nativeReplay(content,{...origin,seed:-1},commands).error.code,'INVALID_SEED');
});

test('bounded content transport cache never reuses edited content under the same fingerprint',async t=>{
  const {built}=await fixture(t);const content=built.snapshot;
  const old=nativeCreateRun(content,42,{mode:'full'}).state;
  const variants=[];
  for(let n=0;n<20;n++){
    const variant=structuredClone(content);variant.project.initialState.attributes.money=100+n;freeze(variant);variants.push(variant);
    assert.equal(nativeCreateRun(variant,42,{mode:'full'}).state.attributes.money,100+n);
  }
  assert.deepEqual(nativeCreateRun(content,42,{mode:'full'}).state,old);
  const mutable=structuredClone(content);mutable.project.initialState.attributes.money=333;
  assert.equal(nativeCreateRun(mutable,42,{mode:'full'}).state.attributes.money,333);
  mutable.project.initialState.attributes.money=444;
  assert.equal(nativeCreateRun(mutable,42,{mode:'full'}).state.attributes.money,444);
});

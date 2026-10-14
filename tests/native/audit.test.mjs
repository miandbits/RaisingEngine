import test from 'node:test';
import assert from 'node:assert/strict';
import { content as fixtureContent,doc,numeric,replaceEvents,event } from '../runtime/helpers.mjs';
import { createRun,dispatchCommand } from '../../dist/packages/core/run.js';
import { evaluateNumber,evaluateCondition } from '../../dist/packages/core/evaluation.js';
import { nativeRequest,nativeCreateRun,nativeDispatchCommand,nativeReplay } from '../../dist/packages/native-bridge/index.js';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import { WasmRuntime } from '../../dist/packages/native-bridge/wasm.js';
const wasm=new WasmRuntime(await createModule());

test('native/Wasm int32 expression matrix matches reference, including signed multiplication overflow',async()=>{
  const content=await fixtureContent(),state=createRun(content,42,{mode:'full'}).state;
  const values=[-2147483648,-1,0,1,2147483647];
  for(const op of ['add','subtract','multiply'])for(const left of values)for(const right of values){
    const expression={op,left:{op:'literal',value:left},right:{op:'literal',value:right}};
    let expected;
    try{expected={ok:true,value:evaluateNumber(content,state,expression)};}catch(error){expected={ok:false,error:{code:error.code,path:error.path}};}
    for(const actual of [nativeRequest({operation:'evaluate',content,state,expression}),wasm.request({operation:'evaluate',content,state,expression})]){
      // The shared contract is JSON int32: JavaScript's transient -0 serializes
      // as 0 and has no distinct representation in saved states or C++ integers.
      if(expected.ok)assert.deepEqual(actual,JSON.parse(JSON.stringify(expected)),JSON.stringify(expression));
      else {assert.equal(actual.ok,false);assert.equal(actual.error.code,expected.error.code);assert.equal(actual.error.path,expected.error.path);}
    }
  }
  for(const comparator of ['eq','ne','gt','gte','lt','lte']){
    const condition={op:'compare',comparator,left:{op:'literal',value:-2147483648},right:{op:'literal',value:2147483647}};
    assert.equal(nativeRequest({operation:'evaluate',content,state,condition}).value,evaluateCondition(content,state,condition));
  }
});

test('large Korean/emoji narrative roundtrips through native IPC, Wasm and batch replay',async()=>{
  const text=('한글 대사 🌸👩🏽‍🚀 가족 👨‍👩‍👧‍👦\n"따옴표" \\ 경로\t조합 e\u0301\r\n').repeat(1800);
  const content=await fixtureContent(project=>replaceEvents(project,[event('unicode',[{id:'talk',type:'dialogue',speakerId:'mentor',text,next:'end'},{id:'end',type:'end'}])]));
  const origin={seed:42,mode:'full'};
  const commands=[{type:'setSchedule',activityIds:['rest','rest','rest']},{type:'executeActivity'},{type:'acknowledgeResult'},{type:'resolvePeriodEnd'}];
  let state=createRun(content,42,origin).state;
  for(const command of commands){const expected=dispatchCommand(content,state,command,state.stateRevision);assert.equal(expected.ok,true);assert.deepEqual(nativeDispatchCommand(content,state,command,state.stateRevision),expected);assert.deepEqual(wasm.request({operation:'command',content,state,command,expectedRevision:state.stateRevision}),expected);state=expected.state;}
  assert.equal(nativeRequest({operation:'view',content,state}).narrative.node.text,text);
  assert.equal(wasm.request({operation:'view',content,state}).narrative.node.text,text);
  assert.deepEqual(nativeReplay(content,origin,commands),{ok:true,state});
});

test('two replay forks preserve original state and reject wrong fingerprints atomically',async()=>{
  const content=await fixtureContent();const origin={seed:4294967295,mode:'full'};
  const first=[{type:'setSchedule',activityIds:['rest','rest','rest']}];
  const branchA=nativeReplay(content,origin,first);const saved=JSON.stringify(branchA.state);
  const branchB=nativeReplay(content,origin,[...first,{type:'executeActivity'}]);
  assert.equal(branchA.state.stateRevision,1);assert.equal(branchB.state.stateRevision,2);assert.equal(JSON.stringify(branchA.state),saved);
  assert.deepEqual(nativeReplay(content,origin,first),branchA);
  const edited={...content,contentFingerprint:'sha256:'+'f'.repeat(64)};
  const result=nativeDispatchCommand(edited,branchA.state,{type:'executeActivity'},1);
  assert.equal(result.ok,false);assert.equal(result.error.code,'CONTENT_MISMATCH');assert.equal(result.state,branchA.state);
});

test('shallow-frozen content remains uncached when nested data can change',async()=>{
  const original=await fixtureContent();const content=Object.freeze(structuredClone(original));
  content.project.initialState.attributes.money=111;
  assert.equal(nativeCreateRun(content,42,{mode:'full'}).state.attributes.money,111);
  content.project.initialState.attributes.money=222;
  assert.equal(nativeCreateRun(content,42,{mode:'full'}).state.attributes.money,222);
});

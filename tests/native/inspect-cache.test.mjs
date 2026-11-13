import test from 'node:test';
import assert from 'node:assert/strict';
import {fixture} from '../gameplay/fixture.mjs';
import {nativeRequest,closeNativeRuntimes} from '../../dist/packages/native-bridge/index.js';

function processTransport(t){
  const previous=process.env.RAISING_NATIVE_TRANSPORT;
  closeNativeRuntimes();process.env.RAISING_NATIVE_TRANSPORT='process';
  t.after(()=>{closeNativeRuntimes();if(previous===undefined)delete process.env.RAISING_NATIVE_TRANSPORT;else process.env.RAISING_NATIVE_TRANSPORT=previous;});
}

test('immutable inspection reuses native content while preserving successful, rejected and event traces',async t=>{
  processTransport(t);
  const {built}=await fixture(t);assert.ok(built.ok);
  const content=built.snapshot,uncached=structuredClone(content);
  let state=nativeRequest({operation:'create',content:uncached,seed:42,options:{mode:'full'}}).state;
  const commands=[{type:'executeActivity'},{type:'visit',locationId:'city'},{type:'visit',locationId:'market'},
    {type:'buy',shopId:'florist',itemId:'flower',quantity:1},
    {type:'buy',shopId:'florist',itemId:'flower',quantity:99},
    {type:'visit',locationId:'city'},{type:'interact',interactionId:'gift-mentor'}];
  let passed=0,blocked=0;
  for(const command of commands){
    const request={operation:'inspect',state,command,expectedRevision:state.stateRevision};
    const expected=nativeRequest({...request,content:uncached});
    const actual=nativeRequest({...request,content});
    assert.deepEqual(actual,expected,'cached inspection must retain exact state, errors and checks');
    assert.deepEqual(nativeRequest({...request,contentId:'content-1'}),expected,'inspection alone must register the immutable project');
    assert.ok(Array.isArray(actual.trace));
    if(actual.ok){passed++;state=actual.state;}else blocked++;
  }
  assert.ok(passed>=5);assert.equal(blocked,2);
  const subject={operation:'inspect',state,subject:{kind:'event',id:'gift-friendship'}};
  assert.deepEqual(nativeRequest({...subject,content}),nativeRequest({...subject,content:uncached}));
  const stale={operation:'inspect',state,command:{type:'advanceNarrative'},expectedRevision:state.stateRevision-1};
  const rejected=nativeRequest({...stale,content});assert.equal(rejected.ok,false);
  assert.deepEqual(rejected,nativeRequest({...stale,content:uncached}));
  state=nativeRequest({operation:'create',content:uncached,seed:42,options:{mode:'full'}}).state;
  for(const command of [{type:'navigate',screenId:'schedule'},{type:'setSchedule',activityIds:['study','rest','rest']},{type:'executeActivity'}]){
    const request={operation:'inspect',state,command,expectedRevision:state.stateRevision};
    const actual=nativeRequest({...request,content});assert.equal(actual.ok,true);
    assert.deepEqual(actual,nativeRequest({...request,content:uncached}));
    if(command.type==='executeActivity')assert.ok(actual.trace.some(check=>check.kind==='effect'),'successful activity effect checks remain observable');
    state=actual.state;
  }
});

test('inspection never caches mutable or shallow-frozen project rules',async t=>{
  processTransport(t);
  const {built}=await fixture(t);assert.ok(built.ok);
  for(const shallow of [false,true]){
    const content=structuredClone(built.snapshot);
    if(shallow)Object.freeze(content);
    let state=nativeRequest({operation:'create',content,seed:42,options:{mode:'full'}}).state;
    for(const locationId of ['city','market'])state=nativeRequest({operation:'inspect',content,state,command:{type:'visit',locationId},expectedRevision:state.stateRevision}).state;
    const request={operation:'inspect',content,state,command:{type:'buy',shopId:'florist',itemId:'flower',quantity:1},expectedRevision:state.stateRevision};
    const before=nativeRequest(request);assert.equal(before.ok,true);
    content.project.gameplay.shops[0].offers[0].price=17;
    const after=nativeRequest(request);assert.equal(after.ok,true);
    assert.equal(before.state.attributes.money-after.state.attributes.money,7,'same object and fingerprint must observe edited nested rules');
  }
  assert.equal(nativeRequest({operation:'inspect',contentId:'content-1',state:{},command:{type:'executeActivity'},expectedRevision:0}).error.code,'UNKNOWN_CONTENT');
});

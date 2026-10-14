import test from 'node:test';
import assert from 'node:assert/strict';
import { fixture } from '../gameplay/fixture.mjs';
import { createRun, dispatchCommand } from '../../dist/packages/core/run.js';
import { NativeSession } from '../../dist/packages/native-bridge/index.js';

test('native gameplay: editor-authored travel, shopping, gifts, immediate event and rollback match reference',async t=>{
  const {built}=await fixture(t);assert.equal(built.ok,true,JSON.stringify(built));
  const content=built.snapshot;const native=new NativeSession();t.after(()=>native.close());
  let expected=createRun(content,42,{mode:'full'});
  assert.deepEqual(await native.request({operation:'create',content,seed:42,options:{mode:'full'}}),expected);
  assert.equal(expected.ok,true);let state=expected.state;
  const command=async cmd=>{
    const before=structuredClone(state);
    const ts=dispatchCommand(content,state,cmd,state.stateRevision);
    const cpp=await native.request({operation:'command',content,state,command:cmd,expectedRevision:state.stateRevision});
    if(ts.ok)assert.deepEqual(cpp,ts,JSON.stringify(cmd));
    else {assert.equal(cpp.ok,false);assert.equal(cpp.error.code,ts.error.code);assert.equal(cpp.error.path,ts.error.path);assert.deepEqual(cpp.state,before);}
    state=ts.state;return ts;
  };
  await command({type:'navigate',screenId:'schedule'});
  await command({type:'visit',locationId:'market'}); // disconnected
  await command({type:'visit',locationId:'city'});
  await command({type:'interact',interactionId:'gift-mentor'}); // missing item
  await command({type:'visit',locationId:'market'});
  await command({type:'buy',shopId:'florist',itemId:'flower',quantity:4}); // stack overflow
  await command({type:'buy',shopId:'florist',itemId:'flower',quantity:2});
  await command({type:'visit',locationId:'city'});
  await command({type:'interact',interactionId:'gift-mentor'});
  assert.equal(state.activeNarrative.returnPhase,'planning');
  await command({type:'visit',locationId:'home'}); // narrative blocks travel
  await command({type:'advanceNarrative'});await command({type:'advanceNarrative'});
  assert.equal(state.periodIndex,0);assert.equal(state.phase,'planning');
  await command({type:'interact',interactionId:'gift-mentor'}); // cooldown
  await command({type:'setSchedule',activityIds:['rest','rest','rest']});
  await command({type:'executeActivity'});await command({type:'acknowledgeResult'});await command({type:'resolvePeriodEnd'});
  while(state.phase==='narrative')await command({type:'advanceNarrative'});
  await command({type:'interact',interactionId:'gift-mentor'});
  assert.equal(state.gameplay.inventory.flower,0);
  assert.equal(state.relationships.mentor.trust,30);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import { WasmRuntime } from '../../dist/packages/native-bridge/wasm.js';
import { NativeSession } from '../../dist/packages/native-bridge/index.js';
import { createRun,dispatchCommand } from '../../dist/packages/core/run.js';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { runScenario } from '../../dist/packages/runtime-service/scenario.js';
import { fixture } from '../gameplay/fixture.mjs';
const wasm=new WasmRuntime(await createModule());

test('Wasm is the compiled C++ core and retains structured errors after a rejected request',()=>{
  assert.equal(wasm.request({operation:'info'}).implementation,'C++17');
  assert.equal(wasm.request({operation:'create',content:{},seed:1,options:{mode:'full'}}).ok,false);
  assert.equal(wasm.request({operation:123}).error.code,'UNKNOWN_OPERATION');
  assert.equal(wasm.request({operation:'info'}).ok,true);
});

test('Wasm and native PCG32 exact vectors with rejection sampling',async t=>{
  const native=new NativeSession();t.after(()=>native.close());
  for(const seed of [0,1,42,4294967295])for(const bound of [1,10000,2147483649,4294967296]){
    const request={operation:'random',seed,count:256,bound};
    assert.deepEqual(wasm.request(request),await native.request(request));
  }
});

for(const [sample,scenario] of [['academy-demo','certified-route'],['academy-demo','ordinary-route'],['academy-year','knight-year'],['academy-year','scholar-year'],['academy-year','artisan-year'],['academy-year','neighbor-year'],['academy-year','wanderer-year']]){
  test(`Wasm full state and transition parity ${sample}/${scenario}`,async()=>{
    const root=`examples/${sample}`;
    const built=await buildSnapshot(await loadProject(root));assert.equal(built.ok,true);
    const content=built.snapshot;
    const definition=JSON.parse(await readFile(`${root}/scenarios/${scenario}.json`,'utf8'));
    const route=await runScenario(content,definition);assert.equal(route.ok,true,JSON.stringify(route));
    const origin=route.session.origin;
    let ts=createRun(content,origin.seed,origin);
    assert.deepEqual(wasm.request({operation:'create',content,seed:origin.seed,options:origin}),ts);
    let state=ts.state;
    for(const command of route.session.commands){
      const expected=dispatchCommand(content,state,command,state.stateRevision);
      assert.deepEqual(wasm.request({operation:'command',content,state,command,expectedRevision:state.stateRevision}),expected);
      assert.equal(expected.ok,true);state=expected.state;
    }
    assert.equal(state.phase,'finished');assert.deepEqual(state,route.session.state);
  });
}

test('Wasm gift flow, native-error transaction and save-compatible state',async t=>{
  const {built}=await fixture(t);assert.equal(built.ok,true);const content=built.snapshot;
  let state=createRun(content,42,{mode:'full'}).state;
  const commands=[{type:'visit',locationId:'city'},{type:'visit',locationId:'market'},{type:'buy',shopId:'florist',itemId:'flower',quantity:2},{type:'visit',locationId:'city'},{type:'interact',interactionId:'gift-mentor'},{type:'advanceNarrative'},{type:'advanceNarrative'},{type:'interact',interactionId:'gift-mentor'}];
  for(const command of commands){const expected=dispatchCommand(content,state,command,state.stateRevision);const actual=wasm.request({operation:'command',content,state,command,expectedRevision:state.stateRevision});assert.deepEqual(actual,expected);state=expected.state;}
  assert.equal(state.phase,'planning');assert.equal(state.periodIndex,0);assert.equal(state.gameplay.inventory.flower,1);
});

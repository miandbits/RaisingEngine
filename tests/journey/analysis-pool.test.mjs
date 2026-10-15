import test from 'node:test';
import assert from 'node:assert/strict';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {runAnalysisPool} from '../../dist/packages/analysis-service/pool.js';
import {validateOptions} from '../../dist/packages/analysis-service/model.js';
import {replayTrace} from '../../dist/packages/analysis-service/runner.js';
import {freeze} from '../../dist/packages/core/util.js';
const built=await buildSnapshot(await loadProject('tests/fixtures/projects/editor-workbench-lab'));assert.ok(built.ok);const content=freeze(built.snapshot);
const options={runs:20,seed:42,strategy:'random',maxCommands:1000,timeoutMs:60000};
test('one and four real worker threads preserve ordered seeds, all commands, full state and graph counts',{timeout:60000},async()=>{
  for(const strategy of ['random','focus','conserve']){
    const one=[],four=[],input={...options,strategy,attributeId:content.attributes[0].id,runs:strategy==='random'?20:4};
    const serial=await runAnalysisPool(content,{...input,workers:1},{trace:t=>{one.push(t);}});
    const parallel=await runAnalysisPool(content,{...input,workers:4},{trace:t=>{four.push(t);}});
    assert.deepEqual(four,one,`${strategy} complete trace equality`);assert.deepEqual(parallel,serial,`${strategy} aggregates`);
    assert.deepEqual(four.map(t=>t.run),Array.from({length:input.runs},(_,i)=>i));
    assert.deepEqual(four.map(t=>t.seed),Array.from({length:input.runs},(_,i)=>42+i));
  }
});
test('parallel cancellation drains an ordered replayable prefix and stops all threads',{timeout:30000},async()=>{
  const traces=[];let cancelled=false;
  const result=await runAnalysisPool(content,{...options,runs:200,workers:4},{cancelled:()=>cancelled,trace:t=>{traces.push(t);if(traces.length===4)cancelled=true;}});
  assert.equal(result.stopReason,'cancelled');assert.ok(result.completed>=4&&result.completed<200);
  assert.deepEqual(traces.map(t=>t.run),Array.from({length:traces.length},(_,i)=>i));
  assert.deepEqual(replayTrace(content,traces.at(-1)).state,traces.at(-1).state);
  const settled=result.completed;await new Promise(r=>setTimeout(r,100));assert.equal(result.completed,settled);
});
test('parallel timeout returns bounded partial results; thread count validation rejects invalid requests',{timeout:10000},async()=>{
  const traces=[],result=await runAnalysisPool(content,{...options,runs:200,workers:4,timeoutMs:100},{trace:t=>{traces.push(t);}});
  assert.equal(result.stopReason,'time-limit');assert.ok(result.completed<200);
  for(const workers of [0,33,1.5,NaN])assert.throws(()=>validateOptions({...options,workers},content),/1~32/);
  assert.equal(validateOptions(options,content).workers,1);
});
test('coverage-dependent explore keeps serial semantics when four workers requested',{timeout:30000},async()=>{
  const a=await runAnalysisPool(content,{...options,runs:2,strategy:'explore',workers:1});
  const b=await runAnalysisPool(content,{...options,runs:2,strategy:'explore',workers:4});assert.deepEqual(b,a);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile,rm} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {createHash} from 'node:crypto';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {runAnalysis,replayTrace} from '../../dist/packages/analysis-service/runner.js';
import {runAnalysisPool} from '../../dist/packages/analysis-service/pool.js';
import {AnalysisJobs} from '../../dist/packages/analysis-service/jobs.js';
import {initialBalance,observeBalance,mergeBalance} from '../../dist/packages/analysis-service/balance.js';
import {initialPeriodSeries,observePeriodSeries,mergePeriodSeries} from '../../dist/packages/analysis-service/balance-series.js';
import {compareBalanceJobs} from '../../dist/packages/analysis-service/balance-compare.js';
import {createRun,dispatchCommand,runtimeBackend} from '../../dist/packages/native-bridge/backend.js';
assert.equal(runtimeBackend(),'native','Use the actual native rule engine.');
const cache=resolve('.cache/balance-series');await mkdir(cache,{recursive:true});
const root=await mkdtemp(join(cache,'game-'));await cp('examples/academy-demo',root,{recursive:true});
for(const dir of ['activities','events','scenarios']){await rm(join(root,dir),{recursive:true,force:true});await mkdir(join(root,dir));}
const project=JSON.parse(await readFile(join(root,'project.json')));project.durationMonths=2;await writeFile(join(root,'project.json'),JSON.stringify(project));
const add=(attributeId,value)=>({op:'add',target:{kind:'attribute',attributeId},value:{op:'literal',value}});
await writeFile(join(root,'activities/rest.json'),JSON.stringify({formatVersion:'0.1.0',id:'rest',label:'Rest and earn',description:'A deterministic observation fixture.',requirements:{op:'always'},costs:[],baseEffects:[add('money',2)]}));
await writeFile(join(root,'events/repair.json'),JSON.stringify({formatVersion:'0.1.0',id:'repair',label:'Repair then refund',window:{fromPeriod:0,throughPeriod:0},requirements:{op:'always'},priority:100,entryNodeId:'cost',nodes:[{id:'cost',type:'effect',effects:[add('money',-9)],next:'refund'},{id:'refund',type:'effect',effects:[add('money',9)],next:'end'},{id:'end',type:'end'}]}));
const built=await buildSnapshot(await loadProject(root));assert.ok(built.ok,JSON.stringify(built.diagnostics));const content=built.snapshot;
const options={runs:3,seed:42,strategy:'random',balance:'sampled-v1',maxCommands:1000,timeoutMs:60000};
const result=await runAnalysis(content,options);
function job(id,value=result,changes={}){return {id,root,sourceVersion:'saved',revision:null,fingerprint:content.contentFingerprint,snapshotHash:'same-snapshot',createdAt:'2026-10-26T00:00:00.000Z',status:'completed',options:structuredClone(options),result:structuredClone(value),error:null,...changes};}
function stateAt(period,money,phase='planning'){const state=createRun(content,42,{mode:'full'}).state;return {...state,periodIndex:period,phase,attributes:{...state.attributes,money}};}
const sha=bytes=>createHash('sha256').update(bytes).digest('hex');

test('period observations are the last actual accepted state, not candidates or intra-period minima',()=>{
 assert.equal(result.outcomes.finished,3);
 for(const trace of result.traces){
  let state=createRun(content,trace.seed,{mode:'full'}).state;const expected=new Map([[state.periodIndex,{state,index:0}]]),all=[state.attributes.money];
  for(const [index,command] of trace.commands.entries()){const reply=dispatchCommand(content,state,command,state.stateRevision);assert.ok(reply.ok);state=reply.state;expected.set(state.periodIndex,{state,index:index+1});all.push(state.attributes.money);}
  const series=trace.balance.periodSeries;assert.equal(series.stride,1);assert.equal(series.observedPeriods,expected.size);assert.equal(series.points.length,expected.size);
  for(const point of series.points){const observed=expected.get(point.periodIndex);assert.equal(point.commandIndex,observed.index);assert.equal(point.phase,observed.state.phase);assert.equal(point.values.money,observed.state.attributes.money);assert.equal(point.complete,true);}
  assert.equal(trace.balance.attributes.money.min,Math.min(...all));assert.ok(series.points[0].values.money>trace.balance.attributes.money.min,'Refund is recorded at period end; the separate extrema report retains the repair dip.');
  assert.deepEqual(replayTrace(content,trace).state,trace.state);
 }
 for(const point of result.balance.periodSeries.points){assert.equal(point.runs,3);assert.equal(point.completeRuns,3);assert.equal(point.attributes.money.count,3);}
});

test('stopped runs contribute only observed periods, with one value per run and explicit termination counts',()=>{
 const stopped=initialBalance(content,stateAt(0,10));observeBalance(stopped,stateAt(0,4),1);stopped.termination='command-limit';
 const finished=initialBalance(content,stateAt(0,20));observeBalance(finished,stateAt(0,18),1);observeBalance(finished,stateAt(1,16),2);observeBalance(finished,stateAt(2,15,'finished'),3);finished.termination='finished';
 const aggregate=mergeBalance(mergeBalance(undefined,stopped,0),finished,1),series=aggregate.periodSeries;
 assert.equal(series.observedRuns,2);assert.equal(series.observedPeriods,4);assert.equal(series.recordedPeriods,4);
 assert.deepEqual(series.points.map(p=>[p.periodIndex,p.runs,p.completeRuns,p.attributes.money.sum,p.attributes.money.count]),[[0,2,1,22,2],[1,1,1,16,1],[2,1,1,15,1]]);
 assert.equal(series.points[0].terminations['command-limit'],1);assert.equal(series.points[2].terminations.finished,1);assert.equal(series.points[1].terminations.finished,undefined);
 assert.equal(aggregate.terminations['command-limit'],1);assert.equal(aggregate.terminations.finished,1);
});

test('long games and large attribute catalogs have explicit bounded sampling and no invented terminal point',()=>{
 const long={...content,project:{...content.project,durationMonths:1200,periodsPerMonth:12},attributes:Array.from({length:90},(_,i)=>({id:`value-${String(i).padStart(3,'0')}`}))};
 const state={...stateAt(0,0),attributes:Object.fromEntries(long.attributes.map(a=>[a.id,7]))},series=initialPeriodSeries(long,state);
 for(let period=0;period<=14400;period++)observePeriodSeries(series,{...state,periodIndex:period,phase:period===14400?'finished':'planning'},period);
 assert.equal(series.attributeIds.length,64);assert.equal(series.omittedAttributeIds.length,26);assert.ok(series.points.length<=256);assert.equal(series.points.at(-1).periodIndex,14400);assert.equal(series.observedPeriods,14401);
 const partial=initialPeriodSeries(long,state);observePeriodSeries(partial,state,0);observePeriodSeries(partial,{...state,periodIndex:1},1);
 const aggregate=mergePeriodSeries(undefined,partial,'time-limit');assert.deepEqual(aggregate.points.map(p=>p.periodIndex),[0]);assert.equal(aggregate.points[0].completeRuns,1);assert.deepEqual(aggregate.points[0].terminations,{});assert.equal(aggregate.observedPeriods,2);assert.equal(aggregate.recordedPeriods,1);
});

test('ordered aggregation is identical with one and four workers, and opt-in never changes accepted commands',async()=>{
 const one=await runAnalysisPool(content,{...options,runs:8,workers:1}),four=await runAnalysisPool(content,{...options,runs:8,workers:4});assert.deepEqual(one,four);
 const {balance,...plainOptions}=options,plain=await runAnalysis(content,plainOptions);assert.deepEqual(result.traces.map(t=>t.commands),plain.traces.map(t=>t.commands));assert.deepEqual(result.outcomes,plain.outcomes);assert.equal(plain.balance,undefined);
});

test('comparison discloses content and setup changes, retains labels/counts and leaves both reports untouched',()=>{
 const before=job('before'),after=job('after',result,{fingerprint:'sha256:changed',snapshotHash:'changed-snapshot'});
 after.result.balance.attributes.money.label='Renamed credits';for(const point of after.result.balance.periodSeries.points){const metric=point.attributes.money;metric.sum+=metric.count*2;metric.min+=2;metric.max+=2;}
 const saved=JSON.stringify([before,after]),comparison=compareBalanceJobs(before,after);assert.equal(comparison.status,'matched-setup');assert.equal(comparison.contentChanged,true);assert.equal(comparison.snapshotChanged,true);
 const money=comparison.attributes.find(a=>a.id==='money');assert.equal(money.label,'Renamed credits');assert.equal(money.beforeLabel,before.result.balance.attributes.money.label);assert.ok(money.points.every(p=>p.meanDifference===2&&p.before.count===3&&p.after.count===3));assert.equal(JSON.stringify([before,after]),saved);comparison.before.terminations.finished=999;comparison.after.options.seed=999;money.points[0].before.terminations.finished=999;assert.equal(JSON.stringify([before,after]),saved,'The returned comparison must not alias the original jobs.');
 for(const patch of [{seed:9},{maxCommands:20},{timeoutMs:1000},{runs:1},{workers:4},{priorityProfile:{formatVersion:1,priorities:[{attributeId:'money',direction:'increase',weight:2}],bands:[]}}]){const changed=structuredClone(after);Object.assign(changed.options,patch);const compared=compareBalanceJobs(before,changed);assert.equal(compared.status,'changed-setup');assert.ok(compared.setupChanges.includes(Object.keys(patch)[0]));}
});

test('incompatible or legacy reports never fabricate differences; unobserved later periods remain null',()=>{
 const before=job('before');for(const change of [j=>{delete j.result.balance.periodSeries;},j=>{delete j.result.balance;}]){const old=job('old');change(old);const compare=compareBalanceJobs(old,before);assert.equal(compare.status,'not-recorded');assert.deepEqual(compare.attributes,[]);}
 for(const change of [j=>j.result.balance.periodSeries.axis.periodsPerMonth++,j=>j.result.balance.periodSeries.engineVersion='different-core',j=>j.result.balance.periodSeries.runnerVersion='different-runner',j=>j.options.target={kind:'ending',id:'ordinary'},j=>j.status='running']){const changed=job('changed');change(changed);const comparison=compareBalanceJobs(before,changed);assert.equal(comparison.status,'incomparable');assert.deepEqual(comparison.attributes,[]);}
 const changed=job('changed');changed.result.balance.attributes.money.declaredMax++;const metric=compareBalanceJobs(before,changed).attributes.find(a=>a.id==='money');assert.equal(metric.comparable,false);assert.ok(metric.points.every(p=>p.meanDifference===null));
 const stopped=job('stopped');stopped.result.balance.periodSeries.points=stopped.result.balance.periodSeries.points.slice(0,1);stopped.result.stopReason='time-limit';stopped.result.completed=1;const compare=compareBalanceJobs(before,stopped);assert.ok(compare.reasons.includes('INCOMPLETE_RUN_COHORT'));assert.ok(compare.reasons.includes('OBSERVED_RUN_COUNTS_DIFFER'));assert.ok(compare.attributes.find(a=>a.id==='money').points.slice(1).every(p=>p.after===null&&p.meanDifference===null));
});

test('stored immutable reports compare after project edits, validate snapshots and never write archives or game content',{timeout:30000},async()=>{
 const jobs=new AnalysisJobs();async function run(){let j=await jobs.start(root,content,'saved',null,{...options,runs:1});for(let i=0;i<400&&j.status==='running';i++){await new Promise(r=>setTimeout(r,25));j=await jobs.get(root,j.id);}assert.equal(j.status,'completed');return j;}
 const before=await run(),after=await run(),files=[join(root,'.studio/analysis',before.id+'.json'),join(root,'.studio/analysis',after.id+'.json'),join(root,'project.json')];
 const original=await Promise.all(files.map(f=>readFile(f)));const edited=JSON.parse(original[2]);edited.title='A later authoring edit';await writeFile(files[2],JSON.stringify(edited));
 const hashes=await Promise.all(files.map(async f=>sha(await readFile(f))));const comparison=await jobs.compareBalance(root,before.id,after.id);assert.equal(comparison.status,'matched-setup');assert.deepEqual(await Promise.all(files.map(async f=>sha(await readFile(f)))),hashes);
 await writeFile(join(cache,'comparison.json'),JSON.stringify(comparison,null,2));await writeFile(join(cache,'current-job.json'),JSON.stringify(after,null,2));await writeFile(join(cache,'before-job.json'),JSON.stringify(before,null,2));
 const snapshot=join(root,'.studio/analysis',before.id+'.snapshot.json');const bytes=await readFile(snapshot),broken=JSON.parse(bytes);broken.project.title='Tampered archive';await writeFile(snapshot,JSON.stringify(broken));await assert.rejects(jobs.compareBalance(root,before.id,after.id),/snapshot|스냅샷|hash|해시/i);await writeFile(snapshot,bytes);
});

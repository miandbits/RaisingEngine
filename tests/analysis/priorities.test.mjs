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
import {validateOptions} from '../../dist/packages/analysis-service/model.js';
import {validatePriorityProfile,priorityScore} from '../../dist/packages/analysis-service/priorities.js';
import {initialBalance,observeBalance} from '../../dist/packages/analysis-service/balance.js';
import {createRun,dispatchCommand,runtimeBackend} from '../../dist/packages/native-bridge/backend.js';
assert.equal(runtimeBackend(),'native','These tests require the actual native rule engine.');
const base={runs:2,seed:42,strategy:'priorities',maxCommands:1000,timeoutMs:60000};
const profile={formatVersion:1,priorities:[{attributeId:'skill',direction:'increase',weight:4}],bands:[{attributeId:'money',min:7},{attributeId:'stress',max:5}]};
const built=await buildSnapshot(await loadProject('examples/academy-demo'));assert.equal(built.ok,true);const original=built.snapshot;
const add=(attributeId,value)=>({op:'add',target:{kind:'attribute',attributeId},value:{op:'literal',value}});
async function fixture(withSetback=false){
 await mkdir('.cache/balance-priorities',{recursive:true});const root=await mkdtemp(resolve('.cache/balance-priorities/game-'));await cp('examples/academy-demo',root,{recursive:true});
 for(const folder of ['events','activities','scenarios']){await rm(join(root,folder),{recursive:true,force:true});await mkdir(join(root,folder));}
 const p=JSON.parse(await readFile(join(root,'project.json')));p.durationMonths=1;await writeFile(join(root,'project.json'),JSON.stringify(p));
 const attributes=JSON.parse(await readFile(join(root,'attributes.json')));for(const row of attributes.attributes){row.max=20;if(row.id==='money')row.initial=10;if(row.id==='stamina')row.initial=20;}
 await writeFile(join(root,'attributes.json'),JSON.stringify(attributes));
 const activities=withSetback?[{id:'rest',costs:[],baseEffects:[]}]:[
  {id:'train',costs:[{attributeId:'money',amount:3}],baseEffects:[add('skill',6),add('stress',4)]},
  {id:'rest',costs:[],baseEffects:[add('money',2),add('stress',-3)]},
  {id:'tempting',costs:[{attributeId:'money',amount:8}],baseEffects:[add('skill',20),add('stress',15)]}
 ];for(const activity of activities)await writeFile(join(root,'activities',activity.id+'.json'),JSON.stringify({formatVersion:'0.1.0',label:activity.id,description:activity.id,requirements:{op:'always'},...activity}));
 if(withSetback)await writeFile(join(root,'events/setback.json'),JSON.stringify({formatVersion:'0.1.0',id:'setback',label:'Unavoidable repair',window:{fromPeriod:0,throughPeriod:0},requirements:{op:'always'},priority:100,entryNodeId:'cost',nodes:[
  {id:'cost',type:'effect',effects:[add('money',-9),add('stress',10)],next:'refund'},
  {id:'refund',type:'effect',effects:[add('money',9),add('stress',-10)],next:'end'},
  {id:'end',type:'end'}]}));
 const result=await buildSnapshot(await loadProject(root));assert.equal(result.ok,true,JSON.stringify(result.diagnostics));return {root,content:result.snapshot};
}
const regular=await fixture(),setback=await fixture(true);
function measured(content,trace,policy){const first=createRun(content,trace.seed,{mode:'full'});assert.ok(first.ok);let state=first.state;const balance=initialBalance(content,state,policy);for(const [index,command]of trace.commands.entries()){const r=dispatchCommand(content,state,command,state.stateRevision);assert.ok(r.ok);state=r.state;observeBalance(balance,state,index+1);}if(trace.balance?.termination)balance.termination=trace.balance.termination;return balance;}

test('profile validation bounds metadata and accepts resource/condition preferences without mutating input',()=>{
 const input={...profile,id:'careful',name:'Careful learner'};const copy=validatePriorityProfile(input,regular.content);assert.deepEqual(copy,input);copy.priorities[0].weight=9;assert.equal(input.priorities[0].weight,4);
 for(const invalid of [
  {...profile,formatVersion:2},{...profile,unknown:true},{...profile,priorities:[],bands:[]},{...profile,id:'x'},
  {...profile,priorities:[{attributeId:'missing',direction:'increase',weight:1}]},
  {...profile,priorities:[{attributeId:'skill',direction:['increase'],weight:1}]},
  {...profile,priorities:[{attributeId:'skill',direction:'increase',weight:1.5}]},
  {...profile,priorities:Array(33).fill(profile.priorities[0])},
  {...profile,bands:[{attributeId:'skill',min:0}]},{...profile,bands:[{attributeId:'money'}]},
  {...profile,bands:[{attributeId:'money',min:-1}]},{...profile,bands:[{attributeId:'money',min:12,max:8}]},
  {...profile,bands:[{attributeId:'money',min:7},{attributeId:'money',max:15}]}
 ])assert.throws(()=>validatePriorityProfile(invalid,regular.content));
 assert.doesNotThrow(()=>validatePriorityProfile({...profile,bands:[{attributeId:'money',min:0,max:20},{attributeId:'stress',max:20}]},regular.content));
 assert.throws(()=>validateOptions({...base},regular.content),/profile/);assert.throws(()=>validateOptions({...base,priorityProfile:profile,target:{kind:'ending',id:'ordinary'}},regular.content),/target/);
 assert.throws(()=>validateOptions({...base,priorityProfile:profile},{...regular.content,project:{...regular.content.project,roster:{}}}),/single-character/);
});
test('draft profile repair rejects malformed referenced attributes but tolerates unrelated broken content',()=>{
 const damaged={...regular.content,events:null,attributes:[...regular.content.attributes,null,{id:'unrelated',label:9,min:'broken'}]};assert.doesNotThrow(()=>validatePriorityProfile(profile,damaged));
 for(const patch of [{kind:'unknown'},{label:null},{min:'0'},{max:undefined},{min:15,max:5},{initial:99},{min:-2147483649},{max:Infinity}]){
  const draft={...regular.content,attributes:regular.content.attributes.map(row=>row.id==='money'?{...row,...patch}:row)};assert.throws(()=>validatePriorityProfile(profile,draft),/Repair the referenced attribute/);
 }
 assert.throws(()=>validatePriorityProfile(profile,{...regular.content,attributes:[...regular.content.attributes,regular.content.attributes.find(a=>a.id==='money')]}),/Repair the referenced attribute/);
});
test('legacy strategies remain byte-for-byte identical to captured9bec baseline',async()=>{
 const hashes={random:'4c1097592782c965aca39dcc918e32c6fb256f76539075ead8bc26480b92d5e7',focus:'dc8f29e75ad8ad424aae235e1014e517fb51ee40f44baea9550e1649114b28f6',conserve:'9ed5fdfa51c8f9ac2c633eb10e325a75fe9347e4db644bd9182c433f63c90fd2',explore:'55ab122952eee5203f93ab668e91afd99b3a0700b734914b05bfd9780d36cbe8'};
 for(const [strategy,hash]of Object.entries(hashes)){const result=await runAnalysis(original,{...base,strategy,attributeId:'skill'});assert.equal(createHash('sha256').update(JSON.stringify(result)).digest('hex'),hash,strategy);assert.equal(result.balance,undefined);}
});
test('priorities use actual candidate outcomes, protect lower and upper bands, and never count unchosen candidates',async()=>{
 const result=await runAnalysis(regular.content,{...base,priorityProfile:profile});assert.equal(result.outcomes.finished,2);assert.equal(result.balance.observedRuns,2);
 for(const trace of result.traces){const plans=trace.commands.filter(command=>command.type==='setSchedule');assert.equal(plans[0].activityIds[0],'train');assert.ok(!plans.some(command=>command.activityIds.includes('tempting')));assert.deepEqual(trace.balance,measured(regular.content,trace,profile));assert.equal(trace.balance.observedStates,trace.commands.length+1);assert.deepEqual(replayTrace(regular.content,trace).state,trace.state);}
 assert.equal(result.balance.attributes.money.runsBelowBand,0);assert.equal(result.balance.attributes.stress.runsAboveBand,0);assert.ok(result.balance.attributes.skill.max<20);assert.ok(result.balance.attributes.stress.max<15);
 const first=createRun(regular.content,1,{mode:'full'}).state;const deficient={...first,attributes:{...first.attributes,money:1}};const recovering={...first,attributes:{...first.attributes,money:3}};assert.ok(priorityScore(regular.content,deficient,recovering,profile).deficit<priorityScore(regular.content,deficient,deficient,profile).deficit);
});
test('decrease preferences and initial band violations are measured without changing game values',async()=>{
 const decreasing={formatVersion:1,priorities:[{attributeId:'stress',direction:'decrease',weight:2}],bands:[]};
 const before=createRun(regular.content,1,{mode:'full'}).state;const high={...before,attributes:{...before.attributes,stress:10}},low={...before,attributes:{...before.attributes,stress:4}};
 assert.equal(priorityScore(regular.content,high,low,decreasing).gain,.6);assert.equal(priorityScore(regular.content,low,high,decreasing).gain,-.6);
 const initialBreach={formatVersion:1,priorities:[],bands:[{attributeId:'money',min:15}]};const result=await runAnalysis(regular.content,{...base,runs:1,priorityProfile:initialBreach});
 assert.equal(result.balance.attributes.money.firstBelow.commandIndex,0);assert.equal(result.balance.attributes.money.firstBelow.periodIndex,0);assert.equal(result.balance.attributes.money.runsBelowBand,1);
 assert.equal(replayTrace(regular.content,result.traces[0],0).state.attributes.money,10);
});
test('unavoidable intermediate breaches remain visible after recovery and replay at exact witness commands',async()=>{
 const result=await runAnalysis(setback.content,{...base,runs:1,priorityProfile:profile});assert.equal(result.outcomes.finished,1);const trace=result.traces[0],money=result.balance.attributes.money,stress=result.balance.attributes.stress;
 assert.equal(trace.state.attributes.money,10);assert.equal(trace.state.attributes.stress,0);assert.equal(money.min,1);assert.equal(money.runsBelowBand,1);assert.equal(stress.max,10);assert.equal(stress.runsAboveBand,1);
 assert.equal(replayTrace(setback.content,trace,money.firstBelow.commandIndex).state.attributes.money,1);assert.equal(replayTrace(setback.content,trace,stress.firstAbove.commandIndex).state.attributes.stress,10);
 assert.equal(money.firstBelow.run,trace.run);assert.deepEqual(trace.balance,measured(setback.content,trace,profile));assert.equal(stress.runsAtMin,1,'Initial bound contact is counted once per run');
});
test('opt-in metrics preserve existing strategy decisions and separate incomplete/cancelled observations',async()=>{
 for(const strategy of ['random','focus','conserve','explore']){
  const options={...base,runs:1,strategy,attributeId:'skill'};const plain=await runAnalysis(regular.content,options),observed=await runAnalysis(regular.content,{...options,balance:'sampled-v1',priorityProfile:profile});
  assert.deepEqual(observed.traces.map(t=>t.commands),plain.traces.map(t=>t.commands),strategy);assert.deepEqual(observed.outcomes,plain.outcomes);assert.deepEqual(observed.traces[0].balance,measured(regular.content,observed.traces[0],profile));
 }
 const limited=await runAnalysis(regular.content,{...base,runs:1,maxCommands:1,priorityProfile:profile});assert.equal(limited.outcomes.finished??0,0);assert.equal(limited.balance.observedRuns,1);assert.equal(limited.balance.observedStates,1);assert.equal(limited.balance.terminations['command-limit'],1);
 let stop=false;const cancelled=await runAnalysis(regular.content,{...base,runs:50,priorityProfile:profile},{cancelled:()=>stop,progress:result=>{if(result.completed)stop=true;}});assert.equal(cancelled.stopReason,'cancelled');assert.equal(cancelled.completed,1);assert.equal(cancelled.balance.observedRuns,1);assert.equal(cancelled.requested,50);assert.equal(cancelled.balance.terminations.finished,1);
 const never=await runAnalysis(regular.content,{...base,priorityProfile:profile},{cancelled:()=>true});assert.equal(never.completed,0);assert.equal(never.balance,undefined);
});
test('per-run termination distinguishes low budgets, timeouts, cancellation and actual blocked states',async()=>{
 await assert.rejects(()=>runAnalysis(regular.content,{...base,maxCommands:0,priorityProfile:profile}),/1~10000/);
 let ticks=0;const timeout=await runAnalysis(regular.content,{...base,runs:10,timeoutMs:100,priorityProfile:profile},{now:()=>ticks+=30});assert.equal(timeout.stopReason,'time-limit');assert.equal(timeout.balance.terminations['time-limit'],1);assert.equal(timeout.balance.observedRuns,1);assert.equal(timeout.outcomes.finished??0,0);
 let checks=0;const cancelled=await runAnalysis(regular.content,{...base,runs:10,priorityProfile:profile},{cancelled:()=>++checks>=4});assert.equal(cancelled.stopReason,'cancelled');assert.equal(cancelled.balance.terminations.cancelled,1);assert.equal(cancelled.balance.observedRuns,1);assert.equal(cancelled.outcomes.finished??0,0);
 const capped=await runAnalysis(regular.content,{...base,runs:1,maxCommands:2,priorityProfile:profile});assert.equal(capped.balance.terminations['command-limit'],1);assert.equal(capped.outcomes.finished??0,0);
 const root=await mkdtemp(resolve('.cache/balance-priorities/blocked-'));await cp(regular.root,root,{recursive:true});await rm(join(root,'activities'),{recursive:true});await mkdir(join(root,'activities'));await writeFile(join(root,'activities/locked.json'),JSON.stringify({formatVersion:'0.1.0',id:'locked',label:'Unavailable',description:'Requires more credits than the run has.',requirements:{op:'always'},costs:[{attributeId:'money',amount:20}],baseEffects:[]}));
 const built=await buildSnapshot(await loadProject(root));assert.ok(built.ok,JSON.stringify(built.diagnostics));const blocked=await runAnalysis(built.snapshot,{...base,runs:1,priorityProfile:profile});assert.equal(blocked.balance.terminations.blocked,1);assert.equal(blocked.balance.terminations['command-limit'],undefined);assert.equal(blocked.balance.observedStates,1);
});
test('one and four workers preserve full priority traces and ordered witness metrics',async()=>{
 const one=[],four=[],options={...base,runs:8,priorityProfile:{...profile,id:'careful',name:'Careful learner'}};
 const serial=await runAnalysisPool(setback.content,{...options,workers:1},{trace:t=>one.push(t)}),parallel=await runAnalysisPool(setback.content,{...options,workers:4},{trace:t=>four.push(t)});
 assert.deepEqual(four,one);assert.deepEqual(parallel,serial);assert.equal(parallel.balance.attributes.money.firstBelow.run,0);assert.equal(parallel.balance.observedRuns,8);
});
test('pool timeout is classified as time-limit rather than the worker cancellation transport',{timeout:15000},async()=>{
 const root=await mkdtemp(resolve('.cache/balance-priorities/long-'));await cp(setback.root,root,{recursive:true});const file=join(root,'project.json'),project=JSON.parse(await readFile(file));project.durationMonths=1200;project.periodsPerMonth=12;await writeFile(file,JSON.stringify(project));
 const built=await buildSnapshot(await loadProject(root));assert.ok(built.ok,JSON.stringify(built.diagnostics));const traces=[];
 const result=await runAnalysisPool(built.snapshot,{...base,runs:100,maxCommands:10000,timeoutMs:1000,workers:4,priorityProfile:profile},{trace:t=>traces.push(t)});
 assert.equal(result.stopReason,'time-limit');assert.ok(result.completed<100);assert.ok(traces.some(t=>t.balance.termination==='time-limit'));assert.equal(result.balance.terminations.cancelled,undefined);
 assert.equal(Object.values(result.balance.terminations).reduce((a,b)=>a+b,0),result.balance.observedRuns);assert.equal(result.balance.observedRuns,traces.length);
});
test('archived jobs retain immutable profile, labels and replayable breach witnesses after project edits',async()=>{
 const jobs=new AnalysisJobs(),input={...base,runs:1,priorityProfile:{...profile,id:'careful',name:'Careful learner'}};let job=await jobs.start(setback.root,setback.content,'draft-test',1,input);input.priorityProfile.bands[0].min=0;
 const file=join(setback.root,'attributes.json'),attrs=JSON.parse(await readFile(file));attrs.attributes.find(a=>a.id==='money').label='Changed later';await writeFile(file,JSON.stringify(attrs));
 for(let i=0;i<200&&job.status==='running';i++){await new Promise(r=>setTimeout(r,25));job=await jobs.get(setback.root,job.id);}assert.equal(job.status,'completed');assert.equal(job.options.priorityProfile.bands[0].min,7);assert.equal(job.options.priorityProfile.name,'Careful learner');
 const metric=job.result.balance.attributes.money;assert.notEqual(metric.label,'Changed later');const replay=await jobs.replay(setback.root,job.id,metric.firstBelow.run,metric.firstBelow.commandIndex);assert.equal(replay.state.attributes.money,1);assert.equal((await jobs.stored(setback.root,job.id)).content.contentFingerprint,setback.content.contentFingerprint);
});

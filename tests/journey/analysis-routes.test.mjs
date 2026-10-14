import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,cp,readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {AnalysisJobs} from '../../dist/packages/analysis-service/jobs.js';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';

async function fixture(){await mkdir('.cache/analysis-routes-tests',{recursive:true});const root=await mkdtemp(path.resolve('.cache/analysis-routes-tests/run-'));await cp('examples/academy-demo',root,{recursive:true});const built=await buildSnapshot(await loadProject(root));assert.ok(built.ok);return {root,content:built.snapshot};}
async function completed(jobs,root,id){for(let i=0;i<200;i++){const job=await jobs.get(root,id);if(job.status!=='running')return job;await delay(50);}throw Error('Analysis timed out');}
const options={runs:40,seed:42,strategy:'random',maxCommands:500,timeoutMs:60000};
test('every run is retained, route visits differ from run coverage, filters and exact prefix replay use immutable records',async()=>{
  const {root,content}=await fixture(),jobs=new AnalysisJobs();let job=await jobs.start(root,content,'draft-version',7,options);job=await completed(jobs,root,job.id);assert.equal(job.status,'completed',job.error);assert.equal(job.result.completed,40);
  const all=await jobs.runs(root,job.id,{limit:500});assert.equal(all.total,40);assert.equal(all.rows.length,40);assert.equal(job.result.routeGraph.runCount,40);
  const indexText=await readFile(path.join(root,'.studio','analysis',`${job.id}.runs.jsonl`),'utf8');assert.equal(JSON.parse(indexText.split('\n')[0]).encoding,'dictionary-v1');assert.ok(Buffer.byteLength(indexText)<Buffer.byteLength(JSON.stringify(all.rows))/2,'shared path IDs should not be duplicated in every run index');
  const light=await jobs.get(root,job.id,false);assert.equal(light.result.completed,40);assert.equal(light.result.routeGraph,undefined);assert.deepEqual(light.result.outcomes,job.result.outcomes);
  for(const node of job.result.routeGraph.nodes){assert.ok(node.visits>=node.runs);assert.ok(node.runs<=40);const filtered=await jobs.runs(root,job.id,{nodeId:node.id,limit:500});assert.equal(filtered.total,node.runs);}
  for(const edge of job.result.routeGraph.edges){const filtered=await jobs.runs(root,job.id,{edgeId:edge.id,limit:500});assert.equal(filtered.total,edge.runs);}
  const trace=await jobs.run(root,job.id,39);assert.equal(trace.run,39);const initial=await jobs.replay(root,job.id,39,0);assert.equal(initial.state.stateRevision,0);assert.equal(initial.transition,null);
  const end=await jobs.replay(root,job.id,39,trace.commands.length);assert.deepEqual(end.state,trace.state);assert.equal(end.beforeState.stateRevision,end.state.stateRevision-1);
  const activity=trace.route.find(p=>p.kind==='activity');assert.ok(activity);const step=await jobs.replay(root,job.id,39,activity.step);assert.equal(step.transition.activity.success?.chance??null,activity.chance);assert.equal(step.transition.activity.success?.roll??null,activity.roll);
  const attributeId=Object.keys(trace.state.attributes)[0],value=trace.state.attributes[attributeId];const filtered=await jobs.runs(root,job.id,{attributeId,min:value,max:value,limit:500});assert.ok(filtered.rows.some(r=>r.run===39));assert.ok(filtered.rows.every(r=>r.attributes[attributeId]===value));
  const current=path.join(root,'project.json');const project=JSON.parse(await readFile(current,'utf8'));project.title='Edited after analysis';await writeFile(current,JSON.stringify(project));assert.deepEqual((await jobs.replay(root,job.id,39,trace.commands.length)).state,trace.state);
  const service=new QtStudioService();const list=await service.handle({method:'analysisList',params:{root}});assert.equal(list.analysis.jobs[0].id,job.id);const reply=await service.handle({method:'analysisReplay',params:{root,jobId:job.id,run:39,step:activity.step}});assert.deepEqual(reply.analysis.replay.state,step.state);
  const traceFile=path.join(root,'.studio','analysis',`${job.id}.runs`,'39.json');const raw=await readFile(traceFile,'utf8');await writeFile(traceFile,raw.replace('"run":39','"run":38'));await assert.rejects(jobs.replay(root,job.id,39,1),/변경/);
});
test('cancel preserves the completed prefix and every persisted partial run remains replayable',async()=>{
  const {root,content}=await fixture(),jobs=new AnalysisJobs();let job=await jobs.start(root,content,'draft-version',0,{...options,runs:5000,workers:4});assert.equal(job.effectiveWorkers,4);
  for(let i=0;i<50;i++){await delay(30);const runs=await jobs.runs(root,job.id);if(runs.total>0)break;}
  await jobs.cancel(root,job.id);job=await completed(jobs,root,job.id);assert.equal(job.status,'cancelled');assert.ok(job.result.completed>0&&job.result.completed<5000);const runs=await jobs.runs(root,job.id,{limit:500});assert.equal(runs.total,job.result.completed);const last=await jobs.run(root,job.id,runs.rows.at(-1).run);assert.deepEqual((await jobs.replay(root,job.id,last.run,last.commands.length)).state,last.state);
});

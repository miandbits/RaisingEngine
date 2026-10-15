import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir,mkdtemp} from 'node:fs/promises';
import {resolve} from 'node:path';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {createSession} from '../../dist/packages/runtime-service/index.js';
import {runAssurance,inspectCommands,stateInvariants} from '../../dist/packages/analysis-service/assurance.js';
import {runAnalysis} from '../../dist/packages/analysis-service/runner.js';
import {createBenchmarkProject} from '../../apps/qt-studio/benchmarks.mjs';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';

test('template factory rejects overwrites and invalid inputs',async()=>{
 const base=await mkdtemp(resolve('.cache/assurance-input-'));
 await assert.rejects(createBenchmarkProject(base,{kind:'schedule',title:'existing'}));
 await assert.rejects(createBenchmarkProject(resolve(base,'invalid'),{kind:'unknown',title:'bad'}));
 await assert.rejects(createBenchmarkProject(resolve(base,'budget'),{kind:'schedule',title:'bad',budget:-1}));
});

test('six GUI-authored complete genres: deterministic replay, saves, rejection and invariants',{timeout:240000},async t=>{
 const proof=JSON.parse(await readFile('.cache/genre-suite-proof.json','utf8'));assert.ok(proof.guiTemplates&&proof.saveReopen);
 const summary=[];
 for(const item of proof.projects)await t.test(item.kind,async()=>{
  const b=await buildSnapshot(await loadProject(item.root));assert.equal(b.ok,true,JSON.stringify(b.diagnostics));const content=b.snapshot;
  const first=createSession(content,42,{mode:'full'});assert.equal(first.ok,true);assert.deepEqual(stateInvariants(content,first.session.state),[]);
  const bad=structuredClone(first.session.state);bad.attributes.money=-1;assert.ok(stateInvariants(content,bad).some(s=>s.includes('money')));
  if(bad.roster){const id=Object.keys(bad.roster.inventory)[0];bad.roster.inventory[id].broken=-1;assert.ok(stateInvariants(content,bad).some(s=>s.includes('broken')));}
  const before=JSON.stringify(first.session.state),catalog=inspectCommands(content,first.session.state);assert.ok(catalog.total>0);assert.ok(catalog.rows.some(r=>r.available));assert.equal(JSON.stringify(first.session.state),before);
  const report=await runAssurance(content,{runs:128,seed:42,maxCommands:150,timeoutMs:90000});
  const evidence=resolve('docs/evidence/genre-suite',item.kind);await mkdir(evidence,{recursive:true});await writeFile(resolve(evidence,'assurance-128.json'),JSON.stringify(report,null,2));
  assert.equal(report.ok,true,JSON.stringify(report.failures));assert.equal(report.result.completed,128);assert.equal(report.checks.saves,128);assert.equal(report.checks.rejections,128);
  for(const ending of ['success','ordinary'])assert.ok(report.result.endings[ending]>0,JSON.stringify(report.result.endings));
  assert.ok(report.witnesses.some(w=>w.state.endingId==='success'));
  if(item.kind==='free')assert.ok(report.checks.cancellations>0);
  summary.push({kind:item.kind,checks:report.checks,endings:report.result.endings,outcomes:report.result.outcomes,ok:report.ok});
 });
 await writeFile('docs/evidence/genre-suite/summary.json',JSON.stringify(summary,null,2));
});

test('cancelled verification is not presented as a completed success',async()=>{
 const proof=JSON.parse(await readFile('.cache/genre-suite-proof.json','utf8')),b=await buildSnapshot(await loadProject(proof.projects[0].root));
 const report=await runAssurance(b.snapshot,{runs:2},()=>true);assert.equal(report.ok,false);assert.equal(report.result.completed,0);
});

test('insufficient resources expose target and rejection reason without changing state',async()=>{
 const base=await mkdtemp(resolve('.cache/assurance-poor-')),root=resolve(base,'game');
 await createBenchmarkProject(root,{kind:'free',title:'자원 부족 검사',budget:0});
 const b=await buildSnapshot(await loadProject(root));assert.equal(b.ok,true);
 const s=createSession(b.snapshot,42,{mode:'full'});assert.equal(s.ok,true);const before=structuredClone(s.session.state);
 const row=inspectCommands(b.snapshot,s.session.state).rows.find(r=>r.commands.some(c=>c.type==='executeAction'&&c.actionId==='work'));
 assert.ok(row&&!row.available);assert.ok(row.code&&row.reason);assert.deepEqual(s.session.state,before);
 const report=await runAssurance(b.snapshot,{runs:16,seed:42});assert.equal(report.ok,true);
 assert.ok(report.blockedExamples.some(e=>e.commands.some(c=>c.actionId==='work')&&e.reason));
 await writeFile('docs/evidence/genre-suite/insufficient-resources.json',JSON.stringify(report,null,2));
});

test('goal search keeps bounded misses distinct from witnessed reachable endings',{timeout:240000},async()=>{
 const proof=JSON.parse(await readFile('.cache/genre-suite-proof.json','utf8')),summary=[];
 for(const item of proof.projects){
  const b=await buildSnapshot(await loadProject(item.root));assert.equal(b.ok,true);
  const target=await runAnalysis(b.snapshot,{runs:8,seed:42,strategy:'explore',maxCommands:100,timeoutMs:30000,target:{kind:'ending',id:'success'},beamWidth:8,maxStates:4000});
  const evidence=resolve('docs/evidence/genre-suite',item.kind);await writeFile(resolve(evidence,'target-search.json'),JSON.stringify(target,null,2));
  const witnessed=JSON.parse(await readFile(resolve(evidence,'assurance-128.json'),'utf8')).witnesses.find(w=>w.state.endingId==='success');assert.ok(witnessed);
  assert.notEqual(target.targetStatus,'impossible');
  summary.push({kind:item.kind,searchStatus:target.targetStatus,stopReason:target.stopReason,exploredStates:target.exploredStates,witnessSeed:witnessed.seed,witnessCommands:witnessed.commands.length});
 }
 await writeFile('docs/evidence/genre-suite/target-summary.json',JSON.stringify(summary,null,2));
});

test('existing projects load without schema migration',async()=>{
 const roots=['examples/academy-demo','examples/academy-year','examples/academy-adventure','examples/academy-journey','examples/academy-visual','projects/character-state-demo','projects/time-session-demo','projects/authoring-workspace','projects/ui-composition-lab','projects/party-tactics-lab','projects/direction-lab'];
 const results=[];
 for(const root of roots){const before=await readFile(resolve(root,'project.json'),'utf8'),b=await buildSnapshot(await loadProject(root));assert.equal(b.ok,true,root+': '+JSON.stringify(b.diagnostics));assert.equal(await readFile(resolve(root,'project.json'),'utf8'),before);results.push({root,valid:true,unchanged:true});}
 await writeFile('docs/evidence/genre-suite/compatibility.json',JSON.stringify(results,null,2));
});

test('Qt sample creation preserves dirty work and assurance checks the exact draft revision',async()=>{
 const base=await mkdtemp(resolve('.cache/assurance-service-')),root=resolve(base,'sample'),s=new QtStudioService();
 await s.handle({method:'createBenchmark',params:{root,kind:'free',title:'초안 보존'}});
 const project=structuredClone(s.editor.documents.find(d=>d.kind==='project').data);project.title='저장 전 제목';
 await s.handle({method:'edit',params:{revision:s.editor.revision,edit:{type:'replace',file:'project.json',data:project}}});
 await assert.rejects(s.handle({method:'createBenchmark',params:{root:resolve(base,'other'),kind:'schedule',title:'다른 프로젝트'}}),/저장/);
 assert.equal(s.editor.root,root);assert.equal(s.editor.view().dirty,true);
 await assert.rejects(s.handle({method:'assuranceRun',params:{revision:s.editor.revision-1,runs:1}}));
 const result=await s.handle({method:'assuranceRun',params:{revision:s.editor.revision,runs:2}});assert.equal(result.assurance.ok,true);assert.equal(s.editor.view().dirty,true);
 assert.notEqual(JSON.parse(await readFile(resolve(root,'project.json'),'utf8')).title,project.title);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {createSession,dispatchSession,encodeSave,decodeSave} from '../../dist/packages/runtime-service/index.js';
import {runAssurance,stateInvariants} from '../../dist/packages/analysis-service/assurance.js';

async function content(){const proof=JSON.parse(await readFile('.cache/commercial-pilot-proof.json','utf8'));assert.ok(proof.guiEdited&&proof.saveReopen);const b=await buildSnapshot(await loadProject(proof.project));assert.equal(b.ok,true,JSON.stringify(b.diagnostics));return b.snapshot;}
test('three-month pilot: random coverage and saved deterministic replay',{timeout:240000},async()=>{
 const c=await content();assert.equal(c.project.durationMonths,3);assert.equal(c.project.periodsPerMonth,3);assert.equal(c.endings.length,3);
 const report=await runAssurance(c,{runs:128,seed:42,maxCommands:800,timeoutMs:180000});await writeFile('docs/evidence/commercial-pilot/assurance-128.json',JSON.stringify(report,null,2));
 assert.equal(report.ok,true,JSON.stringify(report.failures));assert.equal(report.result.completed,128);assert.equal(report.checks.saves,128);assert.equal(report.result.outcomes.error??0,0);assert.equal(report.result.outcomes.blocked??0,0);assert.ok(report.result.outcomes.finished>0);
});
test('all three authored goal routes replay through save boundaries with matching native endings',async()=>{
 const c=await content(),results=[];
 for(const kind of ['success','relationship','ordinary']){
  const commands=JSON.parse(await readFile('docs/evidence/commercial-pilot/'+kind+'-commands.json','utf8'));let session=createSession(c,42,{mode:'full'}).session;const phases=new Set([session.state.phase]);let saves=0;
  for(let i=0;i<commands.length;i++){const next=dispatchSession(c,session,commands[i],session.state.stateRevision);assert.equal(next.ok,true,JSON.stringify(next.error));session=next.session;phases.add(session.state.phase);assert.deepEqual(stateInvariants(c,session.state),[]);if(i%7===0){const saved=await encodeSave(c,session);assert.equal(saved.ok,true);const restored=await decodeSave(c,saved.text);assert.equal(restored.ok,true);assert.deepEqual(restored.session.state,session.state);session=restored.session;saves++;}}
  assert.equal(session.state.phase,'finished');assert.equal(session.state.endingId,{success:'knight',relationship:'neighbor',ordinary:'wanderer'}[kind]);
  const player=JSON.parse(await readFile('docs/evidence/commercial-pilot/'+(kind==='success'?'play':kind)+'-report.json','utf8'));assert.equal(player.ok,true);assert.deepEqual(session.state,player.steps.find(s=>s.command.checkpoint==='finished').ui.state);
  if(kind==='success')for(const phase of ['planning','ready','activity-result','narrative','expedition','battle','finished'])assert.ok(phases.has(phase),phase);
  results.push({kind,ending:session.state.endingId,commands:commands.length,saves,phases:[...phases]});
 }
 await writeFile('docs/evidence/commercial-pilot/goal-replay.json',JSON.stringify(results,null,2));
});
test('pilot preparation gate and rejected purchase preserve state',async()=>{
 const c=await content();let session=createSession(c,42,{mode:'full'}).session;
 const departed=dispatchSession(c,session,{type:'executeAction',actionId:'tactics-depart',quantity:1},session.state.stateRevision);assert.equal(departed.ok,true);session=departed.session;const before=structuredClone(session.state);
 const blocked=dispatchSession(c,session,{type:'moveExpedition',nodeId:'clearing'},session.state.stateRevision);assert.equal(blocked.ok,false);assert.deepEqual(session.state,before);
 session=createSession(c,42,{mode:'full'}).session;let next=dispatchSession(c,session,{type:'visit',locationId:'market'},session.state.stateRevision);assert.equal(next.ok,true);session=next.session;const state=structuredClone(session.state);
 const purchase=dispatchSession(c,session,{type:'buy',shopId:'florist',itemId:'flower',quantity:1000},session.state.stateRevision);assert.equal(purchase.ok,false);assert.deepEqual(session.state,state);
 await writeFile('docs/evidence/commercial-pilot/rejections.json',JSON.stringify({preparation:blocked.error,purchase:purchase.error,statePreserved:true},null,2));
});

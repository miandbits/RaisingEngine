import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdtemp,mkdir} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/index.js';

test('discovered pilot ending plays through real native controls and restores mid-route',{timeout:120000},async()=>{
 const evidence=resolve('docs/evidence/goal-search');await mkdir(evidence,{recursive:true});
 const proof=JSON.parse(await readFile(resolve(evidence,'search-and-registration.json'),'utf8'));const project=proof.results.find(r=>r.target.kind==='ending').root;
 const route=JSON.parse(await readFile(resolve(project,'playtests/routes/auto-wanderer.json'),'utf8'));const content=(await buildSnapshot(await loadProject(project))).snapshot;
 const game=resolve(await mkdtemp(resolve('.cache/goal-search-native-')),'game');
 await exportGame({content,projectRoot:project,destination:game,target:'windows',runtimes:{windows:resolve('dist/native-player-routes'),web:resolve('dist/web')}});
 let session=createSession(content,route.origin.seed,route.origin).session;const steps=[{click:'start'}],expected={};
 const send=c=>{const r=dispatchSession(content,session,c,session.state.stateRevision);assert.equal(r.ok,true,JSON.stringify(r.error));session=r.session;};
 const node=()=>{const n=session.state.activeNarrative;return n?(n.kind==='event'?content.events:content.endings).find(e=>e.id===n.id).nodes.find(x=>x.id===n.nodeId):null;};
 const settle=()=>{for(let i=0;i<100;i++){if(session.state.phase==='period-end')send({type:'resolvePeriodEnd'});else if(session.state.phase==='narrative'&&!['dialogue','choice'].includes(node()?.type))send({type:'advanceNarrative'});else return;}throw Error('settle overflow');};
 const action=(c,ui)=>{steps.push(...ui);send(c);settle();};
 const checkpoint=id=>{steps.push({checkpoint:id});expected[id]=structuredClone(session.state);};
 const choices=route.commands.filter(c=>c.type==='choose');let choiceIndex=0;
 const story=()=>{while(session.state.phase==='narrative'){const c=node()?.type==='choice'?choices[choiceIndex++]:{type:'advanceNarrative'};assert.ok(c);action(c,[{click:c.type==='choose'?'choose-'+c.choiceId:'advance'}]);}};
 const schedules=route.commands.filter(c=>c.type==='setSchedule');assert.equal(schedules.length,9);
 for(let month=0;month<3;month++){
  action({type:'navigate',screenId:'schedule'},[{click:'schedule-link'}]);const activityIds=schedules.slice(month*3,month*3+3).map((c,i)=>c.activityIds[i]);
  action({type:'setSchedule',activityIds},[...activityIds.map((value,i)=>({select:'plan-'+i,value})),{click:'advance'}]);
  for(let period=0;period<3;period++){
   action({type:'executeActivity'},[{click:'advance'}]);
   if(month===1&&period===0){const saved=structuredClone(session);steps.push({key:'F5'});checkpoint('saved');action({type:'acknowledgeResult'},[{click:'advance'}]);steps.push({key:'F9'});session=saved;checkpoint('restored');}
   action({type:'acknowledgeResult'},[{click:'advance'}]);story();
  }
  if(session.state.phase!=='finished'){action({type:'visit',locationId:'city'},[{click:'button-new'}]);action({type:'visit',locationId:'home'},[{click:'visit-home'}]);}
 }
 assert.equal(session.state.endingId,route.target.id);checkpoint('ending');steps.push({key:'F5'},{screenshot:resolve(evidence,'native-found-ending.png')});
 const script=resolve(evidence,'native-found-steps.json'),report=resolve(evidence,'native-found-report.json'),save=resolve(evidence,'native-found-save.json');await writeFile(script,JSON.stringify(steps));
 const run=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--seed',String(route.origin.seed),'--save',save,'--automation',script,'--report',report,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:100000});
 const result=JSON.parse(await readFile(report,'utf8'));assert.equal(run.status,0,JSON.stringify(result.error??run.stderr));assert.equal(result.ok,true);
 for(const [id,state] of Object.entries(expected))assert.deepEqual(result.steps.find(s=>s.command.checkpoint===id).ui.state,state,id);
 assert.deepEqual(expected.saved,expected.restored);
 const freshScript=resolve(evidence,'native-found-load-steps.json'),freshReport=resolve(evidence,'native-found-load-report.json');await writeFile(freshScript,JSON.stringify([{key:'F9'},{checkpoint:'fresh'}]));
 const fresh=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',save,'--automation',freshScript,'--report',freshReport,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:30000});assert.equal(fresh.status,0,fresh.stderr);const loaded=JSON.parse(await readFile(freshReport,'utf8'));assert.deepEqual(loaded.steps.find(s=>s.command.checkpoint==='fresh').ui.state,expected.ending);
 await writeFile(resolve(evidence,'native-verification.json'),JSON.stringify({ok:true,route:route.id,ending:session.state.endingId,realControls:true,middleSaveLoad:true,freshLoad:true,game,project},null,2));
});

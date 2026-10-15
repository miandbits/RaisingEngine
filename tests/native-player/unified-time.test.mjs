import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile,rm} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/index.js';
test('new player preserves legacy independent-clock UI and saved pending sessions',{timeout:90000},async t=>{
 await mkdir(resolve('.cache'),{recursive:true});const root=await mkdtemp(resolve('.cache/legacy-time-player-'));t.after(()=>rm(root,{recursive:true,force:true}));const project=resolve('tests/fixtures/projects/time-session-demo'),built=await buildSnapshot(await loadProject(project));assert.ok(built.ok);assert.equal(built.snapshot.project.progression.model,undefined);
 const bundle=resolve(root,'game');await exportGame({content:built.snapshot,projectRoot:project,destination:bundle,target:'windows',runtimes:{windows:resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-unified'),web:resolve('dist/web')}});
 const script=resolve(root,'steps.json'),report=resolve(root,'report.json');await writeFile(script,JSON.stringify([{click:'start'},{click:'progress__rules-1__wait'},{key:'F5'},{checkpoint:'pending'},{click:'progress__rules-1__advance'},{checkpoint:'complete'},{key:'F9'},{checkpoint:'restored'},{click:'progress__rules-1__advance'},{checkpoint:'replayed'}]));
 const child=spawnSync(resolve(bundle,'RaisingPlayer.exe'),['--bundle',bundle,'--automation',script,'--report',report,'--save',resolve(root,'save.json'),'--hidden'],{encoding:'utf8',windowsHide:true,timeout:60000});const result=JSON.parse(await readFile(report,'utf8'));assert.equal(child.status,0,JSON.stringify(result.error??child.stderr));assert.ok(result.ok);const state=id=>result.steps.find(s=>s.command.checkpoint===id).ui.state;assert.deepEqual(state('pending'),state('restored'));assert.deepEqual(state('complete'),state('replayed'));assert.equal(state('complete').progression.tick,1);assert.equal(state('complete').periodIndex,0);assert.equal(state('complete').progression.sessions,undefined);assert.equal(state('complete').progression.actors,undefined);
});
test('GUI-authored unified clock: parallel play, appointments, calendar, cancel and mid-session restore', {timeout:180000},async t=>{
 await mkdir(resolve('.cache'),{recursive:true});const root=await mkdtemp(resolve('.cache/unified-player-'));t.after(()=>rm(root,{recursive:true,force:true}));
 let project=process.env.RAISING_UNIFIED_PROJECT;
 if(!project){project=resolve(root,'project');await cp(resolve('tests/fixtures/projects/time-session-demo'),project,{recursive:true});await cp(resolve('tests/fixtures/unified-time-project.json'),resolve(project,'project.json'));}
 const built=await buildSnapshot(await loadProject(project));assert.ok(built.ok,JSON.stringify(built.diagnostics));const content=built.snapshot;assert.equal(content.project.progression.model,'unified-v2');
 const evidence=resolve('.cache/unified-time');await mkdir(evidence,{recursive:true});const bundle=resolve(root,'game');
 const runtime=resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-unified');
 const old=resolve(root,'old-renderer');await mkdir(old);const caps=JSON.parse(await readFile(resolve(runtime,'capabilities.json'),'utf8'));caps.visualFeatures=caps.visualFeatures.filter(x=>x!=='unified-time-v2');await writeFile(resolve(old,'capabilities.json'),JSON.stringify(caps));await writeFile(resolve(old,'RaisingPlayer.exe'),'packaging fixture');await writeFile(resolve(old,'index.html'),'packaging fixture');
 for(const target of ['windows','web'])await assert.rejects(exportGame({content,projectRoot:project,destination:resolve(root,'old-'+target),target,runtimes:{windows:old,web:old}}),error=>error.message.includes('unified-time-v2'));
 await exportGame({content,projectRoot:project,destination:bundle,target:'windows',runtimes:{windows:resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-unified'),web:resolve('dist/web')}});
 let session=createSession(content,42,{mode:'full'}).session;const steps=[{click:'start'}],expected={};
 const node=()=>{const n=session.state.activeNarrative;return n?(n.kind==='event'?content.events:content.endings).find(e=>e.id===n.id).nodes.find(x=>x.id===n.nodeId):null;};
 function send(command){const next=dispatchSession(content,session,command,session.state.stateRevision);assert.ok(next.ok,JSON.stringify(next.error));session=next.session;}
 function settle(){for(let i=0;i<50;i++){if(session.state.phase==='period-end')send({type:'resolvePeriodEnd'});else if(session.state.phase==='narrative'&&!['dialogue','choice'].includes(node().type))send({type:'advanceNarrative'});else return;}assert.fail('settle limit');}
 function act(command,click){steps.push({click});send(command);settle();}
 function checkpoint(id){expected[id]=structuredClone(session.state);steps.push({checkpoint:id,screenshot:resolve(evidence,id+'.png')});}
 function story(){let n=0;while(session.state.phase==='narrative'){assert.ok(++n<50);assert.equal(node().type,'dialogue');act({type:'advanceNarrative'},'advance');}}
 const advance={type:'advanceSession'},actor=(actorId,actionId)=>({type:'executeActorAction',actorId,actionId,quantity:1});
 act(actor('apprentice','action-1'),'actor-action__rules-1__apprentice__action-1');act(actor('trainee-2','action-2'),'actor-action__rules-1__trainee-2__action-2');checkpoint('parallel');assert.equal(session.state.progression.sessions.length,2);
 steps.push({key:'F5'});const saved=structuredClone(session);act(advance,'advance');checkpoint('appointment');assert.equal(session.state.progression.tick,2);steps.push({key:'F9'});session=saved;checkpoint('parallel-restored');act(advance,'advance');story();
 act(advance,'advance');assert.equal(session.state.progression.tick,4);story();act(advance,'advance');story();checkpoint('short-complete');assert.equal(session.state.progression.sessions.length,1);assert.equal(session.state.roster.attributes['trainee-2'].skill,45);
 act(advance,'advance');story();checkpoint('actions-complete');assert.equal(session.state.progression.tick,5);assert.equal(session.state.progression.pending,null);assert.equal(session.state.roster.inventory.apprentice.flower,1);
 act({type:'assignActivities',assignments:session.state.roster.assignments},'advance');act({type:'executeActivity'},'advance');checkpoint('calendar-pending');assert.equal(session.state.progression.pending.remaining,5);
 act(advance,'advance');assert.equal(session.state.progression.tick,6);checkpoint('repeat-in-calendar');steps.push({key:'F5'});const mid=structuredClone(session);story();act(advance,'advance');checkpoint('calendar-result');steps.push({key:'F9'});session=mid;checkpoint('repeat-restored');story();act(advance,'advance');act({type:'acknowledgeResult'},'advance');story();checkpoint('calendar-complete');assert.equal(session.state.periodIndex,1);assert.equal(session.state.progression.tick,10);assert.deepEqual(session.state.progression.handled.map(x=>x.atTick),[2,4,6]);
 act(actor('apprentice','action-1'),'actor-action__rules-1__apprentice__action-1');act({type:'cancelActorSession',actorId:'apprentice'},'progress__rules-1__cancel__apprentice');checkpoint('cancelled');steps.push({key:'F5'});
 const script=resolve(evidence,'steps.json'),report=resolve(evidence,'report.json'),save=resolve(evidence,'save.json');await writeFile(script,JSON.stringify(steps,null,2));
 const child=spawnSync(resolve(bundle,'RaisingPlayer.exe'),['--bundle',bundle,'--seed','42','--automation',script,'--report',report,'--save',save,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:90000});const result=JSON.parse(await readFile(report,'utf8'));assert.equal(child.status,0,JSON.stringify(result.error??child.stderr));assert.ok(result.ok);
 for(const [id,state]of Object.entries(expected))assert.deepEqual(result.steps.find(s=>s.command.checkpoint===id)?.ui.state,state,id);
 const reload=resolve(evidence,'reload.json'),reloadReport=resolve(evidence,'reload-report.json');await writeFile(reload,JSON.stringify([{click:'load'},{checkpoint:'loaded'}]));const again=spawnSync(resolve(bundle,'RaisingPlayer.exe'),['--bundle',bundle,'--automation',reload,'--report',reloadReport,'--save',save,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:30000});assert.equal(again.status,0,again.stderr);const resumed=JSON.parse(await readFile(reloadReport,'utf8'));assert.ok(resumed.ok);assert.deepEqual(resumed.steps.find(s=>s.command.checkpoint==='loaded').ui.state,expected.cancelled);
});

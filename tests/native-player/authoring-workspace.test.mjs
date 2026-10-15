import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('Qt workspace-authored branches and time sessions: actual player interruption, deferred effects, mid-session save, cancel and fresh load',{timeout:120000},async()=>{
 const proof=JSON.parse(await readFile('.cache/authoring-workspace-proof.json','utf8'));assert.ok(proof.guiEdited&&proof.saveReopen&&proof.undoRedo);
 const built=await buildSnapshot(await loadProject(proof.project));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));const c=built.snapshot,widget=c.project.visual.screens.flatMap(s=>s.elements).find(e=>e.kind==='actions');assert.ok(widget);
 const members=c.project.roster.members,button=(i)=>`actor-action__${widget.id}__${members[i].characterId}__${members[i].actionIds[0]}`,control=s=>`progress__${widget.id}__${s}`;
 const evidence=resolve('docs/evidence/authoring-workspace');await mkdir(evidence,{recursive:true});await mkdir('.cache/workspace-play',{recursive:true});const root=await mkdtemp(resolve('.cache/workspace-play/run-')),game=resolve(root,'game');
 await assert.rejects(exportGame({content:c,projectRoot:proof.project,destination:resolve(root,'old'),target:'windows',runtimes:{windows:resolve('dist/native-player-character-state'),web:resolve('dist/web')}}),/progression-sessions-v1/);
 await exportGame({content:c,projectRoot:proof.project,destination:game,target:'windows',runtimes:{windows:resolve('dist/native-player-workspace'),web:resolve('dist/web')}});
 const save=resolve(evidence,'play-save.json');async function play(steps,name){const script=resolve(evidence,name+'-steps.json'),report=resolve(evidence,name+'-report.json');await writeFile(script,JSON.stringify(steps,null,2));const child=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',save,'--automation',script,'--report',report,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:90000});assert.equal(child.status,0,child.stderr);const result=JSON.parse(await readFile(report,'utf8'));assert.equal(result.ok,true,JSON.stringify(result));return result;}
 const result=await play([{click:'start'},{checkpoint:'initial'},
  {click:button(0)},{checkpoint:'pending',screenshot:resolve(evidence,'pending.png')},{key:'F5'},
  {click:control('advance')},{checkpoint:'appointment',screenshot:resolve(evidence,'appointment.png')},{key:'F5'},{checkpoint:'saved'},
  {click:'advance'},{click:control('advance')},{checkpoint:'settled'},{click:'advance'},{checkpoint:'first-complete'},
  {key:'F9'},{checkpoint:'restored'},{click:'advance'},{click:control('advance')},{click:'advance'},
  {checkpoint:'complete',screenshot:resolve(evidence,'complete.png')},{key:'F5'},
  {click:button(1)},{checkpoint:'second-pending'},{click:control('cancel')},{checkpoint:'cancelled'},
  {click:control('wait')},{click:control('advance')},{checkpoint:'waited'},
  {key:'F9'},{checkpoint:'final-restored'}],'play');
 const state=tag=>result.steps.find(x=>x.command.checkpoint===tag).ui.state,first=members[0].characterId,second=members[1].characterId;
 assert.equal(state('pending').progression.tick,0);assert.equal(state('pending').progression.actionPoints,1);assert.equal(state('pending').roster.inventory[first].flower,2);
 assert.equal(state('appointment').activeNarrative.id,'time-visit');assert.equal(state('appointment').progression.tick,3);assert.equal(state('appointment').progression.pending.remaining,3);
 assert.equal(state('settled').progression.pending,null);assert.equal(state('settled').activeNarrative.id,'personal-gift');assert.equal(state('settled').roster.inventory[first].flower,1);
 assert.deepEqual(state('restored'),state('saved'));assert.deepEqual(state('complete'),state('first-complete'));assert.equal(state('complete').progression.tick,6);assert.equal(state('complete').progression.actionPoints,6);assert.equal(state('complete').roster.attributes[first].skill,state('initial').roster.attributes[first].skill+2);assert.equal(state('complete').roster.attributes[second].skill,state('initial').roster.attributes[second].skill);
 assert.equal(state('cancelled').progression.tick,6);assert.equal(state('cancelled').progression.actionPoints,4);assert.equal(state('cancelled').progression.pending,null);assert.equal(state('cancelled').roster.inventory[second].book,0);
 assert.equal(state('waited').progression.tick,7);assert.equal(state('waited').progression.actionPoints,5);assert.deepEqual(state('final-restored'),state('complete'));
 const fresh=await play([{key:'F9'},{checkpoint:'fresh'}],'fresh');assert.deepEqual(fresh.steps.find(x=>x.command.checkpoint==='fresh').ui.state,state('complete'));
 await writeFile(resolve(evidence,'verification.json'),JSON.stringify({project:proof.project,game,guiAuthored:true,scheduledInterruption:true,deferredEffects:true,cancelAndRecovery:true,midSessionSaveRestore:true,freshProcessLoad:true,oldRuntimeRejected:true},null,2));
});

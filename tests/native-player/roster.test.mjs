import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir, mkdtemp, readFile, writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

// Input is authored by rosterAuthoringRoundtrip through the production Qt dialogs.
// This test never edits project data; it exercises actual rendered player controls.
test('Qt-authored two trainees: Windows UI assignment, play, save and fresh-process load', {timeout:120000}, async()=>{
 const proof=JSON.parse(await readFile('.cache/roster-authoring-proof.json','utf8'));
 assert.ok(proof.authoredThroughWidgets&&proof.saveReopen&&proof.undoRedo);
 const built=await buildSnapshot(await loadProject(proof.project));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
 const root=resolve('docs/evidence/roster');await mkdir(root,{recursive:true});await mkdir('.cache/roster-play',{recursive:true});
 const runRoot=await mkdtemp(resolve('.cache/roster-play/run-')),game=resolve(runRoot,'game'),unsupported=resolve(runRoot,'old-runtime');
 await mkdir(unsupported);const caps=JSON.parse(await readFile('dist/native-player-roster/capabilities.json','utf8'));
 caps.visualFeatures=caps.visualFeatures.filter(f=>f!=='roster-activities-v1');await writeFile(resolve(unsupported,'capabilities.json'),JSON.stringify(caps));
 await writeFile(resolve(unsupported,'RaisingPlayer.exe'),'unsupported-runtime-test-fixture');await writeFile(resolve(unsupported,'index.html'),'unsupported-runtime-test-fixture');
 for(const target of ['windows','web'])await assert.rejects(exportGame({content:built.snapshot,projectRoot:proof.project,destination:resolve(runRoot,target+'-unsupported'),target,runtimes:{windows:unsupported,web:unsupported}}),/roster-activities-v1/);
 await exportGame({content:built.snapshot,projectRoot:proof.project,destination:game,target:'windows',runtimes:{windows:resolve('dist/native-player-roster'),web:resolve('dist/web')}});
 const save=resolve(root,'play-save.json');
 async function play(steps,name){
  const commands=resolve(root,name+'-steps.json'),report=resolve(root,name+'-report.json');await writeFile(commands,JSON.stringify(steps,null,2));
  const child=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',save,'--automation',commands,'--report',report,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:90000});
  assert.equal(child.status,0,child.stderr);const result=JSON.parse(await readFile(report,'utf8'));assert.equal(result.ok,true,JSON.stringify(result));return result;
 }
 const result=await play([
  {click:'start'},{checkpoint:'initial'},
  {select:'roster-apprentice',value:'study'},{select:'roster-trainee-2',value:'rest'},
  {click:'roster-assign'},{checkpoint:'assigned',screenshot:resolve(root,'player-assigned.png')},
  {click:'roster-execute'},{checkpoint:'first-result',screenshot:resolve(root,'player-result.png')},
  {key:'F5'},{checkpoint:'saved'},{click:'advance'},{checkpoint:'next-period'},
  {select:'roster-apprentice',value:'rest'},{select:'roster-trainee-2',value:'study'},
  {click:'roster-assign'},{click:'roster-execute'},{checkpoint:'second-result'},
  {key:'F9'},{checkpoint:'restored',screenshot:resolve(root,'player-restored.png')}
 ],'play');
 const state=tag=>result.steps.find(x=>x.command.checkpoint===tag).ui.state;
 assert.deepEqual(state('assigned').roster.assignments,{apprentice:'study','trainee-2':'rest'});
 assert.equal(state('assigned').phase,'ready');
 assert.equal(state('first-result').phase,'activity-result');
 assert.equal(state('first-result').roster.attributes.apprentice.skill,18);
 assert.equal(state('first-result').roster.attributes.apprentice.stamina,70);
 assert.equal(state('first-result').roster.attributes['trainee-2'].skill,40);
 assert.ok(state('first-result').roster.attributes['trainee-2'].stamina>30);
 assert.equal(state('first-result').attributes.money,55);
 assert.equal(state('next-period').periodIndex,1);
 assert.equal(state('second-result').roster.attributes['trainee-2'].skill,48);
 assert.equal(state('second-result').roster.attributes.apprentice.skill,18);
 assert.deepEqual(state('restored'),state('saved'));
 const reopened=await play([{key:'F9'},{checkpoint:'fresh-load'}],'fresh');
 assert.deepEqual(reopened.steps.find(x=>x.command.checkpoint==='fresh-load').ui.state,state('saved'));
 await writeFile(resolve(root,'verification.json'),JSON.stringify({project:proof.project,game,qtAuthoring:true,playerControls:true,distinctEffects:true,sharedMoney:true,swappedActivities:true,saveLoad:true,freshProcessLoad:true,unsupportedRuntimeRejected:true},null,2));
});

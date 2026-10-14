import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('Qt-authored character actions and personal events: real UI, mid-story save and fresh load', {timeout:120000},async()=>{
 const proof=JSON.parse(await readFile('.cache/character-state-authoring-proof.json','utf8'));assert.ok(proof.guiCreated&&proof.logicOwnerWidgets&&proof.saveReopen);
 const built=await buildSnapshot(await loadProject(proof.project));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));const c=built.snapshot;
 const widget=c.project.visual.screens.flatMap(s=>s.elements).find(e=>e.kind==='actions');assert.ok(widget);
 const members=c.project.roster.members;assert.equal(members[0].actionIds.length,1);assert.equal(members[1].actionIds.length,1);
 const first=members[0].characterId,second=members[1].characterId,button=(actor,index)=>`actor-action__${widget.id}__${actor}__${members[index].actionIds[0]}`;
 const evidence=resolve('docs/evidence/character-state');await mkdir(evidence,{recursive:true});await mkdir('.cache/character-state-play',{recursive:true});
 const root=await mkdtemp(resolve('.cache/character-state-play/run-')),game=resolve(root,'game');
 await assert.rejects(exportGame({content:c,projectRoot:proof.project,destination:resolve(root,'old'),target:'windows',runtimes:{windows:resolve('dist/native-player-roster'),web:resolve('dist/web')}}),/character-scoped-rules-v1/);
 await exportGame({content:c,projectRoot:proof.project,destination:game,target:'windows',runtimes:{windows:resolve('dist/native-player-character-state'),web:resolve('dist/web')}});
 const save=resolve(evidence,'play-save.json');
 async function play(steps,name){const script=resolve(evidence,name+'-steps.json'),report=resolve(evidence,name+'-report.json');await writeFile(script,JSON.stringify(steps,null,2));
  const child=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',save,'--automation',script,'--report',report,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:90000});assert.equal(child.status,0,child.stderr);
  const result=JSON.parse(await readFile(report,'utf8'));assert.equal(result.ok,true,JSON.stringify(result));return result;
 }
 const result=await play([{click:'start'},{checkpoint:'initial',screenshot:resolve(evidence,'player-actions.png')},
  {click:button(first,0)},{checkpoint:'first-story',screenshot:resolve(evidence,'player-first-story.png')},{key:'F5'},{checkpoint:'saved'},
  {click:'advance'},{checkpoint:'first-complete'},{click:button(second,1)},{checkpoint:'second-story'},
  {key:'F9'},{checkpoint:'restored'},{click:'advance'},{click:button(second,1)},{click:'advance'},
  {checkpoint:'both-complete',screenshot:resolve(evidence,'player-results.png')},{key:'F5'},{checkpoint:'final-save'}],'play');
 const state=tag=>result.steps.find(x=>x.command.checkpoint===tag).ui.state;
 assert.equal(state('first-story').activeNarrative.actorId,first);assert.equal(state('first-story').activeNarrative.id,'personal-gift');
 assert.equal(state('first-story').roster.inventory[first].flower,1);assert.equal(state('first-story').roster.inventory[second].flower,1);
 assert.equal(state('first-story').roster.relationships[first].mentor.trust,18);assert.equal(state('first-story').roster.relationships[second].mentor.trust,7);
 assert.equal(state('second-story').activeNarrative.actorId,second);assert.equal(state('second-story').activeNarrative.id,'personal-study');
 assert.deepEqual(state('restored'),state('saved'));
 const done=state('both-complete');assert.deepEqual(done.roster.completedEvents[first],['personal-gift']);assert.deepEqual(done.roster.completedEvents[second],['personal-study']);
 assert.equal(done.roster.attributes[second].skill,45);assert.equal(done.roster.attributes[first].skill,10);assert.equal(done.roster.inventory[second].book,1);assert.equal(done.roster.inventory[first].book,0);assert.equal(done.roster.relationships[second].mentor.trust,27);
 assert.equal(done.relationships.mentor.trust,state('initial').relationships.mentor.trust);assert.equal(done.gameplay.inventory.book,0);
 assert.equal(done.roster.relationships[second].mentor.respect,20);assert.equal(done.roster.relationships[first].mentor.respect,0);
 const reopened=await play([{key:'F9'},{checkpoint:'fresh'}],'fresh');assert.deepEqual(reopened.steps.find(x=>x.command.checkpoint==='fresh').ui.state,state('final-save'));
 await writeFile(resolve(evidence,'verification.json'),JSON.stringify({project:proof.project,game,guiAuthored:true,distinctActionsAndEvents:true,independentInventoryAndRelationships:true,midStorySaveRestore:true,freshProcessLoad:true,oldRuntimeRejected:true},null,2));
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {authoringWorkspace,workspaceImpact} from '../../dist/packages/authoring-workspace/index.js';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {fixture} from '../gameplay/fixture.mjs';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/session.js';
import {encodeSave} from '../../dist/packages/runtime-service/save.js';

test('draft index: transitive branches, qualified relationships, dangling and duplicate definitions',()=>{
 const docs=[{file:'project.json',kind:'project',data:{gameplay:{actions:[{id:'a',flow:[{thenActionId:'b'}]},{id:'b',effects:[{target:{kind:'relationship',characterId:'mentor',relationshipId:'trust'}},{target:{itemId:'missing'}}],flow:[{thenActionId:'a'}]}]},roster:{members:[{characterId:'child',initialInventory:{flower:2}}]}}},{file:'mentor.json',kind:'character',data:{id:'mentor',relationships:[{id:'trust'}]}}];
 const index=authoringWorkspace(docs),impact=workspaceImpact(index,'relationship:mentor/trust');
 assert.ok(impact.paths.some(e=>e.from==='action:a'&&e.depth===2));assert.ok(index.edges.some(e=>e.to==='item:missing'&&!e.resolved));assert.ok(index.edges.some(e=>e.from==='actor:child'&&e.to==='item:flower'));
 assert.ok(impact.paths.length<20);assert.equal(impact.staticOnly,true);
 docs[0].data.gameplay.actions.push({id:'a',eventId:'absent'});const dup=authoringWorkspace(docs);assert.deepEqual(dup.duplicates,['action:a']);assert.equal(dup.nodes.filter(n=>n.key.startsWith('action:a@')).length,2);assert.ok(dup.edges.some(e=>e.from.startsWith('action:a@')&&e.to==='event:absent'));assert.ok(dup.edges.some(e=>e.to==='action:a'&&!e.resolved));
 assert.doesNotThrow(()=>authoringWorkspace([{file:'x',kind:'project',data:{gameplay:{actions:'invalid'},roster:{members:[null]},progression:{actions:42}}}]));
 assert.throws(()=>workspaceImpact(index,'action:a',99),/1..8/);
});

test('workspace uses unsaved draft, reports reference paths and rejects stale edits',async t=>{
 const {root}=await fixture(t,p=>{p.gameplay.actions=[{id:'a',name:'A',costs:[],effects:[]}];});const service=new QtStudioService();await service.handle({method:'open',params:{root}});const editor=service.editor,doc=editor.documents.find(d=>d.kind==='project'),disk=await readFile(root+'/'+doc.file),data=structuredClone(doc.data),before=editor.revision;
 data.gameplay.actions[0].flow=[{id:'next',thenActionId:'missing',quantity:1}];editor.edit({type:'replace',file:doc.file,data},before);
 const result=await service.handle({method:'authoringWorkspace',params:{revision:editor.revision,key:'action:a'}});assert.ok(result.workspace.edges.some(e=>e.path==='/gameplay/actions/0/flow/0/thenActionId'&&!e.resolved));assert.deepEqual(await readFile(root+'/'+doc.file),disk);
 await assert.rejects(service.handle({method:'authoringWorkspace',params:{revision:before}}),/갱신/);
});

test('C++ debugger: actor conditions, timed preview, real execution trace, middle save replay',async()=>{
 const service=new QtStudioService();await service.handle({method:'open',params:{root:new URL('../../projects/time-session-demo/',import.meta.url).pathname.replace(/^\/(\w:)/,'$1')}});const editor=service.editor,unchanged=editor.view();const started=await service.handle({method:'debugStart',params:{revision:editor.revision,seed:73}});assert.equal(started.debug.ok,true,JSON.stringify(started.debug));let debug=started.debug;
 const params=()=>({revision:editor.revision,sessionId:debug.sessionId});const inspect=async subject=>(await service.handle({method:'debugInspect',params:{...params(),subject}})).debug;
 const initial=structuredClone(debug.state);const preview=await inspect({kind:'action',id:'action-1',actorId:'apprentice'});assert.equal(preview.inspection.eligible,true);assert.deepEqual(preview.state,initial);assert.ok(preview.inspection.trace.some(x=>x.kind==='session-start'));assert.ok(!preview.inspection.trace.some(x=>x.kind==='effect'));
 const privateEvent=await inspect({kind:'event',id:'personal-gift',actorId:'apprentice'});assert.equal(privateEvent.inspection.eligible,false);assert.equal(privateEvent.inspection.condition.value,false);
 const execute=async command=>{debug=(await service.handle({method:'debugCommand',params:{...params(),expectedStateRevision:debug.stateRevision,command}})).debug;assert.equal(debug.ok,true,JSON.stringify(debug.error));return debug;};
 await execute({type:'executeActorAction',actorId:'apprentice',actionId:'action-1',quantity:1});assert.equal(debug.state.progression.pending.remaining,5);assert.equal(debug.state.roster.inventory.apprentice.flower,2);assert.ok(debug.trace.some(x=>x.kind==='session-start'));
 await execute({type:'advanceSession'});assert.equal(debug.state.progression.tick,2);assert.equal(debug.state.activeNarrative.id,'time-visit');assert.ok(debug.trace.some(x=>x.kind==='appointment'));
 const snapshot=(await editor.snapshot(editor.revision)).snapshot;let session=createSession(snapshot,73).session;for(const command of [{type:'executeActorAction',actorId:'apprentice',actionId:'action-1',quantity:1},{type:'advanceSession'}]){const r=dispatchSession(snapshot,session,command,session.state.stateRevision);assert.equal(r.ok,true);session=r.session;}
 const encoded=await encodeSave(snapshot,session);assert.equal(encoded.ok,true);assert.deepEqual(debug.state,session.state);const middle=structuredClone(debug.state);
 async function finish(){let guard=0;while(debug.state.phase==='narrative'&&guard++<12)await execute({type:'advanceNarrative'});await execute({type:'advanceSession'});assert.ok(debug.trace.some(x=>x.kind==='session-complete'));assert.ok(debug.trace.some(x=>x.kind==='effect'));while(debug.state.phase==='narrative'&&guard++<24)await execute({type:'advanceNarrative'});return structuredClone(debug.state);}
 const final=await finish();assert.equal(final.progression.tick,5);assert.equal(final.roster.inventory.apprentice.flower,1);
 debug=(await service.handle({method:'debugLoad',params:{revision:editor.revision,text:encoded.text}})).debug;assert.equal(debug.ok,true);assert.deepEqual(debug.state,middle);assert.deepEqual(await finish(),final);
 assert.deepEqual(editor.view(),unchanged);
});

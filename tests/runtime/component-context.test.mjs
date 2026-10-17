import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,writeFile,readFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {content,doc,loaded,temporary,samplePath} from './helpers.mjs';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/index.js';
import {buildSnapshot} from '../../dist/packages/project-service/index.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {resolveScreenElements} from '../../dist/packages/visual/components.js';
import {projectDataUi} from '../../dist/packages/visual/data-ui-projection.js';
import {nativeRequest} from '../../dist/packages/native-bridge/index.js';

function setup(p){
 const project=doc(p,'project');project.roster={stateModel:'character-v1',localAttributeIds:['skill'],members:[{characterId:'apprentice',activityId:'rest',initialAttributes:{skill:10}}]};
 project.visual={width:1280,height:720,routing:{defaultScreenId:'home',phases:[]},components:[{id:'card',label:'Card',width:300,height:100,elements:[{id:'value',kind:'text',x:0,y:0,width:300,height:40,binding:'data-value',dataBinding:{kind:'attribute',attributeId:'skill',owner:{kind:'actor'}},visibleCondition:{op:'compare',comparator:'gte',left:{op:'read',target:{kind:'attribute',attributeId:'skill',owner:{kind:'actor'}}},right:{op:'literal',value:5}}}]}],screens:[{id:'home',label:'Home',elements:[{id:'card-one',kind:'component',x:0,y:0,width:300,height:100,componentId:'card',uiContext:{actorId:'apprentice'}}]}]};
}
test('missing instance inputs and unknown typed context references fail with authored location',async()=>{
 const base=structuredClone(loaded);base.documents=base.documents.filter(d=>d.kind!=='scenario');setup(base);
 let b=await buildSnapshot(base);assert.equal(b.ok,true,JSON.stringify(b.diagnostics));
 const p=doc(base,'project');delete p.visual.screens[0].elements[0].uiContext;
 b=await buildSnapshot(base);assert.equal(b.ok,false);assert.ok(b.diagnostics.some(d=>d.code==='UI_CONTEXT_REQUIRED'&&d.pointer.includes('/visual/screens/0/elements/0')));
 p.visual.screens[0].elements[0].uiContext={actorId:'unknown'};b=await buildSnapshot(base);assert.equal(b.ok,false);assert.ok(b.diagnostics.some(d=>d.code==='UI_CONTEXT_REFERENCE'));
});
test('context predicates use native evaluation with a concrete owner, without mutation',async()=>{
 const c=await content(setup);const init=createSession(c,42).session;
 // The request used by the UI receives the expanded owner, never an ambient actor.
 const observed=[];projectDataUi(c,null,null,input=>{observed.push(input);return {ok:true,value:true};});
 const e=resolveScreenElements(c.project.visual,'home')[0];assert.equal(e.dataBinding.owner.characterId,'apprentice');assert.ok(observed.some(i=>i.condition?.left?.target?.owner?.characterId==='apprentice'));
 const before=JSON.stringify(c);assert.equal(nativeRequest({operation:'evaluate',content:c,state:init.state,condition:e.visibleCondition}).ok,true);assert.equal(JSON.stringify(c),before);
});
test('unsupported renderers reject context even in unused components before publishing',async t=>{
 const root=await temporary(t),runtime=resolve(root,'runtime');await mkdir(runtime);for(const file of ['RaisingPlayer.exe','index.html'])await writeFile(resolve(runtime,file),'placeholder');
 await writeFile(resolve(runtime,'capabilities.json'),JSON.stringify({formatVersion:'1',imageExtensions:['.png'],audioExtensions:['.wav'],visualFeatures:['free-screens-v1','components-v1']}));
 const c=await content(setup);for(const target of ['windows','web'])await assert.rejects(exportGame({content:c,projectRoot:samplePath,destination:resolve(root,target),target,runtimes:{windows:runtime,web:runtime}}),/component-context-v1/);
});

test('context targets reject conflicting selection sources and incompatible relationship fields',async()=>{
 const base=structuredClone(loaded);base.documents=base.documents.filter(d=>d.kind!=='scenario');setup(base);
 const visual=doc(base,'project').visual,child=visual.components[0].elements[0],instance=visual.screens[0].elements[0];
 instance.uiContext.characterId='mentor';child.contextBindings=['characterId'];
 child.dataBinding={kind:'relationship',characterId:'mentor',relationshipId:'trust'};
 let snapshot=await buildSnapshot(base);assert.equal(snapshot.ok,true,JSON.stringify(snapshot.diagnostics));
 instance.uiContext.characterId='apprentice';snapshot=await buildSnapshot(base);
 assert.equal(snapshot.ok,false);assert.ok(snapshot.diagnostics.some(d=>d.code==='UI_CONTEXT_RELATIONSHIP'&&d.pointer.includes('/visual/screens/0/elements/0')));
 instance.uiContext.characterId='mentor';child.selectionBindings=[{property:'characterId',field:'id'}];
 snapshot=await buildSnapshot(base);assert.equal(snapshot.ok,false);assert.ok(snapshot.diagnostics.some(d=>d.code==='UI_CONTEXT_CONFLICT'));
});

test('published GUI-authored cards retain independent actions, activity assignment and save replay through ending',async()=>{
 const {loadProject}=await import('../../dist/packages/project-service/loader.js');
 const {encodeSave,decodeSave}=await import('../../dist/packages/runtime-service/save.js');
 const {widgetCommand}=await import('../../dist/packages/visual/commands.js');
 const info=JSON.parse(await readFile(new URL('../fixtures/component-context.json',import.meta.url),'utf8'));
 const built=await buildSnapshot(await loadProject(info.root));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
 const c=built.snapshot,original=JSON.stringify(c),cards=resolveScreenElements(c.project.visual,info.screen);
 const actions=cards.filter(e=>e.action==='executeActorAction'),assign=cards.filter(e=>e.action==='assignActivity');
 assert.equal(actions.length,2);assert.equal(assign.length,2);
 let session=createSession(c,42,{mode:'full'}).session;
 const send=command=>{const r=dispatchSession(c,session,command,session.state.stateRevision);assert.equal(r.ok,true,JSON.stringify(r.error));session=r.session;};
 const restore=async()=>{const saved=await encodeSave(c,session);assert.equal(saved.ok,true);const loaded=await decodeSave(c,saved.text);assert.equal(loaded.ok,true);assert.deepEqual(loaded.session.state,session.state);session=loaded.session;};
 send(widgetCommand(actions[1],session.state));assert.equal(session.state.roster.attributes[info.actors[0]].skill,10);assert.equal(session.state.roster.attributes[info.actors[1]].skill,47);await restore();
 let results=0,choices=0;
 for(let i=0;i<120&&session.state.phase!=='finished';i++){
  if(['planning','ready'].includes(session.state.phase)){
   for(const card of assign)send(widgetCommand(card,session.state));
   for(const card of assign)assert.equal(session.state.roster.assignments[card.actorId],card.activityId);
   send({type:'executeActivity'});
  }else if(session.state.phase==='activity-result'){++results;await restore();send({type:'acknowledgeResult'});}
  else if(session.state.phase==='narrative'){
   const active=session.state.activeNarrative,source=(active.kind==='event'?c.events:c.endings).find(e=>e.id===active.id),node=source.nodes.find(n=>n.id===active.nodeId);
   if(node.type==='choice'){++choices;await restore();send({type:'choose',choiceId:node.choices[0].id});}else send({type:'advanceNarrative'});
  }else if(session.state.phase==='period-end')send({type:'resolvePeriodEnd'});else throw Error(session.state.phase);
 }
 assert.equal(session.state.phase,'finished');assert.equal(session.state.endingId,'certified');assert.equal(results,6);assert.ok(choices>0);await restore();assert.equal(JSON.stringify(c),original);
});

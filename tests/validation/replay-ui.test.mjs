import test from 'node:test';
import assert from 'node:assert/strict';
import {resolve} from 'node:path';
import {mkdtemp,writeFile} from 'node:fs/promises';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {validateProject} from '../../dist/packages/validation/index.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

const actions=['recollectionPrev','recollectionNext','recollectionClose','recollectionSave','recollectionLoad'];
test('replay UI validates dedicated controls, screen references and reusable component overrides',async()=>{
  const loaded=await loadProject(resolve('tests/fixtures/projects/last-grandson-polished')),p=loaded.documents.find(d=>d.kind==='project').data;
  const screen={id:'replay-test',label:'Replay test',elements:actions.map((action,i)=>({id:'button-'+i,kind:'button',x:10+i*150,y:10,width:140,height:44,text:action,action}))};
  p.visual.screens.push(screen);p.gallery.replayControlsScreenId=screen.id;
  let r=await validateProject(loaded);assert.equal(r.valid,true,JSON.stringify(r.diagnostics));
  const playback={id:'playback',kind:'button',x:10,y:60,width:200,height:44,text:'Playback',action:'recollectionAuto'};
  screen.elements.push(playback);
  for(const action of ['recollectionAuto','recollectionSpeed','recollectionSkip']){playback.action=action;r=await validateProject(loaded);assert.equal(r.valid,true,JSON.stringify(r.diagnostics));}
  screen.elements.pop(); // The three playback controls are optional in existing projects.
  p.gallery.replayControlsScreenId='missing';r=await validateProject(loaded);assert.ok(r.diagnostics.some(d=>d.pointer==='/gallery/replayControlsScreenId'));
  p.gallery.replayControlsScreenId=screen.id;screen.elements[3].action='save';r=await validateProject(loaded);assert.ok(r.diagnostics.some(d=>d.code==='REPLAY_UI_ACTION'));assert.ok(r.diagnostics.some(d=>d.code==='REPLAY_UI_CONTROLS'));
  screen.elements[3].action='recollectionSave';
  const component={id:'replay-component',label:'Replay controls',width:800,height:80,elements:screen.elements};p.visual.components.push(component);
  screen.elements=[{id:'toolbar',kind:'component',componentId:component.id,x:0,y:0,width:800,height:80}];
  r=await validateProject(loaded);assert.equal(r.valid,true,JSON.stringify(r.diagnostics));
  screen.elements[0].overrides=[{elementId:'button-2',values:{action:'start'}}];r=await validateProject(loaded);assert.ok(r.diagnostics.some(d=>d.code==='REPLAY_UI_ACTION'));
  delete screen.elements[0].overrides;screen.elements.push({id:'schedule',kind:'schedule',x:0,y:100,width:500,height:400});r=await validateProject(loaded);assert.ok(r.diagnostics.some(d=>d.code==='REPLAY_UI_ELEMENT'));
});

test('old runtime refuses replay controls, even in component overrides',async()=>{
  const projectRoot=resolve('tests/fixtures/projects/last-grandson-polished'),loaded=await loadProject(projectRoot),built=await buildSnapshot(loaded);assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const runtime=await mkdtemp(resolve('.cache/replay-old-runtime-'));await writeFile(resolve(runtime,'RaisingPlayer.exe'),'fixture, never executed');await writeFile(resolve(runtime,'capabilities.json'),JSON.stringify({formatVersion:'1',imageExtensions:[],audioExtensions:[],visualFeatures:[]}));
  const content=structuredClone(built.snapshot),v=content.project.visual,c=v.components[0];
  delete content.project.gallery.replayControlsScreenId;
  v.screens[0].elements.push({id:'replay-override',kind:'component',componentId:c.id,x:0,y:0,width:c.width,height:c.height,overrides:[{elementId:c.elements[0].id,values:{action:'recollectionClose'}}]});
  await assert.rejects(exportGame({content,projectRoot,destination:resolve(runtime,'rejected'),target:'windows',runtimes:{windows:runtime,web:runtime}}),/authored-recollection-v1/);
  v.screens[0].elements.at(-1).overrides[0].values.action='recollectionAuto';
  await assert.rejects(exportGame({content,projectRoot,destination:resolve(runtime,'rejected-auto'),target:'windows',runtimes:{windows:runtime,web:runtime}}),/recollection-playback-v1/);
});

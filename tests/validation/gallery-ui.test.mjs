import test from 'node:test';
import assert from 'node:assert/strict';
import {resolve} from 'node:path';
import {mkdtemp,writeFile} from 'node:fs/promises';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {validateProject} from '../../dist/packages/validation/index.js';
import {galleryRows} from '../../dist/packages/visual/gallery-data.js';

test('gallery projections enforce privacy, record identity, filters and selection without changing input',()=>{
  const config={title:'Memories',entries:[
    {id:'cg',kind:'cg',sourceId:'secret-cg',title:'Secret artwork',category:'Art',hint:'Find it'},
    {id:'music',kind:'music',sourceId:'secret-track',title:'Secret music',category:'Music',hint:'Listen',lockedDisplay:'hidden'},
    {id:'event',kind:'event',sourceId:'event',title:'Event',category:'Story',hint:'Play',thumbnailAssetId:'portrait'},
    {id:'ending',kind:'ending',sourceId:'ending',title:'Ending',category:'Story',hint:'Finish'},
  ]};
  const before=structuredClone(config);const locked=galleryRows(config);assert.equal(locked.length,3);
  assert.equal(locked[0].values.name,'???');assert.equal(locked[0].values.imageAssetId,'');assert.equal(locked[0].values.enabled,false);assert.equal(locked[0].enabled,true,'lock decoration stays visible');
  assert.ok(!JSON.stringify(locked).includes('secret-'));assert.deepEqual(galleryRows(config,{selectedId:'cg'},true),[]);
  const ui={entries:{cg:{records:[{kind:'cg',sourceId:'old-art'}]},event:{records:[{kind:'event',sourceId:'event',replayFrames:[{}]}]}}},snapshot=structuredClone(ui);
  assert.equal(galleryRows(config,ui)[0].values.unlocked,false,'reusing an ID does not unlock different content');
  assert.deepEqual(galleryRows(config,{...ui,status:'unlocked'}).map(r=>r.id),['event']);
  assert.deepEqual(galleryRows(config,{...ui,kind:'ending',status:'locked'}).map(r=>r.id),['ending']);
  assert.deepEqual(galleryRows(config,{...ui,category:'Art'}).map(r=>r.id),['cg']);
  const selected=galleryRows(config,{...ui,kind:'cg',status:'locked',selectedId:'event',recordIndex:-50},true);
  assert.equal(selected[0].values.canReplay,true);assert.equal(selected[0].values.recordLabel,'1 / 1');assert.equal(selected[0].values.imageAssetId,'portrait');
  assert.deepEqual(ui,snapshot);assert.deepEqual(config,before);
});

test('authored gallery schema validates screens, sources, entry references and filter actions',async()=>{
  const loaded=await loadProject(resolve('examples/last-grandson-polished'));const project=loaded.documents.find(d=>d.kind==='project').data;
  project.gallery.screenId='missing';let r=await validateProject(loaded);assert.ok(r.diagnostics.some(d=>d.pointer==='/gallery/screenId'));
  project.gallery.screenId='title';project.gallery.detailScreenId='title';r=await validateProject(loaded);assert.equal(r.valid,false);
  delete project.gallery.screenId;project.gallery.detailScreenId='title';r=await validateProject(loaded);assert.equal(r.valid,false);
  delete project.gallery.detailScreenId;
  const screen=project.visual.screens[0],button={id:'gallery-test',kind:'button',x:0,y:0,width:120,height:40,text:'Gallery',action:'galleryFilter',galleryFilter:'kind:cg'};screen.elements.push(button);
  assert.equal((await validateProject(loaded)).valid,true);
  button.galleryFilter='not-a-filter';assert.equal((await validateProject(loaded)).valid,false);delete button.galleryFilter;assert.equal((await validateProject(loaded)).valid,false);
  button.action='galleryView';button.entryId='missing';r=await validateProject(loaded);assert.ok(r.diagnostics.some(d=>d.pointer.endsWith('/entryId')));
  button.entryId='promise';assert.equal((await validateProject(loaded)).valid,true);
  delete project.gallery;assert.equal((await validateProject(loaded)).valid,false);
});

test('data-bound card images are valid, but unbound collections and standalone instances still need an image',async()=>{
  const loaded=await loadProject(resolve('examples/last-grandson-polished'));const project=loaded.documents.find(d=>d.kind==='project').data;
  const component={id:'gallery-image-test',label:'Gallery image',width:160,height:100,elements:[{id:'art',kind:'image',x:0,y:0,width:160,height:100,imageSource:'asset'}]};
  project.visual.components.push(component);
  const collection={id:'gallery-data-test',kind:'collection',x:0,y:0,width:320,height:200,componentId:component.id,source:{type:'galleryEntries'},bindings:[{elementId:'art',property:'imageAssetId',field:'imageAssetId'}]};
  const screen=project.visual.screens[0];screen.elements.push(collection);
  let r=await validateProject(loaded);assert.equal(r.valid,true,JSON.stringify(r.diagnostics));
  collection.bindings=[];r=await validateProject(loaded);assert.ok(r.diagnostics.some(d=>d.code==='UI_IMAGE_REQUIRED'));
  collection.bindings=[{elementId:'art',property:'imageAssetId',field:'imageAssetId'}];
  screen.elements.push({id:'unbound-image-test',kind:'component',x:0,y:0,width:160,height:100,componentId:component.id});
  r=await validateProject(loaded);assert.ok(r.diagnostics.some(d=>d.code==='UI_IMAGE_REQUIRED'&&d.pointer.includes('component/art')));
});

test('old runtimes reject authored gallery actions including component overrides',async()=>{
  const projectRoot=resolve('examples/last-grandson-polished');const loaded=await loadProject(projectRoot);const built=await buildSnapshot(loaded);assert.equal(built.ok,true);
  const runtime=await mkdtemp(resolve('.cache/gallery-ui-old-runtime-'));await writeFile(resolve(runtime,'RaisingPlayer.exe'),'fixture, never executed');
  await writeFile(resolve(runtime,'capabilities.json'),JSON.stringify({formatVersion:'1',imageExtensions:[],audioExtensions:[],visualFeatures:[]}));
  const content=structuredClone(built.snapshot),visual=content.project.visual,component=visual.components[0];
  visual.screens[0].elements.push({id:'gallery-override-test',kind:'component',componentId:component.id,x:0,y:0,width:component.width,height:component.height,overrides:[{elementId:component.elements[0].id,values:{action:'galleryOpen'}}]});
  await assert.rejects(exportGame({content,projectRoot,destination:resolve(runtime,'rejected-export'),target:'windows',runtimes:{windows:runtime,web:runtime}}),/authored-gallery-v1/);
});

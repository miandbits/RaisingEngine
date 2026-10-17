import test from 'node:test';
import assert from 'node:assert/strict';
import {resolve} from 'node:path';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {validateProject} from '../../dist/packages/validation/index.js';

test('gallery contract: legacy projects, stable IDs, missing targets and thumbnail types',async()=>{
  const loaded=await loadProject(resolve('examples/last-grandson-polished'));
  const doc=loaded.documents.find(d=>d.kind==='project');
  // Isolate the data contract from the sample's optional gallery screens/components.
  // Authored gallery UI is covered by its own tests and requires a gallery definition.
  delete doc.data.visual;
  delete doc.data.gallery;
  assert.equal((await validateProject(loaded)).valid,true);
  const entry={id:'promise',kind:'event',sourceId:'exam-invitation',title:'Promise',category:'Events',hint:'Complete the story',thumbnailAssetId:'winter-courtyard'};
  doc.data.gallery={title:'Memories',entries:[entry]};assert.equal((await validateProject(loaded)).valid,true);
  doc.data.gallery.entries.push({...entry});let r=await validateProject(loaded);assert.ok(r.diagnostics.some(d=>d.code==='DUPLICATE_ID'&&d.pointer==='/gallery/entries/1/id'));
  doc.data.gallery.entries=[{...entry,sourceId:'missing'}];r=await validateProject(loaded);assert.ok(r.diagnostics.some(d=>d.pointer==='/gallery/entries/0/sourceId'));
  doc.data.gallery.entries=[{...entry,thumbnailAssetId:'winter-motif'}];r=await validateProject(loaded);assert.ok(r.diagnostics.some(d=>d.code==='ASSET_TYPE'));
  doc.data.gallery.entries=[{...entry,kind:'unknown'}];assert.equal((await validateProject(loaded)).valid,false);
  const media={...entry,kind:'cg',sourceId:'winter-courtyard',lockedDisplay:'hidden',fit:'cover',unlockCondition:{op:'compare',comparator:'gte',left:{op:'read',target:{kind:'attribute',attributeId:'skill'}},right:{op:'literal',value:12}}};
  doc.data.gallery.entries=[media,{...entry,id:'music',kind:'music',sourceId:'winter-motif'}];assert.equal((await validateProject(loaded)).valid,true);
  doc.data.gallery.entries=[{...media,sourceId:'winter-motif'}];r=await validateProject(loaded);assert.ok(r.diagnostics.some(d=>d.code==='ASSET_TYPE'&&d.pointer.endsWith('/sourceId')));
  doc.data.gallery.entries=[{...media,unlockCondition:{op:'compare',comparator:'gte',left:{op:'read',target:{kind:'attribute',attributeId:'missing'}},right:{op:'literal',value:1}}}];r=await validateProject(loaded);assert.ok(r.diagnostics.some(d=>d.pointer.includes('unlockCondition')));
});

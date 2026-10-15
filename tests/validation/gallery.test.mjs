import test from 'node:test';
import assert from 'node:assert/strict';
import {resolve} from 'node:path';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {validateProject} from '../../dist/packages/validation/index.js';

test('gallery contract: legacy projects, stable IDs, missing targets and thumbnail types',async()=>{
  const loaded=await loadProject(resolve('examples/last-grandson-polished'));
  const doc=loaded.documents.find(d=>d.kind==='project');
  delete doc.data.gallery;
  assert.equal((await validateProject(loaded)).valid,true);
  const entry={id:'promise',kind:'event',sourceId:'exam-invitation',title:'Promise',category:'Events',hint:'Complete the story',thumbnailAssetId:'winter-courtyard'};
  doc.data.gallery={title:'Memories',entries:[entry]};assert.equal((await validateProject(loaded)).valid,true);
  doc.data.gallery.entries.push({...entry});let r=await validateProject(loaded);assert.ok(r.diagnostics.some(d=>d.code==='DUPLICATE_ID'&&d.pointer==='/gallery/entries/1/id'));
  doc.data.gallery.entries=[{...entry,sourceId:'missing'}];r=await validateProject(loaded);assert.ok(r.diagnostics.some(d=>d.pointer==='/gallery/entries/0/sourceId'));
  doc.data.gallery.entries=[{...entry,thumbnailAssetId:'winter-motif'}];r=await validateProject(loaded);assert.ok(r.diagnostics.some(d=>d.code==='ASSET_TYPE'));
  doc.data.gallery.entries=[{...entry,kind:'unknown'}];assert.equal((await validateProject(loaded)).valid,false);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, mkdir, readFile } from 'node:fs/promises';
import path from 'node:path';
import { QtStudioService } from '../../apps/qt-studio/service.mjs';

const cache=path.resolve('.cache/qt-service');
async function fixture(){await mkdir(cache,{recursive:true});const root=await mkdtemp(path.join(cache,'project-'));const service=new QtStudioService();await service.handle({method:'create',params:{root}});return {root,service};}
const project=s=>s.editor.documents.find(d=>d.kind==='project');
test('Qt project creation selects a template or reuses an independent content project',async()=>{
  await mkdir(cache,{recursive:true});const parent=await mkdtemp(path.join(cache,'creation-'));const service=new QtStudioService();
  await service.handle({method:'create',params:{root:path.join(parent,'minimal'),template:'academy-demo'}});
  assert.equal(project(service).data.durationMonths,2);
  await service.handle({method:'create',params:{root:path.join(parent,'copy'),template:'existing',source:path.join(parent,'minimal')}});
  assert.equal(project(service).data.durationMonths,2);assert.equal(service.editor.root,path.join(parent,'copy'));
  let p=structuredClone(project(service).data);p.title='Eight-year growth';p.durationMonths=96;p.startYear=1209;p.startMonth=4;
  await service.handle({method:'edit',params:{revision:service.editor.revision,edit:{type:'replace',file:'project.json',data:p}}});
  await service.handle({method:'save',params:{revision:service.editor.revision}});
  assert.equal(JSON.parse(await readFile(path.join(parent,'copy','project.json'),'utf8')).durationMonths,96);
  assert.equal(JSON.parse(await readFile(path.join(parent,'minimal','project.json'),'utf8')).durationMonths,2);
});
test('Qt shares authoring history, semantic validation and asset containment',async()=>{
  const {service}=await fixture();let p=structuredClone(project(service).data);
  const home=p.visual.screens.find(s=>s.id==='home');home.elements.push({id:'qt-test-shop',kind:'button',x:980,y:600,width:220,height:56,text:'상점',action:'visit',locationId:'market'});
  const revision=service.editor.revision;
  let r=await service.handle({method:'edit',params:{revision,edit:{type:'replace',file:'project.json',data:p}}});
  assert.equal(r.view.dirty,true);assert.equal(r.view.diagnostics.filter(d=>d.severity==='error').length,0);
  assert.ok(r.assets.length>0);assert.ok(r.assets.every(a=>a.absolutePath.startsWith(service.editor.root)));
  await assert.rejects(service.handle({method:'edit',params:{revision,edit:{type:'undo'}}}),/갱신/);
  await service.handle({method:'edit',params:{revision:r.view.revision,edit:{type:'undo'}}});
  assert.equal(project(service).data.visual.screens.find(s=>s.id==='home').elements.some(e=>e.id==='qt-test-shop'),false);
  await service.handle({method:'edit',params:{revision:service.editor.revision,edit:{type:'redo'}}});
  p=structuredClone(project(service).data);p.visual.screens.find(s=>s.id==='home').elements.find(e=>e.id==='qt-test-shop').locationId='missing-place';
  r=await service.handle({method:'edit',params:{revision:service.editor.revision,edit:{type:'replace',file:'project.json',data:p}}});
  assert.ok(r.view.diagnostics.some(d=>d.severity==='error'&&d.pointer.includes('/visual/screens/')));
  await assert.rejects(service.handle({method:'play',params:{revision:service.editor.revision}}),/오류/);
});
test('external authoring save cannot overwrite the Qt draft; retry preserves both versions',async()=>{
  const {root,service}=await fixture();const peer=new QtStudioService();await peer.handle({method:'open',params:{root}});
  let p=structuredClone(project(service).data);p.title='Qt local draft';
  await service.handle({method:'edit',params:{revision:service.editor.revision,edit:{type:'replace',file:'project.json',data:p}}});
  p=structuredClone(project(peer).data);p.title='External authoring change';
  await peer.handle({method:'edit',params:{revision:peer.editor.revision,edit:{type:'replace',file:'project.json',data:p}}});
  await peer.handle({method:'save',params:{revision:peer.editor.revision}});
  await assert.rejects(service.handle({method:'save',params:{revision:service.editor.revision}}));
  assert.equal(project(service).data.title,'Qt local draft');assert.equal(service.editor.view().dirty,true);
  assert.equal(JSON.parse(await readFile(path.join(root,'project.json'),'utf8')).title,'External authoring change');
});

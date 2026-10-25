import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp, mkdir, readFile, writeFile, symlink, readdir} from 'node:fs/promises';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {firstGameRoute} from './first-game-route.mjs';
import {runScenario} from '../../dist/packages/runtime-service/index.js';
import {encodeSave, decodeSave} from '../../dist/packages/runtime-service/save.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

async function fixture() {
  await mkdir('.cache/first-game-tests', {recursive:true});
  const work=await mkdtemp(path.resolve('.cache/first-game-tests/run-'));
  const runtime=path.join(work,'runtime');await mkdir(runtime);
  await writeFile(path.join(runtime,'index.html'),'export fixture');
  await writeFile(path.join(runtime,'capabilities.json'),JSON.stringify({formatVersion:'1',imageExtensions:['.png'],audioExtensions:[],visualFeatures:['readable-layout-v1','readable-actions-v1','compact-planning-v1','planning-guidance-v1','reading-history-v1','narrative-actor-binding-v1','component-context-v1','save-panel-v1','components-v1']}));
  const service=new QtStudioService({runtimes:{web:runtime,windows:runtime}});
  const reply=await service.handle({method:'create',params:{root:path.join(work,'game'),template:'my-first-raising-game'}});
  return {service,work,reply,runtime};
}
const request=(service,method,params={})=>service.handle({method,params:{revision:service.editor.revision,...params}});
const row=(reply,id)=>reply.firstGame.rows.find(r=>r.id===id);

test('starter has two reachable endings, save restoration, and valid controls for both routes',async()=>{
  const {service,reply}=await fixture();
  assert.equal(reply.view.diagnostics.length,0);
  const built=await service.editor.snapshot(service.editor.revision);assert.equal(built.ok,true);
  assert.deepEqual(built.snapshot.project.visual.readingHistory,{},'New designers receive the session reading utility by default');
  for(const doc of service.editor.documents.filter(d=>d.kind==='scenario')) {
    let checkpoints=0;
    const result=await runScenario(built.snapshot,doc.data,{checkpoint:async session=>{
      ++checkpoints;const encoded=await encodeSave(built.snapshot,session);assert.equal(encoded.ok,true);return decodeSave(built.snapshot,encoded.text);
    }});
    assert.equal(result.ok,true,JSON.stringify(result.error));assert.ok(checkpoints>5);
  }
  assert.equal(firstGameRoute(built.snapshot,true).ending,'together');
  assert.equal(firstGameRoute(built.snapshot,false).ending,'own-path');
  const peer=await fixture();
  assert.notEqual(service.editor.documents.find(d=>d.kind==='project').data.gameId,peer.service.editor.documents.find(d=>d.kind==='project').data.gameId,'separate games do not share save storage');
});

test('checklist records edits and verified play version, persists on reopen, and invalidates stale completion',async()=>{
  const {service,work,reply}=await fixture();assert.equal(reply.firstGame.rows.length,8);
  assert.ok(reply.firstGame.rows.every(r=>!r.complete));
  const data=structuredClone(service.editor.documents.find(d=>d.kind==='project').data);data.title='Willow Days';
  let r=await request(service,'edit',{edit:{type:'replace',file:'project.json',data}});
  assert.equal(row(r,'name').complete,true);
  r=await request(service,'edit',{edit:{type:'undo'}});assert.equal(row(r,'name').complete,false);
  r=await request(service,'edit',{edit:{type:'redo'}});assert.equal(row(r,'name').complete,true);
  const built=await service.editor.snapshot(service.editor.revision);
  service.firstGame.play={root:service.editor.root,version:service.editor.draftVersion(),fingerprint:built.snapshot.contentFingerprint};
  r=await request(service,'firstGameObserve',{phase:'finished',fingerprint:'wrong'});assert.equal(row(r,'check').complete,false);
  r=await request(service,'firstGameObserve',{phase:'finished',fingerprint:built.snapshot.contentFingerprint});assert.equal(row(r,'check').complete,true);
  r=await request(service,'export',{target:'web',destination:path.join(work,'published')});assert.equal(row(r,'export').complete,true);
  await request(service,'save');
  const reopened=new QtStudioService();r=await reopened.handle({method:'open',params:{root:service.editor.root}});
  assert.equal(row(r,'check').complete,true);assert.equal(row(r,'export').complete,true);
  data.title='Another season';r=await request(service,'edit',{edit:{type:'replace',file:'project.json',data}});
  assert.equal(row(r,'check').complete,false);assert.equal(row(r,'export').complete,false);
  r=await request(service,'firstGameObserve',{phase:'finished',fingerprint:built.snapshot.contentFingerprint});assert.equal(row(r,'check').complete,false,'an old player cannot certify a new draft');
});

test('malformed and symlinked progress cannot break project editing or escape storage',async()=>{
  const {service,work}=await fixture(),file=path.join(service.editor.root,'.studio/first-game.json');
  await writeFile(file,'{invalid');const r=await request(service,'view');assert.match(r.firstGame.warning,/could not be read/);assert.ok(r.view.documents.length>0);
  const other=path.join(work,'outside');await mkdir(other);
  const root=path.join(work,'symlink-project');await mkdir(root);await symlink(other,path.join(root,'.studio'));
  await assert.rejects(service.firstGame.file(root),/inside the project/);
  assert.deepEqual(await readdir(other),[]);
});

test('macOS places game resources inside its app and failed preparation cleans staging',async()=>{
  const {service,work,runtime}=await fixture();
  const executable=path.join(runtime,'RaisingPlayer.app/Contents/MacOS');await mkdir(executable,{recursive:true});
  await writeFile(path.join(executable,'RaisingPlayer'),'package fixture, never executed');
  const content=(await service.editor.snapshot(service.editor.revision)).snapshot;
  const options={content,projectRoot:service.editor.root,runtimes:{windows:runtime,web:runtime,macos:runtime,ios:runtime},target:'macos',destination:path.join(work,'mac')};
  const result=await exportGame(options);
  const bundle=JSON.parse(await readFile(path.join(result.directory,'RaisingPlayer.app/Contents/Resources/game.json'),'utf8'));
  assert.equal(bundle.content.project.title,content.project.title);
  await assert.rejects(exportGame({...options,destination:path.join(work,'failed'),prepareBundle:async()=>{throw Error('prepare failed');}}),/prepare failed/);
  assert.ok(!(await readdir(work)).some(f=>f.startsWith('.raising-export-')||f==='failed'));
  await assert.rejects(exportGame({...options,target:'ios',destination:path.join(work,'ios')}),/source preparation/);
});


test('new documents keep verified play and export completion after save and reopen',async()=>{
  const {service,work}=await fixture();
  await request(service,'edit',{edit:{type:'add',kind:'activity',id:'pottery'}});
  const built=await service.editor.snapshot(service.editor.revision);
  const version=service.editor.draftVersion();
  service.firstGame.play={root:service.editor.root,version,fingerprint:built.snapshot.contentFingerprint};
  await request(service,'firstGameObserve',{phase:'finished',fingerprint:built.snapshot.contentFingerprint});
  await request(service,'export',{target:'web',destination:path.join(work,'with-pottery')});
  await request(service,'save');
  const reopened=new QtStudioService();const r=await reopened.handle({method:'open',params:{root:service.editor.root}});
  assert.equal(reopened.editor.draftVersion(),version);
  assert.equal(row(r,'check').complete,true);assert.equal(row(r,'export').complete,true);
});

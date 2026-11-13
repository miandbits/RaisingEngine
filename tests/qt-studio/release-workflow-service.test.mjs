import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {createSession} from '../../dist/packages/runtime-service/session.js';
import {encodeSave} from '../../dist/packages/runtime-service/save.js';

const call=(service,method,params={})=>service.handle({method,params:{root:service.editor.root,revision:service.editor.revision,...params}});
async function fixture(t){
  await mkdir('.cache/release-workflow-service',{recursive:true});
  const work=await mkdtemp(path.resolve('.cache/release-workflow-service/run-')),runtime=path.join(work,'runtime');
  await mkdir(runtime);await writeFile(path.join(runtime,'index.html'),'Test runtime');
  await writeFile(path.join(runtime,'capabilities.json'),JSON.stringify({formatVersion:'1',imageExtensions:['.png'],fontExtensions:['.ttf'],audioExtensions:[],visualFeatures:['readable-layout-v1','readable-actions-v1','compact-planning-v1','planning-guidance-v1','reading-history-v1','narrative-actor-binding-v1','component-context-v1','save-panel-v1','components-v1']}));
  const service=new QtStudioService({runtimes:{web:runtime}});
  t.after(async()=>{await service.releaseWorkflow.close();await service.goalRouteBatches.close();await service.releases.close();await service.editor.close();});
  await call(service,'create',{root:path.join(work,'project'),template:'my-first-raising-game'});
  return {service,work};
}
async function finished(service,id){
  const deadline=Date.now()+60000;
  while(Date.now()<deadline){const {releaseJob}=await call(service,'releaseWorkflowStatus',{jobId:id});if(releaseJob.status!=='running')return releaseJob;await delay(25);}
  throw Error('Release verification exceeded its integration-test deadline.');
}

test('release RPCs fence the captured project; editor identity is available before a project is open',async t=>{
  const empty=new QtStudioService();t.after(()=>empty.editor.close());
  const identity=await empty.handle({method:'editorIdentity'});
  assert.equal(identity.editorIdentity.product,'Raising Studio');
  assert.equal(identity.editorIdentity.source.binaryVerified,false);
  await assert.rejects(empty.handle({method:'releaseWorkflowList',params:{root:null}}),/RELEASE_PROJECT_CHANGED/);
  const {service}=await fixture(t);
  for(const suffix of ['List','Register','AddCase','Start','Status','Cancel','Distribution'])
    await assert.rejects(call(service,'releaseWorkflow'+suffix,{root:path.join(service.editor.root,'previous-project')}),/RELEASE_PROJECT_CHANGED/);
  assert.deepEqual((await call(service,'releaseWorkflowList')).releaseWorkflow.releases,[]);
});

test('editor authoring stays usable during release checks and restart retains checks for exported bytes',async t=>{
  const {service:s,work}=await fixture(t),projectRoot=s.editor.root;
  const snapshot=(await s.editor.snapshot(s.editor.revision)).snapshot;
  const encoded=await encodeSave(snapshot,createSession(snapshot,42).session);assert.equal(encoded.ok,true);
  const save=path.join(work,'original-save.json');await writeFile(save,encoded.text);
  const exported=(await call(s,'export',{target:'web',releaseVersion:'1.0.0',destination:path.join(work,'release')})).exported;
  let entry=(await call(s,'releaseWorkflowRegister',{directory:exported.directory,name:'First release'})).releaseEntry;
  entry=(await call(s,'releaseWorkflowAddCase',{releaseId:entry.id,entryRevision:entry.entryRevision,name:'Opening save',file:save})).releaseEntry;
  const job=(await call(s,'releaseWorkflowStart',{releaseId:entry.id,entryRevision:entry.entryRevision,kind:'save'})).releaseJob;
  const data=structuredClone(s.editor.documents.find(d=>d.kind==='project').data);data.title='Next release draft';
  const edit=await call(s,'edit',{edit:{type:'replace',file:'project.json',data}});assert.equal(edit.view.dirty,true);
  const result=await finished(s,job.id);assert.equal(result.status,'completed',JSON.stringify(result));
  const row=(await call(s,'releaseWorkflowList')).releaseWorkflow.releases[0];
  assert.equal(row.identity.contentFingerprint,snapshot.contentFingerprint);
  assert.equal(row.cases[0].check.status,'passed');assert.equal(row.cases[0].check.directLoad,true);
  assert.ok(row.distribution.every(r=>r.status==='pending'),'Automated checks must not approve manual distribution');
  assert.equal(await readFile(save,'utf8'),encoded.text);
  assert.equal(s.editor.documents.find(d=>d.kind==='project').data.title,'Next release draft');
  await call(s,'create',{root:path.join(work,'another-project'),template:'my-first-raising-game'});
  await assert.rejects(call(s,'releaseWorkflowStatus',{root:projectRoot,jobId:job.id}),/RELEASE_PROJECT_CHANGED/);
  const reopened=new QtStudioService();t.after(async()=>{await reopened.releaseWorkflow.close();await reopened.editor.close();});
  await call(reopened,'open',{root:projectRoot});
  const retained=(await call(reopened,'releaseWorkflowList')).releaseWorkflow.releases[0];
  assert.equal(retained.id,row.id);assert.deepEqual(retained.cases,row.cases);
  assert.equal(reopened.editor.documents.find(d=>d.kind==='project').data.title,snapshot.project.title,'Unsaved future draft must not enter the release or source');
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile,access,open,appendFile} from 'node:fs/promises';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {readSampleSaveBytes} from '../../apps/qt-studio/update-compatibility.mjs';
import {MAX_SAVE_BYTES} from '../../dist/packages/runtime-service/save.js';
import {updateFixture} from './update-compatibility-fixture.mjs';
const call=(s,method,params={})=>s.handle({method,params:{revision:s.editor.revision,...params}});
async function fixture(t){await mkdir('.cache/update-compatibility',{recursive:true});const root=await mkdtemp(path.resolve('.cache/update-compatibility/run-')),paths=await updateFixture(root),runtime=path.join(root,'runtime');await mkdir(runtime);await writeFile(path.join(runtime,'index.html'),'test runtime');await writeFile(path.join(runtime,'capabilities.json'),JSON.stringify({formatVersion:'1',imageExtensions:['.png'],fontExtensions:['.ttf'],audioExtensions:[],visualFeatures:['readable-layout-v1','readable-actions-v1','compact-planning-v1','planning-guidance-v1','reading-history-v1','narrative-actor-binding-v1','component-context-v1','save-panel-v1','components-v1']}));const service=new QtStudioService({runtimes:{web:runtime}});t.after(async()=>{await service.releases.close();await service.editor.close();});await call(service,'open',{root:paths.to});return {service,root,...paths};}
async function change(file,edit){const value=JSON.parse(await readFile(file,'utf8'));edit(value);await writeFile(file,JSON.stringify(value,null,2));}

test('saved project compatibility distinguishes exact identity and visual edits without mutating source or history',async t=>{
 const {service:s,from,to,save}=await fixture(t),original=await readFile(save),before=s.editor.view();
 const result=(await call(s,'updateCompatibility',{previousProject:from,sampleSave:save})).updateCompatibility;
 assert.equal(result.classification,'changed-content');assert.equal(result.sample.valid,true);assert.equal(result.sample.commands,1);assert.equal(result.sample.hasPresentation,true);assert.notEqual(result.previousFingerprint,result.currentFingerprint);assert.equal(result.currentProject,to);assert.deepEqual(s.editor.view(),before);assert.deepEqual(await readFile(save),original);
 assert.deepEqual(await s.updateCompatibility.verify(s.editor,result.checkId),Object.fromEntries(Object.entries(result).filter(([k])=>k!=='checkId')));
 const same=(await call(s,'updateCompatibility',{previousProject:to})).updateCompatibility;assert.equal(same.classification,'same-content');assert.equal(same.sample,undefined);assert.equal(same.previousFingerprint,same.currentFingerprint);assert.deepEqual(s.editor.view(),before);
});

test('checks require a saved current draft, reject stale revisions and detect external changes',async t=>{
 const {service:s,from,to}=await fixture(t),oldRevision=s.editor.revision,p=structuredClone(s.editor.documents.find(d=>d.kind==='project').data);p.title+=' draft';await call(s,'edit',{edit:{type:'replace',file:'project.json',data:p}});
 await assert.rejects(call(s,'updateCompatibility',{previousProject:from}),/UPDATE_SAVE_FIRST/);assert.equal(JSON.parse(await readFile(path.join(to,'project.json'),'utf8')).title.endsWith('draft'),false);
 await assert.rejects(s.handle({method:'updateCompatibility',params:{revision:oldRevision,previousProject:from}}));await call(s,'save');const checked=(await call(s,'updateCompatibility',{previousProject:from})).updateCompatibility;
 await change(path.join(to,'project.json'),p=>{p.title+=' outside';});await assert.rejects(s.updateCompatibility.verify(s.editor,checked.checkId));
});

test('invalid samples and different games never receive a direct-load or migration assurance',async t=>{
 const {service:s,from,other,save,root}=await fixture(t);const tampered=path.join(root,'tampered.json');await writeFile(tampered,await readFile(save));await change(tampered,p=>{p.state.attributes[Object.keys(p.state.attributes)[0]]+=1;});
 let result=(await call(s,'updateCompatibility',{previousProject:from,sampleSave:tampered})).updateCompatibility;assert.equal(result.sample.valid,false);assert.equal(result.sample.error.code,'SAVE_STATE_MISMATCH');
 await writeFile(tampered,'{broken');result=(await call(s,'updateCompatibility',{previousProject:from,sampleSave:tampered})).updateCompatibility;assert.equal(result.sample.valid,false);assert.equal(result.sample.error.code,'SAVE_JSON');
 result=(await call(s,'updateCompatibility',{previousProject:other,sampleSave:save})).updateCompatibility;assert.equal(result.classification,'different-game');assert.equal(result.sample.valid,false);assert.equal(result.sample.error.code,'CONTENT_MISMATCH');
 await assert.rejects(call(s,'updateCompatibility',{previousProject:save}),/ENOTDIR|UPDATE_PROJECT_INVALID/);await assert.rejects(s.updateCompatibility.verify(s.editor,result.checkId),/UPDATE_CHECK_STALE/);
});

test('checked export rejects changed baseline or save bytes, then publishes only a freshly verified report',async t=>{
 const {service:s,from,save,root}=await fixture(t);let report=(await call(s,'updateCompatibility',{previousProject:from,sampleSave:save})).updateCompatibility;
 await writeFile(save,(await readFile(save,'utf8'))+'\n');await assert.rejects(call(s,'export',{target:'web',destination:path.join(root,'stale-save'),updateCheck:report.checkId}),/UPDATE_CHECK_STALE/);await assert.rejects(access(path.join(root,'stale-save')));
 report=(await call(s,'updateCompatibility',{previousProject:from,sampleSave:save})).updateCompatibility;await change(path.join(from,'project.json'),p=>{p.title+=' changed backup';});await assert.rejects(call(s,'export',{target:'web',destination:path.join(root,'stale-project'),updateCheck:report.checkId}),/UPDATE_CHECK_STALE/);
 report=(await call(s,'updateCompatibility',{previousProject:from})).updateCompatibility;await writeFile(path.join(from,'project.json'),(await readFile(path.join(from,'project.json'),'utf8'))+'\n');await assert.rejects(s.updateCompatibility.verify(s.editor,report.checkId),/UPDATE_CHECK_STALE/,'A reformatted backup invalidates the checked source receipt too');
 report=(await call(s,'updateCompatibility',{previousProject:from})).updateCompatibility;const output=await call(s,'export',{target:'web',destination:path.join(root,'checked'),updateCheck:report.checkId});assert.equal(output.exported.contentFingerprint,report.currentFingerprint);
});

test('checked export revalidates the previous backup immediately before publication',async t=>{
 const {service:s,from,root}=await fixture(t),report=(await call(s,'updateCompatibility',{previousProject:from})).updateCompatibility;
 const prepare=s.releases.prepare.bind(s.releases);s.releases.prepare=async(...args)=>{const result=await prepare(...args);await change(path.join(from,'project.json'),p=>{p.title+=' changed while packaging';});return result;};
 await assert.rejects(call(s,'export',{target:'web',releaseVersion:'0.2.0',destination:path.join(root,'raced'),updateCheck:report.checkId}),/UPDATE_CHECK_STALE/);await assert.rejects(access(path.join(root,'raced')));
});

test('existing explicit migration remains required, preserves originals and invalidates changed approval',async t=>{
 const {service:s,from,to,save,output}=await fixture(t),original=await readFile(save);
 let r=await call(s,'saveMigrationPreview',{from,to,save});assert.match(r.error,/MIGRATION_PRESENTATION/);
 r=await call(s,'saveMigrationPreview',{from,to,save,resetPresentation:true});assert.equal(r.error,null);assert.equal(r.migration.report.presentationReset,true);assert.deepEqual(r.migration.report.changes.map(x=>x.path),['/contentFingerprint']);
 let published=await call(s,'saveMigrationPublish',{from,to,save,output,resetPresentation:true,approval:r.migration.report.approval});assert.equal(published.error,null);assert.deepEqual(await readFile(save),original);
 published=await call(s,'saveMigrationPublish',{from,to,save,output:save,resetPresentation:true,approval:r.migration.report.approval});assert.match(published.error,/MIGRATION_IO/);assert.deepEqual(await readFile(save),original);
 published=await call(s,'saveMigrationPublish',{from,to,save,output:output+'.stale',resetPresentation:true,approval:'stale'});assert.match(published.error,/MIGRATION_STALE/);
});


test('sample reads accept the exact size boundary and stay bounded when a file grows after stat',async t=>{
 const {root}=await fixture(t),file=path.join(root,'size-boundary.json');await writeFile(file,Buffer.alloc(MAX_SAVE_BYTES,32));
 let handle=await open(file,'r');try{assert.equal((await readSampleSaveBytes(handle)).length,MAX_SAVE_BYTES);}finally{await handle.close();}
 handle=await open(file,'r');let readBytes=0,stats=0;
 try{await assert.rejects(readSampleSaveBytes({stat:async()=>{const info=await handle.stat();if(++stats===1)await appendFile(file,Buffer.alloc(1024,32));return info;},read:async(...args)=>{const result=await handle.read(...args);readBytes+=result.bytesRead;return result;}}),/UPDATE_SAVE_SIZE/);assert.equal(readBytes,MAX_SAVE_BYTES+1,'Growth cannot cause an unbounded file read');}finally{await handle.close();}
 handle=await open(file,'r');try{await assert.rejects(readSampleSaveBytes(handle),/UPDATE_SAVE_SIZE/);}finally{await handle.close();}
});

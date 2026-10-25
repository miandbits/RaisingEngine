import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile,symlink,unlink,readdir} from 'node:fs/promises';
import path from 'node:path';
import yauzl from 'yauzl';
import {materializeProject,digest,projectFiles} from '../../apps/qt-studio/project-files.mjs';
import {setTimeout as delay} from 'node:timers/promises';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {listPriorityProfiles,savePriorityProfile} from '../../apps/qt-studio/analysis-priorities.mjs';
const call=(s,method,params={})=>s.handle({method,params:{revision:s.editor.revision,...params}});
async function fixture(t){await mkdir('.cache/analysis-priorities',{recursive:true});const root=await mkdtemp(path.resolve('.cache/analysis-priorities/run-')),s=new QtStudioService();await call(s,'create',{root:path.join(root,'game'),template:'my-first-raising-game'});t.after(async()=>{for(const j of await s.analysisJobs.list(s.editor.root))if(j.status==='running')await s.analysisJobs.cancel(s.editor.root,j.id);await s.releases.close();await s.editor.close();});const content=(await s.editor.snapshot(s.editor.revision)).snapshot;const attribute=content.attributes[0],resource=content.attributes.find(a=>a.kind==='resource');assert.ok(resource);const profile={formatVersion:1,priorities:[{attributeId:attribute.id,direction:'increase',weight:3}],bands:[{attributeId:resource.id,min:resource.min,max:resource.max}]};return {s,root,content,profile,attribute,resource,file:path.join(s.editor.root,'playtests/player-priorities.json')};}
const list=async s=>(await call(s,'analysisPriorityProfiles')).analysis.priorityProfiles;
const save=async(s,params)=>(await call(s,'analysisPriorityProfileSave',params)).analysis.priorityProfiles;

test('saved priorities reopen without changing game documents, fingerprint, draft or revision',async t=>{
 const {s,content,profile}=await fixture(t),before=s.editor.view(),empty=await list(s);assert.equal(empty.revision,null);assert.deepEqual(empty.profiles,[]);
 const created=await save(s,{profilesRevision:null,name:'Water and learning',profile});assert.match(created.revision,/^[a-f0-9]{64}$/);assert.equal(created.profiles.length,1);assert.deepEqual(created.profiles[0].issues,[]);assert.equal(created.savedId,created.profiles[0].id);assert.deepEqual(s.editor.view(),before);assert.equal((await s.editor.snapshot(s.editor.revision)).snapshot.contentFingerprint,content.contentFingerprint);
 const peer=new QtStudioService();t.after(()=>peer.editor.close());await call(peer,'open',{root:s.editor.root});assert.deepEqual(await list(peer),Object.fromEntries(Object.entries(created).filter(([k])=>k!=='savedId')));
});

test('two editors cannot overwrite a newer profile; delete uses the same optimistic revision',async t=>{
 const {s,profile,file}=await fixture(t);const a=await save(s,{profilesRevision:null,name:'First',profile}),id=a.savedId;
 const b=await save(s,{profilesRevision:a.revision,id,name:'Reviewed',profile}),bytes=await readFile(file);
 await assert.rejects(save(s,{profilesRevision:a.revision,id,name:'Stale',profile}),/changed in another editor/);assert.deepEqual(await readFile(file),bytes);
 await assert.rejects(call(s,'analysisPriorityProfileDelete',{profilesRevision:a.revision,id}),/changed in another editor/);
 const d=(await call(s,'analysisPriorityProfileDelete',{profilesRevision:b.revision,id})).analysis.priorityProfiles;assert.deepEqual(d.profiles,[]);assert.notEqual(d.revision,b.revision);
 await assert.rejects(save(s,{profilesRevision:d.revision,id,name:'Removed',profile}),/no longer exists/);
});

test('unknown attribute references remain visible and can be repaired without losing other profiles',async t=>{
 const {s,profile,file}=await fixture(t);let a=await save(s,{profilesRevision:null,name:'Needs repair',profile});const first=a.savedId;a=await save(s,{profilesRevision:a.revision,name:'Keep me',profile});const second=a.savedId;
 const raw=JSON.parse(await readFile(file,'utf8'));raw.profiles.find(p=>p.id===first).profile.priorities[0].attributeId='removed-attribute';await writeFile(file,JSON.stringify(raw));
 const invalid=await list(s);assert.ok(invalid.profiles.find(p=>p.id===first).issues.length);assert.deepEqual(invalid.profiles.find(p=>p.id===second).issues,[]);
 await assert.rejects(call(s,'analysisStart',{priorityProfileId:first,profilesRevision:invalid.revision,options:{strategy:'priorities',runs:1,maxCommands:100,timeoutMs:1000}}));
 const repaired=await save(s,{profilesRevision:invalid.revision,id:first,name:'Repaired',profile});assert.ok(repaired.profiles.every(p=>p.issues.length===0));assert.equal(repaired.profiles.find(p=>p.id===second).name,'Keep me');
});

test('a running analysis owns the selected profile snapshot and cannot start from stale profile selection',async t=>{
 const {s,profile}=await fixture(t);const a=await save(s,{profilesRevision:null,name:'Original priorities',profile}),id=a.savedId;
 const r=await call(s,'analysisStart',{priorityProfileId:id,profilesRevision:a.revision,options:{strategy:'priorities',runs:1,maxCommands:200,timeoutMs:30000}}),job=r.analysis.job;
 assert.equal(job.options.priorityProfile.id,id);assert.equal(job.options.priorityProfile.name,'Original priorities');assert.equal(job.options.priorityProfile.priorities[0].weight,3);
 const changed=structuredClone(profile);changed.priorities[0].weight=9;await save(s,{profilesRevision:a.revision,id,name:'Later priorities',profile:changed});
 await assert.rejects(call(s,'analysisStart',{priorityProfileId:id,profilesRevision:a.revision,options:{strategy:'priorities',runs:1,maxCommands:20,timeoutMs:1000}}),/changed/);
 let stored;for(let i=0;i<300;i++){stored=await s.analysisJobs.get(s.editor.root,job.id);if(stored.status!=='running')break;await delay(100);}assert.equal(stored.status,'completed');assert.deepEqual(stored.options.priorityProfile,job.options.priorityProfile);assert.equal(stored.options.priorityProfile.priorities[0].weight,3);
 const trace=await s.analysisJobs.run(s.editor.root,job.id,0);const replay=await s.analysisJobs.replay(s.editor.root,job.id,0,trace.commands.length);assert.deepEqual(replay.state,trace.state);
});

test('profiles validate against the current unsaved draft and preserve that draft',async t=>{
 const {s,profile,content}=await fixture(t),doc=s.editor.documents.find(d=>d.kind==='attributes'),data=structuredClone(doc.data),old=s.editor.revision;
 const added={...data.attributes[0],id:'draft-only-attribute',label:'Draft only'};data.attributes.push(added);await call(s,'edit',{edit:{type:'replace',file:doc.file,data}});assert.equal(s.editor.view().dirty,true);
 const draftProfile={...profile,priorities:[{attributeId:added.id,direction:'increase',weight:2}]},before=s.editor.view();const saved=await save(s,{profilesRevision:null,name:'Unsaved attribute',profile:draftProfile});assert.deepEqual(s.editor.view(),before);assert.notEqual((await s.editor.snapshot(s.editor.revision)).snapshot.contentFingerprint,content.contentFingerprint);
 await assert.rejects(s.handle({method:'analysisPriorityProfileSave',params:{revision:old,profilesRevision:saved.revision,id:saved.savedId,name:'Stale draft',profile:draftProfile}}));
});

test('damaged, oversized and linked storage is rejected without modifying bytes or external targets',async t=>{
 const {s,profile,file,root,content}=await fixture(t);await save(s,{profilesRevision:null,name:'Safe',profile});
 for(const text of ['{broken',' '.repeat(1024*1024+1)]){await writeFile(file,text);await assert.rejects(list(s));await assert.rejects(save(s,{profilesRevision:null,name:'Replace',profile}));assert.equal(await readFile(file,'utf8'),text);}
 await unlink(file);const outside=path.join(root,'outside.json');await writeFile(outside,'{"untouched":true}');await symlink(outside,file);await assert.rejects(list(s),/ordinary project file/);assert.equal(await readFile(outside,'utf8'),'{"untouched":true}');await unlink(file);
 const linkedRoot=path.join(root,'linked-project'),external=path.join(root,'external');await mkdir(linkedRoot);await mkdir(external);await symlink(external,path.join(linkedRoot,'playtests'),'dir');await assert.rejects(listPriorityProfiles(linkedRoot,content),/inside the project/);assert.deepEqual(await readdir(external),[]);
});

test('concurrent profile writers have one winner and game identity mismatches never rebind silently',async t=>{
 const {s,profile,content,file}=await fixture(t);const input={profilesRevision:null,name:'Concurrent',profile};const settled=await Promise.allSettled([savePriorityProfile(s.editor.root,content,input),savePriorityProfile(s.editor.root,content,input)]);assert.equal(settled.filter(r=>r.status==='fulfilled').length,1);assert.equal((await list(s)).profiles.length,1);
 const bytes=await readFile(file),other=structuredClone(content);other.project.gameId+='-other';await assert.rejects(listPriorityProfiles(s.editor.root,other),/different game identity/);await assert.rejects(savePriorityProfile(s.editor.root,other,input),/different game identity/);assert.deepEqual(await readFile(file),bytes);
});

async function zipFiles(file){return new Promise((resolve,reject)=>yauzl.open(file,{lazyEntries:true},(error,zip)=>{if(error)return reject(error);const files=new Map();zip.on('error',reject);zip.on('end',()=>resolve(files));zip.on('entry',entry=>zip.openReadStream(entry,(error,stream)=>{if(error)return reject(error);const chunks=[];stream.on('error',reject);stream.on('data',chunk=>chunks.push(chunk));stream.on('end',()=>{files.set(entry.fileName,Buffer.concat(chunks));zip.readEntry();});}));zip.readEntry();}));}

test('release project ZIP and recovery retain exact saved priorities; game ZIP excludes them',async t=>{
 const {s,root,profile,file,content}=await fixture(t),saved=await save(s,{profilesRevision:null,name:'Portable balancing setup',profile}),bytes=await readFile(file);
 const stage=path.join(root,'release');await mkdir(stage);await writeFile(path.join(stage,'index.html'),'game');
 const manifest=await s.releases.prepare(stage,{editor:s.editor,target:'web',version:'1.0.0',content,contentFingerprint:content.contentFingerprint,engineRoot:path.resolve('.')});
 const name='playtests/player-priorities.json',backup=await zipFiles(path.join(stage,'Project-backup.zip')),game=await zipFiles(path.join(stage,'Game.zip'));
 assert.deepEqual(backup.get(name),bytes);assert.equal(manifest.sourceFiles.find(f=>f.file===name).sha256,digest(bytes));assert.equal(game.has(name),false);assert.ok(![...backup.keys()].some(f=>f.startsWith('.studio/')));
 const destination=path.join(root,'from-backup');await materializeProject(backup,destination);const reopened=new QtStudioService();t.after(()=>reopened.editor.close());await call(reopened,'open',{root:destination});assert.deepEqual(await list(reopened),Object.fromEntries(Object.entries(saved).filter(([k])=>k!=='savedId')));
 const doc=s.editor.documents.find(d=>d.kind==='project'),draft=structuredClone(doc.data);draft.title='Recovered priorities';await call(s,'edit',{edit:{type:'replace',file:doc.file,data:draft}});
 const updated=await save(s,{profilesRevision:saved.revision,id:saved.savedId,name:'Latest while draft is dirty',profile});const recoveredBytes=await readFile(file);
 const [recovery]=await s.recovery.list(s.editor.root);assert.ok(recovery);const restored=await s.recovery.restore(s.editor.root,{id:recovery.id,revision:recovery.revision,destination:path.join(root,'from-recovery')});
 assert.deepEqual(await readFile(path.join(restored,name)),recoveredBytes);assert.deepEqual(await listPriorityProfiles(restored,content),Object.fromEntries(Object.entries(updated).filter(([k])=>k!=='savedId')));
 await writeFile(file,' '.repeat(1024*1024+1));await assert.rejects(projectFiles(s.editor),/exceeds 1 MiB/);
});

test('read-only samples allow inspection but reject profile writes',async t=>{
 const {s,profile,file}=await fixture(t),saved=await save(s,{profilesRevision:null,name:'Existing',profile}),bytes=await readFile(file);s.editor.readOnly=true;
 assert.equal((await list(s)).profiles.length,1);
 await assert.rejects(save(s,{profilesRevision:saved.revision,name:'No write',profile}),/read-only/);
 await assert.rejects(call(s,'analysisPriorityProfileDelete',{profilesRevision:saved.revision,id:saved.savedId}),/read-only/);
 assert.deepEqual(await readFile(file),bytes);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile,access,readdir,chmod,rename,cp} from 'node:fs/promises';
import {spawnSync} from 'node:child_process';
import {randomUUID} from 'node:crypto';
import {verifyRelease} from '../../apps/qt-studio/release-workflow-checks.mjs';
import {encodeRgbaPng} from '../../dist/packages/studio-service/png.js';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {ReleaseWorkflow} from '../../apps/qt-studio/release-workflow.mjs';
import {updateFixture} from './update-compatibility-fixture.mjs';
import {createSession} from '../../dist/packages/runtime-service/session.js';
import {encodeSave} from '../../dist/packages/runtime-service/save.js';
import {digest} from '../../apps/qt-studio/project-files.mjs';
import {loadProject,buildSnapshot} from '../../dist/packages/project-service/index.js';
import {readProjectBackup,verifyProjectBackup,restoreProjectBackup} from '../../apps/qt-studio/backup-restore.mjs';
import {treeFiles,writeZip} from '../../apps/qt-studio/release-zip.mjs';
const call=(s,method,params={})=>s.handle({method,params:{revision:s.editor.revision,...params}});
async function fixture(t){
 await mkdir('.cache/release-workflow',{recursive:true});const root=await mkdtemp(path.resolve('.cache/release-workflow/test-')),paths=await updateFixture(root),runtime=path.join(root,'runtime');await mkdir(runtime);await writeFile(path.join(runtime,'index.html'),'test player');
 await writeFile(path.join(runtime,'capabilities.json'),JSON.stringify({formatVersion:'1',imageExtensions:['.png'],fontExtensions:['.ttf'],audioExtensions:[],visualFeatures:['readable-layout-v1','readable-actions-v1','compact-planning-v1','planning-guidance-v1','reading-history-v1','narrative-actor-binding-v1','component-context-v1','save-panel-v1','components-v1']}));
 const service=new QtStudioService({runtimes:{web:runtime}}),workflow=new ReleaseWorkflow();t.after(async()=>{await workflow.close();await service.releases.close();await service.editor.close();});await call(service,'open',{root:paths.to});
 const output=await call(service,'export',{target:'web',releaseVersion:'1.0.0',destination:path.join(root,'release')});assert.ok(output.exported,output.error);const entry=await workflow.register(service.editor,{directory:output.exported.directory,name:'Release one'}),content=(await service.editor.snapshot(service.editor.revision)).snapshot;
 const current=await encodeSave(content,createSession(content,42).session);assert.ok(current.ok);const currentSave=path.join(root,'current-save.json');await writeFile(currentSave,current.text);
 return {root,...paths,service,workflow,entry,content,currentSave,release:output.exported.directory};
}
const params=entry=>({releaseId:entry.id,entryRevision:entry.entryRevision});
async function latest(f){return (await f.workflow.list(f.service.editor)).releases.find(e=>e.id===f.entry.id);}
async function done(f,job){const until=Date.now()+60000;while(job.status==='running'&&Date.now()<until){await new Promise(r=>setTimeout(r,20));job=await f.workflow.status(f.service.editor,{jobId:job.id});}assert.notEqual(job.status,'running','background job completed');return job;}
async function run(f,kind,extra={}){return done(f,await f.workflow.start(f.service.editor,{...params(await latest(f)),kind,...extra}));}

test('release ledger persists immutable identities and explicitly manual pending acceptance without exporting private evidence',async t=>{
 const f=await fixture(t),{workflow:w,service:s,entry,release}=f,manifest=JSON.parse(await readFile(path.join(release,'RELEASE.json'),'utf8'));
 assert.equal(entry.id,digest(await readFile(path.join(release,'RELEASE.json'))));assert.equal(manifest.gameId,f.content.project.gameId);assert.ok('source' in manifest.engineIdentity);assert.deepEqual(entry.identity.engineIdentity,manifest.engineIdentity);
 assert.equal(entry.checks.release.status,'pending');assert.ok(entry.distribution.every(x=>x.status==='pending'&&x.kind==='manual'));
 const duplicate=await w.register(s.editor,{directory:release,name:'Different display name'});assert.deepEqual(duplicate,entry);const reopened=new ReleaseWorkflow();assert.deepEqual((await reopened.list(s.editor)).releases,[entry]);
 assert.ok(!manifest.files.some(f=>f.file.includes('release-workflow')));assert.ok(!manifest.sourceFiles.some(f=>f.file.includes('release-workflow')));
 await assert.rejects(w.distribution(s.editor,{...params(entry),checkId:'rights',status:'passed',note:''}),/ATTESTATION/);
 const noted=await w.distribution(s.editor,{...params(entry),checkId:'rights',status:'passed',note:'Designer reviewed the listed source licenses.'});assert.equal(noted.distribution.find(c=>c.id==='rights').kind,'manual');assert.equal(noted.checks.release.status,'pending');
 await assert.rejects(w.addCase(s.editor,{...params(entry),name:'stale request',file:f.save}),/ENTRY_STALE/);
 assert.match((await w.list(s.editor)).readiness,/No automatic/);
});

test('explicit registration rebinds a moved identical release while preserving identity and invalidating old acceptance',async t=>{
 const f=await fixture(t);assert.equal((await run(f,'recovery')).status,'completed');const before=await latest(f),moved=path.join(f.root,'moved-release');await rename(f.release,moved);
 const rebound=await f.workflow.register(f.service.editor,{directory:moved});assert.equal(rebound.id,before.id);assert.notEqual(rebound.entryRevision,before.entryRevision);assert.equal(rebound.directory,moved);assert.equal(rebound.previousLocations[0].directory,f.release);assert.equal(rebound.checks.recovery.status,'stale');assert.equal(rebound.checks.release.status,'stale');assert.deepEqual(rebound.distribution,before.distribution);
 assert.equal((await run(f,'release')).status,'completed');assert.equal((await latest(f)).checks.release.status,'passed');assert.equal((await latest(f)).checks.recovery.status,'stale','A fresh integrity check does not silently renew old restoration evidence');
});

test('background save cases strictly validate the selected exported release and retain honest migration requirements',async t=>{
 const f=await fixture(t),{workflow:w,service:s}=f,original=await readFile(f.save),current=await readFile(f.currentSave);
 let e=await w.addCase(s.editor,{...params(await latest(f)),name:'Previous release result',file:f.save,sourceProject:f.from});e=await w.addCase(s.editor,{...params(e),name:'Current release',file:f.currentSave,kind:'migrated'});
 const job=await w.start(s.editor,{...params(e),kind:'save'});assert.equal(job.status,'running');assert.equal((await call(s,'view')).error,null,'Ordinary editor view remains available during the check');
 const result=await done(f,job);assert.equal(result.status,'completed',result.message);e=await latest(f);
 assert.equal(e.cases[0].check.status,'migration-required');assert.equal(e.cases[0].check.sourceValidated,true);assert.equal(e.cases[0].check.directLoad,false);assert.equal(e.cases[1].check.status,'passed');assert.equal(e.cases[1].check.caseKind,'migrated');assert.match(e.cases[1].provenance,/not inferred/);
 assert.equal(e.checks.release.status,'passed');assert.ok(e.distribution.every(x=>x.status==='pending'));assert.deepEqual(await readFile(f.save),original);assert.deepEqual(await readFile(f.currentSave),current);
 assert.equal((await readdir(f.root)).some(n=>n.includes('migrated')),false,'No migration output is created by checking');
});

test('verified presentation releases check old saves against exported baseline and disclose different restored authoring identity',async t=>{
 const f=await fixture(t),{service:s,workflow:w}=f,base=(await buildSnapshot(await loadProject(f.from))).snapshot,project=structuredClone(base.project);
 const button=project.visual.screens[0].elements.find(e=>e.kind==='button');button.text='A clearer beginning';button.fill='#223344';await s.editor.close();await writeFile(path.join(f.to,'project.json'),JSON.stringify(project));await call(s,'open',{root:f.to});
 const caps=path.join(f.root,'runtime/capabilities.json'),value=JSON.parse(await readFile(caps,'utf8'));value.visualFeatures.push('presentation-visual-updates-v1');await writeFile(caps,JSON.stringify(value));
 const previousBundle=path.join(f.root,'published-baseline.json');await writeFile(previousBundle,JSON.stringify({formatVersion:'1',content:base,assets:{}}));
 const checked=(await call(s,'presentationUpdateCheck',{target:'web',previousBundle,sampleSave:f.save})).presentationUpdate;assert.equal(checked.classification,'presentation-only');
 const exported=(await call(s,'export',{target:'web',destination:path.join(f.root,'presentation-release'),releaseVersion:'1.0.1',presentationCheck:checked.checkId})).exported;assert.ok(exported);
 let entry=await w.register(s.editor,{directory:exported.directory});assert.notEqual(entry.identity.contentFingerprint,entry.identity.sourceContentFingerprint);entry=await w.addCase(s.editor,{...params(entry),name:'Original baseline save',file:f.save});const selected={...f,entry};
 assert.equal((await run(selected,'save')).status,'completed');assert.equal((await latest(selected)).cases[0].check.status,'passed');assert.equal((await latest(selected)).cases[0].check.directLoad,true);
 const recovery=await run(selected,'recovery',{destination:path.join(f.root,'restored-presentation-source')});assert.equal(recovery.status,'completed',recovery.message);assert.equal(recovery.result.recovery.restored,true);assert.equal(recovery.result.recovery.sourceMatchesRuntime,false);assert.equal(recovery.result.recovery.sourceContentFingerprint,entry.identity.sourceContentFingerprint);assert.equal(recovery.result.recovery.runtimeContentFingerprint,entry.identity.contentFingerprint);
});

test('selected release rejects another valid backup including same-gameplay changes to authoring source before restoration',async t=>{
 const f=await fixture(t),{service:s,workflow:w}=f,outer=JSON.parse(await readFile(path.join(f.release,'RELEASE.json'),'utf8'));
 const profiles=(await call(s,'analysisPriorityProfiles')).analysis.priorityProfiles;
 await call(s,'analysisPriorityProfileSave',{profilesRevision:profiles.revision,name:'Source-only priorities',profile:{formatVersion:1,priorities:[{attributeId:f.content.attributes[0].id,direction:'increase',weight:1}],bands:[]}});
 const metadata=(await call(s,'export',{target:'web',releaseVersion:'1.0.0',destination:path.join(f.root,'metadata-release')})).exported;
 const metadataReceipt=JSON.parse(await readFile(path.join(metadata.directory,'RELEASE.json'),'utf8'));assert.equal(metadataReceipt.contentFingerprint,outer.contentFingerprint);assert.notDeepEqual(metadataReceipt.sourceFiles,outer.sourceFiles);
 await call(s,'open',{root:f.other});const different=(await call(s,'export',{target:'web',releaseVersion:'1.0.0',destination:path.join(f.root,'different-game-release')})).exported;await call(s,'open',{root:f.to});
 for(const [name,other]of [['same-gameplay',metadata],['different-game',different]]){
  const directory=path.join(f.root,'crosswired-'+name),destination=path.join(f.root,'uncreated-'+name,'restored');await cp(f.release,directory,{recursive:true});const archive=path.join(directory,outer.archives.project.file),bytes=await readFile(path.join(other.directory,'Project-backup.zip'));
  const changed=structuredClone(outer);changed.archives.project.sha256=digest(bytes);await writeFile(archive,bytes);await writeFile(path.join(directory,'RELEASE.json'),JSON.stringify(changed));
  assert.equal((await verifyProjectBackup(archive)).ok,true,'The swapped archive is independently valid');
  await assert.rejects(verifyProjectBackup(archive,{expectedSha256:changed.archives.project.sha256,expectedRelease:changed}),/RELEASE_BACKUP_BINDING/);
  await assert.rejects(restoreProjectBackup(archive,{destination,expectedSha256:changed.archives.project.sha256,expectedRelease:changed,originalRoot:f.to}),/RELEASE_BACKUP_BINDING/);
  await assert.rejects(access(path.dirname(destination)),{code:'ENOENT'},'Mismatch rejected before creating destination parents');
  const entry=await w.register(s.editor,{directory}),selected={...f,entry},job=await run(selected,'recovery',{destination});assert.equal(job.status,'failed');assert.match(job.message,/RELEASE_INTEGRITY: RELEASE_BACKUP_BINDING/);
  const recorded=await latest(selected);assert.equal(recorded.checks.release.status,'failed');assert.equal(recorded.integrityKnownStale,true);assert.notEqual(recorded.checks.recovery.status,'passed');await assert.rejects(access(destination),{code:'ENOENT'});
 }
});

test('bound backup acceptance ignores source row order and supports historical receipts without gameId',async t=>{
 const f=await fixture(t),{workflow:w,service:s}=f,directory=path.join(f.root,'historical-release');await cp(f.release,directory,{recursive:true});
 const manifestFile=path.join(directory,'RELEASE.json'),outer=JSON.parse(await readFile(manifestFile,'utf8')),archive=path.join(directory,outer.archives.project.file),backup=await readProjectBackup(archive),source=path.join(f.root,'historical-source');await mkdir(source);
 delete backup.manifest.gameId;backup.files.set('RELEASE.json',Buffer.from(JSON.stringify(backup.manifest)));
 for(const [file,bytes]of backup.files){await mkdir(path.dirname(path.join(source,file)),{recursive:true});await writeFile(path.join(source,file),bytes);}const historicalZip=path.join(f.root,'historical-project.zip');await writeZip(source,await treeFiles(source),historicalZip);await cp(historicalZip,archive);
 outer.archives.project.sha256=digest(await readFile(archive));outer.sourceFiles.reverse();await writeFile(manifestFile,JSON.stringify(outer));
 let entry=await w.register(s.editor,{directory}),selected={...f,entry};let job=await run(selected,'recovery',{destination:path.join(f.root,'legacy-inner-restored')});assert.equal(job.status,'completed',job.message);assert.equal(job.result.recovery.sourceMatchesRuntime,true);
 delete outer.gameId;await writeFile(manifestFile,JSON.stringify(outer));entry=await w.register(s.editor,{directory});selected={...f,entry};job=await run(selected,'recovery',{destination:path.join(f.root,'legacy-both-restored')});assert.equal(job.status,'completed',job.message);assert.equal(job.result.recovery.sourceMatchesRuntime,true);
 await assert.rejects(verifyRelease({...entry,gameId:'another-game'}),/RELEASE_BACKUP_BINDING/,'The registered game identity also binds legacy project bytes');
 const verified=await verifyProjectBackup(archive,{expectedSha256:outer.archives.project.sha256,expectedRelease:outer});assert.equal(verified.contentFingerprintVerified,true);assert.equal(verified.contentFingerprint,f.content.contentFingerprint);
 for(const field of ['version','target','contentFingerprint','sourceContentFingerprint','gameId']){const expected={...outer,[field]:field==='sourceContentFingerprint'?`sha256:${'f'.repeat(64)}`:'different'};await assert.rejects(readProjectBackup(archive,{expectedRelease:expected}),/RELEASE_BACKUP_BINDING/,field);}
});

test('changed original saves, source projects and exported files revoke checked results instead of retaining a successful badge',async t=>{
 const f=await fixture(t),{workflow:w,service:s}=f;let e=await w.addCase(s.editor,{...params(await latest(f)),name:'Original',file:f.save,sourceProject:f.from});assert.equal((await run(f,'save')).status,'completed');
 const sourceFile=path.join(f.from,'project.json'),oldSource=await readFile(sourceFile);await writeFile(sourceFile,Buffer.concat([oldSource,Buffer.from('\n')]));let result=await run(f,'save');assert.equal(result.status,'completed',result.message);assert.equal((await latest(f)).cases[0].check.status,'stale');await writeFile(sourceFile,oldSource);
 e=await w.addCase(s.editor,{...params(await latest(f)),name:'Unchanged current case',file:f.currentSave});
 const oldSave=await readFile(f.save);await writeFile(f.save,Buffer.concat([oldSave,Buffer.from('\n')]));result=await run(f,'save');assert.equal(result.status,'completed',result.message);assert.equal((await latest(f)).cases[0].check.status,'stale');assert.equal((await latest(f)).cases[1].check.status,'passed','One changed sample does not prevent independent cases from finishing');await writeFile(f.save,oldSave);
 assert.equal((await run(f,'recovery')).status,'completed');
 await writeFile(path.join(f.release,'index.html'),'tampered exported runtime');result=await run(f,'release');assert.equal(result.status,'stale',result.message);e=await latest(f);assert.equal(e.checks.release.status,'stale');assert.equal(e.checks.recovery.status,'stale');assert.ok(e.cases.every(c=>c.check.status==='stale'));assert.equal(e.integrityKnownStale,true);
});

test('previous asset bytes participate in save-source staleness even when document bytes are unchanged',async t=>{
 const f=await fixture(t),{workflow:w,service:s}=f,loaded=await loadProject(f.from),assets=loaded.documents.find(d=>d.kind==='assets'),file=path.join(f.from,'assets/probe.png');
 assets.data.assets.push({id:'probe',kind:'image',path:'assets/probe.png'});await writeFile(path.join(f.from,assets.file),JSON.stringify(assets.data));await writeFile(file,Buffer.from(encodeRgbaPng({width:1,height:1,pixels:Buffer.from([1,2,3,255])})));
 const source=await buildSnapshot(await loadProject(f.from));assert.ok(source.ok,JSON.stringify(source.diagnostics));const save=await encodeSave(source.snapshot,createSession(source.snapshot,42).session);assert.ok(save.ok);await writeFile(f.save,save.text);
 await w.addCase(s.editor,{...params(await latest(f)),name:'Previous source',file:f.save,sourceProject:f.from});assert.equal((await run(f,'save')).status,'completed');
 const before=await readFile(file);await writeFile(file,Buffer.concat([before,Buffer.from([0])]));
 const result=await run(f,'save');assert.equal(result.status,'completed');assert.equal((await latest(f)).cases[0].check.status,'stale');
});

test('archive hashes alone never accept a symlink payload and legacy receipts still bind the actual game identity',async t=>{
 const f=await fixture(t);await assert.rejects(verifyRelease({...f.entry,gameId:'another-game'}),/RELEASE_GAME|RELEASE_BACKUP_BINDING/);
 const archive=path.join(f.release,'Game.zip'),bytes=await readFile(archive),central=bytes.indexOf(Buffer.from([0x50,0x4b,0x01,0x02]));assert.ok(central>=0);bytes.writeUInt32LE(((0xa000|0o777)<<16)>>>0,central+38);await writeFile(archive,bytes);
 const file=path.join(f.release,'RELEASE.json'),manifest=JSON.parse(await readFile(file,'utf8'));manifest.archives.game.sha256=digest(bytes);await writeFile(file,JSON.stringify(manifest));
 const entry=await f.workflow.register(f.service.editor,{directory:f.release,name:'Unsafe archive probe'}),job=await done(f,await f.workflow.start(f.service.editor,{...params(entry),kind:'release'}));assert.equal(job.status,'failed');assert.match(job.message,/RELEASE_ARCHIVE/);
});

test('Game.zip CRC corruption is rejected even when the archive hash was freshly registered',async t=>{
 const f=await fixture(t),archive=path.join(f.release,'Game.zip'),bytes=await readFile(archive),central=bytes.indexOf(Buffer.from([0x50,0x4b,0x01,0x02]));bytes.writeUInt32LE((bytes.readUInt32LE(central+16)^0x01010101)>>>0,central+16);await writeFile(archive,bytes);
 const file=path.join(f.release,'RELEASE.json'),manifest=JSON.parse(await readFile(file,'utf8'));manifest.archives.game.sha256=digest(bytes);await writeFile(file,JSON.stringify(manifest));const entry=await f.workflow.register(f.service.editor,{directory:f.release}),job=await done(f,await f.workflow.start(f.service.editor,{...params(entry),kind:'release'}));assert.equal(job.status,'failed');assert.match(job.message,/CRC/);
});

test('dead writer leases are recovered without removing a live writer and close waits for acceptance persistence',async t=>{
 const f=await fixture(t),{workflow:w,service:s}=f,dead=spawnSync(process.execPath,['-e','process.exit(0)']);assert.equal(dead.status,0);const folder=path.join(f.to,'.studio/release-workflow/writers',dead.pid+'_'+randomUUID());await mkdir(folder);await writeFile(path.join(folder,'ticket.json'),JSON.stringify({version:1,number:1}));
 await w.addCase(s.editor,{...params(await latest(f)),name:'After interrupted writer',file:f.currentSave});await assert.rejects(access(folder));
 const job=await w.start(s.editor,{...params(await latest(f)),kind:'release'});await w.close();assert.notEqual((await w.status(s.editor,{jobId:job.id})).status,'running');assert.notEqual((await latest(f)).checks.release.status,'running');
});

test('cancelling a blocked process-transport replay retires the owned native descendant',{skip:process.platform==='win32',timeout:30000},async t=>{
 const f=await fixture(t),{workflow:w,service:s}=f;await w.addCase(s.editor,{...params(await latest(f)),name:'Current save',file:f.currentSave});
 const pidFile=path.join(f.root,'native.pid'),executable=path.join(f.root,'blocked-native.cjs');await writeFile(executable,'#!'+process.execPath+'\n'+`require('node:fs').writeFileSync(${JSON.stringify(pidFile)},String(process.pid)); process.stdin.resume(); setInterval(()=>{},1000);`);await chmod(executable,0o755);
 const oldTransport=process.env.RAISING_NATIVE_TRANSPORT,oldExe=process.env.RAISING_NATIVE_EXECUTABLE;let job;
 try{process.env.RAISING_NATIVE_TRANSPORT='process';process.env.RAISING_NATIVE_EXECUTABLE=executable;job=await w.start(s.editor,{...params(await latest(f)),kind:'save'});}finally{if(oldTransport===undefined)delete process.env.RAISING_NATIVE_TRANSPORT;else process.env.RAISING_NATIVE_TRANSPORT=oldTransport;if(oldExe===undefined)delete process.env.RAISING_NATIVE_EXECUTABLE;else process.env.RAISING_NATIVE_EXECUTABLE=oldExe;}
 let pid;for(let i=0;i<300;i++){try{pid=Number(await readFile(pidFile,'utf8'));break;}catch{await new Promise(r=>setTimeout(r,20));}}
 assert.ok(pid,'The fake native transport actually started');assert.equal((await w.cancel(s.editor,{jobId:job.id})).status,'cancelled');
 for(let i=0;i<100;i++){try{process.kill(pid,0);await new Promise(r=>setTimeout(r,20));}catch(e){assert.equal(e.code,'ESRCH');return;}}assert.fail('Owned native descendant remained alive after cancellation');
});

test('recovery verifies archives and restores only to new folders, preserving the release and original project',async t=>{
 const f=await fixture(t),manifest=await readFile(path.join(f.release,'RELEASE.json')),original=await readFile(path.join(f.to,'project.json'));
 let job=await run(f,'recovery');assert.equal(job.status,'completed',job.message);assert.equal(job.result.recovery.restored,false);assert.equal(job.result.recovery.verification.contentFingerprint,f.content.contentFingerprint);
 const destination=path.join(f.root,'recovered');job=await run(f,'recovery',{destination});assert.equal(job.status,'completed',job.message);assert.equal(job.result.recovery.directory,destination);assert.equal(job.result.recovery.restored,true);
 const rebuilt=await buildSnapshot(await loadProject(destination));assert.ok(rebuilt.ok);assert.equal(rebuilt.snapshot.contentFingerprint,f.content.contentFingerprint);
 assert.equal((await run(f,'recovery',{destination})).status,'failed');assert.equal((await run(f,'recovery',{destination:path.join(f.release,'nested')})).status,'failed');await assert.rejects(access(path.join(f.release,'nested')));
 assert.deepEqual(await readFile(path.join(f.release,'RELEASE.json')),manifest);assert.deepEqual(await readFile(path.join(f.to,'project.json')),original);
});

test('a moved previous-source case does not block independent backup recovery or permit overwriting its old location',async t=>{
 const f=await fixture(t);await f.workflow.addCase(f.service.editor,{...params(await latest(f)),name:'Previous save',file:f.save,sourceProject:f.from});await rename(f.from,f.from+'-moved');
 assert.equal((await run(f,'recovery',{destination:path.join(f.root,'independent-recovery')})).status,'completed');const blocked=await run(f,'recovery',{destination:path.join(f.from,'nested')});assert.equal(blocked.status,'failed');assert.match(blocked.message,/RELEASE_RECOVERY_LOCATION/);await assert.rejects(access(f.from));
});

test('cancellation retires its owned worker and records no successful acceptance; legacy wrong-game manifests fail closed',async t=>{
 const f=await fixture(t),{workflow:w,service:s}=f;let job=await w.start(s.editor,{...params(await latest(f)),kind:'release'}),child=w.workers.get(job.id);job=await w.cancel(s.editor,{jobId:job.id});assert.equal(job.status,'cancelled');assert.ok(child.exitCode!==null||child.signalCode!==null);assert.equal((await latest(f)).checks.release.status,'cancelled');
 const file=path.join(f.release,'RELEASE.json'),m=JSON.parse(await readFile(file,'utf8'));m.gameId='another-game';await writeFile(file,JSON.stringify(m));await assert.rejects(w.register(s.editor,{directory:f.release}),/RELEASE_GAME/);
 delete m.gameId;delete m.engineIdentity;await writeFile(file,JSON.stringify(m));const legacy=await w.register(s.editor,{directory:f.release});assert.equal(legacy.identity.engineIdentity.source.commit,null);assert.equal(legacy.checks.release.status,'pending');
 const changed=JSON.parse(await readFile(path.join(f.release,'game.json'),'utf8'));changed.content.project.gameId='different-game';await writeFile(path.join(f.release,'game.json'),JSON.stringify(changed));const invalid=await done(f,await w.start(s.editor,{...params(legacy),kind:'release'}));assert.notEqual(invalid.status,'completed');
});

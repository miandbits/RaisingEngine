import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,mkdir,readFile,writeFile,readdir,symlink,chmod} from 'node:fs/promises';
import {existsSync} from 'node:fs';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
import yauzl from 'yauzl';
import {get} from 'node:http';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {encodeRgbaPng} from '../../dist/packages/studio-service/png.js';
import {RuleDebugSession} from '../../dist/packages/rule-debug/index.js';
import {firstGameRoute} from './first-game-route.mjs';
import {digest,projectFiles} from '../../apps/qt-studio/project-files.mjs';
import {writeZip,treeFiles} from '../../apps/qt-studio/release-zip.mjs';
const call=(s,method,params={})=>s.handle({method,params:{revision:s.editor.revision,...params}});
async function fixture() {
  await mkdir('.cache/creator-tests',{recursive:true});const work=await mkdtemp(path.resolve('.cache/creator-tests/run-'));
  const runtime=path.join(work,'runtime');await mkdir(runtime);await writeFile(path.join(runtime,'index.html'),'<h1>Game</h1>');
  await writeFile(path.join(runtime,'capabilities.json'),JSON.stringify({formatVersion:'1',imageExtensions:['.png'],fontExtensions:['.ttf'],audioExtensions:[],visualFeatures:['narrative-actor-binding-v1','component-context-v1','save-panel-v1','components-v1']}));
  const service=new QtStudioService({runtimes:{web:runtime}});await call(service,'create',{root:path.join(work,'game'),template:'my-first-raising-game'});return {service,work,runtime};
}
async function rename(s,title) {const data=structuredClone(s.editor.documents.find(d=>d.kind==='project').data);data.title=title;return call(s,'edit',{edit:{type:'replace',file:'project.json',data}});}
async function unzip(file) {
  return new Promise((resolve,reject)=>yauzl.open(file,{lazyEntries:true},(error,zip)=>{
    if(error)return reject(error);const files=new Map();zip.on('error',reject);zip.on('end',()=>resolve(files));
    zip.on('entry',entry=>zip.openReadStream(entry,(error,stream)=>{if(error)return reject(error);const chunks=[];stream.on('error',reject);stream.on('data',b=>chunks.push(b));stream.on('end',()=>{files.set(entry.fileName,{bytes:Buffer.concat(chunks),mode:entry.externalFileAttributes>>>16});zip.readEntry();});}));zip.readEntry();
  }));
}
test('recovery survives lost session/import cleanup, preserves original and checklist, rejects stale/corrupt copies',async()=>{
  const {service:s,work}=await fixture(),root=s.editor.root;
  await rename(s,'Recovered garden');
  const png=Buffer.from(encodeRgbaPng({width:1,height:1,pixels:Buffer.from([1,2,3,255])})),file=path.join(work,'portrait.png');await writeFile(file,png);
  await call(s,'importImage',{file});const asset=s.editor.documents.find(d=>d.kind==='assets').data.assets[0];
  const assets=structuredClone(s.editor.documents.find(d=>d.kind==='assets').data);assets.assets.push({...asset,id:'shared-portrait'});await call(s,'edit',{edit:{type:'replace',file:s.editor.documents.find(d=>d.kind==='assets').file,data:assets}});
  const [recovery]=await s.recovery.list(root);assert.ok(recovery.revision);await s.editor.close();assert.equal(existsSync(path.join(root,asset.path)),false);
  const reopened=new QtStudioService();await call(reopened,'open',{root});
  await assert.rejects(call(reopened,'recoveryRestore',{...recovery,revision:reopened.editor.revision,recoveryRevision:'stale',destination:path.join(work,'stale')}));
  // Recovery revision is separate from the live editor's revision.
  const params={id:recovery.id,recoveryRevision:recovery.revision,destination:path.join(work,'restored')};
  await assert.rejects(call(reopened,'recoveryRestore',{...params,destination:path.join(root,'nested')}),/outside/);
  const result=await call(reopened,'recoveryRestore',params);const restored=result.restoredProject;
  assert.equal(JSON.parse(await readFile(path.join(root,'project.json'))).title,'My first raising game');
  assert.equal(JSON.parse(await readFile(path.join(restored,'project.json'))).title,'Recovered garden');
  assert.deepEqual(await readFile(path.join(restored,asset.path)),png);
  const r=await call(reopened,'open',{root:restored});assert.ok(r.firstGame.rows.find(r=>r.id==='name').complete);
  const journal=path.join(root,'.studio/recovery',recovery.id),value=JSON.parse(await readFile(path.join(journal,'draft.json')));
  await writeFile(path.join(journal,value.files[0].sha256),'corrupt');
  await assert.rejects(reopened.recovery.restore(root,{...recovery,destination:path.join(work,'bad')}),/changed|checksum/);
  assert.equal(existsSync(path.join(work,'bad')),false);
});
test('saving/discarding only removes the current session recovery; symlinks never become recovery storage',async()=>{
  const {service:s,work}=await fixture(),root=s.editor.root;await rename(s,'Draft one');
  const peer=new QtStudioService();await call(peer,'open',{root});await rename(peer,'Draft two');await call(peer,'save');
  assert.equal((await s.recovery.list(root)).length,1);assert.equal((await s.recovery.list(root))[0].id,s.recovery.id);
  await call(s,'discardDraftRecovery');await call(s,'view');assert.equal((await s.recovery.list(root)).length,0);
  await rename(s,'Draft three');assert.equal((await s.recovery.list(root)).length,1);
  const external=path.join(work,'external'),fake=path.join(work,'linked');await mkdir(external);await mkdir(fake);await symlink(external,path.join(fake,'.studio'));
  await assert.rejects(s.recovery.folder(fake),/symbolic/);assert.deepEqual(await readdir(external),[]);
});
test('versioned release preserves exact draft and player bytes, rejects invalid versions, launches loopback preview',async t=>{
  const {service:s,work}=await fixture();t.after(()=>s.releases.close());await rename(s,'Unsaved release');
  let r=await call(s,'releaseTarget',{target:'web'});assert.equal(r.releaseCompatibility.target,'web');assert.equal(r.releaseCompatibility.rows.find(r=>r.target==='web').compatible,true);assert.equal(r.releaseCompatibility.rows.find(r=>r.target==='macos').available,false);
  await assert.rejects(call(s,'export',{target:'web',releaseVersion:'invalid',destination:path.join(work,'failed')}),/release version/);assert.equal(existsSync(path.join(work,'failed')),false);
  assert.ok(!(await readdir(work)).some(f=>f.startsWith('.raising-export-')));
  r=await call(s,'export',{target:'web',releaseVersion:'0.2.0',destination:path.join(work,'release')});const exported=r.exported;
  const manifest=JSON.parse(await readFile(path.join(exported.directory,'RELEASE.json')));assert.equal(manifest.version,'0.2.0');assert.ok(Object.keys(manifest.dependencies).length);
  for(const archive of Object.values(manifest.archives))assert.equal(digest(await readFile(path.join(exported.directory,archive.file))),archive.sha256);
  const game=await unzip(exported.gameArchive),project=await unzip(exported.projectArchive);
  for(const f of manifest.files)assert.equal(digest(game.get(f.file).bytes),f.sha256);
  for(const f of manifest.sourceFiles)assert.equal(digest(project.get(f.file).bytes),f.sha256);
  assert.equal(JSON.parse(project.get('project.json').bytes).title,'Unsaved release');assert.equal(game.has('project.json'),false);assert.ok(project.has('RELEASE.json'));
  const launch=(await call(s,'releaseLaunch',{token:exported.launchToken})).launch;assert.match(launch.url,/^http:\/\/127\.0\.0\.1:/);assert.match(await (await fetch(launch.url)).text(),/Game/);
  for(const name of ['Project-backup.zip','RELEASE.json','missing','%2e%2e/project.json'])assert.equal((await fetch(launch.url+name)).status,404);
  assert.equal((await fetch(launch.url,{method:'POST'})).status,405);assert.equal(await new Promise((resolve,reject)=>get(launch.url,{headers:{Host:'example.com'}},res=>{res.resume();resolve(res.statusCode);}).on('error',reject)),404);
  assert.equal((await call(s,'releaseLaunch',{token:exported.launchToken})).launch.url,launch.url);
});
test('ZIPs preserve executable mode and Unicode, pass independent CRC verification, and refuse symlinks',async()=>{
  const {work}=await fixture(),root=path.join(work,'zip');await mkdir(root);await writeFile(path.join(root,'play'),'#!/bin/sh\nexit 0\n');await chmod(path.join(root,'play'),0o755);await writeFile(path.join(root,'정원.txt'),'Garden 🌿');
  const zip=path.join(work,'proof.zip');await writeZip(root,await treeFiles(root),zip);const entries=await unzip(zip);assert.equal(entries.get('정원.txt').bytes.toString(),'Garden 🌿');if(process.platform!=='win32'){assert.equal(entries.get('play').mode&0o111,0o111);execFileSync('/usr/bin/unzip',['-t',zip]);}
  await symlink(path.join(root,'play'),path.join(root,'link'));await assert.rejects(treeFiles(root),/symbolic|regular|Unsupported/i);
});
test('ending explanation matches both actual completed routes, names unmet values and preserves debug state',async()=>{
  const {service:s}=await fixture(),content=(await s.editor.snapshot(s.editor.revision)).snapshot,debug=new RuleDebugSession(content,1);
  const before=debug.view().state,initial=debug.inspect({kind:'ending',id:'together'}).inspection;
  assert.equal(initial.eligible,false);assert.equal(initial.selectedEndingId,'own-path');assert.match(JSON.stringify(initial.condition),/Skill is 0; requires at least 30/);
  for(const accept of [true,false]){const route=firstGameRoute(content,accept);const result=debug.inspectSnapshot(route.expected.ending,{kind:'ending',id:route.ending});assert.equal(result.eligible,true);assert.equal(result.selectedEndingId,route.ending);}
  assert.deepEqual(debug.view().state,before);
  const altered=structuredClone(content);const duplicate=structuredClone(altered.endings.find(e=>e.id==='together'));duplicate.id='aaa';altered.endings.push(duplicate);
  const tie=new RuleDebugSession(altered,1).inspectSnapshot(firstGameRoute(content,true).expected.ending,{kind:'ending',id:'together'});assert.equal(tie.eligible,false);assert.equal(tie.selectedEndingId,'aaa');assert.match(tie.reason,/selected first/);
});

test('project backup contains the exact pinned plugin store and refuses modified dependency files',async()=>{
  const {service:s,work}=await fixture();const {makePackage}=await import('../plugins/host-helpers.mjs');const {installPackage}=await import('../../dist/packages/plugin-host/index.js');
  const store=path.join(work,'plugin-store'),archive=await makePackage(work);const {plugin}=await installPackage(store,archive);
  await call(s,'pluginStore',{directory:store});
  await call(s,'pluginEnable',{id:plugin.id,version:plugin.version});await call(s,'pluginDisable',{id:plugin.id});
  const content=(await s.editor.snapshot(s.editor.revision)).snapshot;assert.ok(content);
  const stage=path.join(work,'stage');await mkdir(stage);await writeFile(path.join(stage,'index.html'),'game');
  const manifest=await s.releases.prepare(stage,{editor:s.editor,target:'web',version:'1.0.0',content,contentFingerprint:content.contentFingerprint,engineRoot:path.resolve('.')});
  const backup=await unzip(path.join(stage,'Project-backup.zip'));const prefix='release-dependencies/plugin-store/installed/test-plugin/1.0.0/';
  assert.equal(digest(backup.get(prefix+'schema/thing.schema.json').bytes),plugin.manifest.files['schema/thing.schema.json']);assert.equal(manifest.pluginDependencies[0].packageSha256,plugin.sha256);
  await writeFile(path.join(plugin.directory,'schema/thing.schema.json'),'changed');const failed=path.join(work,'failed-stage');await mkdir(failed);
  await assert.rejects(s.releases.prepare(failed,{editor:s.editor,target:'web',version:'1.0.1',content,contentFingerprint:content.contentFingerprint,engineRoot:path.resolve('.')}),/pinned plugin file changed/);
});


test('project snapshots retain named routes and approved baseline bytes',async()=>{
  const {service:s}=await fixture();
  for(const group of ['routes','baselines']) {
    const source=path.resolve('examples/starlight-three-months/playtests',group),name=(await readdir(source)).find(n=>n.endsWith('.json'));
    const bytes=await readFile(path.join(source,name));await mkdir(path.join(s.editor.root,'playtests',group),{recursive:true});await writeFile(path.join(s.editor.root,'playtests',group,name),bytes);
    assert.deepEqual((await projectFiles(s.editor)).get('playtests/'+group+'/'+name),bytes);
  }
});

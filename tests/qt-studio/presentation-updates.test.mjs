import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,mkdir,readFile,writeFile,access} from 'node:fs/promises';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {updateFixture} from './update-compatibility-fixture.mjs';
import {loadProject,buildSnapshot} from '../../dist/packages/project-service/index.js';
import {decodeSave,encodeSave} from '../../dist/packages/runtime-service/save.js';
import {dispatchSession} from '../../dist/packages/runtime-service/session.js';
import {verifyProjectBackup} from '../../apps/qt-studio/backup-restore.mjs';
import {nativePlayerCapabilities} from '../../scripts/lib/native-player-capabilities.mjs';
const call=(s,method,params={})=>s.handle({method,params:{revision:s.editor.revision,...params}});
async function fixture(t){
  await mkdir('.cache/presentation-updates',{recursive:true});const root=await mkdtemp(path.resolve('.cache/presentation-updates/run-')),f=await updateFixture(root);
  const base=(await buildSnapshot(await loadProject(f.from))).snapshot,project=JSON.parse(await readFile(path.join(f.from,'project.json'),'utf8'));
  const button=project.visual.screens[0].elements.find(e=>e.kind==='button');button.text='A new beginning';button.fill='#223344';button.x+=4;
  await writeFile(path.join(f.to,'project.json'),JSON.stringify(project));
  const previousBundle=path.join(root,'game.json');await writeFile(previousBundle,JSON.stringify({formatVersion:'1',content:base,assets:{}}));
  const runtime=path.join(root,'runtime');await mkdir(runtime);await writeFile(path.join(runtime,'index.html'),'test runtime');
  await writeFile(path.join(runtime,'capabilities.json'),JSON.stringify(nativePlayerCapabilities('darwin')));
  const s=new QtStudioService({runtimes:{web:runtime}});t.after(async()=>{await s.releases.close();await s.editor.close();});await call(s,'open',{root:f.to});
  return {...f,root,base,previousBundle,runtime,s,check:()=>call(s,'presentationUpdateCheck',{target:'web',previousBundle,sampleSave:f.save})};
}
test('explicit reviewed update preserves full canonical save content, displays a separate overlay and restores its editable backup',async t=>{
  const f=await fixture(t),before=f.s.editor.view(),original=await readFile(f.save),report=(await f.check()).presentationUpdate;
  assert.equal(report.classification,'presentation-only');assert.equal(report.sample.valid,true);assert.equal(report.sample.commands,1);
  assert.notEqual(report.baselineFingerprint,report.sourceFingerprint);assert.ok(report.changedPaths.some(p=>p.endsWith('/text')));
  assert.deepEqual(f.s.editor.view(),before);assert.deepEqual(await readFile(f.save),original);
  const dest=path.join(f.root,'safe-update'),exported=(await call(f.s,'export',{target:'web',destination:dest,releaseVersion:'0.2.0',presentationCheck:report.checkId})).exported;
  const bundle=JSON.parse(await readFile(path.join(dest,'game.json'),'utf8'));
  assert.equal(exported.contentFingerprint,f.base.contentFingerprint);assert.deepEqual(bundle.content,f.base);
  assert.equal(bundle.presentationRevision.policy,'paint-layout-v1');assert.notDeepEqual(bundle.presentationRevision.visual,f.base.project.visual);
  const loaded=await decodeSave(bundle.content,original.toString());assert.equal(loaded.ok,true);
  // Continue the original history, save again, and validate in an independent load.
  const next=dispatchSession(bundle.content,loaded.session,{type:'executeActivity'},loaded.session.state.stateRevision);assert.equal(next.ok,true,JSON.stringify(next));
  const saved=await encodeSave(bundle.content,next.session);assert.equal(saved.ok,true);
  const restored=await decodeSave(f.base,saved.text);assert.equal(restored.ok,true);assert.deepEqual(restored.session,next.session);
  const receipt=JSON.parse(await readFile(path.join(dest,'RELEASE.json'),'utf8'));
  assert.equal(receipt.contentFingerprint,report.baselineFingerprint);assert.equal(receipt.sourceContentFingerprint,report.sourceFingerprint);
  const backup=await verifyProjectBackup(path.join(dest,'Project-backup.zip'));assert.equal(backup.ok,true);assert.equal(backup.contentFingerprint,report.sourceFingerprint);
});
test('ordinary export still uses the edited snapshot and does not imply compatibility',async t=>{
  const f=await fixture(t),report=(await f.check()).presentationUpdate,dest=path.join(f.root,'ordinary');
  const exported=(await call(f.s,'export',{target:'web',destination:dest})).exported;
  assert.equal(exported.contentFingerprint,report.sourceFingerprint);
  const bundle=JSON.parse(await readFile(path.join(dest,'game.json'),'utf8'));assert.equal(bundle.presentationRevision,undefined);
  assert.equal((await decodeSave(bundle.content,await readFile(f.save,'utf8'))).ok,false);
});
test('failed checks, altered sample or baseline, different target and changed rules cannot reuse an approval',async t=>{
  const f=await fixture(t);let report=(await f.check()).presentationUpdate;
  await assert.rejects(call(f.s,'export',{target:'macos',destination:path.join(f.root,'wrong-target'),presentationCheck:report.checkId}),/PRESENTATION_CHECK_STALE/);
  await writeFile(f.previousBundle,(await readFile(f.previousBundle,'utf8'))+'\n');
  await assert.rejects(call(f.s,'export',{target:'web',destination:path.join(f.root,'stale'),presentationCheck:report.checkId}),/PRESENTATION_CHECK_STALE/);await assert.rejects(access(path.join(f.root,'stale')));
  report=(await f.check()).presentationUpdate;await writeFile(f.save,'{"corrupt":true}');await assert.rejects(f.check(),/PRESENTATION_SAVE_INVALID/);
  await assert.rejects(call(f.s,'export',{target:'web',destination:path.join(f.root,'old-check'),presentationCheck:report.checkId}),/PRESENTATION_SAVE_INVALID/);
  const project=structuredClone(f.s.editor.documents.find(d=>d.kind==='project').data);project.durationMonths+=1;
  await call(f.s,'edit',{edit:{type:'replace',file:'project.json',data:project}});
  await assert.rejects(call(f.s,'presentationUpdateCheck',{target:'web',previousBundle:f.previousBundle}),/PRESENTATION_SAVE_FIRST/);
  await call(f.s,'save');await assert.rejects(call(f.s,'presentationUpdateCheck',{target:'web',previousBundle:f.previousBundle}),/PRESENTATION_CONTENT_CHANGED|PRESENTATION_PROJECT_INVALID/);
});
test('source is rechecked after packaging and obsolete runtimes reject visual overlays',async t=>{
  const f=await fixture(t);let report=(await f.check()).presentationUpdate;
  const caps=JSON.parse(await readFile(path.join(f.runtime,'capabilities.json'),'utf8'));caps.visualFeatures=caps.visualFeatures.filter(v=>v!=='presentation-visual-updates-v1');await writeFile(path.join(f.runtime,'capabilities.json'),JSON.stringify(caps));
  await assert.rejects(call(f.s,'export',{target:'web',destination:path.join(f.root,'old-runtime'),presentationCheck:report.checkId}),/presentation-visual-updates-v1/);
  await writeFile(path.join(f.runtime,'capabilities.json'),JSON.stringify(nativePlayerCapabilities('darwin')));
  const prepare=f.s.releases.prepare.bind(f.s.releases);f.s.releases.prepare=async(...args)=>{const result=await prepare(...args);await writeFile(f.previousBundle,(await readFile(f.previousBundle,'utf8'))+'\n');return result;};
  await assert.rejects(call(f.s,'export',{target:'web',destination:path.join(f.root,'race'),releaseVersion:'0.2.0',presentationCheck:report.checkId}),/PRESENTATION_CHECK_STALE/);await assert.rejects(access(path.join(f.root,'race')));
});

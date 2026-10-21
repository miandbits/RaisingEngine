import test from 'node:test';
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFile, writeFile, mkdir, cp, access } from 'node:fs/promises';
import path from 'node:path';
import { fixture } from '../gameplay/fixture.mjs';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot, canonicalJson } from '../../dist/packages/project-service/snapshot.js';
import { exportGame } from '../../dist/packages/export-service/export.js';
import { preparePresentationRevision } from '../../dist/packages/export-service/presentation-revision.js';

const sha = bytes => createHash('sha256').update(bytes).digest('hex');
function resign(snapshot) {
  const {contentFingerprint, ...data} = snapshot;
  return {...data, contentFingerprint:`sha256:${sha(canonicalJson(data))}`};
}
async function setup(t) {
  const f = await fixture(t, undefined, async root => {
    const file = path.join(root,'assets/manifest.json');
    const manifest = JSON.parse(await readFile(file,'utf8'));
    await cp('assets/visual-sample/courtyard.png', path.join(root,'assets/revision.png'));
    manifest.assets.push({id:'revision-image',kind:'image',path:'assets/revision.png'});
    await writeFile(file,JSON.stringify(manifest));
  });
  assert.ok(f.built.ok,JSON.stringify(f.built.diagnostics));
  const base = {formatVersion:'1',content:f.built.snapshot,assets:{}};
  const file = path.join(f.root,'assets/revision.png');
  const bytes = Buffer.concat([await readFile(file),Buffer.from('presentation revision')]);
  await writeFile(file,bytes);
  const updated = await buildSnapshot(await loadProject(f.root)); assert.ok(updated.ok);
  return {...f,base,content:updated.snapshot,file,bytes};
}

test('image revision preserves the entire save baseline and records current pixel checksums',async t=>{
  const f=await setup(t), original=JSON.stringify(f.base);
  const result=preparePresentationRevision(f.content,f.base);
  assert.deepEqual(result.content,f.base.content);
  assert.notEqual(f.content.contentFingerprint,result.content.contentFingerprint);
  assert.equal(result.presentationRevision.sourceContentFingerprint,f.content.contentFingerprint);
  assert.deepEqual(Object.keys(result.presentationRevision.images),['revision-image']);
  assert.equal(result.presentationRevision.images['revision-image'].sha256,sha(f.bytes));
  assert.equal(JSON.stringify(f.base),original);
  // A second visual revision still keeps the original replay identity.
  assert.deepEqual(preparePresentationRevision(f.content,{...f.base,...result}),result);
});

test('rules, layouts, dialogue, asset definitions, non-image hashes and plugins cannot use image-only compatibility',async t=>{
  const f=await setup(t);
  for(const mutate of [
    s=>s.project.title+=' changed',
    s=>s.project.visual.screens[0].label+=' changed',
    s=>s.events[0].nodes[0].text='changed dialogue',
    s=>s.assets.find(a=>a.id==='revision-image').path='assets/other.png',
  ]) {
    const changed=structuredClone(f.content);mutate(changed);
    assert.throws(()=>preparePresentationRevision(resign(changed),f.base),/PRESENTATION_CONTENT_CHANGED/);
  }
  const old=structuredClone(f.base.content), next=structuredClone(f.content);
  for(const s of [old,next]) s.assets.find(a=>a.id==='revision-image').kind='audio';
  assert.throws(()=>preparePresentationRevision(resign(next),{...f.base,content:resign(old)}),/PRESENTATION_ASSET_CHANGED/);
  assert.throws(()=>preparePresentationRevision(f.content,{...f.base,extensions:{plugins:[]}}),/PRESENTATION_PLUGINS_CHANGED/);
  const invalid=structuredClone(f.base);invalid.content.project.title='tampered';
  assert.throws(()=>preparePresentationRevision(f.content,invalid),/PRESENTATION_BASE_INVALID/);
  const bad=structuredClone(f.content);bad.assetHashes['unknown']='0'.repeat(64);
  assert.throws(()=>preparePresentationRevision(resign(bad),f.base),/PRESENTATION_BASE_INVALID/);
});

test('Windows and web export carry new image bytes, unchanged save snapshot, and an explicit art audit',async t=>{
  const f=await setup(t), runtime=path.join(f.root,'fake-runtime');await mkdir(runtime);
  await writeFile(path.join(runtime,'index.html'),'fixture runtime');await writeFile(path.join(runtime,'RaisingPlayer.exe'),'fixture runtime');
  // This test checks packaging; actual native play is checked separately with the real runtime.
  await writeFile(path.join(runtime,'capabilities.json'),JSON.stringify({formatVersion:'1',imageExtensions:['.png'],audioExtensions:['.ogg','.wav'],visualFeatures:['generic-rules-v1']}));
  for(const target of ['windows','web']) {
    const destination=path.join(f.root,'out-'+target);
    const exported=await exportGame({content:f.content,presentationBase:f.base,projectRoot:f.root,destination,target,runtimes:{web:runtime,windows:runtime}});
    const bundle=JSON.parse(await readFile(path.join(destination,'game.json'),'utf8'));
    assert.deepEqual(bundle.content,f.base.content);assert.equal(exported.contentFingerprint,f.base.content.contentFingerprint);
    assert.deepEqual(await readFile(path.join(destination,bundle.assets['revision-image'])),f.bytes);
    assert.equal(bundle.presentationRevision.images['revision-image'].sha256,sha(f.bytes));
    const meta=JSON.parse(await readFile(path.join(destination,'export.json'),'utf8'));
    assert.deepEqual(meta.presentationRevision,bundle.presentationRevision);
  }
  await writeFile(f.file,'modified after validation');
  const destination=path.join(f.root,'stale');
  await assert.rejects(()=>exportGame({content:f.content,presentationBase:f.base,projectRoot:f.root,destination,target:'windows',runtimes:{windows:runtime,web:runtime}}),/revision-image/);
  await assert.rejects(()=>access(destination),{code:'ENOENT'});
});

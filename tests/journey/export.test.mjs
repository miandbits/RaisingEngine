import test from 'node:test';
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { mkdir, mkdtemp, readFile, writeFile, readdir, access, symlink, cp } from 'node:fs/promises';
import { resolve, dirname } from 'node:path';
import { fixture as gameplayFixture } from '../gameplay/fixture.mjs';
import {requiredPlayerFeatures,missingPlayerFeatures} from '../../dist/packages/export-service/compatibility.js';
import { exportGame } from '../../dist/packages/export-service/export.js';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';

async function fixture(t) {
  const original = await gameplayFixture(t);
  const manifestPath = resolve(original.root, 'assets/manifest.json');
  const manifest = JSON.parse(await readFile(manifestPath, 'utf8'));
  await cp(resolve('assets/visual-sample/courtyard.png'), resolve(original.root, 'assets/courtyard.png'));
  manifest.assets.push({ id: 'visual-courtyard', kind: 'image', path: 'assets/courtyard.png' });
  await writeFile(manifestPath, JSON.stringify(manifest));
  const built = await buildSnapshot(await loadProject(original.root));
  assert.equal(built.ok, true, JSON.stringify(built.diagnostics));
  return { root: original.root, built };
}

async function output() {
  const root = resolve('.cache/journey-export-tests');
  await mkdir(root, { recursive: true });
  return mkdtemp(`${root}/run-`);
}
const runtimes = () => ({ web: resolve('dist/web'), windows: resolve('dist/native-player-app') });
async function absent(path) { await assert.rejects(() => access(path), { code: 'ENOENT' }); }
async function noStaging(path) { assert.equal((await readdir(path)).some(name => name.startsWith('.raising-export-')), false); }

test('web export contains actual runtime and byte-identical validated assets and content', async t => {
  const { root, built } = await fixture(t); assert.equal(built.ok, true);
  assert.ok(built.snapshot.assets.length > 0);
  const workspace = await output(), destination = resolve(workspace, 'web');
  const result = await exportGame({ content: built.snapshot, projectRoot: root, destination, target: 'web', runtimes: runtimes() });
  assert.equal(result.directory, destination); assert.equal(result.assetCount, built.snapshot.assets.length);
  const bundle = JSON.parse(await readFile(resolve(destination, 'game.json'), 'utf8'));
  assert.equal(bundle.formatVersion, '1'); assert.deepEqual(bundle.content, built.snapshot);
  await access(resolve(destination, 'index.html'));
  for (const asset of built.snapshot.assets) {
    assert.match(bundle.assets[asset.id], /^assets\/[a-z][a-z0-9-]*\.[a-z0-9]+$/);
    const bytes = await readFile(resolve(destination, bundle.assets[asset.id]));
    assert.equal(createHash('sha256').update(bytes).digest('hex'), built.snapshot.assetHashes[asset.id]);
  }
  assert.equal(JSON.parse(await readFile(resolve(destination, 'export.json'), 'utf8')).target, 'web');
  await noStaging(workspace);
});

test('stale asset bytes abort export atomically without leaving target or staging files', async t => {
  const { root, built } = await fixture(t); assert.equal(built.ok, true);
  const asset = built.snapshot.assets[0];
  await writeFile(resolve(root, asset.path), 'changed after validation');
  const workspace = await output(), destination = resolve(workspace, 'web');
  await assert.rejects(() => exportGame({ content: built.snapshot, projectRoot: root, destination, target: 'web', runtimes: runtimes() }), /에셋이 변경/);
  await absent(destination); await noStaging(workspace);
});

test('existing destinations and project ancestors are never replaced', async t => {
  const { root, built } = await fixture(t); assert.equal(built.ok, true);
  const workspace = await output(), destination = resolve(workspace, 'existing');
  await mkdir(destination); await writeFile(resolve(destination, 'keep.txt'), 'preserve me');
  await assert.rejects(() => exportGame({ content: built.snapshot, projectRoot: root, destination, target: 'web', runtimes: runtimes() }), /이미 있습니다/);
  assert.equal(await readFile(resolve(destination, 'keep.txt'), 'utf8'), 'preserve me');
  await assert.rejects(() => exportGame({ content: built.snapshot, projectRoot: root, destination: dirname(root), target: 'web', runtimes: runtimes() }), /상위 폴더/);
  await noStaging(workspace);
});

test('export independently blocks forged asset escapes and runtime symlinks', async t => {
  const { root, built } = await fixture(t); assert.equal(built.ok, true);
  const workspace = await output(), destination = resolve(workspace, 'escaped');
  const outside = resolve(workspace, 'outside.png'); await cp(resolve('assets/visual-sample/courtyard.png'), outside);
  const forged = structuredClone(built.snapshot); forged.assets[0].path = outside;
  await assert.rejects(() => exportGame({ content: forged, projectRoot: root, destination, target: 'web', runtimes: runtimes() }), /프로젝트 밖/);
  await absent(destination); await noStaging(workspace);
  const runtime = resolve(workspace, 'runtime'), linked = resolve(workspace, 'linked');
  await mkdir(runtime); await mkdir(linked); await writeFile(resolve(runtime, 'index.html'), '<!doctype html>');
  await cp(resolve('dist/web/capabilities.json'), resolve(runtime, 'capabilities.json'));
  await writeFile(resolve(linked, 'private.txt'), 'must not copy');
  await symlink(linked, resolve(runtime, 'unsafe'), 'junction');
  await assert.rejects(() => exportGame({ content: built.snapshot, projectRoot: root, destination, target: 'web', runtimes: { ...runtimes(), web: runtime } }), /심볼릭 링크/);
  await absent(destination); await noStaging(workspace);
});

test('UI feature capabilities reject old runtimes before packaging, while legacy projects still export',async t=>{
  const {root,built}=await fixture(t),workspace=await output();
  const capabilities={formatVersion:'1',imageExtensions:['.png','.jpg','.jpeg','.webp'],audioExtensions:['.wav','.mp3','.ogg'],fontExtensions:['.ttf','.otf']};
  for(const target of ['web','windows']){
    // Minimal package fixture tests the gate without claiming this stub is runnable.
    const runtime=resolve(workspace,`${target}-runtime`);await mkdir(runtime);
    await writeFile(resolve(runtime,target==='web'?'index.html':'RaisingPlayer.exe'),'capability-test-fixture');
    await writeFile(resolve(runtime,'capabilities.json'),JSON.stringify(capabilities));
    const options={projectRoot:root,target,runtimes:{...runtimes(),[target]:runtime}};
    await exportGame({...options,content:built.snapshot,destination:resolve(workspace,`${target}-legacy`)});
    const variations=[
      ['components-v1',visual=>{visual.components=[{id:'card',label:'Card',width:100,height:40,elements:[]}];}],
      ['anchors-v1',visual=>{visual.screens[0].elements[0].anchorX='right';}],
      ['responsive-v1',visual=>{visual.viewportMode='responsive';}],
      ['readable-layout-v1',visual=>{visual.readableLayout={};visual.screens[0].readableLayout={items:[]};}],
      ['readable-actions-v1',visual=>{visual.readableLayout={};const s=visual.screens[0];s.readableLayout??={items:[]};s.elements.push({id:'mobile-next',kind:'button',text:'Continue',action:'advance',x:0,y:0,width:100,height:44});Object.assign(s.readableLayout,{actionLayout:'grouped',primaryAction:'mobile-next',actions:['mobile-next']});}],
      ['compact-planning-v1',visual=>{visual.readableLayout={};const s=visual.screens[0];s.readableLayout??={items:[]};s.elements.push({id:'mobile-planner',kind:'schedule',x:0,y:0,width:300,height:400});s.readableLayout.items.push({elementId:'mobile-planner',schedulePresentation:'compact'});}],
      ['planning-guidance-v1',visual=>{visual.readableLayout={};const s=visual.screens[0];s.readableLayout??={items:[]};s.elements.push({id:'guided-planner',kind:'schedule',x:0,y:0,width:300,height:400});s.readableLayout.items.push({elementId:'guided-planner',schedulePresentation:'compact',planningGuidance:'first-use'});}]
    ];
    for(const [feature,change] of variations){
      const content=structuredClone(built.snapshot);change(content.project.visual);
      const destination=resolve(workspace,`${target}-${feature}-old`);
      await assert.rejects(()=>exportGame({...options,content,destination}),new RegExp(feature));
      await absent(destination);await noStaging(workspace);
    }
    const content=structuredClone(built.snapshot);for(const [,change] of variations)change(content.project.visual);
    await writeFile(resolve(runtime,'capabilities.json'),JSON.stringify({...capabilities,visualFeatures:'components-v1 anchors-v1 responsive-v1'}));
    await assert.rejects(()=>exportGame({...options,content,destination:resolve(workspace,`${target}-malformed`)}),/components-v1/);
    await writeFile(resolve(runtime,'capabilities.json'),JSON.stringify({...capabilities,visualFeatures:variations.map(([feature])=>feature)}));
    const destination=resolve(workspace,`${target}-supported`);await exportGame({...options,content,destination});
    assert.deepEqual(JSON.parse(await readFile(resolve(destination,'game.json'),'utf8')).content.project.visual,content.project.visual);
  }
});


test('authored commit action exports independently of board widgets and honors older native capability',async t=>{
 const {built}=await fixture(t),content=structuredClone(built.snapshot);
 content.project.visual={width:1280,height:720,screens:[{id:'schedule',label:'Schedule',elements:[{id:'commit',kind:'button',action:'commitPlan',text:'Begin',x:0,y:0,width:100,height:44}]}]};
 const required=requiredPlayerFeatures(content);assert.ok(required.includes('commit-plan-action-v1'));assert.ok(!required.includes('schedule-board-v1'));
 assert.deepEqual(missingPlayerFeatures(['commit-plan-action-v1'],['schedule-board-v1']),[]);
 assert.deepEqual(missingPlayerFeatures(['commit-plan-action-v1'],['commit-plan-action-v1']),[]);
 assert.deepEqual(missingPlayerFeatures(['schedule-board-v1'],['commit-plan-action-v1']),['schedule-board-v1']);
 content.project.visual.screens[0].elements.push({id:'board',kind:'schedule',scheduleLayout:'board',x:0,y:0,width:300,height:400});
 assert.ok(requiredPlayerFeatures(content).includes('schedule-board-v1'),'Board widget still needs the broad capability');
});

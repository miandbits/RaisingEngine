import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir,mkdtemp} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {exportBundle} from '../../dist/packages/plugin-host/bundle.js';
import {galleryBranchRows} from '../../dist/packages/visual/gallery-branches.js';
import {route} from './recollection-fixture.mjs';

test('Qt-authored branch cards unlock across runs, select actual recorded paths and preserve save/replay isolation',{timeout:240000},async()=>{
  const evidence=resolve('.cache/gallery-branches');await mkdir(evidence,{recursive:true});
  const project=(await readFile(resolve(evidence,'editor-project.txt'),'utf8')).trim();
  const loaded=await loadProject(project),built=await buildSnapshot(loaded);assert.equal(built.ok,true,JSON.stringify(built.diagnostics));const content=built.snapshot;
  assert.ok(content.project.gallery.branchScreenId);
  const runtime=resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-gallery-branches');
  const caps=JSON.parse(await readFile(resolve(runtime,'capabilities.json'),'utf8'));
  const extensions=await exportBundle({lock:loaded.documents.find(d=>d.kind==='pluginLock')?.data,documents:loaded.documents,visual:content.project.visual,target:'windows',assets:{root:project,assets:content.assets,hashes:content.assetHashes},hostFeatures:caps.visualFeatures});
  const root=await mkdtemp(resolve('.cache/gallery-branches-play-')),game=resolve(root,'game'),save=resolve(root,'slot.json'),profile=resolve(root,`collection-${content.project.gameId}.json`);
  await exportGame({extensions,content,projectRoot:project,destination:game,target:'windows',runtimes:{windows:runtime,web:resolve('dist/web')}});
  const ui=(r,id)=>r.steps.find(s=>s.command.checkpoint===id).ui,b=id=>'replay-ui__'+id;
  async function play(name,steps){const script=resolve(evidence,name+'-steps.json'),report=resolve(evidence,name+'-report.json');await writeFile(script,JSON.stringify(steps));const p=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--seed','42','--save',save,'--automation',script,'--report',report,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:120000});const r=JSON.parse(await readFile(report,'utf8'));assert.equal(p.status,0,JSON.stringify(r.error??p.stderr));assert.equal(r.ok,true);return r;}
  const open=[{key:'F9'},{click:'gallery-open'},{click:'gallery-entry-promise'},{click:'gallery-branches'}];
  const button=id=>'branches__'+id+'__play';
  await play('first-run',[...route(content,'survival').steps,{key:'F5'}]);
  const profileOne=JSON.parse(await readFile(profile,'utf8'));
  const rows=galleryBranchRows(content,{selectedId:'promise',entries:profileOne.entries});
  assert.equal(rows.length,2);const seen=rows.find(r=>r.values.unlocked),locked=rows.find(r=>r.values.locked);assert.ok(seen&&locked);
  const first=await play('one-branch',[...open,{checkpoint:'cards'},{screenshot:resolve(evidence,'one-branch.png')},{click:button(locked.id)},{checkpoint:'locked'},{click:button(seen.id)},{checkpoint:'replay'},{click:b('next')},{checkpoint:'choice'},{click:b('close')}]);
  assert.equal(ui(first,'cards').screenId,content.project.gallery.branchScreenId);
  assert.equal(ui(first,'cards').elements.find(e=>e.id===button(locked.id)).enabled,false);
  assert.equal(ui(first,'locked').recollection.active,false);assert.equal(ui(first,'replay').recollection.active,true);
  assert.equal(ui(first,'choice').elements.find(e=>e.id==='choose-register').enabled,true);
  // A second real run shares the cross-run profile, without injecting unlock data.
  await play('second-run',[...route(content,'farewell').steps,{key:'F5'}]);
  const mainBytes=await readFile(save,'utf8'),profileBytes=await readFile(profile,'utf8'),profileTwo=JSON.parse(profileBytes);
  const both=galleryBranchRows(content,{selectedId:'promise',entries:profileTwo.entries});assert.equal(both.filter(r=>r.values.unlocked).length,2);
  const mismatch=await play('wrong-branch-save',[...open,{click:button(both[0].id)},{click:b('save')},{click:b('close')},
    {click:button(both[1].id)},{checkpoint:'before'},{click:b('load')},{checkpoint:'refused'},{click:b('close')}]);
  assert.match(ui(mismatch,'refused').recollection.notice,/다른 선택 경로/);
  assert.deepEqual(ui(mismatch,'refused').recollection.state,ui(mismatch,'before').recollection.state);
  assert.equal(ui(mismatch,'refused').recollection.cursor,ui(mismatch,'before').recollection.cursor);
  assert.equal(await readFile(save,'utf8'),mainBytes);assert.equal(await readFile(profile,'utf8'),profileBytes);
  for(const row of both){
    const expected=profileTwo.entries.promise.records[row.values.recordIndex];
    const result=await play(row.id,[...open,{checkpoint:'base'},{screenshot:resolve(evidence,'both-branches.png')},
      {click:button(row.id)},{checkpoint:'replay'},{extensionAdvanceMs:197},{click:b('speed')},{click:b('save')},{checkpoint:'saved'},
      {click:b('next')},{checkpoint:'choice'},{click:b('load')},{checkpoint:'restored'},{click:b('next')},{checkpoint:'choice-again'},
      {click:b('close')},{checkpoint:'returned'},{click:'back'},{click:'gallery-close'},{key:'F9'},{checkpoint:'main-loaded'}]);
    assert.deepEqual(ui(result,'choice').recollection.state,expected.replayFrames[1].state);
    assert.deepEqual(ui(result,'choice-again').recollection.state,expected.replayFrames[1].state);
    assert.deepEqual(ui(result,'saved').recollection.extensions.instances,ui(result,'restored').recollection.extensions.instances);
    assert.deepEqual(ui(result,'saved').recollection.playback,ui(result,'restored').recollection.playback);
    for(const id of ['replay','saved','choice','restored','returned','main-loaded']){
      assert.deepEqual(ui(result,id).state,ui(result,'base').state,id);assert.deepEqual(ui(result,id).commands,ui(result,'base').commands,id);
      assert.deepEqual(ui(result,id).gallery.data,ui(result,'base').gallery.data,id);
    }
    const restarted=await play(row.id+'-restart',[...open,{click:button(row.id)},{click:b('load')},{checkpoint:'restored'},{click:b('next')},{checkpoint:'choice'},{click:b('close')}]);
    assert.deepEqual(ui(restarted,'restored').recollection.extensions.instances,ui(result,'saved').recollection.extensions.instances);
    assert.deepEqual(ui(restarted,'choice').recollection.state,expected.replayFrames[1].state);
    assert.equal(await readFile(save,'utf8'),mainBytes);assert.equal(await readFile(profile,'utf8'),profileBytes);
  }
  await writeFile(resolve(evidence,'game-path.txt'),game);await writeFile(resolve(evidence,'save-path.txt'),save);
});

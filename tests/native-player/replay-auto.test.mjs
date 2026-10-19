import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir,mkdtemp} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {exportBundle} from '../../dist/packages/plugin-host/bundle.js';
import {route} from './recollection-fixture.mjs';


test('recollection autoplay, speed, skip and saved timers preserve both recorded choices and the live game', {timeout:240000}, async()=>{
  const runtime=resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player');
  const project=resolve(process.env.RAISING_REPLAY_AUTO_PROJECT??'tests/fixtures/projects/last-grandson-polished');
  const loaded=await loadProject(project),built=await buildSnapshot(loaded);assert.equal(built.ok,true,JSON.stringify(built.diagnostics));const content=built.snapshot;
  const controlScreen=content.project.visual.screens.find(s=>s.id===content.project.gallery.replayControlsScreenId);
  for(const action of ['recollectionAuto','recollectionSpeed','recollectionSkip'])assert.ok(controlScreen.elements.some(e=>e.action===action));
  const root=await mkdtemp(resolve('.cache/replay-auto-native-')),game=resolve(root,'game'),evidence=resolve('.cache/replay-auto');await mkdir(evidence,{recursive:true});
  const capabilities=JSON.parse(await readFile(resolve(runtime,'capabilities.json'),'utf8'));
  const extensions=await exportBundle({lock:loaded.documents.find(d=>d.kind==='pluginLock')?.data,documents:loaded.documents,visual:content.project.visual,target:'windows',assets:{root:project,assets:content.assets,hashes:content.assetHashes},hostFeatures:capabilities.visualFeatures});
  await exportGame({extensions,content,projectRoot:project,destination:game,target:'windows',runtimes:{windows:runtime,web:resolve('dist/web')}});await writeFile(resolve(evidence,'game-path.txt'),game);
  const ui=(r,id)=>r.steps.find(s=>s.command.checkpoint===id).ui,b=id=>'replay-ui__'+id;
  for(const ending of ['survival','farewell']){
    const save=resolve(root,ending,'slot.json'),profile=resolve(root,ending,`collection-${content.project.gameId}.json`),replay=resolve(root,ending,`recollection-${content.project.gameId}.json`);
    async function play(name,steps){const script=resolve(evidence,ending+'-'+name+'-steps.json'),report=resolve(evidence,ending+'-'+name+'-report.json');await writeFile(script,JSON.stringify(steps));const result=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--seed','42','--save',save,'--automation',script,'--report',report,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:120000});const r=JSON.parse(await readFile(report,'utf8'));assert.equal(result.status,0,JSON.stringify(r.error??result.stderr));assert.equal(r.ok,true);return r;}
    const recordSteps=route(content,ending).steps;recordSteps.push({key:'F5'});await play('record',recordSteps);
    const mainBytes=await readFile(save,'utf8'),profileBytes=await readFile(profile,'utf8'),record=JSON.parse(profileBytes).entries.promise.records[0];
    assert.equal(record.replayFrames.length,2);assert.equal(record.replayFrames[1].choiceId,ending==='survival'?'register':'decline');
    const result=await play('playback',[
      {key:'F9'},{click:'gallery-open'},{checkpoint:'baseline'},{click:'gallery-entry-promise'},{click:'gallery-replay'},{checkpoint:'first'},
      {click:b('speed')},{extensionAdvanceMs:100},{checkpoint:'speed2'},
      {click:b('auto')},{recollectionAdvanceMs:500},{checkpoint:'partial'},{click:b('save')},{checkpoint:'saved'},
      {recollectionAdvanceMs:1000},{checkpoint:'auto-choice'},{click:b('auto')},{recollectionAdvanceMs:60000},{checkpoint:'paused'},
      {click:b('previous')},{click:b('speed')},{click:b('auto')},{recollectionAdvanceMs:750},{checkpoint:'fast-choice'},
      {screenshot:resolve(evidence,ending+'-fast.png')},{click:b('previous')},{click:b('skip')},{checkpoint:'skipped'},
      {click:b('previous')},{click:b('load')},{checkpoint:'loaded'},{recollectionAdvanceMs:1000},{checkpoint:'continued'},
      {recollectionAdvanceMs:1500},{checkpoint:'auto-finished'},
      {click:'gallery-replay'},{click:b('next')},{checkpoint:'manual-choice'},{click:b('close')},{checkpoint:'closed'},
      {click:'gallery-close'},{key:'F9'},{checkpoint:'live-loaded'}]);
    const base=ui(result,'baseline');
    for(const id of ['first','speed2','partial','saved','auto-choice','paused','fast-choice','skipped','loaded','continued','auto-finished','manual-choice','closed','live-loaded']){
      const current=ui(result,id);assert.deepEqual(current.state,base.state,id+': main state/RNG');assert.deepEqual(current.commands,base.commands,id+': main commands');assert.deepEqual(current.gallery.data,base.gallery.data,id+': unlocks');assert.deepEqual(current.extensions.instances,base.extensions.instances,id+': main animation');
    }
    assert.equal(ui(result,'speed2').recollection.playback.speed,2);assert.equal(ui(result,'speed2').recollection.extensions.clockMs-ui(result,'first').recollection.extensions.clockMs,200);
    assert.deepEqual(ui(result,'saved').recollection.playback,{autoplay:true,speed:2,elapsedMs:1000});
    assert.deepEqual(ui(result,'loaded').recollection.playback,ui(result,'saved').recollection.playback);
    assert.deepEqual(ui(result,'loaded').recollection.state,ui(result,'saved').recollection.state);
    const pose=r=>r.extensions.instances;
    assert.deepEqual(pose(ui(result,'loaded').recollection),pose(ui(result,'saved').recollection),'exact loop time and rendered pose after load');
    for(const id of ['auto-choice','paused','fast-choice','skipped','continued','manual-choice']){
      assert.deepEqual(ui(result,id).recollection.state,record.replayFrames[1].state,id+': same recorded branch');
      assert.equal(ui(result,id).elements.find(e=>e.id==='choose-'+record.replayFrames[1].choiceId).enabled,true);
      assert.equal(ui(result,id).elements.find(e=>e.id==='choose-'+(ending==='survival'?'decline':'register')).enabled,false);
    }
    assert.equal(ui(result,'paused').recollection.playback.autoplay,false);assert.equal(ui(result,'fast-choice').recollection.playback.speed,4);assert.equal(ui(result,'skipped').recollection.playback.autoplay,false);assert.equal(ui(result,'auto-finished').recollection.active,false);
    assert.equal(await readFile(save,'utf8'),mainBytes);assert.equal(await readFile(profile,'utf8'),profileBytes);
    const reopened=await play('restart',[{click:'gallery-open'},{click:'gallery-entry-promise'},{click:'gallery-replay'},{click:b('load')},{checkpoint:'resumed'},{recollectionAdvanceMs:400},{recollectionAdvanceMs:600},{checkpoint:'partitioned'},{click:b('close')}]);
    assert.deepEqual(pose(ui(reopened,'resumed').recollection),pose(ui(result,'saved').recollection),'fresh process resumes the same animation phase');
    assert.equal(ui(reopened,'resumed').state,null);assert.deepEqual(ui(reopened,'resumed').recollection.playback,{autoplay:true,speed:2,elapsedMs:1000});assert.deepEqual(ui(reopened,'partitioned').recollection.state,ui(result,'continued').recollection.state);assert.deepEqual(ui(reopened,'partitioned').recollection.playback,ui(result,'continued').recollection.playback);
    const saved=JSON.parse(await readFile(replay,'utf8'));delete saved.playback;await writeFile(replay,JSON.stringify(saved));
    const legacy=await play('legacy',[{click:'gallery-open'},{click:'gallery-entry-promise'},{click:'gallery-replay'},{click:b('load')},{checkpoint:'loaded'},{recollectionAdvanceMs:60000},{checkpoint:'still'},{click:b('close')}]);
    assert.deepEqual(ui(legacy,'loaded').recollection.playback,{autoplay:false,speed:1,elapsedMs:0});assert.equal(ui(legacy,'still').recollection.cursor,0);
    saved.playback={autoplay:true,speed:0,elapsedMs:3000};await writeFile(replay,JSON.stringify(saved));
    const bad=await play('bad-playback',[{key:'F9'},{click:'gallery-open'},{click:'gallery-entry-promise'},{click:'gallery-replay'},{checkpoint:'before'},{click:b('load')},{checkpoint:'error'},{click:b('close')}]);
    assert.ok(ui(bad,'error').recollection.notice);assert.deepEqual(ui(bad,'error').recollection.state,ui(bad,'before').recollection.state);assert.equal(ui(bad,'error').recollection.playback.autoplay,false);assert.equal(await readFile(save,'utf8'),mainBytes);assert.equal(await readFile(profile,'utf8'),profileBytes);
    // Save mid-loop at each speed while auto is running, then compare continued motion,
    // not just the restored clip name. The extension clock is driven explicitly here.
    for(const speed of [1,2,4]){
      const exact=await play('exact-'+speed,[{key:'F9'},{click:'gallery-open'},{checkpoint:'base'},
        {click:'gallery-entry-promise'},{click:'gallery-replay'},
        ...Array.from({length:speed===1?0:speed===2?1:2},()=>({click:b('speed')})),
        {click:b('auto')},{extensionAdvanceMs:2777},{recollectionAdvanceMs:100},{click:b('save')},{checkpoint:'saved'},
        {extensionAdvanceMs:113},{recollectionAdvanceMs:123},{checkpoint:'expected'},
        {click:b('load')},{checkpoint:'restored'},
        {extensionAdvanceMs:113},{recollectionAdvanceMs:123},{checkpoint:'continued'},
        {screenshot:resolve(evidence,ending+'-exact-'+speed+'.png')},{click:b('close')}]);
      const at=id=>ui(exact,id),savedReplay=at('saved').recollection;
      assert.ok(savedReplay.extensions.instances.some(i=>i.state.tracks.some(t=>t.timeMs>0)),'nonzero loop phase');
      assert.deepEqual(pose(at('restored').recollection),pose(savedReplay),'restore pose at '+speed+'x');
      assert.deepEqual(pose(at('continued').recollection),pose(at('expected').recollection),'continued pose at '+speed+'x');
      assert.deepEqual(at('continued').recollection.playback,at('expected').recollection.playback,'continued auto timer');
      for(const id of ['saved','expected','restored','continued']){
        assert.deepEqual(at(id).state,at('base').state);assert.deepEqual(at(id).commands,at('base').commands);
        assert.deepEqual(at(id).gallery.data,at('base').gallery.data);assert.deepEqual(at(id).extensions.instances,at('base').extensions.instances);
      }
      const disk=JSON.parse(await readFile(replay,'utf8'));
      assert.ok(Object.values(disk.extensions['cutout-animation'].instances).every(i=>i.state.version===2));
      assert.equal(await readFile(save,'utf8'),mainBytes);assert.equal(await readFile(profile,'utf8'),profileBytes);
    }
    // Leave a usable paused review save, not the malformed test fixture.
    saved.playback={autoplay:false,speed:1,elapsedMs:0};await writeFile(replay,JSON.stringify(saved));
    if(ending==='survival'){
      const realtime=await play('realtime',[{key:'F9'},{click:'gallery-open'},{checkpoint:'before'},{click:'gallery-entry-promise'},{click:'gallery-replay'},{click:b('speed')},{click:b('speed')},{click:b('auto')},{timelineRealtime:true},{waitFrames:240},{timelineRealtime:false},{checkpoint:'finished'}]);
      assert.equal(ui(realtime,'finished').recollection.active,false);assert.deepEqual(ui(realtime,'finished').state,ui(realtime,'before').state);assert.equal(await readFile(save,'utf8'),mainBytes);assert.equal(await readFile(profile,'utf8'),profileBytes);
    }
    if(ending==='survival')await writeFile(resolve(evidence,'save-path.txt'),save);
  }
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir,mkdtemp} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {exportBundle} from '../../dist/packages/plugin-host/bundle.js';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/index.js';

function route(content,ending){
  let session=createSession(content,42,{mode:'full'}).session;
  const steps=[{click:'start'},{key:'F5'},{checkpoint:'new-run'}];let midSaved=false;
  const node=()=>{const n=session.state.activeNarrative;return n?(n.kind==='event'?content.events:content.endings).find(x=>x.id===n.id)?.nodes.find(x=>x.id===n.nodeId):null;};
  function send(command){const r=dispatchSession(content,session,command,session.state.stateRevision);assert.equal(r.ok,true,JSON.stringify(r.error));session=r.session;}
  function settle(){for(let i=0;i<100;i++){if(session.state.phase==='period-end')send({type:'resolvePeriodEnd'});else if(session.state.phase==='narrative'&&!['dialogue','choice'].includes(node()?.type))send({type:'advanceNarrative'});else return;}throw new Error('settle limit');}
  for(let month=0;month<2;month++){
    const plan=ending==='survival'?(month===0?['study','rest','study']:['study','rest','work']):['rest','rest','rest'];
    steps.push(...plan.map((value,i)=>({select:'plan-'+i,value})),{click:'advance'});send({type:'setSchedule',activityIds:plan});
    for(let slot=0;slot<3;slot++){
      steps.push({scheduleAdvanceMs:750});send({type:'executeActivity'});settle();
      if(month===0&&slot===0){steps.push({timelineAdvanceMs:1200},{checkpoint:'before-gallery'},{click:'gallery-open'},{timelineAdvanceMs:5000},{extensionAdvanceMs:500},{scheduleAdvanceMs:5000},{checkpoint:'gallery-paused'},{click:'gallery-close'},{checkpoint:'gallery-closed'});}
      steps.push({click:'timeline-skip'},{scheduleAdvanceMs:750});send({type:'acknowledgeResult'});settle();
      while(session.state.phase==='narrative'){
        const n=node(),choice=n.type==='choice'?(ending==='survival'?'register':'decline'):null;
        if(choice&&!midSaved){steps.push({key:'F5'},{checkpoint:'mid-saved'},{key:'F9'},{checkpoint:'mid-loaded'});midSaved=true;}
        steps.push({click:choice?'choose-'+choice:'advance'});send(choice?{type:'choose',choiceId:choice}:{type:'advanceNarrative'});settle();
      }
    }
  }
  steps.push({checkpoint:'finished'});assert.equal(session.state.phase,'finished');return {steps,state:session.state};
}

test('recollection autoplay, speed, skip and saved timers preserve both recorded choices and the live game', {timeout:240000}, async()=>{
  const runtime=resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player');
  const project=resolve(process.env.RAISING_REPLAY_AUTO_PROJECT??'examples/last-grandson-polished');
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
    // The cutout plugin's existing contract restarts decorative loops on load.
    // Preserve clip/expression identity while testing the replay timer independently.
    const look=r=>r.extensions.instances.map(i=>({expression:i.state.expression,skin:i.state.skin,clips:i.state.tracks.map(t=>t.clip)}));
    assert.deepEqual(look(ui(result,'loaded').recollection),look(ui(result,'saved').recollection));
    for(const id of ['auto-choice','paused','fast-choice','skipped','continued','manual-choice']){
      assert.deepEqual(ui(result,id).recollection.state,record.replayFrames[1].state,id+': same recorded branch');
      assert.equal(ui(result,id).elements.find(e=>e.id==='choose-'+record.replayFrames[1].choiceId).enabled,true);
      assert.equal(ui(result,id).elements.find(e=>e.id==='choose-'+(ending==='survival'?'decline':'register')).enabled,false);
    }
    assert.equal(ui(result,'paused').recollection.playback.autoplay,false);assert.equal(ui(result,'fast-choice').recollection.playback.speed,4);assert.equal(ui(result,'skipped').recollection.playback.autoplay,false);assert.equal(ui(result,'auto-finished').recollection.active,false);
    assert.equal(await readFile(save,'utf8'),mainBytes);assert.equal(await readFile(profile,'utf8'),profileBytes);
    const reopened=await play('restart',[{click:'gallery-open'},{click:'gallery-entry-promise'},{click:'gallery-replay'},{click:b('load')},{checkpoint:'resumed'},{recollectionAdvanceMs:400},{recollectionAdvanceMs:600},{checkpoint:'partitioned'},{click:b('close')}]);
    assert.equal(ui(reopened,'resumed').state,null);assert.deepEqual(ui(reopened,'resumed').recollection.playback,{autoplay:true,speed:2,elapsedMs:1000});assert.deepEqual(ui(reopened,'partitioned').recollection.state,ui(result,'continued').recollection.state);assert.deepEqual(ui(reopened,'partitioned').recollection.playback,ui(result,'continued').recollection.playback);
    const saved=JSON.parse(await readFile(replay,'utf8'));delete saved.playback;await writeFile(replay,JSON.stringify(saved));
    const legacy=await play('legacy',[{click:'gallery-open'},{click:'gallery-entry-promise'},{click:'gallery-replay'},{click:b('load')},{checkpoint:'loaded'},{recollectionAdvanceMs:60000},{checkpoint:'still'},{click:b('close')}]);
    assert.deepEqual(ui(legacy,'loaded').recollection.playback,{autoplay:false,speed:1,elapsedMs:0});assert.equal(ui(legacy,'still').recollection.cursor,0);
    saved.playback={autoplay:true,speed:0,elapsedMs:3000};await writeFile(replay,JSON.stringify(saved));
    const bad=await play('bad-playback',[{key:'F9'},{click:'gallery-open'},{click:'gallery-entry-promise'},{click:'gallery-replay'},{checkpoint:'before'},{click:b('load')},{checkpoint:'error'},{click:b('close')}]);
    assert.ok(ui(bad,'error').recollection.notice);assert.deepEqual(ui(bad,'error').recollection.state,ui(bad,'before').recollection.state);assert.equal(ui(bad,'error').recollection.playback.autoplay,false);assert.equal(await readFile(save,'utf8'),mainBytes);assert.equal(await readFile(profile,'utf8'),profileBytes);
    // Leave a usable paused review save, not the malformed test fixture.
    saved.playback={autoplay:false,speed:1,elapsedMs:0};await writeFile(replay,JSON.stringify(saved));
    if(ending==='survival'){
      const realtime=await play('realtime',[{key:'F9'},{click:'gallery-open'},{checkpoint:'before'},{click:'gallery-entry-promise'},{click:'gallery-replay'},{click:b('speed')},{click:b('speed')},{click:b('auto')},{timelineRealtime:true},{waitFrames:240},{timelineRealtime:false},{checkpoint:'finished'}]);
      assert.equal(ui(realtime,'finished').recollection.active,false);assert.deepEqual(ui(realtime,'finished').state,ui(realtime,'before').state);assert.equal(await readFile(save,'utf8'),mainBytes);assert.equal(await readFile(profile,'utf8'),profileBytes);
    }
    if(ending==='survival')await writeFile(resolve(evidence,'save-path.txt'),save);
  }
});

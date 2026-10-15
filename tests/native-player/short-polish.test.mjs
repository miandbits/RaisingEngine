import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {exportBundle} from '../../dist/packages/plugin-host/bundle.js';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/index.js';

function route(content,kind,evidence){
  let session=createSession(content,42,{mode:'full'}).session;
  const steps=[{checkpoint:'title',screenshot:resolve(evidence,'title.png')},{click:'start'}],expected={},restores=[];
  const node=()=>{const n=session.state.activeNarrative;return n?(n.kind==='event'?content.events:content.endings).find(x=>x.id===n.id)?.nodes.find(x=>x.id===n.nodeId):null;};
  function send(command){const next=dispatchSession(content,session,command,session.state.stateRevision);assert.equal(next.ok,true,JSON.stringify(next.error));session=next.session;}
  function settle(){for(let n=0;n<100;n++){if(session.state.phase==='period-end')send({type:'resolvePeriodEnd'});else if(session.state.phase==='narrative'&&!['dialogue','choice'].includes(node()?.type))send({type:'advanceNarrative'});else return;}assert.fail('settle limit');}
  function checkpoint(id,image=false){expected[id]=structuredClone(session.state);steps.push({checkpoint:id,...(image?{screenshot:resolve(evidence,kind+'-'+id+'.png')}:{})});}
  function action(command,clicks){steps.push(...clicks);send(command);settle();}
  function restore(command,clicks,id){const saved=structuredClone(session);steps.push({key:'F5'});checkpoint(id+'-saved',true);action(command,clicks);checkpoint(id+'-changed');steps.push({key:'F9'});session=saved;checkpoint(id+'-restored');restores.push(id);}
  function stories(){let guard=0;while(session.state.phase==='narrative'){const n=node();assert.ok(n);steps.push({timelineAdvanceMs:10000},{scheduleAdvanceMs:5000});checkpoint(`story-paused-${session.state.stateRevision}`);const choice=n.type==='choice'?(kind==='survival'?'register':'decline'):null;const command=choice?{type:'choose',choiceId:choice}:{type:'advanceNarrative'},clicks=[{click:choice?'choose-'+choice:'advance'}];if(session.state.periodIndex===0&&n.type==='choice')restore(command,clicks,'promise');action(command,clicks);if(++guard>30)assert.fail('story limit');}}
  checkpoint('started',true);
  steps.push({extensionAdvanceMs:1200},{checkpoint:'breathing',screenshot:resolve(evidence,kind+'-breathing.png')});
  steps.push({resize:{width:960,height:540}},{checkpoint:'small',screenshot:resolve(evidence,kind+'-small.png')},{resize:{width:1600,height:900}},{checkpoint:'large',screenshot:resolve(evidence,kind+'-large.png')},{resize:{width:1280,height:720}});
  steps.push({click:'sound'},{checkpoint:'muted'},{click:'sound'});
  for(let month=0;month<2;month++){
    const plan=kind==='survival'?(month===0?['study','rest','study']:['study','rest','work']):['rest','rest','rest'];
    action({type:'setSchedule',activityIds:plan},[...plan.map((value,i)=>({select:'plan-'+i,value})),{click:'advance'}]);
    for(let slot=0;slot<3;slot++){
      action({type:'executeActivity'},[{scheduleAdvanceMs:750}]);checkpoint(`month-${month}-slot-${slot}`,month===0);
      if(month===0&&slot===0){steps.push({timelineAdvanceMs:1200});restore({type:'acknowledgeResult'},[{timelineAdvanceMs:60000},{scheduleAdvanceMs:750}],'training');}
      if(month===0&&slot===1)restore({type:'acknowledgeResult'},[{timelineAdvanceMs:60000},{scheduleAdvanceMs:750}],'midpoint');
      const sdId=`sd-${month}-${slot}`;
      steps.push({checkpoint:sdId+'-base'},{extensionAdvanceMs:plan[slot]==='study'?1200:plan[slot]==='work'?320:1800},{checkpoint:sdId+'-motion',screenshot:resolve(evidence,kind+'-'+sdId+'.png')});
      steps.push({timelineAdvanceMs:60000},{checkpoint:sdId+'-complete'});
      action({type:'acknowledgeResult'},[{scheduleAdvanceMs:750}]);stories();
    }
  }
  checkpoint('finished',true);assert.equal(session.state.phase,'finished');assert.equal(session.state.endingId,kind==='survival'?'certified':'ordinary');
  steps.push({key:'F5'});return {steps,expected,restores};
}

test('polished short: real animation plugin, both endings and save/resume', {timeout:180000},async()=>{
  const project=resolve(process.env.RAISING_SHORT_PROJECT??'examples/last-grandson-polished');
  const loaded=await loadProject(project);const built=await buildSnapshot(loaded);assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const content=built.snapshot;assert.equal(content.project.gameId,'last-grandson');assert.equal(content.project.visual.screens.length,4);
  const root=await mkdtemp(resolve('.cache/short-native-')),game=resolve(root,'game'),evidence=resolve('.cache/short-polish');await mkdir(evidence,{recursive:true});
  const capabilities=JSON.parse(await readFile(resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-auto-result','capabilities.json'),'utf8'));
  const extensions=await exportBundle({lock:loaded.documents.find(d=>d.kind==='pluginLock')?.data,documents:loaded.documents,visual:content.project.visual,target:'windows',assets:{root:project,assets:content.assets,hashes:content.assetHashes},hostFeatures:capabilities.visualFeatures});
  assert.ok(extensions);
  await exportGame({extensions,content,projectRoot:project,destination:game,target:'windows',runtimes:{windows:resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-auto-result'),web:resolve('dist/web')}});
  await writeFile(resolve(evidence,'game-path.txt'),game);
  for(const kind of ['survival','farewell']){
    const plan=route(content,kind,evidence),script=resolve(evidence,kind+'-steps.json'),report=resolve(evidence,kind+'-native-report.json'),save=resolve(evidence,kind+'-save.json');
    await writeFile(script,JSON.stringify(plan.steps,null,2));
    const child=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--seed','42','--save',save,'--automation',script,'--report',report,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:90000});
    const result=JSON.parse(await readFile(report,'utf8'));assert.equal(child.status,0,JSON.stringify(result.error??child.stderr));assert.equal(result.ok,true);
    const state=id=>result.steps.find(s=>s.command.checkpoint===id)?.ui.state;
    const ui=id=>result.steps.find(s=>s.command.checkpoint===id)?.ui;
    const animation=id=>ui(id).extensions.instances.find(i=>i.instance==='schedule/sd-character');
    assert.equal(animation('started').available,true);
    assert.notDeepEqual(animation('started').items[0].matrix,animation('breathing').items[0].matrix);
    assert.ok(animation('breathing').state.tracks[0].timeMs>0);
    assert.equal(ui('muted').audio.muted,true);
    assert.equal(ui('started').audio.musicPlaying,true);
    assert.equal(animation('month-0-slot-1').state.tracks[0].clip,'rest');
    for(let month=0;month<2;month++)for(let slot=0;slot<3;slot++){
      const activity=kind==='survival'?(month===0?['study','rest','study']:['study','rest','work'])[slot]:'rest';
      const base=ui(`sd-${month}-${slot}-base`),motion=ui(`sd-${month}-${slot}-motion`);
      const sd=view=>view.extensions.instances.find(i=>i.instance==='schedule/sd-character');
      assert.equal(sd(base)?.available,true,'SD runtime available');
      assert.equal(sd(base).state.tracks[0].clip,activity,'activity selects matching SD clip');
      assert.notDeepEqual(sd(base).items,sd(motion).items,'SD frame or breathing pose advances');
      assert.ok(sd(base).items.length>0,'SD stage has actual draw items');
      assert.ok(!JSON.stringify(base.texts).includes('이번 달, 무엇을 지킬 것인가'),'result hides planning prompt');
      assert.ok(!base.elements.some(e=>/^plan-/.test(e.id)),'result has no schedule selectors');
      assert.ok(!base.elements.some(e=>e.id==='advance'),'automatic results do not require confirmation');
      assert.ok(base.timeline.plan.resultPlayback,'inline stat animation is active');
      assert.equal(ui(`sd-${month}-${slot}-complete`).timeline.cursor.completed,true);
    }
    assert.ok(animation('started').items.length>0,'SD stage stays mounted while planning');
    assert.equal(animation('started').state.tracks[0].clip,'rest');
    // The plugin restarts decorative loops and does not persist cross-fades; playback
    // serials are also internal. Expression/clip selection persists across restore.
    for(const id of plan.restores){const before=ui(id+'-saved').extensions.instances,after=ui(id+'-restored').extensions.instances;assert.deepEqual(before.map(i=>({instance:i.instance,expression:i.state.activeExpression,clips:i.state.tracks.map(t=>t.clip)})),after.map(i=>({instance:i.instance,expression:i.state.activeExpression,clips:i.state.tracks.map(t=>t.clip)})),'animation selection restores '+id);}
    assert.equal(animation('training-restored').state.tracks[0].timeMs,0,'decorative SD loops restart when restored');
    assert.deepEqual(ui('training-saved').timeline,ui('training-restored').timeline,'partial stat display resumes at the saved cursor');
    assert.deepEqual(ui('training-saved').texts.result,ui('training-restored').texts.result,'partial displayed numbers restore');
    if(kind==='survival'){assert.match(ui('training-saved').texts.result,/27/);assert.match(ui('training-saved').texts.result,/\(-3\)/);assert.match(ui('month-0-slot-0').texts.result,/\(\+0\)/);}
    for(const id of ['small','large']){assert.equal(ui(id).screenId,'schedule');assert.ok(ui(id).elements.find(e=>e.id==='advance')?.enabled);assert.equal(animation(id).available,true);}
    assert.ok(JSON.stringify(ui('started').texts).includes('기력 25'),'selected activity description visible');
    for(const [id,expected]of Object.entries(plan.expected))assert.deepEqual(state(id),expected,kind+' '+id);
    for(const id of plan.restores){assert.notDeepEqual(state(id+'-saved'),state(id+'-changed'));assert.deepEqual(state(id+'-saved'),state(id+'-restored'));}
    const restart=resolve(evidence,kind+'-reload.json'),reloadReport=resolve(evidence,kind+'-reload-report.json');await writeFile(restart,JSON.stringify([{click:'load'},{checkpoint:'reopened'}]));
    const again=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',save,'--automation',restart,'--report',reloadReport,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:30000});assert.equal(again.status,0,again.stderr);
    const loaded=JSON.parse(await readFile(reloadReport,'utf8'));assert.equal(loaded.ok,true);assert.deepEqual(loaded.steps.find(s=>s.command.checkpoint==='reopened').ui.state,plan.expected.finished);
  }
  // Exercise the actual wall clock as well: one plan click, no result clicks.
  const realtime=resolve(evidence,'auto-realtime.json'),realtimeReport=resolve(evidence,'auto-realtime-report.json');
  await writeFile(realtime,JSON.stringify([{click:'start'},...['study','rest','study'].map((value,i)=>({select:'plan-'+i,value})),{click:'advance'},{timelineRealtime:true},{waitFrames:1600},{checkpoint:'event-paused',screenshot:resolve(evidence,'auto-event-paused.png')}]));
  const played=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',resolve(root,'realtime-save.json'),'--automation',realtime,'--report',realtimeReport,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:60000});assert.equal(played.status,0,played.stderr);
  const stopped=JSON.parse(await readFile(realtimeReport,'utf8'));assert.equal(stopped.ok,true);assert.equal(stopped.final.state.phase,'narrative');assert.equal(stopped.final.state.periodIndex,0);assert.equal(stopped.final.state.attributes.skill,12);assert.equal(stopped.final.timeline,null);
});

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

function route(content,ending,mode,evidence){
  let session=createSession(content,42,{mode:'full'}).session;
  const steps=[{click:'start'}],expected={};
  const node=()=>{const n=session.state.activeNarrative;return n?(n.kind==='event'?content.events:content.endings).find(x=>x.id===n.id)?.nodes.find(x=>x.id===n.nodeId):null;};
  function send(command){const r=dispatchSession(content,session,command,session.state.stateRevision);assert.equal(r.ok,true,JSON.stringify(r.error));session=r.session;}
  function settle(){for(let i=0;i<100;i++){if(session.state.phase==='period-end')send({type:'resolvePeriodEnd'});else if(session.state.phase==='narrative'&&!['dialogue','choice'].includes(node()?.type))send({type:'advanceNarrative'});else return;}throw new Error('settle limit');}
  function cp(id){expected[id]=structuredClone(session.state);steps.push({checkpoint:id});}
  for(let month=0;month<2;month++){
    const plan=ending==='survival'?(month===0?['study','rest','study']:['study','rest','work']):['rest','rest','rest'];
    steps.push(...plan.map((value,i)=>({select:'plan-'+i,value})),{click:'advance'});send({type:'setSchedule',activityIds:plan});
    for(let slot=0;slot<3;slot++){
      steps.push({scheduleAdvanceMs:750});send({type:'executeActivity'});settle();cp(`start-${month}-${slot}`);
      if(month===0&&slot===0&&mode!=='normal'){
        steps.push({timelineAdvanceMs:1200},{click:'timeline-speed'},{click:'timeline-pause'});cp('paused');
        steps.push({timelineAdvanceMs:3000},{extensionAdvanceMs:500},{scheduleAdvanceMs:5000});cp('held');
        steps.push({key:'F5'});cp('saved');
        steps.push({click:'timeline-pause'},{timelineAdvanceMs:100},{click:'timeline-speed'},{key:'F9'});cp('loaded');
        steps.push({timelineAdvanceMs:1000},{extensionAdvanceMs:100},{scheduleAdvanceMs:3000});cp('loaded-held');
        steps.push({screenshot:resolve(evidence,ending+'-'+mode+'-paused.png')},{click:'timeline-pause'});cp('resumed');
        steps.push({timelineAdvanceMs:100});cp('fast-frame');
      }
      if(mode==='skip')steps.push({click:'timeline-skip'});
      else steps.push({timelineAdvanceMs:60000});
      cp(`complete-${month}-${slot}`);
      steps.push({scheduleAdvanceMs:750});send({type:'acknowledgeResult'});settle();
      while(session.state.phase==='narrative'){
        const id=`event-${session.state.stateRevision}`;cp(id);
        steps.push({timelineAdvanceMs:60000},{scheduleAdvanceMs:60000});cp(id+'-held');
        const n=node(),choice=n.type==='choice'?(ending==='survival'?'register':'decline'):null;
        steps.push({click:choice?'choose-'+choice:'advance'});send(choice?{type:'choose',choiceId:choice}:{type:'advanceNarrative'});settle();
      }
    }
  }
  cp('finished');assert.equal(session.state.phase,'finished');return {steps,expected};
}

for(const authored of [false,true])test(`${authored?'authored':'legacy'} native controls freeze SD/stats, restore pause and speed, preserve both endings and stop at every event`, {timeout:240000},async()=>{
  const project=resolve('tests/fixtures/projects/last-grandson-polished'),loaded=await loadProject(project);
  if(authored){const visual=loaded.documents.find(d=>d.kind==='project').data.visual;
    visual.screens.find(s=>s.id==='schedule').elements.push(...['timelinePause','timelineSpeed','timelineSkip'].map((action,i)=>({id:`authored-${["pause","speed","skip"][i]}`,kind:'button',action,x:40+i*220,y:650,width:210,height:48,visibleWhen:'result'})));
  }
  const built=await buildSnapshot(loaded);assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const content=built.snapshot,runtime=resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player');
  const root=await mkdtemp(resolve('.cache/result-controls-')),game=resolve(root,'game'),evidence=resolve(authored?'.cache/result-controls-authored':'.cache/result-controls');await mkdir(evidence,{recursive:true});
  const capabilities=JSON.parse(await readFile(resolve(runtime,'capabilities.json'),'utf8'));
  const extensions=await exportBundle({lock:loaded.documents.find(d=>d.kind==='pluginLock')?.data,documents:loaded.documents,visual:content.project.visual,target:'windows',assets:{root:project,assets:content.assets,hashes:content.assetHashes},hostFeatures:capabilities.visualFeatures});
  await exportGame({extensions,content,projectRoot:project,destination:game,target:'windows',runtimes:{windows:runtime,web:resolve('dist/web')}});await writeFile(resolve(evidence,'game-path.txt'),game);
  for(const ending of ['survival','farewell']){
    let baseline;
    for(const mode of ['normal','fast','skip']){
      const path=resolve(evidence,ending+'-'+mode),plan=route(content,ending,mode,evidence),save=path+'-save.json';
      if(authored)for(const step of plan.steps)if(step.click?.startsWith('timeline-'))step.click={'timeline-pause':'authored-pause','timeline-speed':'authored-speed','timeline-skip':'authored-skip'}[step.click];
      await writeFile(path+'-steps.json',JSON.stringify(plan.steps));
      const played=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--seed','42','--save',save,'--automation',path+'-steps.json','--report',path+'-report.json','--hidden'],{encoding:'utf8',windowsHide:true,timeout:90000});
      const report=JSON.parse(await readFile(path+'-report.json','utf8'));assert.equal(played.status,0,JSON.stringify(report.error??played.stderr));assert.equal(report.ok,true);
      const ui=id=>report.steps.find(s=>s.command.checkpoint===id).ui;
      if(authored){assert.ok(ui('start-0-0').elements.some(e=>e.id==='authored-pause'));assert.ok(!ui('start-0-0').elements.some(e=>e.id==='timeline-pause'),'authored controls replace fallback');}
      for(const [id,state]of Object.entries(plan.expected))assert.deepEqual(ui(id).state,state,`${ending}/${mode}/${id}`);
      if(!baseline)baseline=ui('finished').state;else assert.deepEqual(ui('finished').state,baseline,'mode must not change state, RNG, history or ending');
      if(mode!=='normal'){
        const sd=id=>ui(id).extensions.instances.find(i=>i.instance==='schedule/sd-character');
        assert.deepEqual(ui('held').timeline,ui('paused').timeline);assert.deepEqual(sd('held').items,sd('paused').items,'SD freezes');
        assert.equal(ui('held').texts.result,ui('paused').texts.result,'partially changed displayed stats freeze');
        assert.deepEqual(ui('loaded').timeline,ui('saved').timeline);assert.equal(ui('loaded').timeline.cursor.speed,4);assert.equal(ui('loaded').timeline.cursor.paused,true);
        assert.equal(ui('loaded').texts.result,ui('saved').texts.result,'partially changed displayed stats restore');
        assert.deepEqual(ui('loaded-held').timeline,ui('loaded').timeline);assert.deepEqual(sd('loaded-held').items,sd('loaded').items,'restored SD remains frozen');
        assert.equal(ui('fast-frame').timeline.cursor.elapsedMs,0);assert.equal(ui('fast-frame').timeline.cursor.clipIndex,2); // 1200 + 100*4 = 1600
        assert.equal(sd('fast-frame').state.tracks[0].timeMs-sd('resumed').state.tracks[0].timeMs,400,'SD shares 4x speed');
        assert.equal(ui('start-0-1').timeline.cursor.speed,4,'speed carries past the event to the next activity');
        assert.equal(ui('start-1-0').timeline.cursor.speed,4,'speed carries to the next month');
        for(const s of report.steps.filter(s=>s.command.checkpoint?.startsWith('event-')))assert.ok(!s.ui.elements.some(e=>['timeline-skip','timeline-speed','authored-skip','authored-speed'].includes(e.id)),'controls do not leak into narrative');
        // Reopen a paused mid-activity save in a new player process.
        const script=path+'-reload.json';await writeFile(script,JSON.stringify([{click:'load'},{timelineAdvanceMs:2000},{scheduleAdvanceMs:5000},{checkpoint:'reopened'}]));
        const again=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',save,'--automation',script,'--report',path+'-reload-report.json','--hidden'],{encoding:'utf8',windowsHide:true,timeout:30000});assert.equal(again.status,0,again.stderr);
        const reopened=JSON.parse(await readFile(path+'-reload-report.json','utf8'));assert.equal(reopened.ok,true);assert.deepEqual(reopened.final.state,ui('saved').state);assert.deepEqual(reopened.final.timeline,ui('saved').timeline);
      }
    }
  }
});

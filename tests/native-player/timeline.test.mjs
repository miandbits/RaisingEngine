import test from 'node:test';
import assert from 'node:assert/strict';
import { cp,mkdir,mkdtemp,readFile,writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { exportGame } from '../../dist/packages/export-service/export.js';
import { planActivityTimeline } from '../../dist/packages/visual/timeline.js';
import { nativeCreateRun,nativeDispatchCommand } from '../../dist/packages/native-bridge/index.js';

async function fixture(){
  await mkdir(resolve('.cache/native-timeline-tests'),{recursive:true});const root=await mkdtemp(resolve('.cache/native-timeline-tests/run-'));
  let projectRoot=process.env.RAISING_TIMELINE_PROJECT&&resolve(process.env.RAISING_TIMELINE_PROJECT);
  if(!projectRoot){projectRoot=resolve(root,'project');await cp(resolve('examples/academy-journey'),projectRoot,{recursive:true});
    const path=resolve(projectRoot,'project.json'),p=JSON.parse(await readFile(path,'utf8'));
    p.gameplay.ruleCalendar={daysPerMonth:30};p.visual.timelines=['normal','success','failure'].map(id=>({id,name:id,clips:[
      {id:'first',durationMs:1500,imageAssetId:'visual-courtyard',text:'{activity} · {day} · {outcome}',transition:'fade'},
      {id:'second',durationMs:900,imageAssetId:'visual-library',text:'{changes}'}]}));
    p.visual.activityTimelines=[{activityId:'study',timelineId:'normal',repeat:'logicalDays'},{activityId:'rest',timelineId:'normal'},
      {activityId:'expedition',timelineId:'normal',successTimelineId:'success',failureTimelineId:'failure',repeat:'logicalDays'}];
    await writeFile(path,JSON.stringify(p));
  }
  const built=await buildSnapshot(await loadProject(projectRoot));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const destination=resolve(root,'game');await exportGame({content:built.snapshot,projectRoot,destination,target:'windows',runtimes:{windows:resolve(process.env.NATIVE_PLAYER_OUT??'dist/native-player-app'),web:resolve('dist/web')}});
  return {root,destination,content:built.snapshot};
}
function beginning(activity='study',second='rest'){return [{click:'start'},{click:'schedule-link'},
  {select:'plan-0',value:activity},{select:'plan-1',value:second},{select:'plan-2',value:'rest'},
  {click:'advance'},{click:'advance'},{checkpoint:'result'}];}
async function run(f,name,steps,saveName=name,seed=42){
  const input=resolve(f.root,`${name}-steps.json`),output=resolve(f.root,`${name}-report.json`),save=resolve(f.root,`${saveName}-save.json`);
  await writeFile(input,JSON.stringify(steps));const child=spawnSync(resolve(f.destination,'RaisingPlayer.exe'),['--bundle',f.destination,'--save',save,'--seed',String(seed),'--hidden','--automation',input,'--report',output],{encoding:'utf8',timeout:90000,windowsHide:true});
  assert.equal(child.status,0,child.error?.message??child.stderr);const report=JSON.parse(await readFile(output,'utf8'));assert.equal(report.ok,true);
  return {...report,save,ui:tag=>report.steps.find(s=>s.command.checkpoint===tag).ui};
}

test('native authored activity timeline normal/fast/skip preserve committed results and exact paused save cursor',{timeout:180000},async()=>{
  const f=await fixture(),reports=[];
  for(const mode of ['normal','fast','skip']){
    const steps=beginning();
    if(mode==='normal')steps.push({timelineAdvanceMs:375},{click:'timeline-pause'},{checkpoint:'paused'},{key:'F5'},
      {timelineAdvanceMs:12345},{checkpoint:'still-paused'},{click:'timeline-speed'},{key:'F9'},{checkpoint:'loaded'},
      {screenshot:resolve(f.root,'paused-restored.png')},{click:'timeline-pause'},{timelineAdvanceMs:24000});
    if(mode==='fast')steps.push({click:'timeline-speed'},{timelineAdvanceMs:6000});
    if(mode==='skip')steps.push({click:'timeline-skip'});
    steps.push({checkpoint:'complete'},{click:'timeline-continue'},{key:'F5'},{checkpoint:'continued'});
    const result=await run(f,mode,steps);reports.push(result);
    const before=result.ui('result'),completed=result.ui('complete');
    assert.deepEqual(before.timeline.plan,planActivityTimeline(f.content,before.state));
    assert.equal(before.timeline.plan.clips.length,20);
    assert.equal(before.timeline.plan.clips[0].day,1);assert.equal(before.timeline.plan.clips[18].day,10);
    assert.ok(before.timeline.plan.clips[1].text.length>0);assert.equal(before.timeline.plan.clips[3].text,'');
    assert.deepEqual(completed.state,before.state);assert.equal(completed.timeline.cursor.completed,true);
    assert.equal(completed.timeline.cursor.clipIndex,20);assert.equal(result.ui('continued').timeline,null);
    if(mode==='normal'){
      const paused=result.ui('paused');assert.equal(paused.timeline.cursor.elapsedMs,375);assert.equal(paused.timeline.cursor.paused,true);
      assert.deepEqual(result.ui('still-paused').timeline,paused.timeline);assert.deepEqual(result.ui('loaded').timeline,paused.timeline);
      assert.deepEqual(result.ui('loaded').state,paused.state);
    }
  }
  const saves=await Promise.all(reports.map(async r=>JSON.parse(await readFile(r.save,'utf8'))));
  assert.deepEqual(saves[1],saves[0]);assert.deepEqual(saves[2],saves[0]);
});

test('native timeline save rejects invalid cursor atomically and plays selected success/failure branch',{timeout:180000},async()=>{
  const f=await fixture();
  const paused=await run(f,'paused',[...beginning('expedition','expedition'),{timelineAdvanceMs:225},{click:'timeline-pause'},{key:'F5'},{checkpoint:'saved'}]);
  const saved=JSON.parse(await readFile(paused.save,'utf8')),snapshot=structuredClone(saved);
  assert.equal(saved.playerPresentation.version,1);assert.equal(saved.playerPresentation.timeline.elapsedMs,225);
  const plan=paused.ui('saved').timeline.plan;assert.deepEqual(plan,planActivityTimeline(f.content,saved.state));
  const binding=f.content.project.visual.activityTimelines.find(x=>x.activityId==='expedition');
  assert.equal(plan.timelineId,saved.state.lastActivityResult.success.passed?binding.successTimelineId:binding.failureTimelineId);
  saved.playerPresentation.timeline.elapsedMs=plan.clips[0].durationMs;await writeFile(paused.save,JSON.stringify(saved));
  const rejected=await run(f,'rejected',[{click:'start'},{checkpoint:'before'},{key:'F9'},{checkpoint:'after'}],'paused');
  assert.deepEqual(rejected.ui('after').state,rejected.ui('before').state);assert.equal(rejected.ui('after').timeline,null);assert.match(rejected.ui('after').message,/Invalid timeline cursor/);
  await writeFile(paused.save,JSON.stringify(snapshot));
  const resumed=await run(f,'resumed',[{key:'F9'},{checkpoint:'restored'},{click:'timeline-skip'},{click:'timeline-continue'},
    {click:'advance'},{checkpoint:'second'},{click:'timeline-skip'},{click:'timeline-continue'},{click:'advance'},{checkpoint:'once'}],'paused');
  assert.deepEqual(resumed.ui('restored').timeline.cursor,snapshot.playerPresentation.timeline);
  const second=resumed.ui('second');assert.deepEqual(second.timeline.plan,planActivityTimeline(f.content,second.state));
  assert.equal(second.timeline.plan.timelineId,second.state.lastActivityResult.success.passed?binding.successTimelineId:binding.failureTimelineId);
  assert.equal(resumed.ui('once').state.lastActivityResult.activityId,'rest');assert.equal(resumed.ui('once').timeline.plan.clips.length,2);
  let failureSeed;
  for(let seed=0;seed<100;seed++){
    let state=nativeCreateRun(f.content,seed,{mode:'full'}).state;
    for(const command of [{type:'setSchedule',activityIds:['expedition','rest','rest']},{type:'executeActivity'}]){
      const next=nativeDispatchCommand(f.content,state,command,state.stateRevision);assert.equal(next.ok,true,JSON.stringify(next));state=next.state;
    }
    if(!state.lastActivityResult.success.passed){failureSeed=seed;break;}
  }
  assert.notEqual(failureSeed,undefined);
  const failure=await run(f,'failure',[...beginning('expedition'),{timelineAdvanceMs:450},{checkpoint:'failure',screenshot:resolve(f.root,'failure.png')}],'failure',failureSeed);
  assert.equal(failure.ui('failure').state.lastActivityResult.success.passed,false);
  assert.equal(failure.ui('failure').timeline.plan.timelineId,binding.failureTimelineId);
  assert.deepEqual(failure.ui('failure').timeline.plan,planActivityTimeline(f.content,failure.ui('failure').state));
  const live=await run(f,'realtime',[...beginning('rest'),{timelineRealtime:true},{waitFrames:25},{click:'timeline-pause'},{checkpoint:'live-paused'},
    {waitFrames:25},{checkpoint:'live-still'}]);
  assert.deepEqual(live.ui('live-paused').state,live.ui('result').state);
  assert.ok(live.ui('live-paused').timeline.cursor.elapsedMs>0||live.ui('live-paused').timeline.cursor.clipIndex>0);
  assert.deepEqual(live.ui('live-still').timeline,live.ui('live-paused').timeline);
});

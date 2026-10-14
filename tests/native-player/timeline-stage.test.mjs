import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {planActivityTimeline,timelineLayerFrameIndex} from '../../dist/packages/visual/timeline.js';

const frames=[{imageAssetId:'visual-courtyard',durationMs:100},{imageAssetId:'visual-library',durationMs:200}];
const stage={width:400,height:200,layers:[
  {id:'background',x:0,y:0,width:400,height:200,frames:[{imageAssetId:'visual-library',durationMs:60}]},
  {id:'once',x:-20,y:10,width:100,height:80,frames},
  {id:'loop',x:200,y:30,width:100,height:50,frames,loop:true},
]};
async function fixture(){
  await mkdir(resolve('.cache/native-timeline-stage-tests'),{recursive:true});
  const root=await mkdtemp(resolve('.cache/native-timeline-stage-tests/run-')),projectRoot=resolve(root,'project');
  await cp(resolve('examples/academy-journey'),projectRoot,{recursive:true});
  const file=resolve(projectRoot,'project.json'),project=JSON.parse(await readFile(file,'utf8'));
  project.visual.timelines=[{id:'stage-film',name:'Stage film',clips:[{id:'scene',durationMs:900,imageAssetId:'visual-courtyard',text:'{activity}',stage}]},
    {id:'legacy-film',name:'Legacy image',clips:[{id:'still',durationMs:900,imageAssetId:'visual-library',text:'{activity}'}]}];
  project.visual.activityTimelines=[{activityId:'study',timelineId:'stage-film'},{activityId:'rest',timelineId:'legacy-film'}];
  await writeFile(file,JSON.stringify(project));
  const built=await buildSnapshot(await loadProject(projectRoot));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const destination=resolve(root,'game');
  await exportGame({content:built.snapshot,projectRoot,destination,target:'windows',runtimes:{windows:resolve(process.env.NATIVE_PLAYER_OUT??'dist/native-player-app'),web:resolve('dist/web')}});
  return {root,destination,content:built.snapshot};
}
function beginning(activity='study'){
  return [{click:'start'},{click:'schedule-link'},{select:'plan-0',value:activity},{select:'plan-1',value:'rest'},{select:'plan-2',value:'rest'},
    {click:'advance'},{click:'advance'},{checkpoint:'result'}];
}
async function run(f,name,steps){
  const input=resolve(f.root,`${name}-steps.json`),output=resolve(f.root,`${name}-report.json`),save=resolve(f.root,`${name}-save.json`);
  await writeFile(input,JSON.stringify(steps));
  const child=spawnSync(resolve(f.destination,'RaisingPlayer.exe'),['--bundle',f.destination,'--save',save,'--seed','42','--hidden','--automation',input,'--report',output],
    {encoding:'utf8',timeout:90000,windowsHide:true});
  assert.equal(child.status,0,child.error?.message??child.stderr);
  const report=JSON.parse(await readFile(output,'utf8'));assert.equal(report.ok,true);
  return {...report,save,ui:tag=>report.steps.find(s=>s.command.checkpoint===tag).ui};
}
function framesAt(ui,elapsed){
  assert.ok(ui.timelineStage,'The assertion reads rendered RmlUi layers, not a model-only frame list');
  assert.deepEqual(ui.timelineStage.layers.map(l=>l.id),stage.layers.map(l=>l.id));
  assert.deepEqual(ui.timelineStage.layers.map(l=>l.frameIndex),stage.layers.map(l=>timelineLayerFrameIndex(l,elapsed)));
  assert.deepEqual(ui.timelineStage.layers.map(l=>l.imageAssetId),stage.layers.map(l=>l.frames[timelineLayerFrameIndex(l,elapsed)].imageAssetId));
  for(const layer of ui.timelineStage.layers)assert.match(layer.rml,/decorator:.*image\(.*fill/,'layer images stretch into their destination rectangle');
}
const near=(actual,expected)=>assert.ok(Math.abs(actual-expected)<1,`${actual} should be within one pixel of ${expected}`);

test('native stage uses fitted clipped layers, frame boundaries, and exact paused/completed save cursors',{timeout:120000},async t=>{
  const f=await fixture();t.diagnostic(`Evidence: ${f.root}`);
  const steps=[...beginning(),{timelineAdvanceMs:99},{checkpoint:'99'},{timelineAdvanceMs:1},{checkpoint:'100',screenshot:resolve(f.root,'stage-100.png')},
    {click:'timeline-pause'},{key:'F5'},{checkpoint:'paused'},{timelineAdvanceMs:1000},{checkpoint:'still-paused'},
    {click:'timeline-speed'},{key:'F9'},{checkpoint:'loaded'},{click:'timeline-pause'},
    {timelineAdvanceMs:199},{checkpoint:'299'},{timelineAdvanceMs:1},{checkpoint:'300'},
    {timelineAdvanceMs:600},{checkpoint:'completed',screenshot:resolve(f.root,'stage-completed.png')},{key:'F5'},{key:'F9'},{checkpoint:'completed-loaded'}];
  const report=await run(f,'boundaries',steps),initial=report.ui('result');
  assert.deepEqual(initial.timeline.plan,planActivityTimeline(f.content,initial.state));
  assert.equal(initial.timeline.plan.clips[0].imageAssetId,'visual-courtyard','legacy fallback is retained in the plan');
  assert.deepEqual(initial.timeline.plan.clips[0].stage,stage);
  for(const [tag,elapsed] of [['result',0],['99',99],['100',100],['paused',100],['still-paused',100],['loaded',100],['299',299],['300',300],['completed',900],['completed-loaded',900]]){
    const ui=report.ui(tag);framesAt(ui,elapsed);assert.deepEqual(ui.state,initial.state,'visual frames do not change the committed simulation');
  }
  assert.deepEqual(report.ui('loaded').timeline.cursor,report.ui('paused').timeline.cursor);
  assert.deepEqual(report.ui('completed-loaded').timeline.cursor,report.ui('completed').timeline.cursor);
  assert.equal(report.ui('completed').timeline.cursor.completed,true);
  assert.equal(report.ui('completed').timeline.cursor.elapsedMs,0,'existing completed cursor wire format remains unchanged');
  const rendered=initial.timelineStage,scale=rendered.width/stage.width;
  near(rendered.height,stage.height*scale);assert.equal(rendered.clip,'always');assert.equal(rendered.overflow,'hidden');
  for(let i=0;i<stage.layers.length;i++){
    const actual=rendered.layers[i],source=stage.layers[i];
    near(actual.x,rendered.x+source.x*scale);near(actual.y,rendered.y+source.y*scale);
    near(actual.width,source.width*scale);near(actual.height,source.height*scale);assert.equal(Number(actual.zIndex),i);
  }
  assert.ok(rendered.layers[1].x<rendered.x,'off-stage content retains its placement and is clipped by the stage');
  const saved=JSON.parse(await readFile(report.save,'utf8'));
  assert.deepEqual(saved.playerPresentation.timeline,report.ui('completed').timeline.cursor);
});

test('native stage normal, fast, and skip preserve identical RNG state and command history; legacy images still play',{timeout:120000},async t=>{
  const f=await fixture();t.diagnostic(`Evidence: ${f.root}`);const saves=[];
  for(const mode of ['normal','fast','skip']){
    const steps=beginning();
    if(mode==='skip')steps.push({click:'timeline-skip'});
    else {if(mode==='fast')steps.push({click:'timeline-speed'});steps.push({timelineAdvanceMs:mode==='fast'?225:900});}
    steps.push({checkpoint:'completed'},{click:'timeline-continue'},{key:'F5'},{checkpoint:'continued'});
    const report=await run(f,mode,steps);
    assert.deepEqual(report.ui('completed').state,report.ui('result').state);
    framesAt(report.ui('completed'),900);assert.equal(report.ui('continued').timeline,null);
    saves.push(JSON.parse(await readFile(report.save,'utf8')));
  }
  assert.deepEqual(saves[1],saves[0],'speed cannot change RNG, committed effects, origin, or command history');
  assert.deepEqual(saves[2],saves[0],'skip cannot change RNG, committed effects, origin, or command history');
  const legacy=await run(f,'legacy',[...beginning('rest'),{timelineAdvanceMs:900},{checkpoint:'completed'}]);
  assert.equal(legacy.ui('result').timelineStage,null);
  assert.equal(legacy.ui('result').timeline.plan.clips[0].imageAssetId,'visual-library');
  assert.equal(legacy.ui('completed').timeline.cursor.completed,true);
  assert.deepEqual(legacy.ui('completed').state,legacy.ui('result').state);
});

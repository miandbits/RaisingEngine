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

test('authored gallery: Qt cards, kind/status/category filters and isolated original recollection', {timeout:240000}, async(t)=>{
  const runtime=resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-gallery-ui');
  let projectPath=process.env.RAISING_GALLERY_UI_PROJECT;
  if(!projectPath){try{projectPath=(await readFile('.cache/gallery-ui/editor-project.txt','utf8')).trim();}catch(error){if(error.code!=='ENOENT')throw error;t.skip('Run Qt galleryUiAuthoring or set RAISING_GALLERY_UI_PROJECT to its authored project');return;}}
  const project=resolve(projectPath);
  const loaded=await loadProject(project);const built=await buildSnapshot(loaded);assert.equal(built.ok,true,JSON.stringify(built.diagnostics));const content=built.snapshot;
  assert.equal(content.project.gallery.screenId,'gallery');assert.equal(content.project.gallery.detailScreenId,'gallery-detail');
  const root=await mkdtemp(resolve('.cache/gallery-ui-native-')),game=resolve(root,'game'),evidence=resolve('.cache/gallery-ui');await mkdir(evidence,{recursive:true});
  const capabilities=JSON.parse(await readFile(resolve(runtime,'capabilities.json'),'utf8'));
  const extensions=await exportBundle({lock:loaded.documents.find(d=>d.kind==='pluginLock')?.data,documents:loaded.documents,visual:content.project.visual,target:'windows',assets:{root:project,assets:content.assets,hashes:content.assetHashes},hostFeatures:capabilities.visualFeatures});
  await exportGame({extensions,content,projectRoot:project,destination:game,target:'windows',runtimes:{windows:runtime,web:resolve('dist/web')}});await writeFile(resolve(evidence,'game-path.txt'),game);
  const save=resolve(root,'slot.json');
  async function play(name,steps){const script=resolve(evidence,name+'-steps.json'),report=resolve(evidence,name+'-report.json');await writeFile(script,JSON.stringify(steps));const result=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--seed','42','--save',save,'--automation',script,'--report',report,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:120000});const r=JSON.parse(await readFile(report,'utf8'));assert.equal(result.status,0,JSON.stringify(r.error??result.stderr));assert.equal(r.ok,true);return r;}
  const ui=(r,id)=>r.steps.find(s=>s.command.checkpoint===id).ui;
  const card=id=>'entries__'+id+'__open',detail=(id,child)=>'selection__'+id+'__'+child;
  const categoryButton=content.project.visual.screens.find(s=>s.id==='gallery').elements.find(e=>e.action==='galleryFilter'&&e.galleryFilter==='category:인연');assert.ok(categoryButton,'category button authored through inspector');
  const steps=route(content,'survival').steps.map(s=>s.click==='gallery-open'?{click:'open-gallery'}:s.click==='gallery-close'?{click:'close'}:s);
  steps.splice(3,0,{click:'open-gallery'},{checkpoint:'locked'},{click:'tab-cg'},{checkpoint:'locked-cg'},{click:'status-unlocked'},{checkpoint:'empty'},{click:'reset'},{click:'close'});
  steps.push({key:'F5'},{checkpoint:'live-saved'},{click:'open-gallery'},{checkpoint:'unlocked'},{screenshot:resolve(evidence,'player-gallery.png')},
    {click:'tab-cg'},{checkpoint:'cg-filter'},{click:card('courtyard-cg')},{checkpoint:'cg'},{screenshot:resolve(evidence,'player-cg.png')},{click:'back'},
    {click:'tab-music'},{checkpoint:'music-filter'},{click:card('winter-music')},{click:detail('winter-music','music')},{waitFrames:10},{checkpoint:'music-playing'},
    {click:detail('winter-music','music')},{checkpoint:'music-paused'},{click:detail('winter-music','music')},{click:detail('winter-music','stop')},{checkpoint:'music-stopped'},
    {click:'back'},{click:'tab-ending'},{click:'status-locked'},{checkpoint:'ending-locked'},{click:'status-unlocked'},{checkpoint:'ending-unlocked'},
    {click:'reset'},{click:categoryButton.id},{checkpoint:'category-filter'},{click:'reset'},{click:'tab-event'},{click:card('promise')},{click:detail('promise','replay')},{checkpoint:'replay-first'},{screenshot:resolve(evidence,'player-recollection.png')},
    {click:'recollection-save'},{checkpoint:'replay-saved'},{click:'recollection-next'},{checkpoint:'replay-next'},{click:'recollection-load'},{checkpoint:'replay-loaded'},
    {click:'recollection-close'},{checkpoint:'replay-closed'},{click:'close'},{checkpoint:'live-returned'},{key:'F9'},{checkpoint:'live-loaded'});
  const result=await play('authored',steps);
  const ids=(r,id)=>ui(r,id).elements.filter(e=>e.id.startsWith('entries__')&&e.id.endsWith('__open')).map(e=>e.id);
  assert.equal(ui(result,'locked').elements.find(e=>e.id===card('courtyard-cg')).enabled,false);
  assert.ok(!ids(result,'locked').includes(card('winter-music')),'hidden locked music omitted');
  assert.deepEqual(ids(result,'locked-cg'),[card('courtyard-cg')]);assert.deepEqual(ids(result,'empty'),[]);
  assert.deepEqual(ids(result,'cg-filter'),[card('courtyard-cg')]);assert.deepEqual(ids(result,'music-filter'),[card('winter-music')]);
  assert.deepEqual(ids(result,'ending-locked'),[card('farewell')]);assert.deepEqual(ids(result,'ending-unlocked'),[card('survival')]);
  assert.deepEqual(ids(result,'category-filter'),[card('promise')]);
  assert.equal(ui(result,'music-playing').galleryAudio.playing,true);assert.equal(ui(result,'music-paused').galleryAudio.playing,false);assert.equal(ui(result,'music-stopped').galleryAudio.cursor,0);
  const baseline=ui(result,'unlocked');
  for(const id of ['cg','music-playing','replay-first','replay-next','replay-loaded','replay-closed','live-returned']){
    assert.deepEqual(ui(result,id).state,baseline.state,id+': live state and RNG');assert.deepEqual(ui(result,id).commands,baseline.commands,id+': command history');assert.deepEqual(ui(result,id).gallery.data,baseline.gallery.data,id+': unlock profile');
  }
  assert.deepEqual(ui(result,'live-loaded').state,baseline.state);
  assert.deepEqual(ui(result,'mid-loaded').state,ui(result,'mid-saved').state);
  assert.deepEqual(ui(result,'gallery-paused').state,ui(result,'before-gallery').state);
  for(const id of ['gallery-paused','gallery-closed']){
    assert.deepEqual(ui(result,id).extensions.instances,ui(result,'before-gallery').extensions.instances,id+': live plugin mounts and track state');
    assert.equal(ui(result,id).extensions.clockMs,ui(result,'before-gallery').extensions.clockMs,id+': plugin clock frozen');
  }
  assert.deepEqual(ui(result,'replay-loaded').recollection.state,ui(result,'replay-saved').recollection.state);
  assert.equal(ui(result,'replay-loaded').recollection.cursor,ui(result,'replay-saved').recollection.cursor);
  assert.deepEqual(ui(result,'replay-loaded').recollection.extensions.instances.map(i=>i.items),ui(result,'replay-saved').recollection.extensions.instances.map(i=>i.items),'replay visual pose restored');
  assert.equal(ui(result,'finished').state.endingId,'certified');
  // Reopen a fresh process: common unlocks survive, main save remains loadable.
  const reopen=await play('reopened',[{click:'open-gallery'},{checkpoint:'title-gallery'},{click:'tab-cg'},{click:card('courtyard-cg')},{checkpoint:'title-cg'},{click:'close'},{key:'F9'},{checkpoint:'loaded'}]);
  assert.equal(ui(reopen,'title-gallery').state,null);assert.deepEqual(ui(reopen,'loaded').state,baseline.state);
});

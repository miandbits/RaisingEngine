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

test('authored recollection controls preserve live state, saves, recorded choices and current styling', {timeout:240000}, async()=>{
  const runtime=resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player');
  const project=resolve(process.env.RAISING_REPLAY_UI_PROJECT??'examples/last-grandson-polished');
  const loaded=await loadProject(project),built=await buildSnapshot(loaded);assert.equal(built.ok,true,JSON.stringify(built.diagnostics));const content=built.snapshot;
  assert.ok(content.project.gallery.replayControlsScreenId,'Use the Qt-authored sample');
  const root=await mkdtemp(resolve('.cache/replay-ui-native-')),game=resolve(root,'game'),evidence=resolve('.cache/replay-ui');await mkdir(evidence,{recursive:true});
  const capabilities=JSON.parse(await readFile(resolve(runtime,'capabilities.json'),'utf8'));
  const extensions=await exportBundle({lock:loaded.documents.find(d=>d.kind==='pluginLock')?.data,documents:loaded.documents,visual:content.project.visual,target:'windows',assets:{root:project,assets:content.assets,hashes:content.assetHashes},hostFeatures:capabilities.visualFeatures});
  const pack=async(c,dest)=>exportGame({extensions,content:c,projectRoot:project,destination:dest,target:'windows',runtimes:{windows:runtime,web:resolve('dist/web')}});
  await pack(content,game);await writeFile(resolve(evidence,'game-path.txt'),game);
  const save=resolve(root,'slot.json'),profile=resolve(root,`collection-${content.project.gameId}.json`);
  async function play(name,steps,directory=game){const script=resolve(evidence,name+'-steps.json'),report=resolve(evidence,name+'-report.json');await writeFile(script,JSON.stringify(steps));const result=spawnSync(resolve(directory,'RaisingPlayer.exe'),['--bundle',directory,'--seed','42','--save',save,'--automation',script,'--report',report,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:120000});const r=JSON.parse(await readFile(report,'utf8'));assert.equal(result.status,0,JSON.stringify(r.error??result.stderr));assert.equal(r.ok,true);return r;}
  const ui=(r,id)=>r.steps.find(s=>s.command.checkpoint===id).ui;
  const button=id=>'replay-ui__'+id;
  const routeSteps=route(content,'survival').steps;routeSteps.push({key:'F5'},{checkpoint:'live-saved'});
  const initial=await play('record',routeSteps);assert.equal(ui(initial,'finished').state.endingId,'certified');
  const saveBefore=await readFile(save,'utf8'),profileBefore=await readFile(profile,'utf8');
  const result=await play('controls',[
    {key:'F9'},{click:'gallery-open'},{checkpoint:'baseline'},{click:'gallery-entry-promise'},{click:'gallery-replay'},
    {checkpoint:'first'},{screenshot:resolve(evidence,'replay-first.png')},
    {click:button('save')},{checkpoint:'saved'},{click:button('next')},{checkpoint:'next'},
    {click:button('previous')},{checkpoint:'previous'},{click:button('next')},{click:button('load')},{checkpoint:'loaded'},
    {screenshot:resolve(evidence,'replay-loaded.png')},{click:button('close')},{checkpoint:'closed'},
    {click:'gallery-replay'},{click:button('next')},{click:button('next')},{checkpoint:'finished-replay'},
    {click:'gallery-close'},{checkpoint:'returned'},{key:'F9'},{checkpoint:'live-loaded'}]);
  const baseline=ui(result,'baseline');
  for(const id of ['first','saved','next','previous','loaded','closed','finished-replay','returned']){
    const current=ui(result,id);assert.deepEqual(current.state,baseline.state,id+': main state/RNG');assert.deepEqual(current.commands,baseline.commands,id+': command history');assert.deepEqual(current.gallery.data,baseline.gallery.data,id+': unlock profile');
    assert.deepEqual(current.extensions.instances,baseline.extensions.instances,id+': live animation instances');
  }
  assert.equal(ui(result,'first').elements.find(e=>e.id===button('previous')).enabled,false);
  assert.ok(!ui(result,'first').elements.some(e=>e.id==='recollection-close'),'fixed toolbar is replaced');
  assert.equal(ui(result,'next').recollection.cursor,1);assert.equal(ui(result,'previous').recollection.cursor,0);assert.equal(ui(result,'loaded').recollection.cursor,0);
  assert.deepEqual(ui(result,'loaded').recollection.state,ui(result,'saved').recollection.state);
  assert.deepEqual(ui(result,'loaded').recollection.extensions.instances.map(i=>i.items),ui(result,'saved').recollection.extensions.instances.map(i=>i.items));
  assert.equal(ui(result,'closed').recollection.active,false);assert.equal(ui(result,'finished-replay').recollection.active,false);
  assert.deepEqual(ui(result,'live-loaded').state,baseline.state);
  assert.equal(await readFile(save,'utf8'),saveBefore);assert.equal(await readFile(profile,'utf8'),profileBefore);
  // Existing records must use today's authored controls while retaining historical scenes.
  const changed=structuredClone(content),screen=changed.project.visual.screens.find(s=>s.id===changed.project.gallery.replayControlsScreenId);
  screen.elements.find(e=>e.id==='next').text='Updated replay UI';screen.elements.find(e=>e.id==='next').fill='#852c66';
  const updated=resolve(root,'updated');await pack(changed,updated);
  const reopened=await play('updated-controls',[{click:'gallery-open'},{click:'gallery-entry-promise'},{click:'gallery-replay'},{click:button('load')},{checkpoint:'resumed'},{screenshot:resolve(evidence,'replay-updated.png')},{click:button('close')}],updated);
  assert.equal(ui(reopened,'resumed').state,null);assert.equal(ui(reopened,'resumed').recollection.cursor,0);assert.equal(ui(reopened,'resumed').elements.find(e=>e.id===button('next')).label,'Updated replay UI');
  assert.equal(await readFile(save,'utf8'),saveBefore);assert.equal(await readFile(profile,'utf8'),profileBefore);
  // Controls placed on an ordinary gameplay screen never dispatch replay commands.
  changed.project.visual.screens.find(s=>s.id==='title').elements.push({...screen.elements.find(e=>e.id==='save'),id:'inactive-replay-save'});
  const inactive=resolve(root,'inactive');await pack(changed,inactive);const idle=await play('inactive',[{checkpoint:'title'}],inactive);assert.equal(ui(idle,'title').elements.find(e=>e.id==='inactive-replay-save').enabled,false);
  const replaySave=resolve(root,`recollection-${content.project.gameId}.json`),replayBefore=await readFile(replaySave,'utf8');
  await writeFile(replaySave,'{"broken":true}');
  const corrupt=await play('corrupt-replay',[{key:'F9'},{click:'gallery-open'},{click:'gallery-entry-promise'},{click:'gallery-replay'},{checkpoint:'before'},{click:button('load')},{checkpoint:'failed'},{click:button('close')}]);
  assert.ok(ui(corrupt,'failed').recollection.notice);assert.deepEqual(ui(corrupt,'failed').recollection.state,ui(corrupt,'before').recollection.state);assert.deepEqual(ui(corrupt,'failed').state,ui(corrupt,'before').state);
  await writeFile(replaySave,replayBefore);assert.equal(await readFile(save,'utf8'),saveBefore);assert.equal(await readFile(profile,'utf8'),profileBefore);
  await writeFile(resolve(evidence,'save-path.txt'),save);
});

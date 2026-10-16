import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile,rename,cp,rmdir} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync,spawn} from 'node:child_process';
import Ajv2020 from 'ajv/dist/2020.js';
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


test('media gallery: Qt-authored unlocks, CG, music, original presentation and isolated replay saves', {timeout:240000}, async()=>{
  const runtime=resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player');
  const project=resolve(process.env.RAISING_GALLERY_MEDIA_PROJECT??'examples/last-grandson-polished');
  const loaded=await loadProject(project);
  // Keep this suite as the compatibility contract for projects with the built-in replay toolbar.
  delete loaded.documents.find(d=>d.kind==='project').data.gallery.replayControlsScreenId;
  const snapshot=await buildSnapshot(loaded);assert.equal(snapshot.ok,true,JSON.stringify(snapshot.diagnostics));const content=snapshot.snapshot;
  assert.equal(content.project.gallery.entries.length,5);
  assert.equal(content.project.gallery.entries.find(e=>e.id==='courtyard-cg').unlockCondition.op,'compare');
  const root=await mkdtemp(resolve('.cache/gallery-media-native-')),game=resolve(root,'game'),evidence=resolve('.cache/gallery-media');await mkdir(evidence,{recursive:true});
  const capabilities=JSON.parse(await readFile(resolve(runtime,'capabilities.json'),'utf8'));
  const extensions=await exportBundle({lock:loaded.documents.find(d=>d.kind==='pluginLock')?.data,documents:loaded.documents,visual:content.project.visual,target:'windows',assets:{root:project,assets:content.assets,hashes:content.assetHashes},hostFeatures:capabilities.visualFeatures});
  await exportGame({extensions,content,projectRoot:project,destination:game,target:'windows',runtimes:{windows:runtime,web:resolve('dist/web')}});await writeFile(resolve(evidence,'game-path.txt'),game);
  const save=resolve(root,'slot.json'),profile=resolve(root,`collection-${content.project.gameId}.json`);
  async function play(name,steps,gameDirectory=game,saveFile=save){
    const script=resolve(evidence,name+'-steps.json'),reportFile=resolve(evidence,name+'-report.json');await writeFile(script,JSON.stringify(steps));
    const result=spawnSync(resolve(gameDirectory,'RaisingPlayer.exe'),['--bundle',gameDirectory,'--seed','42','--save',saveFile,'--automation',script,'--report',reportFile,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:90000});
    const r=JSON.parse(await readFile(reportFile,'utf8'));assert.equal(result.status,0,JSON.stringify(r.error??result.stderr));assert.equal(r.ok,true);return r;
  }
  const ui=(r,id)=>r.steps.find(s=>s.command.checkpoint===id).ui;
  const steps=route(content,'survival').steps;
  steps.splice(3,0,{click:'gallery-open'},{checkpoint:'locked'},{click:'gallery-close'});
  steps.push({key:'F5'},{checkpoint:'live-saved'},{click:'gallery-open'},{checkpoint:'unlocked'},
    {click:'gallery-entry-courtyard-cg'},{checkpoint:'cg'},{screenshot:resolve(evidence,'cg.png')},{click:'gallery-back'},
    {click:'gallery-entry-winter-music'},{click:'gallery-music-play'},{waitFrames:10},{checkpoint:'music-playing'},{click:'gallery-music-play'},{waitFrames:10},{checkpoint:'music-paused'},{click:'gallery-music-play'},{click:'gallery-music-stop'},{checkpoint:'music-stopped'},{screenshot:resolve(evidence,'music.png')},{click:'gallery-back'},
    {click:'gallery-entry-promise'},{click:'gallery-replay'},{checkpoint:'replay-first'},{screenshot:resolve(evidence,'original-recollection.png')},
    {click:'recollection-save'},{checkpoint:'replay-saved'},{click:'recollection-next'},{checkpoint:'replay-choice'},
    {click:'recollection-load'},{checkpoint:'replay-loaded'},{click:'recollection-next'},{click:'recollection-close'},{checkpoint:'replay-closed'},
    {click:'gallery-close'},{checkpoint:'live-returned'},{key:'F9'},{checkpoint:'live-loaded'});
  const result=await play('media-recollection',steps);
  assert.equal(ui(result,'locked').elements.find(e=>e.id==='gallery-entry-courtyard-cg').enabled,false);
  assert.ok(!ui(result,'locked').elements.some(e=>e.id==='gallery-entry-winter-music'),'hidden entries do not leak titles');
  assert.ok(ui(result,'cg').gallery.data.entries['courtyard-cg']);
  assert.equal(ui(result,'music-playing').galleryAudio.playing,true);assert.ok(ui(result,'music-playing').galleryAudio.loaded>0);
  assert.equal(ui(result,'music-paused').galleryAudio.playing,false);assert.equal(ui(result,'music-stopped').galleryAudio.cursor,0);
  const baseline=ui(result,'unlocked');
  for(const id of ['cg','music-playing','replay-first','replay-choice','replay-loaded','replay-closed']){
    assert.deepEqual(ui(result,id).state,baseline.state,`${id}: live state including RNG`);
    assert.deepEqual(ui(result,id).commands,baseline.commands,`${id}: live command history`);
    assert.deepEqual(ui(result,id).gallery.data,baseline.gallery.data,`${id}: no unlock or reward writes`);
    assert.equal(ui(result,id).audio.musicCursor,baseline.audio.musicCursor,`${id}: original BGM position paused`);
  }
  assert.equal(ui(result,'replay-first').screenId,'story');
  assert.equal(ui(result,'replay-first').recollection.active,true);
  assert.equal(ui(result,'replay-choice').recollection.cursor,1);
  assert.deepEqual(ui(result,'replay-saved').recollection.state,ui(result,'replay-loaded').recollection.state);
  assert.equal(ui(result,'replay-loaded').recollection.cursor,0);
  assert.deepEqual(ui(result,'live-returned').state,baseline.state);assert.deepEqual(ui(result,'live-loaded').state,baseline.state);
  assert.deepEqual(ui(result,'replay-first').extensions.instances,ui(result,'replay-closed').extensions.instances,'private DLL must not mutate live plugin globals');
  const record=baseline.gallery.data.entries.promise.records[0];
  assert.equal(record.replayFrames.length,2);assert.ok(record.replayFrames[0].extensions,'mid-event save retains original animation state');
  const saved=JSON.parse(await readFile(save,'utf8'));assert.deepEqual(saved.state,baseline.state);assert.deepEqual(saved.commands,baseline.commands);
  const stored=await readFile(profile,'utf8');
  const ajv=new Ajv2020({strict:false});ajv.addSchema(JSON.parse(await readFile('packages/schema/common.schema.json','utf8')));
  const validate=ajv.compile(JSON.parse(await readFile('packages/schema/gallery-profile.schema.json','utf8')));assert.equal(validate(JSON.parse(stored)),true,JSON.stringify(validate.errors));
  const reopened=await play('restart-recollection',[{click:'gallery-open'},{click:'gallery-entry-promise'},{click:'gallery-replay'},{key:'F9'},{checkpoint:'resumed'},{click:'recollection-close'},{click:'gallery-close'}]);
  assert.equal(ui(reopened,'resumed').state,null,'recollection available from title without creating a run');assert.equal(ui(reopened,'resumed').recollection.cursor,0);
  assert.equal(await readFile(profile,'utf8'),stored);assert.deepEqual(JSON.parse(await readFile(save,'utf8')),saved);
  // Legacy text-only profiles remain readable. They do not invent original scene data.
  const legacy=JSON.parse(stored);for(const e of Object.values(legacy.entries))for(const r of e.records){delete r.replayFrames;delete r.content;delete r.extensionManifest;}
  await writeFile(profile,JSON.stringify(legacy));const old=await play('legacy-profile',[{click:'gallery-open'},{click:'gallery-entry-promise'},{checkpoint:'legacy'}]);assert.ok(!ui(old,'legacy').elements.some(e=>e.id==='gallery-replay'));
  await writeFile(profile,stored);
  // Changed original media fails closed; transcript remains available and main save is untouched.
  const changed=JSON.parse(stored);changed.entries.promise.records[0].content.assetHashes={changed:'hash'};await writeFile(profile,JSON.stringify(changed));
  const mismatch=await play('asset-mismatch',[{click:'gallery-open'},{click:'gallery-entry-promise'},{click:'gallery-replay'},{checkpoint:'mismatch'}]);assert.equal(ui(mismatch,'mismatch').recollection.active,false);assert.match(ui(mismatch,'mismatch').gallery.error,/에셋/);
  await writeFile(profile,stored);await cp(profile,resolve(evidence,'collection-review.json'));await cp(save,resolve(evidence,'slot-review.json'));
  // Additional engine fixture (not claimed as GUI authorship): scene commands before
  // dialogue AND after the final choice, including BGM, SFX and portrait transforms.
  const event=loaded.documents.find(d=>d.kind==='event'&&d.data.id==='exam-invitation').data;
  const hero=content.characters.find(c=>c.id===content.project.protagonistId);
  loaded.documents.find(d=>d.kind==='character'&&d.data.id===hero.id).data.portraits['gallery']='seol-neutral';
  event.entryNodeId='opening-scene';
  event.nodes.unshift({id:'opening-scene',type:'scene',scene:{backgroundAssetId:'winter-courtyard',musicAssetId:'winter-motif',soundAssetId:'winter-motif',portraits:[{characterId:hero.id,portraitId:'gallery',position:'center',scale:0.8,offsetX:0.1}]},next:'intro'});
  for(const choice of event.nodes.find(n=>n.type==='choice').choices)choice.next='closing-scene';
  event.nodes.push({id:'closing-scene',type:'scene',scene:{backgroundAssetId:'seol-rest',musicAssetId:null,soundAssetId:'winter-motif',portraits:[]},next:'end'});
  const sceneSnapshot=await buildSnapshot(loaded);assert.equal(sceneSnapshot.ok,true,JSON.stringify(sceneSnapshot.diagnostics));
  const sceneGame=resolve(root,'scene-game'),sceneSave=resolve(root,'scene-profile','slot.json');
  await exportGame({extensions,content:sceneSnapshot.snapshot,projectRoot:project,destination:sceneGame,target:'windows',runtimes:{windows:runtime,web:resolve('dist/web')}});
  const sceneSteps=route(sceneSnapshot.snapshot,'survival').steps;
  sceneSteps.push({click:'gallery-open'},{click:'gallery-entry-promise'},{click:'gallery-replay'},{checkpoint:'scene-first'},{click:'recollection-next'},{click:'recollection-next'},{checkpoint:'scene-final'},{click:'recollection-close'},{checkpoint:'scene-return'});
  const scenes=await play('scene-recollection',sceneSteps,sceneGame,sceneSave);
  assert.equal(ui(scenes,'scene-first').preview.displayScene.backgroundAssetId,'winter-courtyard');
  assert.equal(ui(scenes,'scene-first').preview.displayScene.portraits[0].scale,0.8);
  assert.ok(ui(scenes,'scene-first').galleryAudio.loaded>=2,'original BGM and SFX loaded separately');
  assert.equal(ui(scenes,'scene-final').preview.displayScene.backgroundAssetId,'seol-rest');
  assert.equal(ui(scenes,'scene-final').preview.displayScene.musicAssetId,null);
  assert.deepEqual(ui(scenes,'scene-first').state,ui(scenes,'scene-return').state);
});

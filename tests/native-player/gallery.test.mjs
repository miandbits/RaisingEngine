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

test('gallery: actual UI, both endings, immutable recollection, old saves, restart and corrupt profile', {timeout:240000}, async()=>{
  const runtime=resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player');
  const project=resolve(process.env.RAISING_GALLERY_PROJECT??'tests/fixtures/projects/last-grandson-polished');
  const loaded=await loadProject(project);
  // Retain the original narrative-only contract as a compatibility fixture even
  // when the public sample grows CG/music definitions.
  loaded.documents.find(d=>d.kind==='project').data.gallery.entries=loaded.documents.find(d=>d.kind==='project').data.gallery.entries.filter(e=>e.kind==='event'||e.kind==='ending');
  const snapshot=await buildSnapshot(loaded);assert.equal(snapshot.ok,true,JSON.stringify(snapshot.diagnostics));const content=snapshot.snapshot;
  assert.equal(content.project.gallery.entries.length,3);
  const root=await mkdtemp(resolve('.cache/gallery-native-')),game=resolve(root,'game'),evidence=resolve('.cache/gallery');await mkdir(evidence,{recursive:true});
  const capabilities=JSON.parse(await readFile(resolve(runtime,'capabilities.json'),'utf8'));
  await assert.rejects(exportGame({content,projectRoot:project,destination:resolve(root,'unsupported-web'),target:'web',runtimes:{windows:runtime,web:resolve('dist/web')}}),/narrative-gallery-v1/);
  const extensions=await exportBundle({lock:loaded.documents.find(d=>d.kind==='pluginLock')?.data,documents:loaded.documents,visual:content.project.visual,target:'windows',assets:{root:project,assets:content.assets,hashes:content.assetHashes},hostFeatures:capabilities.visualFeatures});
  await exportGame({extensions,content,projectRoot:project,destination:game,target:'windows',runtimes:{windows:runtime,web:resolve('dist/web')}});await writeFile(resolve(evidence,'game-path.txt'),game);
  const save=resolve(root,'slot.json'),profile=resolve(root,`collection-${content.project.gameId}.json`);
  async function play(name,steps,saveFile=save){
    const script=resolve(evidence,name+'-steps.json'),reportFile=resolve(evidence,name+'-report.json');await writeFile(script,JSON.stringify(steps));
    const result=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--seed','42','--save',saveFile,'--automation',script,'--report',reportFile,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:90000});
    const r=JSON.parse(await readFile(reportFile,'utf8'));assert.equal(result.status,0,JSON.stringify(r.error??result.stderr));assert.equal(r.ok,true);return r;
  }
  const ui=(r,id)=>r.steps.find(s=>s.command.checkpoint===id).ui;
  const survival=route(content,'survival');
  survival.steps.push({click:'gallery-open'},{checkpoint:'unlocked'},{screenshot:resolve(evidence,'gallery-unlocked.png')},{click:'gallery-entry-promise'},{checkpoint:'memory-first'},{click:'gallery-next'},{checkpoint:'memory-choice'},{screenshot:resolve(evidence,'gallery-recollection.png')},{click:'gallery-next'},{checkpoint:'memory-stats'},{click:'gallery-close'},{checkpoint:'after-memory'},{key:'F9'},{checkpoint:'old-save'},{click:'gallery-open'},{checkpoint:'after-old-save'},{click:'gallery-close'});
  const first=await play('survival',survival.steps);
  assert.deepEqual(ui(first,'finished').state,survival.state);
  for(const id of ['gallery-paused','gallery-closed']){assert.deepEqual(ui(first,id).state,ui(first,'before-gallery').state);assert.deepEqual(ui(first,id).timeline,ui(first,'before-gallery').timeline);}
  assert.deepEqual(ui(first,'gallery-paused').extensions.instances[0].state,ui(first,'before-gallery').extensions.instances[0].state,'SD is frozen while browsing');
  assert.deepEqual(ui(first,'mid-saved').gallery.data.entries,{},'an unfinished story must remain locked');
  assert.deepEqual(ui(first,'mid-loaded').state,ui(first,'mid-saved').state);
  assert.deepEqual(ui(first,'after-memory').state,survival.state);
  const records=ui(first,'unlocked').gallery.data.entries;assert.deepEqual(Object.keys(records).sort(),['promise','survival']);
  assert.equal(records.promise.records[0].frames.length,2,'mid-event save must retain earlier dialogue');
  assert.equal(records.promise.records[0].frames[1].choiceId,'register');assert.ok(records.promise.records[0].frames[1].text.length>0);
  assert.ok(!ui(first,'unlocked').elements.find(e=>e.id==='gallery-entry-farewell').enabled);
  assert.deepEqual(ui(first,'after-old-save').gallery.data,ui(first,'unlocked').gallery.data,'loading old run cannot erase records');
  const farewell=route(content,'farewell');farewell.steps.push({click:'gallery-open'},{checkpoint:'all'},{click:'gallery-entry-promise'},{click:'gallery-variant'},{checkpoint:'second-variant'},{click:'gallery-close'});
  const second=await play('farewell',farewell.steps);assert.deepEqual(ui(second,'finished').state,farewell.state);
  assert.deepEqual(Object.keys(ui(second,'all').gallery.data.entries).sort(),['farewell','promise','survival']);
  assert.equal(ui(second,'all').gallery.data.entries.promise.records.length,2);
  assert.equal(ui(second,'second-variant').gallery.recordIndex,1);
  const stored=await readFile(profile,'utf8');
  const ajv=new Ajv2020({strict:false});ajv.addSchema(JSON.parse(await readFile(resolve('packages/schema/common.schema.json'),'utf8')));
  const validate=ajv.compile(JSON.parse(await readFile(resolve('packages/schema/gallery-profile.schema.json'),'utf8')));assert.equal(validate(JSON.parse(stored)),true,JSON.stringify(validate.errors));
  await play('repeat-ending',route(content,'survival').steps);assert.equal(await readFile(profile,'utf8'),stored,'repeating the same branch must not duplicate or replace the first record');
  const reopened=await play('restart',[{click:'gallery-open'},{checkpoint:'title-gallery'},{click:'gallery-entry-survival'},{click:'gallery-next'},{checkpoint:'ending-memory'},{click:'gallery-close'},{click:'start'},{checkpoint:'new-game'}]);
  assert.equal(ui(reopened,'title-gallery').state,null);assert.deepEqual(reopened.final.gallery.data,JSON.parse(stored));
  assert.equal(await readFile(profile,'utf8'),stored,'viewing records and new game do not rewrite them');
  // Aborted writes with the main file missing recover from the atomic backup.
  await rename(profile,profile+'.saved');await writeFile(profile+'.bak',stored);
  const recovered=await play('backup-recovery',[{click:'gallery-open'}]);assert.deepEqual(recovered.final.gallery.data,JSON.parse(stored));
  // Malformed input remains intact; failures are visible, no silent reset or overwrite.
  await writeFile(profile,'{broken');const corrupt=await play('corrupt-profile',[{click:'gallery-open'}]);assert.ok(corrupt.final.gallery.error);assert.equal(await readFile(profile,'utf8'),'{broken');
  await writeFile(profile,stored);
  // Keep a review profile with both endings without altering any user's normal profile.
  await cp(profile,resolve(evidence,'collection-review.json'));
  // Retry pending unlocks in the SAME process after a transient cross-process lock.
  const lockedRoot=resolve(root,'locked');await mkdir(lockedRoot);
  const lock=resolve(lockedRoot,`collection-${content.project.gameId}.json.lock`);await mkdir(lock);
  const retrySteps=route(content,'survival').steps;
  retrySteps.push({click:'gallery-open'},{checkpoint:'write-failed'},{waitFrames:120},{click:'gallery-close'},{click:'gallery-open'},{checkpoint:'write-retried'});
  const retryScript=resolve(evidence,'write-retry-steps.json'),retryReport=resolve(evidence,'write-retry-report.json');await writeFile(retryScript,JSON.stringify(retrySteps));
  let unlocked=false,unlockPromise;
  const status=await new Promise((resolveExit,reject)=>{
    const child=spawn(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',resolve(lockedRoot,'slot.json'),'--automation',retryScript,'--report',retryReport,'--editor-events','--hidden'],{windowsHide:true,stdio:['ignore','pipe','pipe']});
    let buffer='';child.stderr.resume();child.stdout.on('data',chunk=>{
      buffer+=chunk.toString();let end;
      while((end=buffer.indexOf('\n'))>=0){const line=buffer.slice(0,end);buffer=buffer.slice(end+1);let event;try{event=JSON.parse(line);}catch{continue;}
        if(!unlocked&&event.event==='editor-view'&&event.view?.gallery?.active&&event.view.gallery.error&&event.view.state?.phase==='finished'){unlocked=true;unlockPromise=rmdir(lock);}
      }
    });
    const timer=setTimeout(()=>{child.kill();reject(new Error('gallery retry process timed out'));},60000);
    child.on('error',reject);child.on('exit',code=>{clearTimeout(timer);resolveExit(code);});
  });
  await unlockPromise;assert.equal(status,0);assert.equal(unlocked,true);
  const retry=JSON.parse(await readFile(retryReport,'utf8'));assert.equal(retry.ok,true);
  assert.ok(ui(retry,'write-failed').gallery.error);assert.equal(ui(retry,'write-retried').gallery.error,'');
  assert.deepEqual(Object.keys(ui(retry,'write-retried').gallery.data.entries).sort(),['promise','survival']);
});

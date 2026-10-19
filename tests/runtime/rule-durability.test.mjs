import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {baseProject,addRules,json} from '../rule-extensions/helpers.mjs';
import {loadProject,buildSnapshot} from '../../dist/packages/project-service/index.js';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/index.js';
import {encodeSave,decodeSave,loadRun} from '../../dist/packages/runtime-service/save.js';
import {previewSaveMigration,publishMigratedSave} from '../../dist/packages/runtime-service/migration.js';
import {commandAttempts} from '../../dist/packages/analysis-service/runner.js';
import {content,doc,start,send} from './helpers.mjs';

const snapshot=async root=>{const r=await buildSnapshot(await loadProject(root));assert.ok(r.ok,JSON.stringify(r.diagnostics));return r.snapshot;};
const bump=value=>{
  if(!value||typeof value!=='object')return;
  if(value.packageId==='dojo'||value.id==='dojo')value.version='1.1.0';
  for(const v of Object.values(value))bump(v);
};

test('rule update migrates costs/effects explicitly; removed or unaffordable commands recover without overwriting',async()=>{
  const from=await baseProject();await addRules(from);const source=await snapshot(from);
  let session=createSession(source,91,{mode:'full'}).session;
  for(const name of ['prepare','practice','complete']){
    const r=dispatchSession(source,session,{type:'executeAction',actionId:`rx-flow-1-${name}`,quantity:1},session.state.stateRevision);
    assert.ok(r.ok,JSON.stringify(r.error));session=r.session;
  }
  const saved=await encodeSave(source,session);assert.ok(saved.ok);
  await mkdir('.cache/durability',{recursive:true});const evidence=await mkdtemp(resolve('.cache/durability/rule-migration-'));
  const to=join(evidence,'updated');await cp(from,to,{recursive:true});
  const project=await json(join(to,'project.json'));bump(project.ruleExtensions);
  project.ruleExtensions.progressions[0].args.gain=15;
  project.ruleExtensions.packages[0].progressions[0].transitions.find(t=>t.id==='practice').costs[0].amount=4;
  await writeFile(join(to,'project.json'),JSON.stringify(project,null,2));const target=await snapshot(to);
  assert.equal((await decodeSave(target,saved.text)).error.code,'CONTENT_MISMATCH');
  const preview=await previewSaveMigration(source,target,saved.text);assert.ok(preview.ok,JSON.stringify(preview));
  const change=path=>preview.report.changes.find(c=>c.path===path);
  assert.equal(change('/attributes/skill').after-change('/attributes/skill').before,3);
  assert.equal(change('/attributes/money').after-change('/attributes/money').before,-1);
  const original=join(evidence,'original.json'),migrated=join(evidence,'migrated.json');await writeFile(original,saved.text);
  const published=await publishMigratedSave(migrated,source,target,saved.text,preview.report.approval);assert.ok(published.ok,JSON.stringify(published));
  const resumed=await loadRun(migrated,target);assert.ok(resumed.ok);
  assert.deepEqual(resumed.session.commands,session.commands);assert.deepEqual(resumed.session.state.rng,session.state.rng);
  const restarted=dispatchSession(target,resumed.session,{type:'executeAction',actionId:'rx-flow-1-restart',quantity:1},resumed.session.state.stateRevision);assert.ok(restarted.ok);
  const roundtrip=await encodeSave(target,restarted.session);assert.ok(roundtrip.ok);assert.deepEqual((await decodeSave(target,roundtrip.text)).session,restarted.session);
  assert.equal((await publishMigratedSave(original,source,target,saved.text,preview.report.approval)).error.code,'MIGRATION_IO');
  const altered=await loadProject(to);altered.documents.find(d=>d.kind==='project').data.ruleExtensions.progressions[0].args.gain=16;
  const alteredBuild=await buildSnapshot(altered);assert.ok(alteredBuild.ok);
  assert.equal((await publishMigratedSave(join(evidence,'stale.json'),source,alteredBuild.snapshot,saved.text,preview.report.approval)).error.code,'MIGRATION_STALE');
  const failures=[];
  for(const mode of ['removed','unaffordable']){
    const changed=await loadProject(to),pack=changed.documents.find(d=>d.kind==='project').data.ruleExtensions.packages[0];
    if(mode==='removed')pack.progressions[0].transitions=pack.progressions[0].transitions.filter(t=>t.id!=='practice');
    else pack.progressions[0].transitions.find(t=>t.id==='practice').costs[0].amount=1000;
    const built=await buildSnapshot(changed);assert.ok(built.ok,JSON.stringify(built.diagnostics));
    const result=await previewSaveMigration(source,built.snapshot,saved.text);assert.equal(result.error.code,'MIGRATION_REPLAY');assert.equal(result.error.path,'/commands/1');failures.push({mode,...result.error});
  }
  assert.equal(await readFile(original,'utf8'),saved.text);assert.deepEqual((await loadRun(original,source)).session,session);
  const fixture={from,to,save:original,output:join(evidence,'qt-migrated.json'),evidence};
  await writeFile('.cache/durability/stage4-migration-ui.json',JSON.stringify(fixture,null,2));
  await writeFile(join(evidence,'report.json'),JSON.stringify({ok:true,changes:preview.report.changes,failures,originalPreserved:true},null,2));
});

test('240-month rule-driven campaign restores exact state, RNG and commands through the ending',{timeout:240000},async()=>{
  const pack=await json('examples/rule-extension-lab/dojo.raiserules');
  const snapshot=await content(p=>{
    const project=doc(p,'project');project.durationMonths=240;project.ruleExtensions={apiVersion:1,packages:[pack],progressions:[]};
    doc(p,'activity','rest').baseEffects.push({op:'ruleEffect',packageId:'dojo',version:'1.0.0',ruleId:'practice',args:{stat:'skill',gain:1}});
  });
  let live=start(snapshot),restored=live;const checkpoints=[],began=performance.now();let nextSave=200;
  while(live.state.phase!=='finished'&&live.commands.length<10000){
    const candidates=live.state.phase==='planning'?[[{type:'setSchedule',activityIds:['rest','rest','rest']}]]:commandAttempts(snapshot,live.state);
    let accepted;
    for(const commands of candidates){let candidate=live;let valid=true;for(const c of commands){const r=dispatchSession(snapshot,candidate,c,candidate.state.stateRevision);if(!r.ok){valid=false;break;}candidate=r.session;}if(valid&&candidate!==live){accepted=commands;break;}}
    assert.ok(accepted,`no progress at ${live.state.phase}`);
    for(const c of accepted){live=send(snapshot,live,c);restored=send(snapshot,restored,c);}
    if(live.commands.length>=nextSave||live.state.phase==='finished'){
      const saved=await encodeSave(snapshot,restored);assert.ok(saved.ok);const loaded=await decodeSave(snapshot,saved.text);assert.ok(loaded.ok);restored=loaded.session;
      assert.deepEqual(restored,live);checkpoints.push({commands:live.commands.length,period:live.state.periodIndex,bytes:Buffer.byteLength(saved.text)});nextSave+=200;
    }
  }
  assert.equal(live.state.phase,'finished');assert.equal(live.state.periodIndex,719);assert.equal(live.state.attributes.skill,100);assert.deepEqual(live,restored);assert.ok(checkpoints.length>=15);
  await mkdir('.cache/durability',{recursive:true});
  await writeFile('.cache/durability/stage4-rule-campaign.json',JSON.stringify({ok:true,months:240,commands:live.commands.length,ending:live.state.endingId,checkpoints,elapsedMs:performance.now()-began},null,2));
});

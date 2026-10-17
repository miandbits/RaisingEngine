import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdtemp} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/index.js';

const authoring=process.env.RAISING_INTEGRATED_AUTHORING;
test('Qt-authored data -> activity/action -> state -> screen/presentation, real Windows save and replay', {skip:!authoring,timeout:180000},async()=>{
  const info=JSON.parse(await readFile(authoring,'utf8')),service=new QtStudioService();
  await service.handle({method:'open',params:{root:info.root}});
  const {snapshot:content}=await service.editor.snapshot(service.editor.revision);
  assert.ok(content,'GUI-authored project must validate before export');
  const output=await mkdtemp(resolve('.cache/integrated-authoring/play-')),bundle=resolve(output,'windows');
  await exportGame({content,projectRoot:info.root,destination:bundle,target:'windows',runtimes:{windows:resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-conditional-routing'),web:resolve('dist/web')}});
  const elements=content.project.visual.screens.find(s=>s.id==='screen-1').elements;
  const button=(action,when)=>elements.find(e=>e.action===action&&(!when||e.visibleWhen===when)).id;
  let session=createSession(content,42,{mode:'full'}).session;
  const expected={},steps=[{click:button('start','title')}];
  const checkpoint=id=>{steps.push({checkpoint:id});expected[id]=structuredClone(session.state);};
  const send=command=>{const result=dispatchSession(content,session,command,session.state.stateRevision);assert.equal(result.ok,true,JSON.stringify(result.error));session=result.session;};
  checkpoint('initial');
  steps.push({click:info.button},{click:'cancel-purchase'});checkpoint('cancelled');
  steps.push({click:info.button},{click:'confirm-purchase'});send({type:'executeAction',actionId:info.action,quantity:1});checkpoint('training');
  assert.equal(session.state.attributes[info.attribute],8);
  steps.push({screenshot:resolve(output,'training.png')},{click:button('save')});
  const saved=structuredClone(session);
  const schedule=()=>{
    const ids=Array(content.project.periodsPerMonth).fill('rest');ids[0]=info.activity;
    steps.push(...ids.map((value,i)=>({select:`plan-${i}`,value})),{click:button('advance')});send({type:'setSchedule',activityIds:ids});
    steps.push({click:button('advance')});send({type:'executeActivity'});
  };
  schedule();checkpoint('banquet');assert.equal(session.state.attributes[info.attribute],7);
  steps.push({screenshot:resolve(output,'banquet.png')},{click:button('load')});session=structuredClone(saved);checkpoint('restored');
  schedule();checkpoint('replayed');assert.deepEqual(expected.replayed,expected.banquet);
  const save=resolve(output,'middle.json');
  async function run(name,commands){
    const script=resolve(output,name+'-steps.json'),report=resolve(output,name+'-report.json');await writeFile(script,JSON.stringify(commands));
    const child=spawnSync(resolve(bundle,'RaisingPlayer.exe'),['--bundle',bundle,'--seed','42','--save',save,'--automation',script,'--report',report],{encoding:'utf8',windowsHide:true,timeout:60000});
    const result=JSON.parse(await readFile(report,'utf8'));assert.equal(child.status,0,JSON.stringify(result.error??child.stderr));assert.equal(result.ok,true);return result;
  }
  const report=await run('play',steps),ui=id=>report.steps.find(x=>x.command.checkpoint===id).ui;
  for(const [id,state] of Object.entries(expected))assert.deepEqual(ui(id).state,state,id);
  for(const id of ['training','restored']){assert.equal(ui(id).screenId,'screen-1-copy-1');assert.equal(ui(id).texts[info.caption],'기운을 모으며 수련 중');}
  for(const id of ['banquet','replayed'])assert.equal(ui(id).screenId,'screen-1-copy-2');
  const fresh=await run('fresh',[{click:button('load')},{checkpoint:'fresh'}]);
  assert.deepEqual(fresh.final.state,expected.training);assert.equal(fresh.final.screenId,'screen-1-copy-1');assert.equal(fresh.final.texts[info.caption],'기운을 모으며 수련 중');
  await writeFile(resolve('.cache/integrated-authoring/play-evidence.json'),JSON.stringify({output,bundle,checks:Object.keys(expected)},null,2));
  console.log('Actual Windows player evidence: '+output);
});

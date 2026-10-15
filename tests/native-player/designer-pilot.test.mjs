import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/index.js';

function route(content,kind,evidence){
  let session=createSession(content,42,{mode:'full'}).session;
  const steps=[{click:'start'}],expected={},restores=[];
  const node=()=>{const n=session.state.activeNarrative;return n?(n.kind==='event'?content.events:content.endings).find(x=>x.id===n.id)?.nodes.find(x=>x.id===n.nodeId):null;};
  function send(command){const next=dispatchSession(content,session,command,session.state.stateRevision);assert.equal(next.ok,true,JSON.stringify(next.error));session=next.session;}
  function settle(){for(let n=0;n<100;n++){if(session.state.phase==='period-end')send({type:'resolvePeriodEnd'});else if(session.state.phase==='narrative'&&!['dialogue','choice'].includes(node()?.type))send({type:'advanceNarrative'});else return;}assert.fail('settle limit');}
  function checkpoint(id,image=false){expected[id]=structuredClone(session.state);steps.push({checkpoint:id,...(image?{screenshot:resolve(evidence,kind+'-'+id+'.png')}:{})});}
  function action(command,clicks){steps.push(...clicks);send(command);settle();}
  function restore(command,clicks,id){const saved=structuredClone(session);steps.push({key:'F5'});checkpoint(id+'-saved',true);action(command,clicks);checkpoint(id+'-changed');steps.push({key:'F9'});session=saved;checkpoint(id+'-restored');restores.push(id);}
  function stories(){let guard=0;while(session.state.phase==='narrative'){const n=node();assert.ok(n);const choice=n.type==='choice'?(kind==='survival'?'register':'decline'):null;const command=choice?{type:'choose',choiceId:choice}:{type:'advanceNarrative'},clicks=[{click:choice?'choose-'+choice:'advance'}];if(session.state.periodIndex===0&&n.type==='choice')restore(command,clicks,'promise');action(command,clicks);if(++guard>30)assert.fail('story limit');}}
  checkpoint('started',true);
  for(let month=0;month<2;month++){
    const plan=kind==='survival'?(month===0?['study','rest','study']:['study','rest','work']):['rest','rest','rest'];
    action({type:'setSchedule',activityIds:plan},[...plan.map((value,i)=>({select:'plan-'+i,value})),{click:'advance'}]);
    for(let slot=0;slot<3;slot++){
      action({type:'executeActivity'},[{click:'advance'}]);checkpoint(`month-${month}-slot-${slot}`);
      if(month===0&&slot===1)restore({type:'acknowledgeResult'},[{click:'advance'}],'midpoint');
      action({type:'acknowledgeResult'},[{click:'advance'}]);stories();
    }
  }
  checkpoint('finished',true);assert.equal(session.state.phase,'finished');assert.equal(session.state.endingId,kind==='survival'?'certified':'ordinary');
  steps.push({key:'F5'});return {steps,expected,restores};
}

test('designer-authored wuxia game: both endings and real player save/resume', {timeout:180000},async()=>{
  const project=resolve(process.env.RAISING_DESIGNER_PROJECT??'examples/last-grandson');
  const built=await buildSnapshot(await loadProject(project));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const content=built.snapshot;assert.equal(content.project.gameId,'last-grandson');assert.equal(content.project.visual.screens.length,4);
  const root=await mkdtemp(resolve('.cache/designer-native-')),game=resolve(root,'game'),evidence=resolve('.cache/designer-pilot');await mkdir(evidence,{recursive:true});
  await exportGame({content,projectRoot:project,destination:game,target:'windows',runtimes:{windows:resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-routes'),web:resolve('dist/web')}});
  for(const kind of ['survival','farewell']){
    const plan=route(content,kind,evidence),script=resolve(evidence,kind+'-steps.json'),report=resolve(evidence,kind+'-native-report.json'),save=resolve(evidence,kind+'-save.json');
    await writeFile(script,JSON.stringify(plan.steps,null,2));
    const child=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--seed','42','--save',save,'--automation',script,'--report',report,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:90000});
    const result=JSON.parse(await readFile(report,'utf8'));assert.equal(child.status,0,JSON.stringify(result.error??child.stderr));assert.equal(result.ok,true);
    const state=id=>result.steps.find(s=>s.command.checkpoint===id)?.ui.state;
    for(const [id,expected]of Object.entries(plan.expected))assert.deepEqual(state(id),expected,kind+' '+id);
    for(const id of plan.restores){assert.notDeepEqual(state(id+'-saved'),state(id+'-changed'));assert.deepEqual(state(id+'-saved'),state(id+'-restored'));}
    const restart=resolve(evidence,kind+'-reload.json'),reloadReport=resolve(evidence,kind+'-reload-report.json');await writeFile(restart,JSON.stringify([{click:'load'},{checkpoint:'reopened'}]));
    const again=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',save,'--automation',restart,'--report',reloadReport,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:30000});assert.equal(again.status,0,again.stderr);
    const loaded=JSON.parse(await readFile(reloadReport,'utf8'));assert.equal(loaded.ok,true);assert.deepEqual(loaded.steps.find(s=>s.command.checkpoint==='reopened').ui.state,plan.expected.finished);
  }
});

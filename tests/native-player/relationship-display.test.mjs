import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/index.js';
import {resolveScreenElements} from '../../dist/packages/visual/components.js';

const bundle=process.env.RAISING_RELATIONSHIP_BUNDLE;
test('GUI-authored relationship text: both choices, ending save/restart/load and fresh-process restoration', {skip:!bundle,timeout:180000},async()=>{
  const packed=JSON.parse(await readFile(resolve(bundle,'game.json'),'utf8')),c=packed.content??packed;
  const flow=c.project.authoring.storyFlows[0],design=c.project.visual;
  const root=await mkdtemp(resolve('.cache/relationship-display/play-'));
  const saveButton=resolveScreenElements(design,'ending').find(e=>e.action==='save').id;
  const loadButton=resolveScreenElements(design,'schedule').find(e=>e.action==='load').id;
  for(const choice of flow.choices){
    let session=createSession(c,42,{mode:'full'}).session;
    const steps=[{click:'start'}],expected={};
    const send=command=>{const r=dispatchSession(c,session,command,session.state.stateRevision);assert.equal(r.ok,true,JSON.stringify(r.error));session=r.session;};
    const node=()=>{const active=session.state.activeNarrative;return active?(active.kind==='event'?c.events:c.endings).find(e=>e.id===active.id).nodes.find(n=>n.id===active.nodeId):null;};
    const settle=()=>{for(let n=0;n<100;n++){if(session.state.phase==='period-end')send({type:'resolvePeriodEnd'});else if(session.state.phase==='narrative'&&!['dialogue','choice'].includes(node()?.type))send({type:'advanceNarrative'});else return;}throw Error('settle limit');};
    const checkpoint=id=>{steps.push({checkpoint:id});expected[id]=structuredClone(session.state);};
    checkpoint('initial');
    for(let n=0;session.state.phase!=='finished'&&n<120;n++){
      switch(session.state.phase){
        case 'planning':{const ids=Array(c.project.periodsPerMonth??3).fill('rest');if(session.state.periodIndex===0)ids[0]=flow.activityId;steps.push(...ids.map((value,i)=>({select:`plan-${i}`,value})),{click:'advance'});send({type:'setSchedule',activityIds:ids});break;}
        case 'ready':steps.push({click:'advance'});send({type:'executeActivity'});break;
        case 'activity-result':steps.push({click:'advance'});send({type:'acknowledgeResult'});break;
        case 'narrative':{
          const current=node();if(current.type==='choice'){
            const id=current.choices.some(x=>x.id===choice.id)?choice.id:current.choices[0].id;
            steps.push({click:`choose-${id}`});send({type:'choose',choiceId:id});
          }else{steps.push({click:'advance'});send({type:'advanceNarrative'});}break;
        }
        default:throw Error(session.state.phase);
      }
      settle();checkpoint(`step-${n}`);
      if(session.state.relationships[flow.characterId]?.[flow.relationshipId]===choice.delta&&node()?.type==='dialogue'&&!steps.some(s=>s.screenshot))steps.push({screenshot:resolve(root,choice.id+'-change.png')});
    }
    assert.equal(session.state.phase,'finished');assert.equal(session.state.relationships[flow.characterId][flow.relationshipId],choice.delta);
    checkpoint('ending');steps.push({screenshot:resolve(root,choice.id+'-ending.png')},{click:saveButton},{checkpoint:'saved'},{click:'start'},{checkpoint:'reset'},{click:loadButton},{checkpoint:'restored'},{screenshot:resolve(root,choice.id+'-restored.png')});
    const script=resolve(root,choice.id+'-steps.json'),save=resolve(root,choice.id+'-save.json'),reportPath=resolve(root,choice.id+'-report.json');await writeFile(script,JSON.stringify(steps));
    const run=spawnSync(resolve(bundle,'RaisingPlayer.exe'),['--bundle',bundle,'--seed','42','--save',save,'--automation',script,'--report',reportPath],{encoding:'utf8',timeout:60000,windowsHide:true});
    const report=JSON.parse(await readFile(reportPath,'utf8'));assert.equal(run.status,0,JSON.stringify(report.error??run.stderr));assert.equal(report.ok,true);
    const ui=id=>report.steps.find(s=>s.command.checkpoint===id).ui;
    for(const [id,state] of Object.entries(expected))assert.deepEqual(ui(id).state,state,id);
    const text=u=>Object.values(u.texts).find(t=>t.includes(flow.characterName)&&t.includes('최근'));
    const sign=choice.delta>0?`+${choice.delta}`:String(choice.delta);
    assert.match(text(ui('initial')),/0 \(최근 0\)/);
    assert.ok(text(ui('ending')).includes(`${choice.delta} (최근 ${sign})`),text(ui('ending')));
    assert.deepEqual(ui('restored').state,ui('ending').state);assert.equal(text(ui('restored')),text(ui('ending')));
    assert.match(text(ui('reset')),/0 \(최근 0\)/);
    const saved=JSON.parse(await readFile(save,'utf8'));assert.deepEqual(saved.state,ui('ending').state);assert.equal(Object.hasOwn(saved,'relationshipChanges'),false);
    const reloadScript=resolve(root,choice.id+'-reload-steps.json'),reloadReport=resolve(root,choice.id+'-reload-report.json');
    await writeFile(reloadScript,JSON.stringify([{click:'load'},{checkpoint:'reopened'},{screenshot:resolve(root,choice.id+'-reopened.png')}]));
    const again=spawnSync(resolve(bundle,'RaisingPlayer.exe'),['--bundle',bundle,'--save',save,'--automation',reloadScript,'--report',reloadReport],{encoding:'utf8',timeout:30000,windowsHide:true});
    const reopened=JSON.parse(await readFile(reloadReport,'utf8'));assert.equal(again.status,0,JSON.stringify(reopened.error??again.stderr));assert.equal(reopened.ok,true);
    assert.deepEqual(reopened.final.state,saved.state);assert.equal(text(reopened.final),text(ui('ending')));
  }
  await writeFile(resolve('.cache/relationship-display/play-evidence.txt'),root);
  console.log(`Actual player evidence: ${root}`);
});

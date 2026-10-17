import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/index.js';
import {resolveScreenElements} from '../../dist/packages/visual/components.js';
import {resolveScreen} from '../../dist/packages/visual/routing.js';

const bundle=process.env.RAISING_FREE_SCREEN_BUNDLE;
test('Qt-authored blank canvas: schedule, relation, dialogue, both choices and mid-story/ending save restoration', {skip:!bundle,timeout:180000},async()=>{
  const packed=JSON.parse(await readFile(resolve(bundle,'game.json'),'utf8')),c=packed.content??packed;
  const flow=c.project.authoring.storyFlows[0],design=c.project.visual,screen=design.routing.defaultScreenId;
  const elements=resolveScreenElements(design,screen),button=(action,when)=>elements.find(e=>e.action===action&&(!when||e.visibleWhen===when)).id;
  const root=await mkdtemp(resolve('.cache/free-screens/play-'));
  for(const choice of flow.choices){
    let session=createSession(c,42,{mode:'full'}).session;
    const steps=[{click:button('start','title')}],expected={};let testedMid=false;
    const send=command=>{const r=dispatchSession(c,session,command,session.state.stateRevision);assert.equal(r.ok,true,JSON.stringify(r.error));session=r.session;};
    const node=()=>{const active=session.state.activeNarrative;return active?(active.kind==='event'?c.events:c.endings).find(e=>e.id===active.id).nodes.find(n=>n.id===active.nodeId):null;};
    const settle=()=>{for(let n=0;n<100;n++){if(session.state.phase==='period-end')send({type:'resolvePeriodEnd'});else if(session.state.phase==='narrative'&&!['dialogue','choice'].includes(node()?.type))send({type:'advanceNarrative'});else return;}throw Error('settle limit');};
    const checkpoint=id=>{steps.push({checkpoint:id});expected[id]=structuredClone(session.state);};
    checkpoint('initial');steps.push({screenshot:resolve(root,choice.id+'-planning.png')});
    for(let n=0;session.state.phase!=='finished'&&n<120;n++){
      const advance=button('advance');
      switch(session.state.phase){
        case 'planning':{const ids=Array(c.project.periodsPerMonth).fill('rest');if(session.state.periodIndex===0)ids[0]=flow.activityId;steps.push(...ids.map((value,i)=>({select:`plan-${i}`,value})),{click:advance});send({type:'setSchedule',activityIds:ids});break;}
        case 'ready':steps.push({click:advance});send({type:'executeActivity'});break;
        case 'activity-result':steps.push({click:advance});send({type:'acknowledgeResult'});break;
        case 'narrative':{
          if(!testedMid){
            const menuElements=resolveScreenElements(design,'title');
            const menuSave=menuElements.find(e=>e.action==='save'),menuReturn=menuElements.find(e=>e.action==='navigate'&&e.screenId===screen);
            if(menuSave&&menuReturn){
              steps.push({click:button('navigate')});send({type:'navigate',screenId:'title'});checkpoint('narrative-menu');
              steps.push({click:menuSave.id});const savedMenu=structuredClone(session);
              steps.push({click:menuReturn.id});send({type:'navigate',screenId:screen});
              steps.push({click:button('load')});session=savedMenu;checkpoint('menu-restored');
              steps.push({click:menuReturn.id});send({type:'navigate',screenId:screen});
            }
          }
          if(!testedMid){testedMid=true;steps.push({click:button('save')},{click:advance});const preserved=structuredClone(session);if(node().type==='dialogue')send({type:'advanceNarrative'});else steps.pop();settle();steps.push({click:button('load')});session=preserved;checkpoint('mid-restored');steps.push({screenshot:resolve(root,choice.id+'-dialogue.png')});}
          const current=node();if(current.type==='choice'){
            const id=current.choices.some(x=>x.id===choice.id)?choice.id:current.choices[0].id;
            steps.push({screenshot:resolve(root,choice.id+'-choice.png')},{click:`choose-${id}`});send({type:'choose',choiceId:id});
          }else{steps.push({click:advance});send({type:'advanceNarrative'});}break;
        }
        default:throw Error(session.state.phase);
      }
      settle();checkpoint(`step-${n}`);
    }
    assert.equal(session.state.phase,'finished');assert.equal(session.state.relationships[flow.characterId][flow.relationshipId],choice.delta);assert.ok(testedMid);
    checkpoint('ending');steps.push({screenshot:resolve(root,choice.id+'-ending.png')},{click:button('save')},{click:button('start','finished')},{click:button('load')});checkpoint('ending-restored');
    const script=resolve(root,choice.id+'-steps.json'),save=resolve(root,choice.id+'-save.json'),reportPath=resolve(root,choice.id+'-report.json');await writeFile(script,JSON.stringify(steps));
    const run=spawnSync(resolve(bundle,'RaisingPlayer.exe'),['--bundle',bundle,'--seed','42','--save',save,'--automation',script,'--report',reportPath],{encoding:'utf8',timeout:60000,windowsHide:true});
    const report=JSON.parse(await readFile(reportPath,'utf8'));assert.equal(run.status,0,JSON.stringify(report.error??run.stderr));assert.equal(report.ok,true);
    const ui=id=>report.steps.find(s=>s.command.checkpoint===id).ui;
    for(const [id,state] of Object.entries(expected)){assert.deepEqual(ui(id).state,state,id);assert.equal(ui(id).screenId,resolveScreen(design,state),'same freely authored canvas');}
    const relation=elements.find(e=>e.binding==='data-value'&&e.dataBinding.kind==='relationship');
    assert.equal(ui('initial').texts[relation.id],`${flow.characterName} · 신뢰: 0`);
    assert.equal(ui('ending-restored').texts[relation.id],`${flow.characterName} · 신뢰: ${choice.delta}`);
    const fresh=resolve(root,choice.id+'-fresh.json'),freshReport=resolve(root,choice.id+'-fresh-report.json');await writeFile(fresh,JSON.stringify([{click:button('load')},{checkpoint:'fresh'}]));
    const again=spawnSync(resolve(bundle,'RaisingPlayer.exe'),['--bundle',bundle,'--save',save,'--automation',fresh,'--report',freshReport],{encoding:'utf8',timeout:30000,windowsHide:true});assert.equal(again.status,0,again.stderr);const restored=JSON.parse(await readFile(freshReport,'utf8'));assert.deepEqual(restored.final.state,session.state);assert.equal(restored.final.screenId,screen);
  }
  await writeFile(resolve('.cache/free-screens/play-evidence.txt'),root);console.log(`Actual player evidence: ${root}`);
});

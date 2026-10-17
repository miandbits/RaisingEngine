import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdtemp} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/index.js';
import {resolveScreenElements} from '../../dist/packages/visual/components.js';
import {resolvePresentation,presentationActivity} from '../../dist/packages/visual/variants.js';
import {nativeRequest} from '../../dist/packages/native-bridge/index.js';

const authoring=process.env.RAISING_REUSABLE_ACTIVITIES;
test('GUI-authored reusable activities: one canvas, shared component, two actors, interrupted branches, endings and exact saves', {skip:!authoring,timeout:180000},async()=>{
  const info=JSON.parse(await readFile(authoring,'utf8')),service=new QtStudioService();await service.handle({method:'open',params:{root:info.root}});
  const {snapshot:c}=await service.editor.snapshot(service.editor.revision);assert.ok(c,'GUI-authored project must validate');
  const out=await mkdtemp(resolve('.cache/reusable-activities/play-')),bundle=resolve(out,'windows');
  await exportGame({content:c,projectRoot:info.root,destination:bundle,target:'windows',runtimes:{windows:resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-conditional-routing'),web:resolve('dist/web')}});
  const design=c.project.visual;assert.deepEqual(design.screens.map(s=>s.id),info.screensBefore);assert.equal(design.routing.defaultScreenId,info.screen);assert.equal(design.routing.conditions?.length??0,0);
  const shared=design.components.find(x=>x.id===info.component);assert.equal(shared.elements[0].variants.length,3);
  const elements=resolveScreenElements(design,info.screen),cards=elements.filter(e=>info.instances.some(i=>e.id===i+'__'+info.child));assert.equal(cards.length,2);
  for(const card of cards)assert.equal(card.text,'함께 성장할 다음 활동을 선택하세요');
  const button=(action,when)=>elements.find(e=>e.action===action&&(!when||e.visibleWhen===when)).id;
  const actors=c.project.roster.members.map(m=>m.characterId),choices=c.events.find(e=>e.id===info.meeting).nodes.find(n=>n.type==='choice').choices;
  assert.equal(actors.length,2);assert.equal(choices.length,2);
  const pairs=[[info.training,info.training],[info.infiltration,info.banquet],[info.banquet,info.infiltration],[info.training,info.banquet],[info.infiltration,info.training],[info.banquet,info.banquet]];
  const evidence=[];
  for(const choice of choices){
    let session=createSession(c,42,{mode:'full'}).session;const steps=[{click:button('start','title')}],expected={},savedKinds=new Set();let sequence=0,choiceSaved=false;
    const send=command=>{const result=dispatchSession(c,session,command,session.state.stateRevision);assert.equal(result.ok,true,JSON.stringify(result.error));session=result.session;};
    const node=()=>{const a=session.state.activeNarrative;return a?(a.kind==='event'?c.events:c.endings).find(e=>e.id===a.id).nodes.find(n=>n.id===a.nodeId):null;};
    const settle=()=>{for(let i=0;i<100;i++){if(session.state.phase==='period-end')send({type:'resolvePeriodEnd'});else if(session.state.phase==='narrative'&&!['dialogue','choice'].includes(node()?.type))send({type:'advanceNarrative'});else return;}throw Error('settling did not terminate');};
    const checkpoint=id=>{steps.push({checkpoint:id});expected[id]=structuredClone(session.state);};checkpoint('initial');
    for(let guard=0;session.state.phase!=='finished'&&guard<120;guard++){
      if(['planning','ready'].includes(session.state.phase)){
        const pair=pairs[session.state.periodIndex%pairs.length],assignments=Object.fromEntries(actors.map((id,i)=>[id,pair[i]]));
        steps.push(...actors.map((id,i)=>({select:'roster-'+id,value:pair[i]})),{click:'roster-assign'});send({type:'assignActivities',assignments});checkpoint('assigned-'+session.state.periodIndex);
        steps.push({click:'roster-execute'});send({type:'executeActivity'});checkpoint('result-'+session.state.periodIndex);
        if(session.state.periodIndex===0){assert.equal(session.state.roster.attributes[actors[0]].skill,20);assert.equal(session.state.roster.attributes[actors[1]].skill,80);assert.equal(session.state.attributes.money,28);}
        steps.push({screenshot:resolve(out,choice.id+'-result-'+session.state.periodIndex+'.png')});
      }else if(session.state.phase==='activity-result'){
        const kind=session.state.lastActivityResult.activityId;
        if(!savedKinds.has(kind)){
          savedKinds.add(kind);steps.push({click:button('save')});const saved=structuredClone(session);
          steps.push({click:button('advance')});send({type:'acknowledgeResult'});settle();
          steps.push({click:button('load')});session=saved;checkpoint('activity-restored-'+kind);
        }
        steps.push({click:button('advance')});send({type:'acknowledgeResult'});
      }else if(session.state.phase==='narrative'){
        const current=node();
        if(current.type==='choice'){
          const id=current.choices.some(v=>v.id===choice.id)?choice.id:current.choices[0].id;
          if(session.state.activeNarrative.id===info.meeting&&!choiceSaved){
            choiceSaved=true;const before=structuredClone(session);
            const blocked=dispatchSession(c,session,{type:'executeActivity'},session.state.stateRevision);assert.equal(blocked.ok,false);assert.deepEqual(session,before);
            steps.push({click:button('save')},{screenshot:resolve(out,choice.id+'-choice.png')},{click:'choose-'+id});send({type:'choose',choiceId:id});settle();
            steps.push({click:button('load')});session=before;checkpoint('choice-restored');assert.equal(node().type,'choice');
          }
          steps.push({click:'choose-'+id});send({type:'choose',choiceId:id});
        }else{steps.push({click:button('advance')});send({type:'advanceNarrative'});}
      }else throw Error('Unexpected phase '+session.state.phase);
      settle();checkpoint('step-'+sequence++);
    }
    assert.equal(session.state.phase,'finished');assert.ok(choiceSaved);assert.equal(savedKinds.size,3);checkpoint('ending');
    const flow=c.project.authoring.storyFlows[0],delta=flow.choices.find(x=>x.id===choice.id).delta;assert.equal(session.state.relationships[flow.characterId][flow.relationshipId],delta);
    steps.push({click:button('save')},{click:button('start','finished')},{click:button('load')});checkpoint('ending-restored');
    const save=resolve(out,choice.id+'-save.json');
    async function run(name,commands){const script=resolve(out,name+'-steps.json'),report=resolve(out,name+'-report.json');await writeFile(script,JSON.stringify(commands));const child=spawnSync(resolve(bundle,'RaisingPlayer.exe'),['--bundle',bundle,'--seed','42','--save',save,'--automation',script,'--report',report],{encoding:'utf8',windowsHide:true,timeout:90000});const result=JSON.parse(await readFile(report,'utf8'));assert.equal(child.status,0,JSON.stringify(result.error??child.stderr));assert.equal(result.ok,true);return result;}
    const actual=await run(choice.id,steps);
    for(const [id,state] of Object.entries(expected)){
      const ui=actual.steps.find(s=>s.command.checkpoint===id).ui;assert.deepEqual(ui.state,state,id);assert.equal(ui.screenId,info.screen,'same authored screen at '+id);
      for(const card of cards){const projected=resolvePresentation(card,presentationActivity(state),condition=>nativeRequest({operation:'evaluate',content:c,state,condition}).value===true);assert.equal(ui.texts[card.id],projected.text,'shared definition at '+id+'/'+card.id);}
    }
    const fresh=await run(choice.id+'-fresh',[{click:button('load')},{checkpoint:'fresh'}]);assert.deepEqual(fresh.final.state,expected.ending);assert.equal(fresh.final.screenId,info.screen);
    evidence.push({choice:choice.id,ending:session.state.endingId,checkpoints:Object.keys(expected).length,relationship:delta,saveRestoredActivities:[...savedKinds]});
  }
  await writeFile(resolve('.cache/reusable-activities/play-evidence.json'),JSON.stringify({out,bundle,evidence},null,2));console.log('Actual Windows play evidence: '+out);
});

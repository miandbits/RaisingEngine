import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/index.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';
import {resolveScreenElements} from '../../dist/packages/visual/components.js';
import {resolvePresentation,presentationActivity} from '../../dist/packages/visual/variants.js';
import {nativeRequest} from '../../dist/packages/native-bridge/index.js';

test('published Qt authoring fixture: reusable rules, actor-local values, both endings and restored command replay',async()=>{
  const info=JSON.parse(await readFile(new URL('../fixtures/reusable-activities.json',import.meta.url),'utf8'));
  const built=await buildSnapshot(await loadProject(info.root));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));const c=built.snapshot;
  const untouched=JSON.stringify(c),actors=c.project.roster.members.map(x=>x.characterId),flow=c.project.authoring.storyFlows[0];
  const pairs=[[info.training,info.training],[info.infiltration,info.banquet],[info.banquet,info.infiltration],[info.training,info.banquet],[info.infiltration,info.training],[info.banquet,info.banquet]];
  const cards=resolveScreenElements(c.project.visual,info.screen).filter(e=>info.instances.some(id=>e.id===id+'__'+info.child));assert.equal(cards.length,2);
  async function play(choice,restore){
    let s=createSession(c,42,{mode:'full'}).session;let saves=0,interruptions=0;const seen=new Set();
    const node=()=>{const a=s.state.activeNarrative;return a?(a.kind==='event'?c.events:c.endings).find(e=>e.id===a.id).nodes.find(n=>n.id===a.nodeId):null;};
    const send=command=>{const r=dispatchSession(c,s,command,s.state.stateRevision);assert.equal(r.ok,true,JSON.stringify(r.error));s=r.session;};
    const roundtrip=async()=>{const encoded=await encodeSave(c,s);assert.equal(encoded.ok,true);const loaded=await decodeSave(c,encoded.text);assert.equal(loaded.ok,true);assert.deepEqual(loaded.session.state,s.state);s=loaded.session;++saves;};
    for(let step=0;s.state.phase!=='finished'&&step<150;++step){
      if(['planning','ready'].includes(s.state.phase)){const p=pairs[s.state.periodIndex%pairs.length];send({type:'assignActivities',assignments:Object.fromEntries(actors.map((id,i)=>[id,p[i]]))});send({type:'executeActivity'});
        if(s.state.periodIndex===0){assert.equal(s.state.roster.attributes[actors[0]].skill,20);assert.equal(s.state.roster.attributes[actors[1]].skill,80);assert.equal(s.state.attributes.money,28);}
      }else if(s.state.phase==='activity-result'){
        const activity=presentationActivity(s.state);seen.add(activity);
        const texts=cards.map(card=>resolvePresentation(card,activity,condition=>nativeRequest({operation:'evaluate',content:c,state:s.state,condition}).value===true).text);assert.equal(texts[0],texts[1]);assert.notEqual(texts[0],cards[0].text);
        if(restore)await roundtrip();send({type:'acknowledgeResult'});
      }else if(s.state.phase==='period-end')send({type:'resolvePeriodEnd'});
      else if(s.state.phase==='narrative'){
        if(node().type==='choice'){++interruptions;assert.equal(dispatchSession(c,s,{type:'executeActivity'},s.state.stateRevision).ok,false);if(restore)await roundtrip();const options=node().choices;send({type:'choose',choiceId:options.some(x=>x.id===choice.id)?choice.id:options[0].id});}
        else send({type:'advanceNarrative'});
      }else throw Error(s.state.phase);
    }
    assert.equal(s.state.phase,'finished');assert.ok(interruptions>0);assert.equal(seen.size,3);assert.equal(s.state.relationships[flow.characterId][flow.relationshipId],choice.delta);
    if(restore){await roundtrip();assert.ok(saves>=8);}return s.state;
  }
  for(const choice of flow.choices)assert.deepEqual(await play(choice,true),await play(choice,false));
  assert.equal(JSON.stringify(c),untouched,'rendering and replay never rewrite component definitions');
});

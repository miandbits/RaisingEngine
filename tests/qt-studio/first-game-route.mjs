import assert from 'node:assert/strict';
import { createSession, dispatchSession } from '../../dist/packages/runtime-service/index.js';
export function firstGameRoute(content,accept) {
  const branch={id:accept?"accept":"decline"};
  const plans=accept
    ?[['walk','study','rest'],['study','study','rest'],['rest','rest','rest']]
    :[['walk','rest','rest'],['rest','rest','rest'],['rest','rest','rest']];
  let session=createSession(content,42,{mode:'full'}).session;
  const steps=[{checkpoint:'title'},{click:'start'}],expected={},commands=[],choiceLabels={};
  const node=()=>{const a=session.state.activeNarrative;return a?(a.kind==='event'?content.events:content.endings).find(n=>n.id===a.id).nodes.find(n=>n.id===a.nodeId):null;};
  const send=command=>{const r=dispatchSession(content,session,command,session.state.stateRevision);assert.equal(r.ok,true,JSON.stringify(r.error));session=r.session;commands.push(command);};
  const settle=()=>{for(let i=0;i<100;i++){
    if(session.state.phase==='period-end')send({type:'resolvePeriodEnd'});
    else if(session.state.phase==='narrative'&&!['dialogue','choice'].includes(node()?.type))send({type:'advanceNarrative'});
    else return;
  }throw Error('Narrative did not settle');};
  const checkpoint=label=>{steps.push({checkpoint:label});expected[label]=structuredClone(session.state);};
  const act=(command,ui)=>{steps.push(...ui);send(command);settle();};
  let narratives=0;
  const story=()=>{while(session.state.phase==='narrative'){
    assert.ok(++narratives<100,'bounded story');
    if(node().type==='choice'){
      const choice=node().choices.find(c=>c.id===branch.id)??node().choices[0];
      choiceLabels[choice.id]=choice.text;
      act({type:'choose',choiceId:choice.id},[{click:'choose-'+choice.id}]);
    }else act({type:'advanceNarrative'},[{click:'advance'}]);
    checkpoint('story-'+narratives);
  }};
  checkpoint('start');
  for(let month=0;month<3;month++){
    assert.equal(session.state.phase,'planning');
    act({type:'setSchedule',activityIds:plans[month]},[...plans[month].map((value,i)=>({select:'plan-'+i,value})),{click:'advance'}]);
    checkpoint('plan-'+month);
    for(let period=0;period<3;period++){
      act({type:'executeActivity'},[{click:'advance'}]);checkpoint(`result-${month}-${period}`);
      if(month===1&&period===0){
        const saved=structuredClone(session);steps.push({key:'F5'});checkpoint('middle-saved');
        act({type:'acknowledgeResult'},[{click:'advance'}]);checkpoint('past-middle');
        steps.push({key:'F9'});session=saved;checkpoint('middle-restored');
      }
      act({type:'acknowledgeResult'},[{click:'advance'}]);story();checkpoint(`period-${month}-${period}`);
    }
  }
  assert.equal(session.state.phase,'finished');
  assert.equal(session.state.endingId,accept?'together':'own-path');
  assert.equal(session.state.relationships['mira']['affection'],accept?20:-10);
  const followup="garden-promise";
  assert.equal(session.state.completedEvents.includes(followup),accept,'follow-up only after the experienced accepting branch');
  assert.ok(commands.some(c=>c.type==='choose'&&c.choiceId===branch.id),'authored branch exercised');
  assert.deepEqual(expected['middle-saved'],expected['middle-restored']);
  checkpoint('ending');steps.push({key:'F5'});
  return {id:accept?'trust-and-survive':'refuse-and-farewell',steps,expected,choiceLabels,commands,ending:session.state.endingId};
}


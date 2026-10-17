import test from 'node:test';
import assert from 'node:assert/strict';
import {content,doc,replaceEvents,literal,start,send,scenarios} from './helpers.mjs';
import {encodeSave,decodeSave,runScenario} from '../../dist/packages/runtime-service/index.js';
import {nativeReplay} from '../../dist/packages/native-bridge/index.js';
import {numericBinding} from '../../dist/packages/visual/bindings.js';
import {createVisual} from '../../dist/packages/visual/model.js';

const binding={binding:'relationship-value',characterId:'mentor',relationshipId:'trust',valueFormat:'{label}: {value} (최근 {delta})'};
test('authored relationship text with recent change passes project validation',async()=>{
  const c=await content(p=>{const design=createVisual();design.screens[0].elements.push({id:'relation',kind:'text',x:20,y:20,width:600,height:48,...binding});doc(p,'project').visual=design;});
  assert.equal(c.project.visual.screens[0].elements.at(-1).valueFormat,binding.valueFormat);
});
test('relationship binding shows positive, negative and untouched change without script evaluation',()=>{
  for(const [delta,text] of [[3,'+3'],[-1,'-1'],[undefined,'0']])assert.equal(numericBinding(binding,{stats:[],relationships:[{characterId:'mentor',relationshipId:'trust',name:'스승',label:'신뢰',value:7,delta}]}),`스승 · 신뢰: 7 (최근 ${text})`);
  assert.equal(numericBinding(binding,{stats:[],relationships:[]}), '—');
});
test('recent change is net/clamped, survives unrelated commands and verified replay; save format stays unchanged',async()=>{
  const c=await content(p=>{
    replaceEvents(p,[]);const character=doc(p,'character','mentor');character.relationships.find(r=>r.id==='trust').max=10;
    const a=doc(p,'activity','rest');delete a.successCheck;delete a.successEffects;delete a.failureEffects;
    a.baseEffects=[{op:'add',target:{kind:'relationship',characterId:'mentor',relationshipId:'trust'},value:literal(20)},
      {op:'add',target:{kind:'relationship',characterId:'mentor',relationshipId:'trust'},value:literal(-2)}];
  });
  let session=start(c);const initial=session.state.relationships.mentor.trust;
  session=send(c,session,{type:'setSchedule',activityIds:['rest','rest','rest']});
  session=send(c,session,{type:'executeActivity'});
  assert.equal(session.state.relationships.mentor.trust,8);assert.equal(session.relationshipChanges.mentor.trust,8-initial);
  session=send(c,session,{type:'acknowledgeResult'});
  const encoded=await encodeSave(c,session);assert.equal(encoded.ok,true,JSON.stringify(encoded));
  assert.equal(Object.hasOwn(JSON.parse(encoded.text),'relationshipChanges'),false);
  const restored=await decodeSave(c,encoded.text);assert.equal(restored.ok,true,JSON.stringify(restored));
  assert.deepEqual(restored.session,session);
  assert.deepEqual(nativeReplay(c,session.origin,session.commands,true).relationshipChanges,session.relationshipChanges);
  assert.deepEqual(nativeReplay(c,session.origin,session.commands),{ok:true,state:session.state},'existing replay API shape remains unchanged');
  session=send(c,session,{type:'resolvePeriodEnd'});session=send(c,session,{type:'executeActivity'});
  assert.equal(session.state.relationships.mentor.trust,8);assert.equal(session.relationshipChanges.mentor.trust,8-initial,'zero net change retains previous effective change');
  assert.deepEqual(start(c).relationshipChanges,{},'new game resets presentation history');
});
test('both complete endings save and reload state plus relationship display history',async()=>{
  const c=await content();
  for(const scenario of scenarios){
    const result=await runScenario(c,scenario);assert.equal(result.ok,true,JSON.stringify(result));
    assert.equal(result.session.state.phase,'finished');
    const saved=await encodeSave(c,result.session);assert.equal(saved.ok,true,JSON.stringify(saved));
    const restored=await decodeSave(c,saved.text);assert.equal(restored.ok,true,JSON.stringify(restored));
    assert.deepEqual(restored.session.state,result.session.state);
    assert.deepEqual(restored.session.relationshipChanges,result.session.relationshipChanges);
  }
});

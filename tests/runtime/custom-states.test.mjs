import test from 'node:test';
import assert from 'node:assert/strict';
import {content,doc,start,send,loaded,replaceEvents,event} from './helpers.mjs';
import {buildSnapshot} from '../../dist/packages/project-service/index.js';
import {encodeSave,decodeSave,dispatchSession} from '../../dist/packages/runtime-service/index.js';
import {nativeRequest} from '../../dist/packages/native-bridge/index.js';
import {createRun,dispatchCommand as dispatch} from '../../dist/packages/core/run.js';
import {evaluateCondition} from '../../dist/packages/core/evaluation.js';
import {timelineChangesText} from '../../dist/packages/visual/timeline.js';
import {buildCatalog,buildRelations} from '../../dist/packages/analysis-service/catalog.js';

const group=(scope='shared')=>({id:'mode',name:'행동 모드',scope,initialValueId:'idle',values:[{id:'idle',name:'평상시'},{id:'training',name:'수련'},{id:'banquet',name:'연회'}]});
const rule=(op,value,owner)=>({op,stateGroupId:'mode',stateValueId:value,...(owner?{owner}: {})});
async function fixture(scoped=false){return content(p=>{
  const project=doc(p,'project');project.stateGroups=[group(scoped?'character':'shared')];
  if(scoped){const ids=p.documents.filter(d=>d.kind==='character').map(d=>d.data.id);const second=ids.find(id=>id!==project.protagonistId);
    project.roster={stateModel:'character-v1',localAttributeIds:['stamina'],members:[{characterId:project.protagonistId,activityId:'study',initialAttributes:{}},{characterId:second,activityId:'rest',initialAttributes:{}}]};
  }
  doc(p,'activity','study').baseEffects.push(rule('setState','training',scoped?{kind:'actor'}:undefined));
  doc(p,'activity','rest').baseEffects.push(rule('setState','banquet',scoped?{kind:'actor'}:undefined));
  replaceEvents(p,[]);
});}

test('shared state starts at authored initial value, changes deterministically and survives journal save',async()=>{
  const c=await fixture();let s=start(c),ref=createRun(c,42,{mode:'full'});assert.equal(ref.ok,true);assert.deepEqual(s.state,ref.state);
  for(const command of [{type:'setSchedule',activityIds:['study','rest','study']},{type:'executeActivity'}]){
    s=send(c,s,command);const result=dispatch(c,ref.state,command,ref.state.stateRevision);assert.equal(result.ok,true,JSON.stringify(result));ref=result;
    assert.deepEqual(s.state,ref.state);
  }
  assert.equal(s.state.customStates.shared.mode,'training');
  const condition=rule('stateIs','training'),before=JSON.stringify(s.state);
  assert.equal(nativeRequest({operation:'evaluate',content:c,state:s.state,condition}).value,true);
  assert.equal(evaluateCondition(c,s.state,condition),true);assert.equal(JSON.stringify(s.state),before);
  assert.match(timelineChangesText(c,s.state.lastActivityResult.changes),/행동 모드: 수련/);
  const saved=await encodeSave(c,s),restored=await decodeSave(c,saved.text);assert.equal(restored.ok,true,JSON.stringify(restored));assert.deepEqual(restored.session,s);
  assert.deepEqual(send(c,restored.session,{type:'acknowledgeResult'}),send(c,s,{type:'acknowledgeResult'}));
  const tampered=JSON.parse(saved.text);tampered.state.customStates.shared.mode='banquet';assert.equal((await decodeSave(c,JSON.stringify(tampered))).ok,false);
});

test('parallel activity actors keep independent values; named owner conditions do not leak',async()=>{
  const c=await fixture(true),[a,b]=c.project.roster.members.map(m=>m.characterId);let s=start(c);
  let reference=createRun(c,42,{mode:'full'}).state;
  for(const command of [{type:'assignActivities',assignments:{[a]:'study',[b]:'rest'}},{type:'executeActivity'}]){s=send(c,s,command);const next=dispatch(c,reference,command,reference.stateRevision);assert.equal(next.ok,true);reference=next.state;assert.deepEqual(s.state,reference);}
  assert.equal(s.state.customStates.characters[a].mode,'training');assert.equal(s.state.customStates.characters[b].mode,'banquet');assert.deepEqual(s.state.customStates.shared,{});
  for(const [id,value]of [[a,'training'],[b,'banquet']]){const condition=rule('stateIs',value,{kind:'character',characterId:id});assert.equal(evaluateCondition(c,s.state,condition),true);assert.equal(nativeRequest({operation:'evaluate',content:c,state:s.state,condition}).value,true);}
  const saved=await encodeSave(c,s),restored=await decodeSave(c,saved.text);assert.equal(restored.ok,true);assert.deepEqual(restored.session,s);
});

test('state references appear in the content catalog as reads and writes',()=>{
  const p=structuredClone(loaded);doc(p,'project').stateGroups=[group()];doc(p,'activity','rest').requirements=rule('stateIs','idle');doc(p,'activity','rest').baseEffects.push(rule('setState','training'));
  const catalog=buildCatalog(p.documents),state=catalog.find(x=>x.kind==='stateGroup');assert.equal(state.label,'행동 모드');
  const relations=buildRelations(catalog).filter(x=>x.to===state.key&&x.file.endsWith('rest.json'));assert.ok(relations.some(x=>x.kind==='reads'));assert.ok(relations.some(x=>x.kind==='writes'));
});

test('event effects and requirements share the state contract; invalid effects roll back all changes',async()=>{
  const c=await content(p=>{doc(p,'project').stateGroups=[group()];doc(p,'activity','rest').baseEffects.push(rule('setState','training'));
    replaceEvents(p,[event('state-event',[{id:'change',type:'effect',effects:[rule('setState','banquet')],next:'line'},{id:'line',type:'dialogue',speakerId:null,text:'연회가 시작된다',next:'end'},{id:'end',type:'end'}],{trigger:'period-end',requirements:rule('stateIs','training')})]);});
  let s=start(c);s=send(c,s,{type:'setSchedule',activityIds:['rest','rest','rest']});s=send(c,s,{type:'executeActivity'});s=send(c,s,{type:'acknowledgeResult'});s=send(c,s,{type:'resolvePeriodEnd'});s=send(c,s,{type:'advanceNarrative'});
  assert.equal(s.state.customStates.shared.mode,'banquet');assert.equal(s.state.phase,'narrative');
  const saved=await encodeSave(c,s);assert.deepEqual((await decodeSave(c,saved.text)).session,s);
  const bad=structuredClone(c);bad.activities.find(a=>a.id==='rest').baseEffects.push(rule('setState','missing'));
  let initial=start(c);initial=send(c,initial,{type:'setSchedule',activityIds:['rest','rest','rest']});const before=JSON.stringify(initial);
  const result=dispatchSession(bad,initial,{type:'executeActivity'},initial.state.stateRevision);assert.equal(result.ok,false);assert.equal(result.error.code,'UNKNOWN_STATE_VALUE');assert.equal(JSON.stringify(initial),before);
});

test('unknown groups/values, duplicate IDs, wrong scope, initial value and orphan references are rejected',async()=>{
  for(const change of [p=>doc(p,'project').stateGroups.push(group()),p=>doc(p,'project').stateGroups[0].values.push({id:'idle',name:'중복'}),p=>doc(p,'project').stateGroups[0].initialValueId='missing',p=>doc(p,'project').stateGroups[0].scope='character',p=>doc(p,'activity','rest').baseEffects.push(rule('setState','missing')),p=>doc(p,'activity','rest').baseEffects.push({...rule('setState','idle'),stateGroupId:'missing'}),p=>doc(p,'activity','rest').baseEffects.push(rule('setState','idle',{kind:'actor'}))]){
    const p=structuredClone(loaded);p.documents=p.documents.filter(d=>d.kind!=='scenario');doc(p,'project').stateGroups=[group()];change(p);const result=await buildSnapshot(p);assert.equal(result.ok,false);assert.ok(result.diagnostics.some(d=>/stateGroups|stateGroupId|stateValueId|owner/.test(d.pointer)),JSON.stringify(result.diagnostics));
  }
});

test('legacy projects preserve state shape and saved sessions',async()=>{
  const c=await content();const s=start(c);assert.equal('customStates' in s.state,false);const saved=await encodeSave(c,s);assert.deepEqual((await decodeSave(c,saved.text)).session,s);
});

test('generic actions use state requirements, charge once and preserve state on rejected requests',async()=>{
  const c=await content(p=>{const project=doc(p,'project');project.stateGroups=[group()];project.gameplay={startLocationId:'home',locations:[{id:'home',name:'집',connections:[]}],items:[],shops:[],interactions:[],actions:[
    {id:'train',name:'수련 시작',costs:[{attributeId:'money',amount:2}],effects:[rule('setState','training')],requirements:rule('stateIs','idle'),rejectNoEffect:true},
    {id:'banquet',name:'연회',costs:[{attributeId:'money',amount:999}],effects:[rule('setState','banquet')]}
  ]};});
  let s=start(c),ref=createRun(c,42,{mode:'full'}).state;const command={type:'executeAction',actionId:'train',quantity:1};const before=s.state.attributes.money;
  s=send(c,s,command);const result=dispatch(c,ref,command,ref.stateRevision);assert.equal(result.ok,true);assert.deepEqual(result.state,s.state);
  assert.equal(s.state.attributes.money,before-2);assert.equal(s.state.customStates.shared.mode,'training');
  for(const actionId of ['train','banquet']){const snapshot=JSON.stringify(s);const rejected=dispatchSession(c,s,{type:'executeAction',actionId,quantity:1},s.state.stateRevision);assert.equal(rejected.ok,false);assert.equal(JSON.stringify(s),snapshot);}
  const saved=await encodeSave(c,s);assert.deepEqual((await decodeSave(c,saved.text)).session,s);
});

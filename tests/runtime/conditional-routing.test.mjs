import test from 'node:test';
import assert from 'node:assert/strict';
import {content,doc,start,send,loaded,replaceEvents} from './helpers.mjs';
import {createVisual} from '../../dist/packages/visual/model.js';
import {explainScreen,resolveScreen} from '../../dist/packages/visual/routing.js';
import {projectDataUi} from '../../dist/packages/visual/data-ui-projection.js';
import {nativeRequest} from '../../dist/packages/native-bridge/index.js';
import {evaluateCondition} from '../../dist/packages/core/evaluation.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/index.js';
import {buildSnapshot} from '../../dist/packages/project-service/index.js';
import {RuleDebugSession} from '../../dist/packages/rule-debug/index.js';

const when={op:'stateIs',stateGroupId:'mode',stateValueId:'training'};
const configure=p=>{
  const project=doc(p,'project');project.stateGroups=[{id:'mode',name:'모드',scope:'shared',initialValueId:'idle',values:[{id:'idle',name:'평상시'},{id:'training',name:'수련'}]}];
  project.visual=createVisual();project.visual.routing={defaultScreenId:'title',phases:[{phase:'planning',screenId:'schedule'},{phase:'dialogue',screenId:'story'}],conditions:[{label:'수련',screenId:'ending',when,phases:['activity-result']},{label:'겹치는 수련',screenId:'story',when}]};
  doc(p,'activity','study').baseEffects.push({op:'setState',stateGroupId:'mode',stateValueId:'training'});replaceEvents(p,[]);
};
const evaluate=(c,s)=>projectDataUi(c,s,null,nativeRequest).conditions;
test('ordered routing uses native predicates, reports collisions, respects phase scope and is pure',async()=>{
  const c=await content(configure),d=c.project.visual;let s=start(c);const before=structuredClone(s);
  assert.equal(resolveScreen(d,s.state,undefined,evaluate(c,s.state)),'schedule');assert.deepEqual(s,before);
  s=send(c,s,{type:'setSchedule',activityIds:['study','rest','study']});s=send(c,s,{type:'executeActivity'});
  const conditions=evaluate(c,s.state);for(const r of d.routing.conditions)assert.equal(conditions[JSON.stringify(r.when)],evaluateCondition(c,s.state,r.when));
  const decision=explainScreen(d,s.state,undefined,conditions);assert.equal(decision.screenId,'ending');assert.equal(decision.candidates[0].selected,true);assert.match(decision.candidates[1].reason,/상위 연결/);
  const debuggerSession=new RuleDebugSession(c,1);debuggerSession.command({type:'setSchedule',activityIds:['study','rest','study']},0);debuggerSession.command({type:'executeActivity'},1);
  assert.deepEqual(debuggerSession.view().projection.routing,decision);
  s=send(c,s,{type:'acknowledgeResult'});assert.equal(resolveScreen(d,s.state,undefined,evaluate(c,s.state)),'story');
  assert.equal(resolveScreen(d,null,undefined,evaluate(c,null)),'title');
});
test('explicit navigation survives save and outranks conditions until the existing reset boundary',async()=>{
  const c=await content(configure),d=c.project.visual;let s=start(c);
  for(const command of [{type:'setSchedule',activityIds:['study','rest','study']},{type:'executeActivity'},{type:'navigate',screenId:'title'}])s=send(c,s,command);
  assert.equal(explainScreen(d,s.state,undefined,evaluate(c,s.state)).candidates[0].source,'explicit');
  const saved=await encodeSave(c,s),restored=await decodeSave(c,saved.text);assert.equal(restored.ok,true);assert.deepEqual(restored.session,s);
  assert.equal(resolveScreen(d,restored.session.state,undefined,evaluate(c,restored.session.state)),'title');
  const next=send(c,s,{type:'acknowledgeResult'}),again=send(c,restored.session,{type:'acknowledgeResult'});assert.deepEqual(next,again);assert.equal(next.state.uiScreenId,undefined);assert.equal(resolveScreen(d,next.state,undefined,evaluate(c,next.state)),'story');
});
test('location, phase, default and legacy routing retain their priority without authored conditions',async()=>{
  const c=await content(),s=start(c).state,d=createVisual();assert.equal(resolveScreen(d,s),'schedule');
  d.routing={defaultScreenId:'title',phases:[{phase:'planning',screenId:'story'}],useLocationScreens:true};
  const state={...s,gameplay:{...s.gameplay,screenId:'ending'}};assert.equal(resolveScreen(d,state),'ending');d.routing.useLocationScreens=false;assert.equal(resolveScreen(d,state),'story');d.routing.phases=[];assert.equal(resolveScreen(d,state),'title');
});
test('route targets and designer-state references are validated with exact paths; no actor context in screen rules',async()=>{
  for(const mutation of [r=>r.screenId='missing',r=>r.when.stateValueId='missing',r=>r.when.owner={kind:'actor'}]){
    const p=structuredClone(loaded);configure(p);const r=doc(p,'project').visual.routing.conditions[0];mutation(r);const result=await buildSnapshot(p);
    assert.equal(result.ok,false);assert.ok(result.diagnostics.some(d=>d.pointer?.startsWith('/visual/routing/conditions/0')||d.path?.startsWith('/visual/routing/conditions/0')),JSON.stringify(result.diagnostics));
  }
  const p=structuredClone(loaded);configure(p);doc(p,'project').visual.screens.find(s=>s.id==='ending').modal=true;assert.equal((await buildSnapshot(p)).ok,false);
});

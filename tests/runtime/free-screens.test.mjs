import test from 'node:test';
import assert from 'node:assert/strict';
import {content,doc,start,send,scenarios,loaded} from './helpers.mjs';
import {buildSnapshot} from '../../dist/packages/project-service/index.js';
import {encodeSave,decodeSave,runScenario} from '../../dist/packages/runtime-service/index.js';
import {dispatchCommand} from '../../dist/packages/core/run.js';
import {resolveScreen} from '../../dist/packages/visual/routing.js';
import {createVisual} from '../../dist/packages/visual/model.js';
import {dataBindingText,dataBindingValue} from '../../dist/packages/visual/data-binding.js';

const design=()=>({width:1280,height:720,routing:{defaultScreenId:'canvas',phases:[]},screens:[
  {id:'canvas',label:'빈 화면에서 조립',elements:[]},{id:'details',label:'상세 보기',elements:[]}
]});
test('one arbitrary screen accepts schedule, relationships and dialogue without gameplay or reserved IDs',async()=>{
  const c=await content(p=>{const v=design();v.screens.pop();v.screens[0].elements=['schedule','relationships','dialogue','choices'].map((kind,i)=>({id:`e-${i}`,kind,x:10,y:i*100,width:500,height:90}));doc(p,'project').visual=v;});
  const s=start(c).state;
  for(const phase of ['planning','ready','activity-result','period-end','narrative','finished','expedition','battle'])assert.equal(resolveScreen(c.project.visual,{...s,phase},'choice'),'canvas');
  assert.equal(resolveScreen(c.project.visual,null),'canvas');
});
test('manual navigation is journalled, works without gameplay, saves exactly and resets on phase transition',async()=>{
  const c=await content(p=>doc(p,'project').visual=design());let session=start(c);
  assert.equal(session.state.gameplay,undefined);
  const command={type:'navigate',screenId:'details'};
  const ts=dispatchCommand(c,session.state,command,session.state.stateRevision);assert.equal(ts.ok,true);
  session=send(c,session,command);assert.deepEqual(session.state,ts.state);
  assert.equal(resolveScreen(c.project.visual,session.state),'details');
  const saved=await encodeSave(c,session),restored=await decodeSave(c,saved.text);assert.equal(restored.ok,true,JSON.stringify(restored));assert.deepEqual(restored.session,session);
  session=send(c,session,{type:'setSchedule',activityIds:['rest','rest','rest']});assert.equal(session.state.uiScreenId,undefined);assert.equal(resolveScreen(c.project.visual,session.state),'canvas');
});
test('explicit dialogue and choice routes take priority over templates; old projects retain inferred routing',()=>{
  const d=design();d.routing.phases=[{phase:'choice',screenId:'details'}];
  assert.equal(resolveScreen(d,{phase:'narrative'},'choice'),'details');assert.equal(resolveScreen(d,{phase:'narrative'},'dialogue'),'canvas');
  const legacy=createVisual();assert.equal(resolveScreen(legacy,null),'title');assert.equal(resolveScreen(legacy,{phase:'narrative'},'choice'),'story');assert.equal(resolveScreen(legacy,{phase:'finished'}),'ending');assert.equal(resolveScreen(legacy,{phase:'planning',gameplay:{screenId:'home'}}),'home');
});
test('missing and duplicate routes and invalid data targets have actionable diagnostics',async()=>{
  const p=structuredClone(loaded);p.documents=p.documents.filter(d=>d.kind!=='scenario');const v=design();
  v.routing.phases=[{phase:'choice',screenId:'missing'},{phase:'choice',screenId:'canvas'}];
  v.screens[0].elements=[{id:'bad',kind:'text',x:10,y:10,width:200,height:40,binding:'data-value',dataBinding:{kind:'relationship',characterId:'mentor',relationshipId:'missing'}}];doc(p,'project').visual=v;
  const r=await buildSnapshot(p);assert.equal(r.ok,false);for(const code of ['UI_ROUTE_REFERENCE','UI_ROUTE_DUPLICATE','UI_BINDING_REFERENCE'])assert.ok(r.diagnostics.some(d=>d.code===code),JSON.stringify(r.diagnostics));
});
test('typed data binding projects shared and individual values without mutating state',()=>{
  const ctx={stats:[{id:'power',label:'내공',value:4}],relationships:[{characterId:'mentor',relationshipId:'trust',name:'스승',label:'신뢰',value:3}],items:[{id:'tea',name:'차',initialQuantity:0}],gameplay:{inventory:[{id:'tea',quantity:2}]},dataState:{roster:{attributes:{child:{power:8}},relationships:{child:{mentor:{trust:7}}},inventory:{child:{tea:5}}}}};
  const before=structuredClone(ctx);
  for(const [target,label,shared,owned] of [[{kind:'attribute',attributeId:'power'},'내공',4,8],[{kind:'relationship',characterId:'mentor',relationshipId:'trust'},'스승 · 신뢰',3,7],[{kind:'inventory',itemId:'tea'},'차',2,5]]){
    assert.equal(dataBindingText({binding:'data-value',dataBinding:target},ctx),`${label}: ${shared}`);
    assert.equal(dataBindingText({binding:'data-value',dataBinding:{...target,owner:{kind:'character',characterId:'child'}}},ctx),`${label}: ${owned}`);
    assert.equal(dataBindingValue({kind:'meter',dataBinding:target},ctx),shared);
    assert.equal(dataBindingValue({kind:'meter',dataBinding:{...target,owner:{kind:'character',characterId:'child'}}},ctx),owned);
  }assert.deepEqual(ctx,before);
  assert.equal(dataBindingValue({kind:'meter',value:75},ctx),undefined);
  assert.equal(dataBindingValue({kind:'meter',dataBinding:{kind:'attribute',attributeId:'missing'}},ctx),undefined);
});

test('gauges accept typed numeric bindings and thin tracks; invalid owners still fail',async()=>{
  const p=structuredClone(loaded);p.documents=p.documents.filter(d=>d.kind!=='scenario');
  const attribute=p.documents.find(d=>d.kind==='attributes').data.attributes[0].id;
  const v=design();v.screens[0].elements=[{id:'gauge',kind:'meter',x:10,y:10,width:200,height:24,meterThickness:4,maxValue:100,binding:'data-value',dataBinding:{kind:'attribute',attributeId:attribute}}];doc(p,'project').visual=v;
  const ok=await buildSnapshot(p);assert.equal(ok.ok,true,JSON.stringify(ok.diagnostics));
  v.screens[0].elements[0].dataBinding.owner={kind:'character',characterId:'missing'};
  const bad=await buildSnapshot(p);assert.equal(bad.ok,false);assert.ok(bad.diagnostics.some(d=>d.code==='UI_DATA_OWNER'));
});
test('both legacy scenarios and arbitrary-screen scenarios reach endings and restore saves',async()=>{
  for(const custom of [false,true]){
    const c=await content(p=>{if(custom)doc(p,'project').visual=design();});
    for(const scenario of scenarios){const r=await runScenario(c,scenario);assert.equal(r.ok,true);assert.equal(r.session.state.phase,'finished');
      let session=r.session;if(custom)session=send(c,session,{type:'navigate',screenId:'details'});
      const saved=await encodeSave(c,session),restored=await decodeSave(c,saved.text);assert.equal(restored.ok,true,JSON.stringify(restored));assert.deepEqual(restored.session,session);
    }
  }
});

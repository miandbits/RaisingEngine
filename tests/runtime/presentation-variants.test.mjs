import test from 'node:test';
import assert from 'node:assert/strict';
import {content,doc,start,send,loaded} from './helpers.mjs';
import {buildSnapshot} from '../../dist/packages/project-service/index.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/index.js';
import {resolvePresentation,presentationActivity} from '../../dist/packages/visual/variants.js';
import {projectDataUi} from '../../dist/packages/visual/data-ui-projection.js';
import {nativeRequest} from '../../dist/packages/native-bridge/index.js';
import {evaluateCondition} from '../../dist/packages/core/evaluation.js';
import {pluginElements} from '../../dist/packages/plugin-host/bundle.js';

const base=()=>({id:'status',kind:'text',text:'기본 모습',binding:'title',x:0,y:0,width:300,height:60,variants:[
  {label:'휴식',when:{op:'always'},activityIds:['rest'],values:{text:'잠시 쉰다'}},
  {label:'공부',when:{op:'always'},activityIds:['study'],values:{text:'책을 읽는다'}}
]});
const design=e=>({width:1280,height:720,routing:{defaultScreenId:'canvas',phases:[]},screens:[{id:'canvas',label:'공용 화면',elements:[e]}]});
test('first match wins; nonmatching state restores authored base and never mutates it',()=>{
  const e=base(),before=structuredClone(e);
  e.variants.push({label:'lower priority',when:{op:'always'},values:{text:'fallback'}});
  assert.equal(resolvePresentation(e,'rest',()=>true).text,'잠시 쉰다');
  assert.equal(resolvePresentation(e,'study',()=>true).text,'책을 읽는다');
  assert.equal(resolvePresentation(e,undefined,()=>true).text,'fallback');
  assert.equal(resolvePresentation(e,'rest',()=>false),e);
  assert.equal(resolvePresentation(e,'rest',()=>true).binding,'none');
  assert.deepEqual(e.variants.slice(0,2),before.variants);
});
test('activity selector does not leak stale results into planning, dialogue or ending',()=>{
  for(const phase of ['planning','ready','narrative','finished','battle'])assert.equal(presentationActivity({phase,lastActivityResult:{activityId:'rest'}}),undefined);
  assert.equal(presentationActivity({phase:'activity-result',lastActivityResult:{activityId:'rest'}}),'rest');
  assert.equal(presentationActivity(null),undefined);
});
test('projection evaluates authored conditions through native rules and matches reference without draws',async()=>{
  const e=base();e.variants[0].when={op:'compare',comparator:'gte',left:{op:'literal',value:2},right:{op:'literal',value:1}};
  const c=await content(p=>doc(p,'project').visual=design(e));let session=start(c);
  session=send(c,session,{type:'setSchedule',activityIds:['rest','study','rest']});session=send(c,session,{type:'executeActivity'});
  const before=JSON.stringify(session),ui=projectDataUi(c,session.state,null,nativeRequest);
  const actual=resolvePresentation(c.project.visual.screens[0].elements[0],presentationActivity(session.state),when=>ui.conditions[JSON.stringify(when)]===true);
  const reference=resolvePresentation(e,presentationActivity(session.state),when=>evaluateCondition(c,session.state,when));
  assert.equal(actual.text,'잠시 쉰다');assert.deepEqual(actual,reference);assert.equal(JSON.stringify(session),before);
  const save=await encodeSave(c,session),restored=await decodeSave(c,save.text);assert.equal(restored.ok,true);assert.deepEqual(restored.session,session);
  assert.deepEqual(resolvePresentation(e,presentationActivity(restored.session.state),()=>true),actual);
});
test('image and plugin choices preserve placement, base reference and unrelated plugin properties',()=>{
  const image={...base(),kind:'image',imageSource:'background',imageAssetId:'old',variants:[{label:'image',when:{op:'always'},values:{imageAssetId:'new'}}]};
  const changed=resolvePresentation(image,undefined,()=>true);assert.equal(changed.imageAssetId,'new');assert.equal(changed.imageSource,'asset');assert.equal(changed.width,image.width);assert.equal(image.imageAssetId,'old');
  const plugin={...base(),kind:'plugin',extension:{plugin:'cutout-animation',type:'character',ref:'idle',props:{scale:1.2}},variants:[{label:'motion',when:{op:'always'},values:{extensionRef:'training'}}]};
  assert.deepEqual(resolvePresentation(plugin,undefined,()=>true).extension,{...plugin.extension,ref:'training'});assert.equal(plugin.extension.ref,'idle');
  const refs=pluginElements(design(plugin));assert.deepEqual(refs.map(x=>x.element.extension.ref),['idle','training']);
});
test('missing activity/image and wrong element overrides report precise rule locations',async()=>{
  const p=structuredClone(loaded);p.documents=p.documents.filter(d=>d.kind!=='scenario');const e=base();e.variants[0].activityIds=['missing'];e.variants[0].values={imageAssetId:'missing'};doc(p,'project').visual=design(e);
  const result=await buildSnapshot(p);assert.equal(result.ok,false);
  for(const suffix of ['/variants/0/activityIds/0','/variants/0/values/imageAssetId'])assert.ok(result.diagnostics.some(d=>d.pointer.endsWith(suffix)),JSON.stringify(result.diagnostics));
  assert.ok(result.diagnostics.some(d=>d.code==='UI_VARIANT_KIND'));
});
test('schema rejects gameplay changes inside a presentation rule',async()=>{
  const p=structuredClone(loaded);p.documents=p.documents.filter(d=>d.kind!=='scenario');const e=base();e.variants[0].values={action:'start'};doc(p,'project').visual=design(e);
  const result=await buildSnapshot(p);assert.equal(result.ok,false);assert.ok(result.diagnostics.some(d=>d.code==='SCHEMA_INVALID'));
});

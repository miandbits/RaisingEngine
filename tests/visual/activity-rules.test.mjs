import test from 'node:test';
import assert from 'node:assert/strict';
import {activityRuleLines} from '../../dist/packages/visual/activity-rules.js';

test('activity rules identify personal/shared owners without evaluating outcomes or leaking flags',()=>{
  const context={attributes:[{id:'money',label:'Funds'},{id:'skill',label:'Craft'}],characters:[{id:'a',name:'One'},{id:'b',name:'Two'}],actorId:'a',localAttributeIds:['skill']};
  const activity={costs:[{attributeId:'money',amount:4},{attributeId:'skill',amount:2}],baseEffects:[
    {op:'add',target:{kind:'attribute',attributeId:'skill'},value:{op:'literal',value:4}},
    {op:'add',target:{kind:'attribute',attributeId:'skill',owner:{kind:'character',characterId:'b'}},value:{op:'literal',value:-2}},
    {op:'set',target:{kind:'attribute',attributeId:'money'},value:{op:'read',target:{kind:'attribute',attributeId:'skill'}}},
    {op:'setFlag',flagId:'secret-ending-flag',value:true}
  ],successCheck:{chance:{op:'literal',value:50}}};
  const before=JSON.stringify({activity,context});const lines=activityRuleLines(activity,context).join('\n');
  assert.match(lines,/공용 · Funds 4/);assert.match(lines,/One · Craft 2/);assert.match(lines,/One · Craft \+4/);assert.match(lines,/Two · Craft -2/);
  assert.match(lines,/공용 · Funds 실행 시 계산/);assert.match(lines,/그 밖의 상태/);assert.match(lines,/성공·실패/);assert.match(lines,/최종 결과 예측 아님/);
  for(const [locale,label] of [['en','Authored rules'],['ja','ルールの要約']]){const localized=activityRuleLines(activity,{...context,locale}).join('\n');assert.ok(localized.includes(label));assert.doesNotMatch(localized,/secret-ending-flag|공용|비용|실행 시 계산/);}
  assert.doesNotMatch(lines,/secret-ending-flag/);assert.equal(JSON.stringify({activity,context}),before);
  assert.match(activityRuleLines({costs:[],baseEffects:[]},context).join('\n'),/비용 · 없음/);
});

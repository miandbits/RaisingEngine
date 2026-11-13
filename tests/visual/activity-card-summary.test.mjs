import test from 'node:test';
import assert from 'node:assert/strict';
import {build} from 'esbuild';
const compiled=await build({entryPoints:['packages/visual/activity-summary.ts'],bundle:true,write:false,format:'esm',platform:'node'});
const {activityCardSummary,nextUnfilledDraftSlot}=await import('data:text/javascript;base64,'+Buffer.from(compiled.outputFiles[0].text).toString('base64'));
const literal=(id,value,op='add')=>({op,target:{kind:'attribute',attributeId:id},value:{op:'literal',value}});
test('card chips preserve distinct ordered literal costs/effects and disclose computed, scoped and outcome rules',()=>{
 const activity={costs:[{attributeId:'money',amount:4}],baseEffects:[literal('energy',-3),literal('skill',7),literal('money',20,'set'),{op:'add',target:{kind:'attribute',attributeId:'skill'},value:{op:'read',target:{kind:'attribute',attributeId:'skill'}}},{op:'setFlag',flagId:'secret-outcome',value:true},{...literal('skill',1),target:{kind:'attribute',attributeId:'skill',owner:{kind:'character',characterId:'hidden-person'}}}],successCheck:{attributeId:'skill'}};
 const before=JSON.stringify(activity),attributes=[{id:'skill',label:'Craft'},{id:'energy',label:'Energy'},{id:'money',label:'Coins'}];
 const result=activityCardSummary(activity,attributes,['money','energy'],3);
 assert.deepEqual(result.chips.map(c=>[c.kind,c.text]),[['cost','Coins -4'],['effect','Coins = 20'],['effect','Energy -3']]);
 assert.deepEqual({...result,chips:[]},{chips:[],remaining:1,computed:true,other:true,conditional:true});assert.doesNotMatch(JSON.stringify(result),/secret-outcome|hidden-person/);assert.equal(JSON.stringify(activity),before);
 assert.equal(activityCardSummary({},attributes).chips.length,0);
});
test('after Apply selects the next unfilled editable slot, wraps, skips completed periods and stays on a complete draft',()=>{
 assert.equal(nextUnfilledDraftSlot(1,7,0,4),3);assert.equal(nextUnfilledDraftSlot(3,10,0,4),0);assert.equal(nextUnfilledDraftSlot(3,10,2,4),2);assert.equal(nextUnfilledDraftSlot(3,15,0,4),3);
 for(let count=1;count<=12;count++)assert.equal(nextUnfilledDraftSlot(count-1,(1<<count)-1,0,count),count-1);
});

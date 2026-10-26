import test from 'node:test';import assert from 'node:assert/strict';
import {activityGrowthRows,growthWords} from '../../dist/packages/visual/growth-summary.js';
import {planActivityTimeline,createTimelineCursor,resultPlaybackText,skipTimeline} from '../../dist/packages/visual/timeline.js';
import {scheduleStep} from '../../dist/packages/player-service/schedule.js';
const change=(target,before,after)=>({kind:'numeric',source:'/effect',target,before,after,delta:after-before,operand:after-before,requested:after,clamped:false});
const result=changes=>({activityId:'study',time:{periodIndex:0,year:1,month:1,periodInMonth:0},success:null,changes});
const freeze=o=>{if(o&&typeof o==='object'){Object.values(o).forEach(freeze);Object.freeze(o);}return o;};
test('growth aggregates committed first/last values by complete target identity and never alters inputs',()=>{
 const input=freeze(result([change({kind:'attribute',attributeId:'skill'},10,15),change({attributeId:'skill',kind:'attribute'},15,13),change({kind:'attribute',attributeId:'skill',actorId:'a'},20,25),change({kind:'attribute',attributeId:'skill',actorId:'b'},20,19),change({kind:'attribute',attributeId:'skill',owner:{kind:'character',characterId:'a'}},40,41),change({kind:'attribute',attributeId:'skill',owner:{characterId:'a',kind:'character'}},41,44),change({kind:'inventory',itemId:'book'},0,1),change({kind:'relationship',characterId:'a',relationshipId:'trust'},5,6),{kind:'flag',flagId:'secret',before:false,after:true}]));
 const before=JSON.stringify(input),rows=activityGrowthRows(input);assert.deepEqual(rows.map(r=>[r.before,r.after,r.delta]),[[10,13,3],[20,25,5],[20,19,-1],[40,44,4],[0,1,1],[5,6,1]]);assert.equal(new Set(rows.map(r=>r.key)).size,6);assert.equal(JSON.stringify(input),before);rows[0].target.attributeId='detached';assert.equal(input.changes[0].target.attributeId,'skill');
});
test('net zero and nonnumeric-only activity results yield explicit localized no-change states',()=>{
 assert.deepEqual(activityGrowthRows(result([change({kind:'attribute',attributeId:'health'},10,15),change({kind:'attribute',attributeId:'health'},15,10)])),[]);
 assert.deepEqual(activityGrowthRows(result([{kind:'flag',flagId:'found',before:false,after:true}])),[]);
 assert.equal(growthWords('en-GB').scope,'This activity');assert.equal(growthWords('ja-JP').none,'数値の差分なし');
});
test('review-result is opt-in, skip reveals committed result and never auto acknowledges it',()=>{
 const content={project:{visual:{resultPlayback:{leadInMs:100,changeMs:100,holdMs:100}}},activities:[{id:'study',label:'Authored name'}],attributes:[{id:'skill',label:'Authored skill'}],characters:[]};
 const state=freeze({phase:'activity-result',stateRevision:3,schedule:['study'],lastActivityResult:{...result([change({kind:'attribute',attributeId:'skill'},0,2)]),success:{passed:true}}}),before=JSON.stringify(state);
 const plan=planActivityTimeline(content,state,'en'),done=skipTimeline(plan,createTimelineCursor(plan));assert.match(resultPlaybackText(plan,done,'en'),/Authored name · Success/);assert.doesNotMatch(resultPlaybackText(plan,done,'en'),/[가-힣]/);assert.match(resultPlaybackText(plan,done,'ja'),/成功/);
 assert.equal(scheduleStep(state,'continuous',{cursor:done}),'continue');assert.equal(scheduleStep(state,'continuous',{cursor:done},'reveal-result'),null);assert.equal(scheduleStep({...state,phase:'ready'},'continuous',null,'reveal-result'),'execute');assert.equal(JSON.stringify(state),before);
});

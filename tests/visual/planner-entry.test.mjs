import test from 'node:test';
import assert from 'node:assert/strict';
import {plannerEntryError} from '../../dist/packages/visual/planner-entry.js';
import {widgetCommand} from '../../dist/packages/visual/commands.js';
const board={x:0,y:0,width:500,height:500,id:'plan',kind:'schedule',scheduleLayout:'activity-board',scheduleGroups:[{id:'learn',label:'Learn',activityIds:['study']}]};
const design={width:1280,height:720,routing:{defaultScreenId:'home',phases:[]},screens:[{id:'home',elements:[]},{id:'planner',elements:[board]}]};
const state={phase:'planning',periodIndex:0,attributes:{money:30},rng:{state:7}};
const activities=[{id:'study'},{id:'rest'}];
const entry={type:'openPlanner',screenId:'planner',plannerElementId:'plan',plannerGroupId:'learn',plannerActivityId:'study'};
test('contextual planner validates presentation targets without touching simulation or draft',()=>{
 const before=structuredClone({design,state,activities,entry});
 assert.equal(plannerEntryError(design,state,activities,entry),null);
 assert.equal(plannerEntryError(design,{...state,phase:'ready'},activities,entry),null);
 assert.equal(plannerEntryError(design,state,activities,{type:'openPlanner',screenId:'planner',plannerElementId:'plan'}),null);
 assert.deepEqual({design,state,activities,entry},before);
 assert.deepEqual(widgetCommand({action:'openPlanner',...entry}),entry);
});
test('contextual planner refuses stale, mismatched, modal, missing-routing and non-planning destinations',()=>{
 for(const override of [{screenId:'missing'},{plannerElementId:'missing'},{plannerGroupId:'missing'},{plannerActivityId:'rest'}])assert.ok(plannerEntryError(design,state,activities,{...entry,...override}));
 for(const phase of ['narrative','activity-result','finished'])assert.ok(plannerEntryError(design,{...state,phase},activities,entry));
 assert.ok(plannerEntryError(design,null,activities,entry));
 assert.ok(plannerEntryError(design,{...state,roster:{}},activities,entry));
 assert.ok(plannerEntryError({...design,routing:undefined},state,activities,entry));
 assert.ok(plannerEntryError({...design,screens:[{id:'planner',modal:true,elements:[board]}]},state,activities,entry));
 assert.ok(plannerEntryError({...design,screens:[{id:'planner',elements:[{...board,scheduleLayout:'columns'}]}]},state,activities,entry));
});

test('hidden and disabled boards fail before navigation, including ancestor conditions',()=>{
 const condition={op:'not',condition:{op:'always'}};
 const variant=element=>({...design,screens:[{id:'planner',elements:[element]}]});
 for(const override of [{hidden:true},{opacity:0},{visibleWhen:'result'},{enabledCondition:condition}])assert.ok(plannerEntryError(variant({...board,...override}),state,activities,entry,()=>false));
 const grouped={...design,screens:[{id:'planner',elements:[{id:'group',kind:'group',x:0,y:0,width:600,height:600,enabledCondition:condition},{...board,parentId:'group'}]}]};
 assert.ok(plannerEntryError(grouped,state,activities,entry,()=>false));
 assert.equal(plannerEntryError(grouped,state,activities,entry,()=>true),null);
});

test('contextual entry checks the active presentation opacity before accepting navigation',()=>{
 const target=(opacity,variantOpacity)=>({...design,screens:[{id:'planner',elements:[{...board,opacity,variants:[{id:'visibility',when:{op:'always'},values:{opacity:variantOpacity}}]}]}]});
 const hidden=target(1,0),visible=target(0,1),before=structuredClone({hidden,visible,state,entry});
 assert.match(plannerEntryError(hidden,state,activities,entry,()=>true),/hidden or disabled/);
 assert.equal(plannerEntryError(visible,state,activities,entry,()=>true),null);
 assert.equal(plannerEntryError(hidden,state,activities,entry,()=>false),null,'An inactive variant does not override the visible authored base');
 assert.ok(plannerEntryError(visible,state,activities,entry,()=>false),'An inactive variant does not reveal a hidden authored base');
 assert.deepEqual({hidden,visible,state,entry},before);
});

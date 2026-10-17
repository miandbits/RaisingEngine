import test from 'node:test';
import assert from 'node:assert/strict';
import {resolveScreenElements} from '../../dist/packages/visual/components.js';
import {applyElementContext} from '../../dist/packages/visual/context.js';
import {widgetCommand} from '../../dist/packages/visual/commands.js';
import {presentationActivity,resolvePresentation} from '../../dist/packages/visual/variants.js';
import {dataBindingText} from '../../dist/packages/visual/data-binding.js';

const numeric={kind:'attribute',attributeId:'skill',owner:{kind:'actor'}};
const condition={op:'compare',comparator:'gte',left:{op:'read',target:numeric},right:{op:'literal',value:20}};
const element=(id,kind,rest={})=>({id,kind,x:0,y:0,width:100,height:30,...rest});
function design(){return {width:400,height:300,screens:[{id:'home',elements:[element('one','component',{componentId:'card',uiContext:{actorId:'one',activityId:'train',actionId:'study'}}),element('two','component',{componentId:'card',uiContext:{actorId:'two',activityId:'banquet',actionId:'rest'}})]}],components:[{id:'card',width:100,height:100,elements:[element('skill','text',{binding:'data-value',dataBinding:numeric}),element('button','button',{action:'executeActorAction',contextBindings:['actionId'],enabledCondition:condition}),element('caption','text',{text:'base',variants:[{label:'train',activityIds:['train'],when:{op:'always'},values:{text:'Training'}},{label:'banquet',activityIds:['banquet'],when:{op:'always'},values:{text:'Banquet'}}]})]}]};}
test('two instances bind display, condition, action and presentation independently without changing the definition',()=>{
  const source=design(),before=structuredClone(source),rows=resolveScreenElements(source,'home');
  const state={roster:{attributes:{one:{skill:10},two:{skill:40}}}};
  assert.equal(dataBindingText(rows[0],{stats:[{id:'skill',label:'Skill',value:0}],dataState:state}),'Skill: 10');
  assert.equal(dataBindingText(rows[3],{stats:[{id:'skill',label:'Skill',value:0}],dataState:state}),'Skill: 40');
  assert.equal(rows[1].enabledCondition.left.target.owner.characterId,'one');assert.equal(rows[4].enabledCondition.left.target.owner.characterId,'two');
  assert.deepEqual(widgetCommand(rows[1]),{type:'executeActorAction',actorId:'one',actionId:'study',quantity:1});
  assert.deepEqual(widgetCommand(rows[4]),{type:'executeActorAction',actorId:'two',actionId:'rest',quantity:1});
  assert.equal(resolvePresentation(rows[2],presentationActivity(null,rows[2]),()=>true).text,'Training');assert.equal(resolvePresentation(rows[5],presentationActivity(null,rows[5]),()=>true).text,'Banquet');assert.deepEqual(source,before);
});
test('nested context overrides preserve parent condition ownership, explicit owners and instance isolation',()=>{
  const d=design();d.components[0].elements=[element('parent','group',{enabledCondition:condition,uiContext:{actorId:'parent'}}),element('child','text',{parentId:'parent',uiContext:{actorId:'child'},binding:'data-value',dataBinding:numeric,visibleCondition:condition}),element('fixed','text',{binding:'data-value',dataBinding:{...numeric,owner:{kind:'character',characterId:'fixed'}}})];
  const rows=resolveScreenElements(d,'home');assert.equal(rows[1].enabledCondition.left.target.owner.characterId,'parent');assert.equal(rows[1].visibleCondition.left.target.owner.characterId,'child');assert.equal(rows[1].dataBinding.owner.characterId,'child');assert.equal(rows[2].dataBinding.owner.characterId,'fixed');
});
test('target replacement is explicit and missing inputs disable action instead of reusing old target',()=>{
  const e=element('button','button',{action:'buy',shopId:'shop',itemId:'old',contextBindings:['itemId'],enabledCondition:{op:'compare',comparator:'gte',left:{op:'read',target:{kind:'inventory',itemId:'old',owner:{kind:'character',characterId:'fixed'}}},right:{op:'literal',value:1}}});
  const bound=applyElementContext(e,{actorId:'actor',itemId:'new'});assert.equal(bound.itemId,'new');assert.equal(bound.enabledCondition.left.target.itemId,'new');assert.equal(bound.enabledCondition.left.target.owner.characterId,'fixed');
  const missing=applyElementContext(e);assert.equal(missing.itemId,'');assert.equal(missing.dataEnabled,false);assert.equal(widgetCommand(missing),null);
  assert.deepEqual(applyElementContext(element('old','text',{text:'unchanged'})),element('old','text',{text:'unchanged'}));
});
test('actor activity follows that actor result, not primary result; old callers retain prior behavior',()=>{
  const state={phase:'activity-result',lastActivityResult:{activityId:'train',actorResults:[{characterId:'one',result:{activityId:'train'}},{characterId:'two',result:{activityId:'rest'}}]}};
  assert.equal(presentationActivity(state),'train');assert.equal(presentationActivity(state,{uiContext:{actorId:'two'}}),'rest');assert.equal(presentationActivity({...state,phase:'narrative'},{uiContext:{actorId:'two'}}),undefined);
});
test('activity card assigns its actor without replacing another actor assignment',()=>{
  const e=applyElementContext(element('assign','button',{action:'assignActivity',contextBindings:['activityId']}),{actorId:'two',activityId:'banquet'});
  const state={roster:{assignments:{one:'train',two:'rest'}}};
  assert.deepEqual(widgetCommand(e,state),{type:'assignActivities',assignments:{one:'train',two:'banquet'}});
  assert.equal(state.roster.assignments.two,'rest');
});

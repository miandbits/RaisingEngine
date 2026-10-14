import test from 'node:test';
import assert from 'node:assert/strict';
import {fixture} from '../systems/fixture.mjs';
import {createRun,dispatchCommand} from '../../dist/packages/core/run.js';
import {getSystemsView} from '../../dist/packages/systems/service.js';
import {collectionRows,resolveCollectionRow} from '../../dist/packages/visual/data-ui.js';
import {resolveScreenElements} from '../../dist/packages/visual/components.js';

const meter={id:'hp',kind:'meter',x:0,y:24,width:240,height:24,value:0,maxValue:100};
function configure(p){p.systems.battleHistoryLimit=128;p.visual.components=[{id:'status-card',label:'Status',width:240,height:80,elements:[{id:'name',kind:'text',x:0,y:0,width:240,height:24,text:'Name'},meter]},{id:'action-card',label:'Action',width:240,height:60,elements:[{id:'choose',kind:'button',x:0,y:0,width:240,height:60,text:'Choose',action:'sound'}]}];p.visual.screens.find(x=>x.id==='home').elements=[{id:'actors',kind:'collection',componentId:'status-card',source:{type:'battleActors'},x:0,y:0,width:480,height:180,bindings:[{elementId:'name',property:'text',field:'name'},{elementId:'hp',property:'value',field:'hp'},{elementId:'hp',property:'maxValue',field:'maxHp'},{elementId:'hp',property:'text',field:'hpText'}]},{id:'commands',kind:'collection',componentId:'action-card',source:{type:'battleActions'},x:0,y:200,width:480,height:200,bindings:[{elementId:'choose',property:'text',field:'displayName'},{elementId:'choose',property:'action',field:'action'},{elementId:'choose',property:'battleAction',field:'battleAction'},{elementId:'choose',property:'itemId',field:'itemId'}]}];}
async function setup(mutate=()=>{}){const f=await fixture(p=>{configure(p);mutate(p)});assert.equal(f.built.ok,true,JSON.stringify(f.built.diagnostics));return f.built.snapshot;}
function send(c,s,command){const r=dispatchCommand(c,s,command,s.stateRevision);assert.equal(r.ok,true,JSON.stringify(r.error));return r.state;}
function enter(c){let s=createRun(c,23,{mode:'full'}).state;s=send(c,s,{type:'equip',equipmentId:'iron-sword'});s=send(c,s,{type:'setSchedule',activityIds:['forest-trip','rest','rest']});s=send(c,s,{type:'executeActivity'});s=send(c,s,{type:'moveExpedition',nodeId:'clearing'});assert.equal(s.phase,'battle');return s;}
function rows(c,s,type,selection=null){return collectionRows(c.project.visual,{kind:'collection',source:{type}},{content:c,state:s,gameplay:null,systems:getSystemsView(c,s),battleSelection:selection});}
test('authored status meters and action cards read canonical one-versus-one battle state',async()=>{
 const c=await setup(),s=enter(c),before=JSON.stringify(s),actors=rows(c,s,'battleActors');assert.deepEqual(actors.map(x=>x.id),['self','enemy']);assert.equal(actors[0].values.hp,60);assert.equal(actors[1].values.name,'숲 슬라임');
 const e=c.project.visual.screens.find(x=>x.id==='home').elements[0],resolved=resolveCollectionRow(c.project.visual,e,actors[1],240);assert.equal(resolved[1].kind,'meter');assert.equal(resolved[1].value,16);assert.equal(resolved[1].maxValue,16);assert.equal(resolved[1].text,'16 / 16');
 const actions=rows(c,s,'battleActions');assert.deepEqual(actions.map(x=>x.id),['attack','guard','flee','use-item--potion']);assert.equal(actions[0].enabled,true);assert.equal(actions[3].enabled,false);assert.ok(actions[3].reason);assert.equal(JSON.stringify(s),before);
});
test('targets expose only the selected action canonical target, with stale selections cleared',async()=>{
 const c=await setup(),s=enter(c),selected={stateRevision:s.stateRevision,battleAction:'attack'};assert.deepEqual(rows(c,s,'battleTargets'),[]);assert.deepEqual(rows(c,s,'battleTargets',selected).map(x=>x.id),['enemy']);
 assert.equal(rows(c,s,'battleTargets',{...selected,target:'enemy'})[0].values.displayName,'✓ 숲 슬라임');assert.deepEqual(rows(c,s,'battleTargets',{...selected,battleAction:'guard'}).map(x=>x.id),['self']);assert.deepEqual(rows(c,s,'battleTargets',{...selected,battleAction:'flee'}),[]);assert.deepEqual(rows(c,s,'battleTargets',{...selected,stateRevision:0}),[]);
 assert.equal(rows(c,s,'battleActions',selected)[0].values.displayName,'✓ 공격');assert.equal(rows(c,s,'battleActions',selected)[1].values.selected,false);
});
test('committed turn logs and actual capped rewards remain visible after battle; no rewards estimated',async()=>{
 const c=await setup(p=>{p.gameplay.items.find(x=>x.id==='gem').initialQuantity=2;p.systems.enemies[0].rewards.effects=[{op:'add',target:{kind:'attribute',attributeId:'money'},value:{op:'literal',value:3}}];});let s=enter(c);assert.deepEqual(rows(c,s,'battleRewards'),[]);
 s=send(c,s,{type:'battleAction',action:'guard'});let log=rows(c,s,'battleLog');assert.equal(log.length,1);assert.equal(log[0].values.battleAction,'guard');assert.ok(log[0].values.heroDamage>0);
 s=send(c,s,{type:'battleAction',action:'useItem',itemId:'potion'});assert.ok(rows(c,s,'battleLog')[1].values.healed>0);
 while(s.phase==='battle')s=send(c,s,{type:'battleAction',action:'attack'});assert.equal(s.systems.lastBattle.outcome,'victory');log=rows(c,s,'battleLog');assert.equal(log.at(-1).values.outcome,'victory');
 const reward=rows(c,s,'battleRewards'),gem=reward.find(x=>x.values.name==='보석');assert.equal(gem.values.amount,0);assert.equal(gem.values.discarded,1);assert.match(gem.values.text,/버림 1/);assert.ok(reward.some(x=>x.values.kind==='attribute'&&x.values.amount===3));assert.deepEqual(rows(c,s,'battleActors'),[]);
});
test('battle collection filters and stable identity survive changed HP and meter scaling',async()=>{
 const c=await setup();let s=enter(c);const element=c.project.visual.screens.find(x=>x.id==='home').elements[0],before=rows(c,s,'battleActors')[0];s=send(c,s,{type:'battleAction',action:'guard'});const after=rows(c,s,'battleActors')[0];assert.equal(before.id,after.id);assert.ok(after.values.hp<before.values.hp);assert.equal(resolveCollectionRow(c.project.visual,element,before,300)[1].id,resolveCollectionRow(c.project.visual,element,after,300)[1].id);
 const filtered=collectionRows(c.project.visual,{...element,rowFilter:{field:'side',operator:'eq',value:'enemy'}},{content:c,state:s,gameplay:null,systems:getSystemsView(c,s)});assert.deepEqual(filtered.map(x=>x.id),['enemy']);
});
test('semantic validation rejects incompatible meter bindings and history without recording',async()=>{
 const f=await fixture(p=>{configure(p);delete p.systems.battleHistoryLimit;const e=p.visual.screens.find(x=>x.id==='home').elements[0];e.bindings[1].field='name';e.bindings.push({elementId:'name',property:'maxValue',field:'maxHp'});p.visual.screens.find(x=>x.id==='home').elements.push({...e,id:'logs',source:{type:'battleLog'},bindings:[]});});assert.equal(f.built.ok,false);for(const code of ['UI_BINDING_TYPE','UI_BINDING_KIND','UI_BATTLE_HISTORY_REQUIRED'])assert.ok(f.built.diagnostics.some(x=>x.code===code),code);
});
test('reusable component overrides preserve fractional meters and explicit battle targets',async()=>{
 const c=await setup(p=>{p.visual.screens.find(x=>x.id==='home').elements=[{id:'status-instance',kind:'component',componentId:'status-card',x:0,y:0,width:240,height:80,overrides:[{elementId:'hp',values:{value:25.5,maxValue:100.75}}]},{id:'target-instance',kind:'component',componentId:'action-card',x:0,y:100,width:240,height:60,overrides:[{elementId:'choose',values:{action:'selectBattleTarget',battleTarget:'enemy'}}]}];});
 const elements=resolveScreenElements(c.project.visual,'home');assert.equal(elements.find(x=>x.kind==='meter').value,25.5);assert.equal(elements.find(x=>x.kind==='meter').maxValue,100.75);assert.equal(elements.find(x=>x.kind==='button').action,'selectBattleTarget');assert.equal(elements.find(x=>x.kind==='button').battleTarget,'enemy');
});
test('designer table battle targets and zero meter maxima fail semantic validation',async()=>{
 const f=await fixture(p=>{configure(p);p.visual.dataTables=[{id:'custom-hud',name:'Custom HUD',columns:[{id:'target',name:'Target',type:'text'},{id:'maximum',name:'Max',type:'number'}],rows:[{id:'bad',values:{target:'party-member-three',maximum:0}}]}];p.visual.components[1].elements[0].action='selectBattleTarget';p.visual.components[1].elements[0].battleTarget='self';const e=p.visual.screens.find(x=>x.id==='home').elements;e[0].source={type:'table',tableId:'custom-hud'};e[0].bindings=[{elementId:'hp',property:'maxValue',field:'maximum'}];e[1].source={type:'table',tableId:'custom-hud'};e[1].bindings=[{elementId:'choose',property:'battleTarget',field:'target'}];});assert.equal(f.built.ok,false);for(const code of ['UI_BATTLE_TARGET','UI_METER_RANGE'])assert.ok(f.built.diagnostics.some(x=>x.code===code),code);
});

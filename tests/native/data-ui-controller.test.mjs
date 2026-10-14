import test from 'node:test';
import assert from 'node:assert/strict';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import {WasmRuntime} from '../../dist/packages/native-bridge/wasm.js';
import {PlayerController} from '../../dist/packages/player-service/controller.js';
import {WebPlayerController} from '../../dist/packages/web-player/controller.js';
import {resolveCollectionRow} from '../../dist/packages/visual/data-ui.js';
import {projectDataUi} from '../../dist/packages/visual/data-ui-projection.js';
import {widgetCommand} from '../../dist/packages/visual/commands.js';
import {nativeRequest} from '../../dist/packages/native-bridge/index.js';
import {fixture} from '../gameplay/fixture.mjs';

const money=value=>({op:'compare',comparator:'gte',left:{op:'read',target:{kind:'attribute',attributeId:'money'}},right:{op:'literal',value}});
async function sample(t){return fixture(t,p=>{
  p.initialState.attributes.money=100;
  p.gameplay.actions=[{id:'recover',name:'Recover',costs:[{attributeId:'money',amount:4}],effects:[{op:'add',target:{kind:'attribute',attributeId:'stamina'},value:{op:'literal',value:2}}],maxQuantity:3},{id:'blocked',name:'Blocked',requirements:money(99999),costs:[],effects:[]}];
  p.visual.components=[{id:'card',label:'Shared card',width:300,height:130,elements:[{id:'title',kind:'text',x:0,y:0,width:280,height:40,text:'Static',visibleCondition:money(95)},{id:'go',kind:'button',x:0,y:45,width:280,height:60,text:'Apply',action:'sound',enabledCondition:money(5)}]}];
  p.visual.dataTables=[{id:'choices',name:'Choices',columns:[['name','text'],['action','text'],['ability','text'],['qty','number'],['show','boolean'],['active','boolean']].map(([id,type])=>({id,name:id,type})),rows:[
    {id:'rest',values:{name:'Rest twice',action:'executeAction',ability:'recover',qty:2,show:true,active:true}},
    {id:'locked',values:{name:'Locked',action:'executeAction',ability:'blocked',qty:1,show:true,active:true}},
    {id:'hidden',values:{name:'Hidden',action:'executeAction',ability:'recover',qty:1,show:false,active:false}}
  ]}];
  const bindings=[{elementId:'title',property:'text',field:'name'},...Object.entries({text:'name',action:'action',actionId:'ability',quantity:'qty',visible:'show',enabled:'active'}).map(([property,field])=>({elementId:'go',property,field}))];
  p.visual.screens.find(s=>s.id==='home').elements=[{id:'menu',kind:'collection',componentId:'card',source:{type:'table',tableId:'choices'},layout:'grid',columns:2,rowHeight:130,gap:8,x:0,y:0,width:700,height:300,bindings,visibleCondition:{op:'always'}}];
  p.visual.screens.find(s=>s.id==='market').elements=[{id:'menu',kind:'collection',componentId:'card',source:{type:'shopOffers',shopId:'florist'},x:0,y:0,width:600,height:300,bindings:[{elementId:'title',property:'text',field:'name'},{elementId:'go',property:'text',field:'name'},...['action','shopId','itemId','quantity'].map(field=>({elementId:'go',property:field,field}))]}];
});}

for(const backend of ['native','wasm'])test(`${backend} collection projection uses canonical predicates/availability, stable row actions and stale guards`,async t=>{
  const {root,built}=await sample(t);assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  let saved=null;const runtime=backend==='wasm'?new WasmRuntime(await createModule()):null;
  const controller=runtime?new WebPlayerController(built.snapshot,runtime,{read:async()=>saved,write:async text=>{saved=text;}},()=>true):new PlayerController();
  if(!runtime)assert.equal((await controller.openProject(root)).error,null);
  const beforeRun=structuredClone(built.snapshot);beforeRun.project.visual.screens.find(s=>s.id==='home').elements[0].visibleCondition={op:'all',conditions:[{op:'always'},{op:'not',condition:{op:'not',condition:{op:'always'}}}]};
  const titleCondition=beforeRun.project.visual.screens.find(s=>s.id==='home').elements[0].visibleCondition;
  const titleProjection=projectDataUi(beforeRun,null,null,input=>runtime?runtime.request(input):nativeRequest(input));
  assert.equal(titleProjection.conditions[JSON.stringify(titleCondition)],true,'state-free nested predicates are valid before starting');
  const initial=await controller.newRun(null);assert.equal(initial.error,null);
  const view=()=>runtime?controller.view():controller.reply();
  const version=()=>({generation:controller.generation,revision:controller.session.state.stateRevision});
  const before=structuredClone(controller.session.state),projection=(await view()).view.dataUi;
  assert.deepEqual(controller.session.state,before,'projection dry runs must not alter state');
  const visibilityKey=JSON.stringify(built.snapshot.project.visual.components[0].elements[0].visibleCondition);
  assert.equal(projection.conditions[visibilityKey],true);
  assert.equal(projection.availability.home.menu__rest__go.enabled,true);
  assert.equal(projection.availability.home.menu__locked__go.enabled,false);assert.ok(projection.availability.home.menu__locked__go.reason);
  assert.equal(projection.collections.home.menu.length,3);assert.equal(projection.collections.market.menu.length,0,'off-location shop rows stay filtered');
  const design=built.snapshot.project.visual,collection=design.screens.find(s=>s.id==='home').elements[0];
  const bound=resolveCollectionRow(design,collection,projection.collections.home.menu[0],320).find(e=>e.id==='menu__rest__go');
  const stale=version(),applied=await controller.act(widgetCommand(bound),version());assert.equal(applied.error,null);assert.equal(applied.view.state.attributes.money,92);assert.equal(applied.view.dataUi.conditions[visibilityKey],false);
  const unchanged=structuredClone(controller.session.state);assert.ok((await controller.act(widgetCommand(bound),stale)).error);assert.deepEqual(controller.session.state,unchanged);
  const reordered=structuredClone(built.snapshot);reordered.project.visual.dataTables[0].rows.reverse();
  const reorderedProjection=projectDataUi(reordered,controller.session.state,applied.view.gameplay,input=>runtime?runtime.request(input):nativeRequest(input));
  assert.equal(reorderedProjection.collections.home.menu[2].id,'rest');
  const sameBound=resolveCollectionRow(reordered.project.visual,collection,reorderedProjection.collections.home.menu[2],450).find(e=>e.kind==='button');assert.equal(sameBound.id,bound.id);assert.deepEqual(widgetCommand(sameBound),widgetCommand(bound));
  if(runtime){assert.equal((await controller.save(version())).error,null);await controller.act(widgetCommand(bound),version());assert.equal((await controller.load(version())).error,null);assert.deepEqual(controller.session.state,unchanged);}
});

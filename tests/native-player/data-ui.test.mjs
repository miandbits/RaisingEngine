import test from 'node:test';
import assert from 'node:assert/strict';
import { cp, mkdir, mkdtemp, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { exportGame } from '../../dist/packages/export-service/export.js';

const literal=value=>({op:'literal',value});
const moneyAtLeast=value=>({op:'compare',comparator:'gte',left:{op:'read',target:{kind:'attribute',attributeId:'money'}},right:literal(value)});
const binding=(elementId,property,field,format)=>({elementId,property,field,...(format?{format}:{})});
const card=(id,source,x,bindings)=>({id,kind:'collection',x,y:110,width:360,height:330,componentId:'data-card',source,layout:'list',gap:8,bindings});

test('native authored collection rows bind canonical actions, scroll, conditions, confirmation and save replay', {timeout:120000},async()=>{
  await mkdir(resolve('.cache/native-data-ui-tests'),{recursive:true});
  const root=await mkdtemp(resolve('.cache/native-data-ui-tests/run-')),projectRoot=resolve(root,'project');
  await cp(resolve('examples/academy-journey'),projectRoot,{recursive:true});
  const path=resolve(projectRoot,'project.json'),project=JSON.parse(await readFile(path,'utf8'));
  project.gameplay.actions=[{id:'gain',name:'Earn one',costs:[],effects:[{op:'add',target:{kind:'attribute',attributeId:'money'},value:literal(1)}]}];
  project.gameplay.policyGroups=[{id:'study',name:'Study',trigger:{type:'beforeActivity'},options:[{id:'light',name:'Light study',actionId:'gain'}]}];
  project.visual.confirmPurchases=true;
  project.visual.components=[{id:'data-card',label:'Authored card',width:340,height:100,elements:[
    {id:'panel',kind:'panel',x:0,y:0,width:340,height:100,fill:'#193349'},
    {id:'title',kind:'text',x:8,y:4,width:320,height:24,text:'Unbound',fontSize:18},
    {id:'value',kind:'text',x:8,y:29,width:320,height:24,text:'Value',fontSize:16},
    {id:'button',kind:'button',x:8,y:58,width:310,height:34,text:'Action',action:'sound',fontSize:16},
    {id:'hidden',kind:'text',x:0,y:0,width:100,height:20,text:'Never visible',visibleCondition:{op:'not',condition:moneyAtLeast(0)}}
  ]}];
  project.visual.dataTables=[{id:'tasks',name:'Tasks',columns:[{id:'name',name:'Name',type:'text'},{id:'command',name:'Command',type:'text'},
    {id:'target',name:'Target',type:'text'},{id:'show',name:'Show',type:'boolean'}],rows:Array.from({length:10},(_,i)=>({id:`task-${i}`,values:{name:`Task ${i}`,command:'executeAction',target:'gain',show:i>0}}))}];
  const custom=card('tasks',{type:'table',tableId:'tasks'},30,[binding('title','text','name'),binding('button','label','name','Run {value}'),binding('button','action','command'),binding('button','actionId','target')]);
  custom.rowFilter={field:'show',operator:'eq',value:true};custom.enabledCondition=moneyAtLeast(60);
  const policies=card('policies',{type:'policyOptions',groupId:'study'},440,[binding('title','text','name'),binding('value','text','selected','Selected: {value}'),binding('button','action','action'),binding('button','groupId','groupId'),binding('button','optionId','optionId')]);
  const relations=card('relations',{type:'relationships',characterId:'mentor'},850,[binding('title','text','name'),binding('value','text','value','Trust: {value}')]);
  const home=project.visual.screens.find(s=>s.id==='home');home.elements=[custom,policies,relations,
    {id:'to-market',kind:'button',x:30,y:550,width:300,height:50,text:'Market',action:'visit',locationId:'market'}];
  const shop=card('offers',{type:'shopOffers',shopId:'florist'},30,[binding('title','text','name'),binding('value','text','owned','Owned: {value}'),binding('button','action','action'),binding('button','shopId','shopId'),binding('button','itemId','itemId'),binding('button','quantity','quantity')]);
  project.visual.screens.find(s=>s.id==='market').elements=[shop,{id:'to-home',kind:'button',x:30,y:550,width:300,height:50,text:'Home',action:'visit',locationId:'home'}];
  await writeFile(path,JSON.stringify(project));
  const built=await buildSnapshot(await loadProject(projectRoot));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const destination=resolve(root,'game');await exportGame({content:built.snapshot,projectRoot,destination,target:'windows',runtimes:{windows:resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app'),web:resolve('dist/web')}});
  const steps=[{click:'start'},{checkpoint:'home'},
    {click:'tasks__task-9__button'},{checkpoint:'confirm'},{click:'cancel-purchase'},{checkpoint:'cancel'},
    {click:'tasks__task-9__button'},{click:'confirm-purchase'},{checkpoint:'earned'},
    {key:'F5'},{resize:{width:1000,height:800}},{waitFrames:10},{checkpoint:'resized'},
    {click:'policies__study--light__button'},{click:'confirm-purchase'},{checkpoint:'policy'},
    {click:'to-market'},{click:'offers__florist--flower__button'},{click:'confirm-purchase'},{checkpoint:'bought'},
    {click:'to-home'},{checkpoint:'poor'},{key:'F9'},{checkpoint:'loaded',screenshot:resolve(root,'restored.png')}];
  const input=resolve(root,'steps.json'),reportPath=resolve(root,'report.json');await writeFile(input,JSON.stringify(steps));
  const child=spawnSync(resolve(destination,'RaisingPlayer.exe'),['--bundle',destination,'--save',resolve(root,'save.json'),'--hidden','--automation',input,'--report',reportPath],{encoding:'utf8',timeout:90000,windowsHide:true});
  assert.equal(child.status,0,child.error?.message??child.stderr);
  const report=JSON.parse(await readFile(reportPath,'utf8'));assert.equal(report.ok,true);
  const ui=tag=>report.steps.find(s=>s.command.checkpoint===tag).ui;
  const node=(tag,id)=>ui(tag).nodes.find(n=>n.id===id);
  const button=(tag,id)=>ui(tag).elements.find(n=>n.id===id);
  assert.equal(node('home','tasks').rowCount,9);assert.equal(ui('home').texts['tasks__task-9__title'],'Task 9');
  assert.equal(button('home','tasks__task-9__button').label,'Run Task 9');
  assert.equal(ui('home').texts['tasks__task-0__title'],undefined);
  assert.equal(ui('home').texts['tasks__task-9__hidden'],undefined);
  assert.deepEqual(ui('cancel').state,ui('home').state);
  assert.equal(ui('earned').state.attributes.money,ui('home').state.attributes.money+1);
  assert.ok(node('earned','tasks').scrollTop>0);assert.equal(node('resized','tasks').scrollTop,node('earned','tasks').scrollTop);
  assert.equal(ui('policy').state.gameplay.policySelections.study,'light');
  assert.equal(button('policy','policies__study--light__button').enabled,false);
  assert.equal(ui('bought').state.gameplay.inventory.flower,1);assert.equal(ui('bought').texts['offers__florist--flower__value'],'Owned: 1');
  assert.equal(button('poor','tasks__task-9__button').enabled,false);
  assert.deepEqual(ui('loaded').state,ui('earned').state);
});

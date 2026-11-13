import test from 'node:test';
import assert from 'node:assert/strict';
import {resolve} from 'node:path';
import {mkdir,mkdtemp,writeFile} from 'node:fs/promises';
import {deriveLastCommittedPlan,planReuseDraft,hasPlanReuse} from '../../dist/packages/player-service/plan-reuse.js';
import {PlayerController} from '../../dist/packages/player-service/controller.js';
import {WebPlayerController} from '../../dist/packages/web-player/controller.js';
import {WasmRuntime} from '../../dist/packages/native-bridge/wasm.js';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import {createSession} from '../../dist/packages/runtime-service/session.js';
import {encodeSave} from '../../dist/packages/runtime-service/save.js';
import {requiredPlayerFeatures,missingPlayerFeatures} from '../../dist/packages/export-service/compatibility.js';
const known=['study','work','rest'];
const source=ids=>({status:'ready',commandIndex:3,activityIds:ids});

test('last confirmed means newest accepted assignment, including replans, never older fallback or actor assignments',()=>{
 const history=[{type:'setSchedule',activityIds:['study','rest','rest']},{type:'executeActivity'},{type:'assignActivities',assignments:{a:'work'}},{type:'setSchedule',activityIds:['study','work','rest']}],before=structuredClone(history);
 const got=deriveLastCommittedPlan(history,3,known);assert.deepEqual(got,{status:'ready',commandIndex:3,activityIds:['study','work','rest']});got.activityIds[0]='rest';assert.deepEqual(history,before);
 assert.deepEqual(deriveLastCommittedPlan([],3,known),{status:'none'});
 assert.deepEqual(deriveLastCommittedPlan(history.slice(1,3),3,known),{status:'none'});
 assert.deepEqual(deriveLastCommittedPlan([...history,{type:'setSchedule',activityIds:['rest']}],3,known),{status:'unavailable',reason:'length'});
 assert.deepEqual(deriveLastCommittedPlan([...history,{type:'setSchedule',activityIds:['removed','work','rest']}],3,known),{status:'unavailable',reason:'activity'});
});

test('review copies only remaining slots, including result intermission, with no assumptions about future costs',()=>{
 const src=source(['work','work','work']),state={phase:'ready',periodIndex:4,schedule:['study','rest','rest'],activityIntermission:true},before=structuredClone({src,state});
 assert.deepEqual(planReuseDraft(src,state,3,known),{status:'ready',activityIds:['study','rest','work'],firstEditableSlot:2});
 assert.deepEqual(planReuseDraft(src,{...state,activityIntermission:false},3,known),{status:'ready',activityIds:['study','work','work'],firstEditableSlot:1});
 assert.deepEqual(planReuseDraft(src,{...state,periodIndex:5},3,known),{status:'unavailable',reason:'finished'});
 for(const schedule of [null,[],['missing','rest','rest']])assert.deepEqual(planReuseDraft(src,{...state,schedule},3,known),{status:'unavailable',reason:'prefix'});
 for(const phase of ['narrative','activity-result','finished'])assert.deepEqual(planReuseDraft(src,{...state,phase},3,known),{status:'unavailable',reason:'phase'});
 assert.deepEqual({src,state},before);
 for(const count of [1,3,5,12])assert.deepEqual(planReuseDraft(source(Array(count).fill('work')),{phase:'planning',periodIndex:count,schedule:null},count,known),{status:'ready',activityIds:Array(count).fill('work'),firstEditableSlot:0});
});

test('native-backed desktop and Wasm projections recover the actual loaded branch even for equal history lengths',async()=>{
 const desktop=new PlayerController();assert.equal((await desktop.openProject(resolve('examples/academy-demo'))).error,null);
 const content=structuredClone(desktop.content);content.project.visual={width:1280,height:720,readableLayout:{},screens:[{id:'main',label:'Main',elements:[]}]};const screen=content.project.visual.screens[0];screen.readableLayout={items:[{elementId:'reuse-test',schedulePresentation:'compact',planReuse:'last-committed'}]};screen.elements.push({id:'reuse-test',kind:'schedule',x:0,y:0,width:200,height:300});desktop.content=content;
 assert.equal(hasPlanReuse(content),true);assert.ok(requiredPlayerFeatures(content).includes('plan-reuse-v1'));assert.deepEqual(missingPlayerFeatures(['plan-reuse-v1'],['compact-planning-v1','schedule-board-v1']),['plan-reuse-v1']);
 const legacy=structuredClone(content);delete legacy.project.visual.screens[0].readableLayout.items[0].planReuse;assert.equal(hasPlanReuse(legacy),false);assert.ok(!requiredPlayerFeatures(legacy).includes('plan-reuse-v1'));legacy.project.roster={};assert.equal(hasPlanReuse(legacy),false);delete legacy.project.roster;
 let saved=null;const web=new WebPlayerController(content,new WasmRuntime(await createModule()),{read:async()=>saved,write:async text=>{saved=text}},()=>true);
 await mkdir('.cache/plan-reuse',{recursive:true});const out=await mkdtemp(resolve('.cache/plan-reuse/controller-'));const version=c=>({generation:c.generation,revision:c.session.state.stateRevision});
 for(const controller of [desktop,web]){
  const view=async()=>controller===desktop?controller.view():(await controller.view()).view;
  assert.equal((await controller.newRun(null)).error,null);assert.deepEqual((await view()).lastCommittedPlan,{status:'none'});
  const backups=[];
  for(const plan of [['study','work','rest'],['rest','rest','rest']]){
   assert.equal((await controller.act({type:'schedule',activityIds:plan},version(controller))).error,null);
   const encoded=await encodeSave(content,controller.session);assert.ok(encoded.ok);backups.push(encoded.text);
   assert.deepEqual((await view()).lastCommittedPlan.activityIds,plan);
   assert.equal((await controller.newRun(version(controller))).error,null);
  }
  assert.equal(JSON.parse(backups[0]).commands.length,JSON.parse(backups[1]).commands.length);
  for(const [i,text] of backups.entries()){
   if(controller===web){saved=text;assert.equal((await controller.load(version(controller))).error,null);}else{const file=resolve(out,'branch-'+i+'.json');await writeFile(file,text);assert.equal((await controller.load(file,version(controller))).error,null);}
   assert.deepEqual((await view()).lastCommittedPlan.activityIds,JSON.parse(text).commands[0].activityIds);
  }
  const before=structuredClone(controller.session),projection=(await view()).lastCommittedPlan;
  if(controller===web){saved='{broken';assert.ok((await controller.load(version(controller))).error);saved=null;assert.equal((await controller.load(version(controller))).error,null);}else{await writeFile(resolve(out,'broken.json'),'{broken');assert.ok((await controller.load(resolve(out,'broken.json'),version(controller))).error);}
  assert.deepEqual(controller.session,before);assert.deepEqual((await view()).lastCommittedPlan,projection);
  assert.ok((await controller.act({type:'schedule',activityIds:['missing','rest','rest']},version(controller))).error);assert.deepEqual((await view()).lastCommittedPlan,projection);
  controller.session=createSession(content,42,{initialOverrides:{attributes:{money:0},flags:{},relationships:{}}}).session;
  assert.ok((await controller.act({type:'commitPlan',activityIds:['study','work','rest']},version(controller))).error);assert.equal(controller.session.commands.length,1);assert.deepEqual((await view()).lastCommittedPlan.activityIds,['study','work','rest'],'Accepted assignment survives an execution failure');
  assert.equal((await controller.newRun(version(controller))).error,null);assert.deepEqual((await view()).lastCommittedPlan,{status:'none'});
 }
 const old=new WebPlayerController(legacy,new WasmRuntime(await createModule()),{read:async()=>null,write:async()=>{}},()=>true);assert.equal((await old.view()).view.lastCommittedPlan,undefined);
});

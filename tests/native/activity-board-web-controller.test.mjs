import test from 'node:test';
import assert from 'node:assert/strict';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import {WasmRuntime} from '../../dist/packages/native-bridge/wasm.js';
import {WebPlayerController} from '../../dist/packages/web-player/controller.js';
import {decodeSave} from '../../dist/packages/runtime-service/save.js';
import {content,doc,replaceEvents} from '../runtime/helpers.mjs';
import validateSave from '../../apps/web/generated/save-validator.mjs';
const runtime=new WasmRuntime(await createModule());
const source=await content(p=>{const project=doc(p,'project');project.periodsPerMonth=4;replaceEvents(p,[]);project.visual={width:1280,height:720,routing:{defaultScreenId:'schedule',phases:[{phase:'title',screenId:'title'}]},screens:[{id:'title',label:'Title',elements:[]},{id:'schedule',label:'Plan',elements:[{id:'plan',kind:'schedule',scheduleLayout:'activity-board',x:0,y:0,width:1000,height:600}]},{id:'other',label:'Other',elements:[]}]};});
function setup(c=source,request=runtime){let file;const player=new WebPlayerController(c,request,{read:async()=>file,write:async value=>file=value},validateSave);return {player,version:()=>player.session?{generation:player.generation,revision:player.session.state.stateRevision}:null,get file(){return file;},set file(value){file=value;}};}
const owner={screenId:'schedule',elementId:'plan'};
const draft=async player=>(await player.view()).view.scheduleDraft;

const sharedSource=await content(p=>{
 const project=doc(p,'project');project.periodsPerMonth=4;replaceEvents(p,[]);
 project.visual={width:1280,height:720,routing:{defaultScreenId:'schedule',phases:[{phase:'title',screenId:'title'}]},
  components:[
   {id:'widgets',label:'Shared widgets',width:1000,height:600,elements:[{id:'plan',kind:'schedule',scheduleLayout:'activity-board',x:0,y:0,width:1000,height:600}]},
   {id:'section',label:'Nested section',width:1000,height:600,elements:[{id:'content',kind:'component',componentId:'widgets',x:0,y:0,width:1000,height:600}]}
  ],
  screens:[{id:'title',label:'Title',elements:[]},{id:'schedule',label:'Plan',elements:[{id:'panel',kind:'component',componentId:'section',x:0,y:0,width:1000,height:600}]}]};
});

test('nested shared activity boards preserve partial drafts across portable save, load and a fresh player without changing the confirmed plan',async()=>{
 const x=setup(sharedSource),p=x.player,sharedOwner={screenId:'schedule',elementId:'panel__content__plan'};
 assert.equal((await p.newRun(null)).error,null);
 const empty=await draft(p);assert.ok(empty,'a rendered nested activity board must own a portable draft');
 assert.equal(p.session.state.schedule,null);assert.deepEqual(p.session.commands,[]);assert.deepEqual(empty,{month:0,slot:0,selected:0,activities:['','','','']});
 const partial={...empty,slot:2,selected:4,activities:['','','rest','']},unplanned=structuredClone(p.session);
 assert.equal((await p.setScheduleDraft(partial,sharedOwner,x.version(),empty)).error,null);assert.deepEqual(p.session,unplanned);
 assert.equal((await p.save(x.version())).error,null);const partialSave=x.file;
 assert.deepEqual(JSON.parse(partialSave).playerPresentation.scheduleDraft,partial);
 const fresh=setup(sharedSource);fresh.file=partialSave;assert.equal((await fresh.player.load(null)).error,null);
 assert.deepEqual(await draft(fresh.player),partial);assert.deepEqual(fresh.player.session.state,unplanned.state);assert.deepEqual(fresh.player.session.commands,[]);

 const plan=Array(4).fill('rest');assert.equal((await p.act({type:'schedule',activityIds:plan},x.version())).error,null);
 assert.equal((await p.act({type:'execute'},x.version())).error,null);assert.equal((await p.act({type:'continue'},x.version())).error,null);
 assert.equal(p.session.state.periodIndex,1);const confirmed=structuredClone(p.session),current=await draft(p);
 const changed={...current,slot:1,selected:3,activities:['rest','study','','']};
 assert.equal((await p.setScheduleDraft(changed,sharedOwner,x.version(),current)).error,null);assert.deepEqual(p.session,confirmed);
 assert.equal((await p.save(x.version())).error,null);const saved=x.file;assert.ok((await decodeSave(sharedSource,saved)).ok);
 const restarted=setup(sharedSource);restarted.file=saved;assert.equal((await restarted.player.load(null)).error,null);
 assert.deepEqual(await draft(restarted.player),changed);assert.deepEqual(restarted.player.session.state,confirmed.state);assert.deepEqual(restarted.player.session.commands,confirmed.commands);
 assert.equal((await restarted.player.save(restarted.version())).error,null);assert.deepEqual(JSON.parse(restarted.file).playerPresentation.scheduleDraft,changed);

 const legacy=JSON.parse(saved);delete legacy.playerPresentation;restarted.file=JSON.stringify(legacy);
 assert.equal((await restarted.player.load(restarted.version())).error,null);assert.deepEqual((await draft(restarted.player)).activities,plan);assert.equal((await draft(restarted.player)).selected,15);assert.deepEqual(restarted.player.session.state,confirmed.state);assert.deepEqual(restarted.player.session.commands,confirmed.commands);
 assert.equal((await restarted.player.newRun(restarted.version())).error,null);assert.equal(restarted.player.session.state.schedule,null);assert.deepEqual(restarted.player.session.commands,[]);assert.deepEqual(await draft(restarted.player),empty);
});

test('nested columns schedules and unused activity-board declarations do not acquire activity-board drafts',async()=>{
 const columns=structuredClone(sharedSource);
 columns.project.visual.components[0].elements[0].scheduleLayout='columns';
 columns.project.visual.components.push({id:'unused-board',label:'Unused',width:1000,height:600,elements:[{id:'unused-plan',kind:'schedule',scheduleLayout:'activity-board',x:0,y:0,width:1000,height:600}]});
 const x=setup(columns);assert.equal((await x.player.newRun(null)).error,null);assert.equal(await draft(x.player),undefined);assert.equal(x.player.session.state.schedule,null);
 assert.equal((await x.player.save(x.version())).error,null);assert.equal(JSON.parse(x.file).playerPresentation?.scheduleDraft,undefined);
 assert.equal((await x.player.act({type:'schedule',activityIds:Array(4).fill('rest')},x.version())).error,null);assert.equal(await draft(x.player),undefined);
});

test('activity board applied drafts survive portable restore and fresh player; old saves load and legacy behavior stays unchanged',async()=>{
 const x=setup(),p=x.player;assert.equal((await p.newRun(null)).error,null);const before=structuredClone(p.session),initial=await draft(p);assert.deepEqual(initial.activities,['','','','']);
 const next={...initial,slot:2,selected:4,activities:['','','rest','']};assert.equal((await p.setScheduleDraft(next,owner,x.version(),initial)).error,null);assert.deepEqual(p.session,before);
 next.activities[2]='invalid';assert.equal((await draft(p)).activities[2],'rest','controller owns a detached draft');
 assert.equal((await p.save(x.version())).error,null);const saved=x.file;assert.deepEqual(JSON.parse(saved).playerPresentation.scheduleDraft,{...initial,slot:2,selected:4,activities:['','','rest','']});assert.ok((await decodeSave(source,saved)).ok);
 const fresh=setup();fresh.file=saved;assert.equal((await fresh.player.load(null)).error,null);assert.deepEqual(await draft(fresh.player),JSON.parse(saved).playerPresentation.scheduleDraft);assert.deepEqual(fresh.player.session.state,before.state);assert.deepEqual(fresh.player.session.commands,before.commands);
 const legacy=JSON.parse(saved);delete legacy.playerPresentation.scheduleDraft;fresh.file=JSON.stringify(legacy);assert.equal((await fresh.player.load(fresh.version())).error,null);assert.deepEqual((await draft(fresh.player)).activities,['','','','']);
 const plain=structuredClone(source);plain.project.visual.screens[1].elements[0].scheduleLayout='columns';const old=setup(plain);await old.player.newRun(null);await old.player.save(old.version());assert.equal(JSON.parse(old.file).playerPresentation?.scheduleDraft,undefined);assert.equal((await old.player.view()).view.scheduleDraft,undefined);
});

test('draft publication rejects stale version, stale draft, wrong/disabled owner and locked prefix without commands',async()=>{
 const x=setup(),p=x.player;await p.newRun(null);const empty=await draft(p),next={...empty,selected:1,activities:['rest','','','']};const state=structuredClone(p.session);
 for(const [target,version] of [[{...owner,screenId:'other'},x.version()],[{...owner,elementId:'missing'},x.version()],[owner,{generation:-1,revision:0}]]){assert.ok((await p.setScheduleDraft(next,target,version,empty)).error);assert.deepEqual(p.session,state);assert.deepEqual(await draft(p),empty);}
 assert.equal((await p.setScheduleDraft(next,owner,x.version(),empty)).error,null);const second={...next,selected:3,activities:['rest','rest','','']};assert.ok((await p.setScheduleDraft(second,owner,x.version(),empty)).error,'same canonical revision does not authorize stale presentation writes');assert.deepEqual(await draft(p),next);
 const disabled=structuredClone(source);disabled.project.visual.screens[1].elements[0].enabledCondition={op:'not',condition:{op:'always'}};const y=setup(disabled);await y.player.newRun(null);assert.ok((await y.player.setScheduleDraft(next,owner,y.version(),await draft(y.player))).error);
 const ids=Array(4).fill('rest');assert.equal((await p.act({type:'schedule',activityIds:ids},x.version())).error,null);assert.deepEqual((await draft(p)).activities,ids);await p.act({type:'execute'},x.version());await p.act({type:'continue'},x.version());const current=await draft(p),locked={...current,slot:0,activities:['study',...ids.slice(1)]};const progressed=structuredClone(p.session);assert.ok((await p.setScheduleDraft(locked,owner,x.version(),current)).error);assert.deepEqual(p.session,progressed);
});

test('invalid draft and failed post-restore settlement preserve previous session, generation and presentation draft',async()=>{
 let fail=false;const request={request(input){if(fail&&input.operation==='command'&&input.command?.type==='resolvePeriodEnd')return {ok:false,error:{code:'PROBE_SETTLEMENT',message:'settlement rejected'}};return runtime.request(input);}};
 const x=setup(source,request),p=x.player;await p.newRun(null);const initial=await draft(p);await p.setScheduleDraft({...initial,selected:2,slot:1,activities:['','rest','','']},owner,x.version(),initial);await p.save(x.version());const good=x.file;
 for(const mutate of [s=>s.playerPresentation.scheduleDraft.activities[1]='removed',s=>s.playerPresentation.scheduleDraft.month=9,s=>s.contentFingerprint='sha256:'+'0'.repeat(64)]){const value=JSON.parse(good);mutate(value);x.file=JSON.stringify(value);const before={session:structuredClone(p.session),generation:p.generation,draft:await draft(p)};assert.ok((await p.load(x.version())).error);assert.deepEqual({session:p.session,generation:p.generation,draft:await draft(p)},before);}
 const route=setup();await route.player.newRun(null);await route.player.act({type:'schedule',activityIds:Array(4).fill('rest')},route.version());await route.player.act({type:'execute'},route.version());await route.player.save(route.version());const pending=JSON.parse(route.file),r=runtime.request({operation:'command',content:source,state:pending.state,command:{type:'acknowledgeResult'},expectedRevision:pending.state.stateRevision});assert.equal(r.ok,true);assert.equal(r.state.phase,'period-end');pending.state=r.state;pending.commands.push(r.transition.command);x.file=JSON.stringify(pending);const before={session:structuredClone(p.session),generation:p.generation,draft:await draft(p)};fail=true;assert.match((await p.load(x.version())).error,/settlement rejected/);assert.deepEqual({session:p.session,generation:p.generation,draft:await draft(p)},before);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import {WasmRuntime} from '../../dist/packages/native-bridge/wasm.js';
import {WebPlayerController} from '../../dist/packages/web-player/controller.js';
import {requiredPlayerFeatures} from '../../dist/packages/export-service/compatibility.js';
import {decodeSave} from '../../dist/packages/runtime-service/save.js';
import {content,doc,event,replaceEvents} from '../runtime/helpers.mjs';
import validateSave from '../../apps/web/generated/save-validator.mjs';

const visual={width:1280,height:720,routing:{defaultScreenId:'schedule',phases:[{phase:'title',screenId:'title'}]},saveSystem:{manualSlots:6,autoSlots:3},screens:[{id:'title',label:'Title',elements:[]},{id:'schedule',label:'Study',elements:[]},{id:'story',label:'Story',elements:[]},{id:'ending',label:'End',elements:[]},...['save','load'].map(kind=>({id:kind,modal:true,label:kind,elements:[{id:'close',kind:'button',action:'closeModal',x:0,y:0,width:100,height:40},{id:'slots',kind:kind==='save'?'saveSlots':'loadSlots',x:0,y:50,width:1200,height:600}]}))]};
class MemoryStore{rows=[];fail=false;async read(){return structuredClone(this.rows);}async update(change){if(this.fail)throw new Error('QuotaExceededError');const row=change(structuredClone(this.rows));if(row){this.rows=this.rows.filter(r=>r.automatic!==row.automatic||r.index!==row.index);this.rows.push(structuredClone(row));}}}
const source=await content(p=>{const project=doc(p,'project');project.visual=structuredClone(visual);project.calendar={monthsPerYear:13,daysPerMonth:28,monthLabel:'Phase',periodLabel:'Week'};project.startMonth=13;project.periodsPerMonth=4;replaceEvents(p,[event('story',[{id:'a',type:'dialogue',speakerId:null,text:'A',next:'choice'},{id:'choice',type:'choice',prompt:'B?',choices:[{id:'yes',text:'Yes',requirements:{op:'always'},effects:[],next:'effect'}]},{id:'effect',type:'effect',effects:[],next:'b'},{id:'b',type:'dialogue',speakerId:null,text:'B',next:'c'},{id:'c',type:'dialogue',speakerId:null,text:'C',next:'end'},{id:'end',type:'end'}])]);});
const runtime=new WasmRuntime(await createModule());
const setup=(store=new MemoryStore())=>{let file=null;const player=new WebPlayerController(source,runtime,{read:async()=>file,write:async text=>file=text},validateSave,null,store);const version=()=>player.session?{generation:player.generation,revision:player.session.state.stateRevision}:null;const slot=async action=>player.slots(action,version());return {player,store,version,slot,get file(){return file;},set file(value){file=value;}};};

test('Web slots preserve exact replay saves, confirmations, changed records, manual isolation and file portability',async()=>{
 const x=setup(),{player,store,version,slot}=x;assert.equal((await player.newRun(null)).error,null);assert.equal((await player.checkpoint(version())).error,null);assert.equal(store.rows.length,1);
 assert.equal((await slot({type:'open',kind:'save'})).error,null);assert.equal((await slot({type:'save'})).error,null);const first=store.rows.find(r=>!r.automatic).text;assert.equal(validateSave(JSON.parse(first)),true,JSON.stringify(validateSave.errors));assert.equal(JSON.parse(first).playerPresentation.saveRecord.thumbnail,'');assert.match(JSON.parse(first).playerPresentation.saveRecord.date,/Phase 13/);assert.equal((await decodeSave(source,first)).ok,true);
 await slot({type:'close'});await player.act({type:'schedule',activityIds:Array(4).fill('rest')},version());await player.checkpoint(version());const state=structuredClone(player.session.state);
 await slot({type:'open',kind:'save'});await slot({type:'save'});assert.equal((await player.view()).view.saveSlots.confirmation,'save');await slot({type:'cancel'});assert.equal(store.rows.find(r=>!r.automatic).text,first);
 await slot({type:'save'});const old=store.rows.find(r=>!r.automatic);old.text+=' ';const conflict=await slot({type:'confirm'});assert.match(conflict.error,/changed/);assert.equal(old.text,first+' ');assert.deepEqual(player.session.state,state);
 await slot({type:'select',index:1});await slot({type:'save'});const second=store.rows.find(r=>!r.automatic&&r.index===1).text;await slot({type:'export'});assert.equal((await decodeSave(source,x.file)).ok,true);await slot({type:'close'});
 await slot({type:'open',kind:'load'});await slot({type:'load'});await slot({type:'confirm'});assert.deepEqual(player.session.state,JSON.parse(first).state);const before=structuredClone(store.rows);await player.checkpoint(version());assert.deepEqual(store.rows,before,'loading does not rotate autosaves');
 await slot({type:'open',kind:'load'});await slot({type:'select',index:1});await slot({type:'load'});await slot({type:'confirm'});assert.deepEqual(player.session.state,JSON.parse(second).state);
 await slot({type:'open',kind:'save'});await slot({type:'group',automatic:true});assert.match((await slot({type:'save'})).error,/protected/);await slot({type:'group',automatic:false});await slot({type:'select',index:5});store.fail=true;assert.match((await slot({type:'save'})).error,/Quota/);assert.equal(store.rows.some(r=>!r.automatic&&r.index===5),false);assert.equal((await slot({type:'export'})).error,null,'file backup still works after storage failure');
});

test('Web autosave ring keeps choices through dialogue and ignores navigation and loading',async()=>{
 const {player,store,version,slot}=setup();await player.newRun(null);await player.checkpoint(version());
 const act=async action=>{assert.equal((await player.act(action,version())).error,null);assert.equal((await player.checkpoint(version())).error,null);};
 await act({type:'schedule',activityIds:Array(4).fill('rest')});await act({type:'execute'});await act({type:'continue'});assert.equal(player.session.state.phase,'narrative');
 let saved=structuredClone(store.rows);await act({type:'continue'});assert.deepEqual(store.rows,saved,'dialogue advance to a choice does not consume ring');
 await act({type:'choose',choiceId:'yes'});assert.notDeepEqual(store.rows,saved,'choice still checkpoints after automatic effect nodes');saved=structuredClone(store.rows);await act({type:'continue'});assert.deepEqual(store.rows,saved,'choice checkpoint survives next line');await act({type:'continue'});assert.notDeepEqual(store.rows,saved,'story completion checkpoints');
 const beforeNavigation=structuredClone(store.rows);await act({type:'navigate',screenId:'schedule'});assert.deepEqual(store.rows,beforeNavigation,'screen navigation does not consume ring');
 const autos=store.rows.filter(r=>r.automatic);assert.equal(autos.length,3);assert.equal(new Set(autos.map(r=>JSON.parse(r.text).playerPresentation.saveRecord.sequence)).size,3);
 await slot({type:'open',kind:'load'});await slot({type:'group',automatic:true});const before=structuredClone(store.rows);await player.checkpoint(version());assert.deepEqual(store.rows,before);await slot({type:'close'});
});

test('slot capability is limited to record modals; corruption and changed content do not replace live state',async()=>{
 assert.ok(requiredPlayerFeatures(source).includes('save-slots-v1'));assert.ok(!requiredPlayerFeatures(source).includes('ui-composition-v1'));
 for(const mutate of [p=>p.visual.screens.find(s=>s.id==='save').elements.push({id:'schedule',kind:'schedule',x:0,y:0,width:400,height:120}),p=>p.visual.screens.push({id:'ordinary',label:'Other',modal:true,elements:[]}),p=>p.visual.screens[0].elements.push({id:'tip',kind:'text',text:'Text',tooltip:'tip',x:0,y:0,width:100,height:50}),p=>p.visual.screens[0].elements.push({id:'flow',kind:'group',flowLayout:{direction:'column'},x:0,y:0,width:100,height:50})]){const c=structuredClone(source);mutate(c.project);assert.ok(requiredPlayerFeatures(c).includes('ui-composition-v1'));}
 const {player,store,version,slot}=setup();await player.newRun(null);await player.checkpoint(version());const valid=store.rows[0].text;store.rows.push({automatic:false,index:0,text:'corrupt'},{automatic:false,index:1,text:valid.replace(source.contentFingerprint,'f'.repeat(64))},{automatic:false,index:2,text:valid.replace('"periodIndex":0','"periodIndex":1')});
 let r=await slot({type:'open',kind:'load'});assert.equal(r.view.saveSlots.rows[0].status,'invalid');assert.equal(r.view.saveSlots.rows[1].status,'incompatible');assert.ok((await slot({type:'load'})).error);const state=structuredClone(player.session.state);
 await slot({type:'select',index:2});await slot({type:'load'});assert.ok((await slot({type:'confirm'})).error);assert.deepEqual(player.session.state,state);assert.equal(store.rows.find(r=>!r.automatic&&r.index===0).text,'corrupt');
});

test('explicit zero autosaves preserves manual storage and defaults remain six plus three',async()=>{
 const c=structuredClone(source);c.project.visual.saveSystem.autoSlots=0;const store=new MemoryStore(),player=new WebPlayerController(c,runtime,{read:async()=>null,write:async()=>{}},validateSave,null,store);
 await player.newRun(null);const version={generation:player.generation,revision:player.session.state.stateRevision};assert.equal((await player.checkpoint(version)).error,null);assert.deepEqual(store.rows,[]);
 const original=structuredClone(source);delete original.project.visual.saveSystem;const defaults=new WebPlayerController(original,runtime,{read:async()=>null,write:async()=>{}},validateSave,null,new MemoryStore());const rows=(await defaults.slots({type:'open',kind:'load'},null)).view.saveSlots.rows;assert.equal(rows.filter(r=>!r.automatic).length,6);assert.equal(rows.filter(r=>r.automatic).length,3);
});

test('portable save keeps pending restored plugin state and rejects an uncommitted new presentation',async()=>{
 const c=await content(p=>delete doc(p,'project').visual);let saved=null;
 const host={active:false,presentationPending:false,restorationPending:false,saved:{sample:{time:150}},reset(){this.restorationPending=false;},save(){return this.saved;},scheduleRestore(value){this.saved=value;this.restorationPending=true;}};
 const player=new WebPlayerController(c,runtime,{read:async()=>saved,write:async text=>saved=text},validateSave,host);await player.newRun(null);const version=()=>({generation:player.generation,revision:player.session.state.stateRevision});assert.equal((await player.save(version())).error,null);const original=saved;
 host.presentationPending=true;assert.match((await player.save(version())).error,/화면 갱신/);assert.equal(saved,original);host.presentationPending=false;
 assert.equal((await player.load(version())).error,null);assert.equal(host.restorationPending,true);assert.equal((await player.save(version())).error,null);assert.deepEqual(JSON.parse(saved).playerPresentation.extensions,{sample:{time:150}});
});

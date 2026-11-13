import test from 'node:test';
import assert from 'node:assert/strict';
import createModule from '../../dist/wasm/raising-wasm.mjs';
import {WasmRuntime} from '../../dist/packages/native-bridge/wasm.js';
import {WebPlayerController} from '../../dist/packages/web-player/controller.js';
import {freeze} from '../../dist/packages/core/util.js';
import {content} from '../runtime/helpers.mjs';

const base=await content();
function measured(module){
 const calls=[];
 const wrapper={get HEAPU8(){return module.HEAPU8;},_malloc:n=>module._malloc(n),_free:p=>module._free(p),UTF8ToString:p=>module.UTF8ToString(p),_raising_process(p){const input=JSON.parse(module.UTF8ToString(p));calls.push({operation:input.operation,contentId:input.contentId,content:!!input.content,bytes:module.UTF8ToString(p).length});return module._raising_process(p);}};
 return {calls,module:wrapper,runtime:new WasmRuntime(wrapper)};
}
const create=(runtime,content)=>runtime.request({operation:'create',content,seed:42,options:{mode:'full'}});

test('Wasm immutable content registration removes repeated full transfers; mutable and shallow-frozen input remains current',async()=>{
 const {runtime,calls}=measured(await createModule());
 const frozen=freeze(structuredClone(base));const state=create(runtime,frozen).state;
 for(let i=0;i<40;i++)assert.equal(runtime.request({operation:'evaluate',content:frozen,state,condition:{op:'always'}}).value,true);
 assert.equal(calls.filter(c=>c.operation==='registerContent').length,1);
 assert.equal(calls.filter(c=>c.content).length,1,'only registration transfers the authored content');
 assert.ok(calls.filter(c=>c.operation==='evaluate').every(c=>c.contentId&&!c.content));
 for(const shallow of [false,true]){
  const mutable=structuredClone(base);if(shallow)Object.freeze(mutable);
  const key=mutable.attributes[0].id;mutable.project.initialState.attributes[key]=1;
  assert.equal(create(runtime,mutable).state.attributes[key],1);
  mutable.project.initialState.attributes[key]=2;
  assert.equal(create(runtime,mutable).state.attributes[key],2);
 }
 assert.equal(calls.filter(c=>c.operation==='registerContent').length,1,'nested mutable JSON must not be cached');
});

test('Wasm cache is bounded, shared per module, keyed by content identity, and retains structured failures',async()=>{
 const {runtime,calls,module}=measured(await createModule()),other=new WasmRuntime(module);
 const first=freeze(structuredClone(base));assert.equal(create(runtime,first).ok,true);assert.equal(create(other,first).ok,true);assert.equal(calls.filter(c=>c.operation==='registerContent').length,1);
 const key=first.attributes[0].id;
 for(let i=0;i<40;i++){const changed=structuredClone(base);changed.project.initialState.attributes[key]=i;assert.equal(create(i%2?runtime:other,freeze(changed)).state.attributes[key],i,'same content fingerprint cannot alias a different snapshot');}
 assert.ok(calls.filter(c=>c.operation==='releaseContent').length>=25);
 assert.equal(create(runtime,first).ok,true,'evicted content can be registered again');
 const invalid=freeze({});assert.equal(create(runtime,invalid).ok,false);assert.equal(create(runtime,invalid).ok,false);assert.equal(create(runtime,first).ok,true);
});

test('Web localized snapshots and projections remain correct across edits, locales, commands, new runs and replay',async()=>{
 const source=structuredClone(base),title=source.project.title;
 source.project.localization={defaultLocale:'en',fallbackLocale:'en',locales:[{id:'en',label:'English'},{id:'ja',label:'日本語'}],entries:[{key:'/project/title',source:title,translations:{en:'English title',ja:'日本語タイトル'}}]};
 const {runtime,calls}=measured(await createModule());let file;
 const player=new WebPlayerController(source,runtime,{read:async()=>file,write:async value=>file=value},()=>true);
 source.project.title='Edited outside the running snapshot';
 let view=(await player.view()).view;assert.equal(view.title,'English title');
 const count=()=>calls.filter(c=>c.operation==='view').length,first=count();
 await player.view();assert.equal(count(),first,'unchanged view does not recompute projection');
 assert.equal((await player.setLocale('ja')).view.title,'日本語タイトル');assert.equal(count(),first+1);
 const next=count();await player.view();assert.equal(count(),next);
 assert.equal((await player.setLocale('en')).view.title,'English title');
 await player.newRun(null);const version=()=>({generation:player.generation,revision:player.session.state.stateRevision});
 const initial=structuredClone(player.session.state);
 await player.act({type:'schedule',activityIds:Array(source.project.periodsPerMonth).fill(source.activities[0].id)},version());
 const planned=structuredClone(player.session.state);assert.notDeepEqual(initial,planned);
 assert.equal((await player.save(version())).error,null);
 await player.newRun(version());assert.deepEqual(player.session.state,initial);
 assert.equal((await player.load(version())).error,null);assert.deepEqual(player.session.state,planned);
 assert.equal((await player.view()).view.time.periodIndex,planned.periodIndex);
 assert.ok(calls.filter(c=>c.operation==='registerContent').length<8);
});

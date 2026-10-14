import test from 'node:test';
import assert from 'node:assert/strict';
import {Worker} from 'node:worker_threads';
import {readFile} from 'node:fs/promises';
import {resolve} from 'node:path';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';

test('32 addon workers isolate content registration, rules and diagnostic traces under simultaneous requests',{timeout:60000},async t=>{
 const manifest=JSON.parse(await readFile('dist/native/addon.json','utf8'));
 const filename=resolve('dist/native',manifest.filename);
 const built=await buildSnapshot(await loadProject('examples/academy-journey'));assert.equal(built.ok,true);
 const barrier=new SharedArrayBuffer(4),workers=[];
 t.after(async()=>{await Promise.all(workers.map(w=>w.terminate()));});
 const source=`
 const {parentPort,workerData}=require('node:worker_threads');
 const assert=require('node:assert/strict'),addon=require(workerData.filename);
 const request=r=>JSON.parse(addon.request(JSON.stringify(r)));
 const {index,content}=workerData;
 content.attributes.find(a=>a.id==='skill').initial=index;
 content.project.gameplay.actions=[{id:'thread-action',name:'Thread action',costs:[],effects:[{op:'add',target:{kind:'attribute',attributeId:'skill'},value:{op:'literal',value:index+1}}]}];
 const registration={operation:'registerContent',contentId:'shared-content-id',content};
 assert.equal(request(registration).ok,true);
 const barrier=new Int32Array(workerData.barrier);Atomics.add(barrier,0,1);Atomics.notify(barrier,0);
 while(Atomics.load(barrier,0)<32)Atomics.wait(barrier,0,Atomics.load(barrier,0),100);
 for(let n=0;n<100;n++){
   const created=request({operation:'create',contentId:'shared-content-id',seed:n,options:{mode:'full'}});
   assert.equal(created.ok,true);assert.equal(created.state.attributes.skill,index);
   const command={operation:'command',contentId:'shared-content-id',state:created.state,command:{type:'executeAction',actionId:'thread-action',quantity:1},expectedRevision:0};
   const inspected=request({...command,operation:'inspect'}),committed=request(command);
   assert.equal(inspected.ok,true,JSON.stringify(inspected.error));assert.equal(committed.ok,true,JSON.stringify(committed.error));assert.deepEqual(inspected.state,committed.state);
   assert.equal(committed.state.attributes.skill,index*2+1);assert.ok(inspected.trace.length>0);
   assert.equal('trace' in committed,false);assert.equal(created.state.attributes.skill,index);
   assert.equal(request({operation:'releaseContent',contentId:'shared-content-id'}).ok,true);
   assert.equal(request({operation:'create',contentId:'shared-content-id',seed:0,options:{mode:'full'}}).error.code,'UNKNOWN_CONTENT');
   assert.equal(request(registration).ok,true);
 }
 parentPort.postMessage({index,calls:100,skill:index*2+1});
 `;
 const results=await Promise.all(Array.from({length:32},(_,index)=>new Promise((resolve,reject)=>{
  const worker=new Worker(source,{eval:true,workerData:{index,filename,content:built.snapshot,barrier}});workers.push(worker);
  let reported=false;worker.once('message',value=>{reported=true;resolve(value);});worker.once('error',reject);
  worker.once('exit',code=>{if(!reported)reject(new Error(`Worker ${index} exited ${code} before reporting`));});
 })));
 assert.equal(new Set(results.map(r=>r.index)).size,32);assert.equal(results.reduce((sum,r)=>sum+r.calls,0),3200);
 results.forEach((r,index)=>assert.equal(r.skill,index*2+1));
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {Worker} from 'node:worker_threads';
import {waitForResponse} from '../../dist/packages/native-bridge/response-wait.js';

function notifier(source){
 const buffer=new SharedArrayBuffer(12),control=new Int32Array(buffer);
 const worker=new Worker(`const {workerData}=require('node:worker_threads');const c=new Int32Array(workerData);Atomics.store(c,1,1);Atomics.notify(c,1);${source}`,{eval:true,workerData:buffer,execArgv:[]});
 const deadline=Date.now()+5000;while(Atomics.load(control,1)===0){assert.ok(Date.now()<deadline,'Worker started');Atomics.wait(control,1,0,50);}
 return {worker,control};
}
test('a notification without a published status deterministically rechecks the predicate',()=>{
 const control=new Int32Array(new SharedArrayBuffer(4)),original=Atomics.wait;let calls=0;
 Atomics.wait=(array,index,expected,timeout)=>{assert.equal(array,control);assert.equal(index,0);assert.equal(expected,0);assert.ok(timeout>0);if(++calls===1)return 'ok';Atomics.store(control,0,1);return 'ok';};
 try{assert.equal(waitForResponse(control,3000),true);assert.equal(calls,2);assert.equal(Atomics.load(control,0),1);}finally{Atomics.wait=original;}
});
test('late notifications cannot return stale response bytes before the next status is published',{timeout:10000},async()=>{
 const {worker,control}=notifier(`setTimeout(()=>{Atomics.add(c,2,1);Atomics.notify(c,0);},10);setTimeout(()=>{Atomics.add(c,2,1);Atomics.notify(c,0);},25);setTimeout(()=>{Atomics.store(c,2,99);Atomics.store(c,0,1);Atomics.notify(c,0);},80);`);
 try{assert.equal(waitForResponse(control,3000),true);assert.equal(Atomics.load(control,0),1);assert.equal(Atomics.load(control,2),99,'Only the newly completed response may be consumed');}finally{await worker.terminate();}
});
test('repeated notifications retain one bounded deadline and do not manufacture a response',{timeout:10000},async()=>{
 const {worker,control}=notifier(`setInterval(()=>{Atomics.add(c,2,1);Atomics.notify(c,0);},5);`);
 try{const began=Date.now();assert.equal(waitForResponse(control,100),false);assert.equal(Atomics.load(control,0),0);assert.ok(Atomics.load(control,2)>0);assert.ok(Date.now()-began<1000,'Notifications must not restart the total timeout');}finally{await worker.terminate();}
});
test('already published success and failure status are returned without resetting them',()=>{
 const control=new Int32Array(new SharedArrayBuffer(4));for(const status of [1,2]){Atomics.store(control,0,status);assert.equal(waitForResponse(control,0),true);assert.equal(Atomics.load(control,0),status);}Atomics.store(control,0,0);assert.equal(waitForResponse(control,0),false);
});

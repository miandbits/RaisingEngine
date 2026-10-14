import test from 'node:test';
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { chromium } from 'playwright';
import { fixture } from '../gameplay/fixture.mjs';
import { createRun,dispatchCommand } from '../../dist/packages/core/run.js';

test('Chromium executes the compiled Wasm core for the whole gift route without a native process',async t=>{
  const {built}=await fixture(t);assert.equal(built.ok,true);const content=built.snapshot;
  const files={
    '/raising-wasm.mjs':['dist/wasm/raising-wasm.mjs','text/javascript'],
    '/raising-wasm.wasm':['dist/wasm/raising-wasm.wasm','application/wasm'],
    '/adapter.js':['dist/packages/native-bridge/wasm.js','text/javascript'],
  };
  const server=createServer(async(req,res)=>{
    if(req.url==='/'){res.setHeader('Content-Type','text/html');res.end('<!doctype html><meta charset="utf-8"><title>Native Wasm core verification</title><p id="status">Loading C++ WebAssembly</p><script type="module">import createModule from "/raising-wasm.mjs";import {WasmRuntime} from "/adapter.js";window.core=new WasmRuntime(await createModule());document.querySelector("#status").textContent=window.core.request({operation:"info"}).implementation;</script>');return;}
    const file=files[req.url];if(!file){res.writeHead(404).end();return;}
    try{res.setHeader('Content-Type',file[1]);res.end(await readFile(file[0]));}catch{res.writeHead(500).end();}
  });
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));t.after(()=>new Promise(resolve=>server.close(resolve)));
  const browser=await chromium.launch({channel:'chrome',headless:true});t.after(()=>browser.close());const page=await browser.newPage();
  const errors=[];page.on('pageerror',error=>errors.push(error.message));await page.goto(`http://127.0.0.1:${server.address().port}/`);
  await page.waitForFunction(()=>document.querySelector('#status').textContent==='C++17');
  const req=async request=>page.evaluate(value=>window.core.request(value),request);
  const expected=createRun(content,42,{mode:'full'});assert.deepEqual(await req({operation:'create',content,seed:42,options:{mode:'full'}}),expected);
  let state=expected.state;
  for(const command of [{type:'visit',locationId:'city'},{type:'visit',locationId:'market'},{type:'buy',shopId:'florist',itemId:'flower',quantity:1},{type:'visit',locationId:'city'},{type:'interact',interactionId:'gift-mentor'},{type:'advanceNarrative'},{type:'advanceNarrative'}]){
    const result=dispatchCommand(content,state,command,state.stateRevision);assert.deepEqual(await req({operation:'command',content,state,command,expectedRevision:state.stateRevision}),result);state=result.state;
  }
  assert.equal(state.completedEvents.includes('gift-friendship'),true);assert.equal(state.periodIndex,0);assert.deepEqual(errors,[]);
});

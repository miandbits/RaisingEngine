import test from 'node:test';
import assert from 'node:assert/strict';
import {build} from 'esbuild';
import {chromium} from 'playwright';

test('event actor text follows the current actor and leaves the speaker independent',async t=>{
  const bundle=await build({stdin:{contents:`import React from 'react';import {createRoot} from 'react-dom/client';import {VisualSurface} from './apps/player/renderer/VisualSurface.tsx';
    const root=createRoot(document.getElementById('root'));
    const design={width:640,height:360,screens:[{id:'story',label:'Story',elements:[{id:'owner',kind:'text',binding:'narrative-actor',x:0,y:0,width:300,height:40},{id:'speaker',kind:'text',binding:'speaker',x:0,y:50,width:300,height:40}]}]};
    window.showActor=(actorId)=>root.render(<VisualSurface design={design} screenId="story" assetUrl={()=>''} context={{phase:'narrative',period:0,stats:[],activities:[],plan:[],speaker:'Narrator',characters:[{id:'one',name:'One <b>literal</b>'},{id:'two',name:'Two'}],dataState:{activeNarrative:actorId===null?null:{actorId}}}}/>);`,resolveDir:process.cwd(),loader:'tsx'},bundle:true,write:false,outfile:'binding-test.js',format:'iife',logLevel:'silent'});
  const browser=await chromium.launch({headless:true,...(process.env.PLAYWRIGHT_CHANNEL?{channel:process.env.PLAYWRIGHT_CHANNEL}:{})});t.after(()=>browser.close());
  const page=await browser.newPage();const errors=[];page.on('pageerror',error=>errors.push(error.message));
  await page.setContent('<div id="root"></div>');await page.addScriptTag({content:bundle.outputFiles.find(f=>f.path.endsWith('.js')).text});
  for(const [actorId,expected] of [['one','One <b>literal</b>'],['two','Two'],['missing',''],[undefined,''],[null,''],['one','One <b>literal</b>']]){
    await page.evaluate(id=>window.showActor(id),actorId);
    await page.waitForFunction(value=>document.querySelector('[data-element-id="owner"]')?.textContent===value,expected);
    assert.equal(await page.locator('[data-element-id="speaker"]').textContent(),'Narrator');
    assert.equal(await page.locator('[data-element-id="owner"] b').count(),0);
  }
  assert.deepEqual(errors,[]);
});

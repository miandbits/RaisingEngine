import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,readFile} from 'node:fs/promises';
import {build} from 'esbuild';
import {chromium} from 'playwright';

test('Chromium renders fitted stage, cropped ordered layers and restored frame without a second clock',async t=>{
  const bundle=await build({stdin:{contents:`
    import React from 'react';
    import {createRoot} from 'react-dom/client';
    import {TimelineOverlay} from './apps/player/renderer/TimelineOverlay.tsx';
    const root=createRoot(document.getElementById('root'));
    window.raisingAssetUrl=id=>'data:image/svg+xml,'+encodeURIComponent('<svg xmlns="http://www.w3.org/2000/svg" width="64" height="64"><rect width="64" height="64" fill="'+({background:'#304d46',a:'#efbe5b',b:'#68c2df',legacy:'#ff00ff'}[id])+'"/></svg>');
    window.renderStageTest=(stage,cursor)=>root.render(<TimelineOverlay timeline={{plan:{clips:[{id:'test',clipId:'test',durationMs:650,stage,imageAssetId:'legacy',text:'Layered activity',day:null,transition:'none'}]},cursor}} generation={1} busy={true} act={()=>{}} save={()=>{}} load={()=>{}}/>);
  `,resolveDir:process.cwd(),loader:'tsx'},bundle:true,write:false,format:'iife',loader:{'.css':'empty'},logLevel:'silent'});
  const browser=await chromium.launch({headless:true,...(process.env.PLAYWRIGHT_CHANNEL?{channel:process.env.PLAYWRIGHT_CHANNEL}:{})});t.after(()=>browser.close());
  const page=await browser.newPage({viewport:{width:1000,height:700}});
  await page.setContent('<html><head></head><body><div id="root"></div></body></html>');
  await page.addStyleTag({content:await readFile('apps/player/renderer/timeline.css','utf8')});
  await page.addScriptTag({content:bundle.outputFiles[0].text});
  const stage={width:320,height:128,layers:[
    {id:'background',x:0,y:0,width:320,height:128,frames:[{imageAssetId:'background',durationMs:100}]},
    {id:'actor',x:-8,y:20,width:64,height:96,loop:true,frames:[{imageAssetId:'a',durationMs:100},{imageAssetId:'b',durationMs:200}]},
  ]};
  const cursor={clipIndex:0,elapsedMs:400,speed:1,paused:true,completed:false};
  const render=async c=>{
    await page.evaluate(({stage,cursor})=>window.renderStageTest(stage,cursor),{stage,cursor:c});
    await page.locator('#timeline-stage').waitFor();
    await page.waitForFunction(()=>[...document.querySelectorAll('.timeline-layer')].every(x=>x.complete&&x.naturalWidth>0));
  };
  await render(cursor);
  assert.equal(await page.locator('[data-layer-id="actor"]').getAttribute('data-frame-index'),'1');
  const measure=()=>page.evaluate(()=>{
    const s=document.querySelector('#timeline-stage'),a=document.querySelector('[data-layer-id="actor"]'),sr=s.getBoundingClientRect(),ar=a.getBoundingClientRect();
    return {ratio:sr.width/sr.height,x:(ar.x-sr.x)/sr.width,y:(ar.y-sr.y)/sr.height,width:ar.width/sr.width,height:ar.height/sr.height,overflow:getComputedStyle(s).overflow,fit:getComputedStyle(a).objectFit,layers:[...s.children].map(x=>x.dataset.layerId),legacy:!!document.querySelector('#timeline-image')};
  });
  for(const size of [{width:1000,height:700},{width:440,height:640}]){
    await page.setViewportSize(size);const m=await measure();
    assert.ok(Math.abs(m.ratio-2.5)<0.001);assert.ok(Math.abs(m.x+8/320)<0.001);
    assert.ok(Math.abs(m.y-20/128)<0.001);assert.ok(Math.abs(m.width-64/320)<0.001);assert.ok(Math.abs(m.height-96/128)<0.001);
    assert.equal(m.overflow,'hidden');assert.equal(m.fit,'fill');assert.deepEqual(m.layers,['background','actor']);assert.equal(m.legacy,false);
  }
  await render(JSON.parse(JSON.stringify(cursor)));assert.equal(await page.locator('[data-layer-id="actor"]').getAttribute('data-frame-index'),'1');
  await page.setViewportSize({width:1000,height:700});await mkdir('output/playwright',{recursive:true});
  await page.screenshot({path:'output/playwright/timeline-stage.png'});
  await render({...cursor,completed:true,elapsedMs:0});
  await page.waitForFunction(()=>document.querySelector('[data-layer-id="actor"]').dataset.frameIndex==='0');
  assert.equal(await page.locator('[data-layer-id="actor"]').getAttribute('data-frame-index'),'0');
});

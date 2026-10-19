import test from 'node:test';
import assert from 'node:assert/strict';
import {build} from 'esbuild';
import {chromium} from 'playwright';

test('expedition nodes reuse action controls and never expose unlisted global actions; speaker portrait follows context',async t=>{
 const bundle=await build({stdin:{contents:`
 import React from 'react';import {createRoot} from 'react-dom/client';
 import {VisualSurface,imageFor} from './apps/player/renderer/VisualSurface.tsx';
 const root=createRoot(document.getElementById('root'));window.commands=[];window.imageFor=imageFor;
 const element={id:'trip',kind:'expedition',x:0,y:0,width:700,height:650};
 const design={width:800,height:700,screens:[{id:'home',elements:[element]}]};
 window.renderTrip=(ids,enabled=true)=>{const context={title:'',description:'',calendar:'',ending:'',speaker:'',story:'',phase:'expedition',background:null,portraits:{},activities:[],plan:[],period:0,choices:[],result:'',stats:[{id:'money',label:'Coins',value:8,min:0,max:99}],gameplay:{location:null,locations:[],shops:[],interactions:[],inventory:[],actions:[{id:'feed',name:'Feed wolf',costs:[{attributeId:'money',amount:2}],effects:[],maxQuantity:2,enabled,reason:enabled?null:'Missing food'},{id:'unlisted',name:'Unlisted action',costs:[],effects:[],maxQuantity:1,enabled:true}]},systems:{expedition:{expeditionId:'hill',name:'Hill',nodeId:'rock',nodeName:'Rock',heroHp:10,stats:{maxHp:10},connections:[],canReturn:true,...(ids===null?{}:{actionIds:ids})}}};root.render(<VisualSurface design={design} screenId="home" context={context} assetUrl={id=>id} onCommand={command=>window.commands.push(command)}/>);};
 `,resolveDir:process.cwd(),loader:'tsx'},loader:{'.css':'empty'},bundle:true,write:false,format:'iife',logLevel:'silent'});
 const browser=await chromium.launch({channel:'chrome',headless:true});t.after(()=>browser.close());const page=await browser.newPage();
 await page.setContent('<style>.visual-viewport{position:relative;width:800px;height:700px}.visual-stage{transform-origin:top left}</style><div id="root"></div>');await page.addScriptTag({content:bundle.outputFiles[0].text});
 const render=async(ids,enabled=true)=>{await page.evaluate(([ids,enabled])=>window.renderTrip(ids,enabled),[ids,enabled]);await page.getByTestId('expedition-widget').waitFor();await page.waitForTimeout(50);};
 await render(['feed']);assert.equal(await page.getByRole('button',{name:'Unlisted action',exact:true}).count(),0);
 await page.getByRole('button',{name:'action__trip__node-rock__feed 수량 늘리기',exact:true}).click();await page.getByRole('button',{name:'Feed wolf',exact:true}).click();
 assert.deepEqual(await page.evaluate(()=>window.commands),[{type:'executeAction',actionId:'feed',quantity:2}]);
 await render(['feed'],false);assert.equal(await page.getByRole('button',{name:'Feed wolf',exact:true}).isDisabled(),true);assert.match(await page.getByTestId('expedition-widget').innerText(),/Missing food/);
 for(const ids of [[],null]){await render(ids);assert.equal(await page.getByTestId('actions-widget').count(),0);}
 const sources=await page.evaluate(()=>['soha','yeon',null].map(speakerPortrait=>window.imageFor({imageSource:'speaker-portrait'},{portraits:{},speakerPortrait})??null));assert.deepEqual(sources,['soha','yeon',null]);
});

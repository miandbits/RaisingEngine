import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,mkdir} from 'node:fs/promises';
import {build} from 'esbuild';
import {chromium} from 'playwright';

test('activity details use current values, escape formats, wrap, and hide only zero result rows',async t=>{
  const bundle=await build({stdin:{contents:`
    import React from 'react';import {createRoot} from 'react-dom/client';
    import {VisualPlayer} from './apps/player/renderer/VisualPlayer.tsx';
    const root=createRoot(document.getElementById('root'));window.commands=[];
    const activities=[{id:'study',label:'Study',previewAttributes:[{attributeId:'tier',label:'단계'},{attributeId:'fee',label:'비용',valueFormat:'<b>{value}</b> $& {value}'}]},{id:'rest',label:'Rest'}];
    const elements=[{id:'schedule',kind:'schedule',x:10,y:10,width:700,height:300,visibleWhen:'always'},{id:'result',kind:'result',x:10,y:320,width:700,height:160,visibleWhen:'result'}];
    const design={width:720,height:500,screens:['title','home','schedule'].map(id=>({id,label:id,elements}))};
    let props={view:{generation:1,title:'Summary',description:'',state:null,initialState:{attributes:{tier:2,fee:9}},attributes:[{id:'tier',label:'Tier',initial:1,min:0,max:10},{id:'fee',label:'Fee',initial:7,min:0,max:100}],characters:[],activities,systems:null},design,busy:false,plan:['study','study','rest'],onPlan:plan=>window.commands.push(plan),act:a=>window.commands.push(a),newRun:()=>{},save:()=>{},load:()=>{},sound:()=>{},muted:true,preview:false,error:null,notice:null};
    window.update=p=>{props={...props,...p,view:{...props.view,...p.view}};root.render(<VisualPlayer {...props}/>);};window.update({});
  `,resolveDir:process.cwd(),loader:'tsx'},bundle:true,write:false,format:'iife',loader:{'.css':'empty'},logLevel:'silent'});
  const browser=await chromium.launch({headless:true,...(process.env.PLAYWRIGHT_CHANNEL?{channel:process.env.PLAYWRIGHT_CHANNEL}:{})});t.after(()=>browser.close());
  const page=await browser.newPage({viewport:{width:900,height:700}});
  await page.setContent('<div id="root"></div>');await page.addStyleTag({content:await readFile('apps/player/renderer/visual.css','utf8')});await page.addScriptTag({content:bundle.outputFiles[0].text});
  await page.locator('.activity-preview').first().waitFor();
  assert.equal(await page.locator('.activity-preview').first().textContent(),'현재 단계 2 · 비용 <b>9</b> $& 9');
  const state={phase:'ready',stateRevision:1,attributes:{tier:3,fee:11},relationships:{},gameplay:{screenId:'home'},presentation:{portraits:[]}};
  await page.evaluate(state=>window.update({view:{state}}),state);
  await page.locator('.activity-preview').first().waitFor();
  assert.equal(await page.locator('.activity-preview').first().textContent(),'현재 단계 3 · 비용 <b>11</b> $& 11');
  assert.equal(await page.locator('.activity-preview b').count(),0);
  assert.equal(await page.getByRole('option',{name:'Rest',exact:true}).count(),3);
  await page.getByRole('combobox',{name:'상순 활동'}).selectOption('rest');assert.deepEqual(await page.evaluate(()=>window.commands),[['rest','study','rest']]);
  // Eight long display entries must stay inside the selected column.
  await page.evaluate(()=>window.update({view:{activities:[{id:'study',label:'Study',previewAttributes:Array.from({length:8},(_,i)=>({attributeId:'fee',label:'정보'+i,valueFormat:'긴정보'.repeat(24)+'{value}'}))},{id:'rest',label:'Rest'}]}}));
  await page.waitForFunction(()=>document.querySelector('.activity-preview')?.textContent.includes('정보7'));
  assert.equal(await page.locator('.activity-preview').first().evaluate(e=>e.scrollWidth<=e.clientWidth),true);
  // Descriptions are opt-in, escaped text and update with the selected activity.
  await page.evaluate(()=>window.update({view:{activities:[{id:'study',label:'Study',description:'<b>집중 수련</b> · 기력 -5'},{id:'rest',label:'Rest',description:'기력 +25'}]}}));
  assert.equal(await page.locator('.activity-description').count(),0);
  await page.evaluate(()=>window.update({design:{width:720,height:500,screens:[{id:'home',label:'home',elements:[{id:'schedule',kind:'schedule',x:10,y:10,width:700,height:300,showActivityDescription:true}]}]}}));
  await page.locator('.activity-description').first().waitFor();
  assert.equal(await page.locator('.activity-description').first().textContent(),'<b>집중 수련</b> · 기력 -5');
  assert.equal(await page.locator('.activity-description b').count(),0);
  await page.evaluate(()=>window.update({plan:['rest','study','rest']}));
  await page.waitForFunction(()=>document.querySelector('.activity-description')?.textContent==='기력 +25');
  await page.evaluate(()=>window.update({design:{width:720,height:500,screens:['home','schedule'].map(id=>({id,label:id,elements:[{id:'result',kind:'result',x:10,y:320,width:700,height:160,visibleWhen:'result'}]}))}}));
  const changes=[{kind:'numeric',target:{kind:'attribute',attributeId:'tier'},before:3,after:3,delta:0},{kind:'numeric',target:{kind:'attribute',attributeId:'fee'},before:11,after:9,delta:-2}];
  await page.evaluate(({state,changes})=>window.update({view:{state:{...state,phase:'activity-result',lastActivityResult:{activityId:'study',success:null,changes}}}}),{state,changes});
  await page.locator('[data-element-id="result"]').waitFor();assert.equal(await page.locator('[data-element-id="result"]').textContent(),'Study 완료\nFee -2');
  await page.evaluate(({state,changes})=>window.update({view:{state:{...state,phase:'activity-result',lastActivityResult:{activityId:'study',success:null,changes:[changes[0]]}}}}),{state,changes});
  await page.waitForFunction(()=>document.querySelector('.visual-result')?.textContent.includes('능력 변화 없음'));
  await mkdir('output/playwright',{recursive:true});await page.screenshot({path:'output/playwright/activity-summary-result.png'});
});

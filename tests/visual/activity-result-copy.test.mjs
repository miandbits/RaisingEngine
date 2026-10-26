import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,mkdir,mkdtemp,writeFile} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {build} from 'esbuild';
import {chromium} from 'playwright';

test('actual result widgets localize engine words and retain authored labels, actor names, timeline text and committed state',{timeout:120000},async t=>{
 const built=await build({stdin:{contents:`
 import React,{useState} from 'react';import {createRoot} from 'react-dom/client';import {VisualPlayer} from './apps/player/renderer/VisualPlayer.tsx';
 const numeric=(target,delta)=>({kind:'numeric',target,delta,before:5,after:5+delta});
 const result=(kind)=>({activityId:'work',success:kind==='none'?null:{passed:kind!=='failure'},changes:kind==='none'?[]:[numeric({kind:'attribute',attributeId:'skill'},2),numeric({kind:'relationship',characterId:'friend',relationshipId:'trust'},3),numeric({kind:'inventory',itemId:'book'},-1)]});
 function App(){const [config,setConfig]=useState({locale:'en',kind:'success'});window.configure=setConfig;
 const r=result(config.kind);if(config.kind==='actor')r.actorResults=[{characterId:'hero',result:result('failure')}];
 const state={mode:'full',phase:'activity-result',stateRevision:7,periodIndex:0,schedule:['work'],attributes:{skill:7},relationships:{friend:{trust:8}},lastActivityResult:r,presentation:{portraits:[]}};
 const visual={width:1280,height:720,screens:[{id:'schedule',elements:[{id:'result',kind:'result',color:'#fff8e6',x:40,y:40,width:1100,height:580}]}]};
 const view={generation:1,title:'Result copy',description:'',initialState:{},state,attributes:[{id:'skill',label:'Authored skill'}],activities:[{id:'work',label:'Authored workshop'}],characters:[{id:'hero',name:'Authored actor',portraits:{},relationships:[]},{id:'friend',name:'Authored friend',portraits:{},relationships:[{id:'trust',label:'Authored trust'}]}],assets:[],gameplayDefinition:{items:[{id:'book',name:'Authored book'}]},narrative:null,locale:config.locale,...(config.kind==='timeline'?{timeline:{plan:{resultPlayback:{title:'Authored timeline · 완료',rows:[]},clips:[]},cursor:{completed:true}}}:{})};
 window.state=state;window.actions=[];
 return <VisualPlayer view={view} design={visual} busy={false} plan={['work']} onPlan={()=>window.actions.push('plan')} act={a=>window.actions.push(a)} newRun={()=>{}} save={()=>{}} load={()=>{}} sound={()=>{}} muted={true} preview={false} error={null} notice={null}/>;
 }createRoot(document.getElementById('root')).render(<App/>);
 `,resolveDir:process.cwd(),loader:'tsx'},bundle:true,write:false,format:'iife',loader:{'.css':'empty'},logLevel:'silent'});
 const browser=await chromium.launch({headless:true,channel:process.env.PLAYWRIGHT_CHANNEL??'chrome'});t.after(()=>browser.close());const page=await browser.newPage({viewport:{width:1280,height:800}}),errors=[];page.on('pageerror',e=>errors.push(e.message));
 await mkdir('.cache/activity-result-copy',{recursive:true});const out=await mkdtemp(resolve('.cache/activity-result-copy/web-'));
 await page.setContent('<div id="root"></div>');await page.addStyleTag({content:await readFile('apps/player/renderer/visual.css','utf8')});await page.addScriptTag({content:built.outputFiles[0].text});await page.waitForFunction(()=>window.configure);
 const receipts=[];
 for(const [locale,complete,success,failure,none] of [['en-GB','complete','Success','Failure','No numeric changes'],['ko','완료','성공','실패','수치 변화 없음'],['ja-JP','完了','成功','失敗','数値の変化なし']])for(const kind of ['success','failure','none','actor','timeline']){
  await page.evaluate(c=>window.configure(c),{locale,kind});await page.evaluate(()=>new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r))));
  const before=await page.evaluate(()=>JSON.stringify(window.state)),text=await page.locator('[data-element-id="result"]').innerText();
  if(kind==='timeline')assert.match(text,/Authored timeline · 완료/,'Authored timeline text remains verbatim');
  else {assert.ok(text.includes(kind==='actor'?'Authored actor · Authored workshop':`Authored workshop ${complete}`),text);
   if(kind==='none')assert.ok(text.includes(none),text);else{assert.ok(text.includes(kind==='success'?success:failure),text);for(const label of ['Authored skill +2','Authored friend / Authored trust +3','Authored book -1'])assert.ok(text.includes(label),text);}
   if(locale.startsWith('en'))assert.doesNotMatch(text,/[가-힣]/);
  }
  assert.equal(await page.evaluate(()=>JSON.stringify(window.state)),before);assert.deepEqual(await page.evaluate(()=>window.actions),[]);receipts.push({locale,kind,text});
  if(kind==='success')await page.screenshot({path:join(out,locale+'.png')});
 }
 assert.deepEqual(errors,[]);await writeFile(join(out,'proof.json'),JSON.stringify({ok:true,cases:receipts},null,2));
});

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

test('actual choice widgets localize unavailable hints and empty results without changing authored copy or disabled semantics',{timeout:120000},async t=>{
 const built=await build({stdin:{contents:`
 import React,{useState} from 'react';import {createRoot} from 'react-dom/client';import {VisualPlayer} from './apps/player/renderer/VisualPlayer.tsx';
 const freeze=value=>{if(value&&typeof value==='object'){Object.values(value).forEach(freeze);Object.freeze(value);}return value;};
 const state=freeze({mode:'full',phase:'narrative',stateRevision:11,periodIndex:4,schedule:['work'],attributes:{skill:3},relationships:{},activeNarrative:{kind:'event',id:'event',nodeId:'decision'},presentation:{portraits:[]}});
 const choices=freeze([{id:'blocked',text:'Authored unavailable · 원문',enabled:false,hint:'Authored effect · 原文'},{id:'allowed',text:'Authored available · 原文',enabled:true,hint:'Authored reward · 원문'}]);
 const visual={width:1280,height:720,screens:[{id:'story',elements:[{id:'choices',kind:'choices',choiceLayout:'cards',showChoiceEffects:true,x:40,y:40,width:1100,height:340},{id:'result',kind:'result',color:'#fff8e6',x:40,y:420,width:1100,height:160}]}]};
 window.state=state;window.choices=choices;window.actions=[];
 function App(){const [locale,setLocale]=useState('en');window.configure=setLocale;
 const view={generation:1,title:'Choice copy',description:'',initialState:{},state,attributes:[],activities:[],characters:[],assets:[],gameplayDefinition:{items:[]},narrative:{node:{id:'decision',type:'choice',prompt:'Authored prompt'},choices},choiceHints:Object.fromEntries(choices.map(choice=>[choice.id,choice.hint])),...(locale===null?{}:{locale})};
 return <VisualPlayer view={view} design={visual} busy={false} plan={['work']} onPlan={p=>window.actions.push({type:'plan',p})} act={a=>window.actions.push(a)} newRun={()=>{}} save={()=>{}} load={()=>{}} sound={()=>{}} muted={true} preview={false} error={null} notice={null}/>;
 }createRoot(document.getElementById('root')).render(<App/>);
 `,resolveDir:process.cwd(),loader:'tsx'},bundle:true,write:false,format:'iife',loader:{'.css':'empty'},logLevel:'silent'});
 const browser=await chromium.launch({headless:true,channel:process.env.PLAYWRIGHT_CHANNEL??'chrome'});t.after(()=>browser.close());const page=await browser.newPage({viewport:{width:1280,height:800}}),errors=[];page.on('pageerror',e=>errors.push(e.message));
 await mkdir('.cache/activity-result-copy',{recursive:true});const out=await mkdtemp(resolve('.cache/activity-result-copy/choice-web-'));
 await page.setContent('<div id="root"></div>');await page.addStyleTag({content:await readFile('apps/player/renderer/visual.css','utf8')});await page.addScriptTag({content:built.outputFiles[0].text});await page.waitForFunction(()=>window.configure);
 const before=await page.evaluate(()=>JSON.stringify({state:window.state,choices:window.choices})),receipts=[];
 for(const [locale,hint,placeholder] of [
  ['en-GB','Requirements not met','Activity results will appear here.'],
  ['ko','조건 미충족','활동 결과가 여기에 표시됩니다.'],
  ['ja-JP','条件を満たしていません','活動結果がここに表示されます。'],
  [null,'조건 미충족','활동 결과가 여기에 표시됩니다.'],
  ['fr-FR','조건 미충족','활동 결과가 여기에 표시됩니다.']
 ]){
  await page.evaluate(locale=>{window.actions.length=0;window.configure(locale);},locale);await page.evaluate(()=>new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r))));
  const buttons=page.locator('[data-element-id="choices"] button'),blocked=buttons.nth(0),allowed=buttons.nth(1);
  assert.equal(await buttons.count(),2);assert.equal(await blocked.isDisabled(),true);assert.equal(await allowed.isEnabled(),true);
  assert.deepEqual(await page.locator('[data-history-choice-label]').allTextContents(),['Authored unavailable · 원문','Authored available · 原文']);
  assert.deepEqual(await page.locator('.choice-hint').allTextContents(),['Authored effect · 原文','Authored reward · 원문']);
  assert.equal(await blocked.locator('small:not(.choice-hint)').innerText(),hint);assert.equal(await allowed.locator('small:not(.choice-hint)').count(),0);
  assert.equal(await page.locator('[data-element-id="result"]').innerText(),placeholder);
  const box=await blocked.boundingBox();assert.ok(box&&box.width>44&&box.height>44);
  await page.mouse.click(box.x+box.width/2,box.y+box.height/2);assert.deepEqual(await page.evaluate(()=>window.actions),[],'A pointer press on an unavailable choice must not dispatch');
  await blocked.evaluate(button=>button.focus());assert.equal(await blocked.evaluate(button=>document.activeElement===button),false,'Unavailable choices cannot gain keyboard focus');
  await allowed.focus();await page.keyboard.press('Shift+Tab');assert.equal(await blocked.evaluate(button=>document.activeElement===button),false,'Tab navigation skips unavailable choices');
  for(let step=0;step<12&&!await allowed.evaluate(button=>document.activeElement===button);step++){await page.keyboard.press('Tab');assert.equal(await blocked.evaluate(button=>document.activeElement===button),false,'Forward Tab navigation skips unavailable choices');}
  assert.equal(await allowed.evaluate(button=>document.activeElement===button),true,'The enabled choice remains keyboard reachable');assert.deepEqual(await page.evaluate(()=>window.actions),[]);
  await page.keyboard.press('Enter');await allowed.click();
  assert.deepEqual(await page.evaluate(()=>window.actions),[{type:'choose',choiceId:'allowed'},{type:'choose',choiceId:'allowed'}],'Enabled pointer and keyboard commands remain exact');
  assert.equal(await page.evaluate(()=>JSON.stringify({state:window.state,choices:window.choices})),before,'Presentation and command dispatch never mutate committed state or authored choices');
  receipts.push({locale,unavailable:hint,emptyResult:placeholder,blockedBounds:box,commands:await page.evaluate(()=>window.actions)});
  if(locale&&locale!=='fr-FR')await page.screenshot({path:join(out,locale+'.png')});
 }
 assert.deepEqual(errors,[]);await writeFile(join(out,'proof.json'),JSON.stringify({ok:true,cases:receipts},null,2));
});

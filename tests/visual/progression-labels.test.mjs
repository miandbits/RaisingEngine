import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {build} from 'esbuild';
import {chromium} from 'playwright';

test('rendered advance labels describe confirmation, execution and results in each locale and honor authored text',{timeout:120000},async t=>{
  const bundle=await build({stdin:{contents:`
    import React,{useState} from 'react';import {createRoot} from 'react-dom/client';
    import {VisualPlayer} from './apps/player/renderer/VisualPlayer.tsx';
    import {progressionButtonLabel} from './apps/player/renderer/progression-label.ts';
    window.label=progressionButtonLabel;window.actions=[];
    const base=()=>({mode:'full',phase:'planning',stateRevision:0,periodIndex:0,schedule:null,activeNarrative:null,attributes:{},flags:{},relationships:{},lastActivityResult:null,presentation:{backgroundAssetId:null,musicAssetId:null,portraits:[]}});
    function App(){const [state,setState]=useState(base),[plan,setPlan]=useState(['study','rest']),[locale,setLocale]=useState('en'),[override,setOverride]=useState(false);
      window.configure=(options={})=>{setState({...base(),...options.state});setPlan(options.plan??['study','rest']);setLocale(options.locale??'en');setOverride(options.override??false);window.actions=[];};window.inspect=()=>({state,plan,actions:window.actions});
      const leaf=(id,kind,extra={})=>({id,kind,x:32,y:32,width:1050,height:360,...extra});
      const controls=[leaf('advance','button',{x:32,y:560,width:260,height:64,action:'advance',text:override?'I choose my own pace':'Continue',...(override?{autoLabel:false}:{})}),leaf('commit','button',{x:320,y:560,width:260,height:64,action:'commitPlan',text:'Begin this plan'})];
      const visual={width:1280,height:720,readableLayout:{maxShortEdge:600},routing:{defaultScreenId:'main',phases:[{phase:'dialogue',screenId:'main'},{phase:'choice',screenId:'main'},{phase:'finished',screenId:'main'}]},screens:[{id:'main',label:'Planning',elements:[leaf('schedule','schedule'),...controls],readableLayout:{items:[{elementId:'schedule',schedulePresentation:'compact',planningGuidance:'first-use'}],actions:['advance','commit'],actionLayout:'grouped',primaryAction:'advance'}}]};
      const view={generation:1,title:'Progression labels',description:'',initialState:{attributes:{},relationships:{}},durationMonths:1,periodsPerMonth:2,attributes:[],characters:[],activities:[{id:'study',label:'Study'},{id:'rest',label:'Rest'}],schedulePlayback:'manual',assets:[],state,time:{year:1,month:1,periodInMonth:0,periodIndex:0},calendarProject:{startYear:1,startMonth:1,periodsPerMonth:2,calendar:{monthsPerYear:12,daysPerMonth:28}},narrative:null,visual,gameplayDefinition:null,gameplay:null,systems:null,locale};
      const act=action=>{window.actions.push(action);setState(s=>({...s,stateRevision:s.stateRevision+1,...(action.type==='schedule'?{phase:'ready',schedule:[...action.activityIds]}:action.type==='execute'?s.activityIntermission?{phase:'ready',activityIntermission:false}:{phase:'activity-result'}:action.type==='commitPlan'?{phase:'activity-result',schedule:[...action.activityIds]}:{phase:'planning',schedule:null})}));};
      return <VisualPlayer view={view} design={visual} busy={false} plan={plan} onPlan={setPlan} act={act} newRun={()=>{}} save={()=>{}} load={()=>{}} sound={()=>{}} muted={true} error={null} notice={null}/>;
    }createRoot(document.getElementById('root')).render(<App/>);
  `,resolveDir:process.cwd(),loader:'tsx'},bundle:true,write:false,format:'iife',loader:{'.css':'empty'},logLevel:'silent'});
  const browser=await chromium.launch({headless:true,channel:process.env.PLAYWRIGHT_CHANNEL??'chrome'});t.after(()=>browser.close());
  const page=await browser.newPage({viewport:{width:1280,height:800}}),errors=[];page.on('pageerror',e=>errors.push(e.message));page.setDefaultTimeout(10000);
  await page.setContent('<base href="http://fixture.test/"><style>html,body,#root{margin:0;width:100%;height:100%}</style><div id="root"></div>');await page.addStyleTag({content:await readFile('apps/player/renderer/visual.css','utf8')});await page.addScriptTag({content:bundle.outputFiles[0].text});
  await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));assert.deepEqual(errors,[]);
  const advance=page.locator('[data-element-id="advance"] button'),commit=page.locator('[data-element-id="commit"] button');await advance.waitFor();
  const configure=async options=>{await page.evaluate(options=>window.configure(options),options);await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))));};
  for(const [locale,confirm,start,continued] of [['en','Confirm schedule','Start activity','Continue'],['ko','일정 확정','활동 시작','결과 확인 · 다음'],['ja','予定を確定','活動を開始','次へ']]){
    await configure({locale});assert.equal(await advance.innerText(),confirm);assert.equal(await commit.innerText(),'Begin this plan');
    await advance.click();assert.equal(await advance.innerText(),start);assert.deepEqual(await page.evaluate(()=>window.actions),[{type:'schedule',activityIds:['study','rest']}]);
    await page.getByRole('combobox').nth(0).selectOption('rest');assert.equal(await advance.innerText(),confirm,'Changing a ready-phase draft needs confirmation, not execution');assert.equal(await page.evaluate(()=>window.inspect().state.phase),'ready');assert.equal(await page.evaluate(()=>window.actions.length),1,'Editing does not execute');
    await advance.click();assert.equal(await advance.innerText(),start);assert.deepEqual((await page.evaluate(()=>window.actions)).at(-1),{type:'schedule',activityIds:['rest','rest']});
    await advance.click();assert.equal(await advance.innerText(),continued);assert.deepEqual((await page.evaluate(()=>window.actions)).at(-1),{type:'execute'});assert.equal(await page.evaluate(()=>window.inspect().state.phase),'activity-result');
    await configure({locale,override:true,state:{phase:'activity-result'}});assert.equal(await advance.innerText(),'I choose my own pace','An explicit override survives the result phase in every locale');
    await configure({locale,override:true});assert.equal(await advance.innerText(),'I choose my own pace');assert.equal(await commit.innerText(),'Begin this plan');
  }
  await configure({state:{phase:'ready',schedule:['study','rest'],activityIntermission:true}});assert.equal(await advance.innerText(),'Continue');
  await page.getByRole('combobox').nth(0).selectOption('rest');assert.equal(await advance.innerText(),'Confirm schedule','Web handler confirms a changed intermission draft before executing');
  await configure({});await commit.click();assert.deepEqual(await page.evaluate(()=>window.actions),[{type:'commitPlan',activityIds:['study','rest']}]);
  // Native-shaped raw phases can differ from the presentation phase. Roster
  // ready has no scalar schedule, and must still describe execution truthfully.
  const edge=await page.evaluate(()=>{
    const e={action:'advance',text:'Fallback'},label=context=>window.label(e,{locale:'en',plan:['rest'],phase:'result',...context});
    return {ready:label({dataState:{phase:'ready',schedule:['rest']}}),result:label({dataState:{phase:'activity-result'}}),narrative:label({dataState:{phase:'narrative'}}),rosterPlanning:label({dataState:{phase:'planning',roster:{}}}),rosterReady:label({dataState:{phase:'ready',roster:{}}}),shortPlan:window.label(e,{locale:'en',phase:'ready',plan:[],dataState:{phase:'ready',schedule:['rest']}})};
  });assert.deepEqual(edge,{ready:'Start activity',result:'Continue',narrative:'Continue story',rosterPlanning:'Confirm schedule',rosterReady:'Start activity',shortPlan:'Confirm schedule'});
  await configure({});await page.setViewportSize({width:390,height:844});await page.locator('[data-reading-id="board-help"]').waitFor();
  const guide=async expected=>{await page.locator('[data-reading-id="board-help"]').click();await page.locator('[data-reading-id="engine-planning-guide-next"]').click();await page.locator('[data-reading-id="engine-planning-guide-next"]').click();assert.match(await page.locator('.planning-guide-body').innerText(),expected);await page.locator('[data-reading-id="engine-planning-guide-done"]').click();};
  await guide(/“Confirm schedule” confirms the schedule\. That step does not execute an activity yet/);await advance.click();await guide(/“Start activity” executes the next activity/);
  await page.locator('[data-reading-id="board-slot-0"]').click();await page.locator('[data-reading-id="board-activity-rest"]').click();await page.locator('[data-reading-id="board-apply"]').click();assert.equal(await advance.innerText(),'Confirm schedule');await guide(/“Confirm schedule” confirms the schedule/);
  assert.deepEqual(errors,[]);
});

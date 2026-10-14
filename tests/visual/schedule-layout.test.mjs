import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,mkdir} from 'node:fs/promises';
import {build} from 'esbuild';
import {chromium} from 'playwright';

test('528px menu keeps two columns, calendar selection and scene portrait geometry',async t=>{
  const bundle=await build({stdin:{contents:`
    import React from 'react';import {createRoot} from 'react-dom/client';import {VisualPlayer} from './apps/player/renderer/VisualPlayer.tsx';
    const root=createRoot(document.getElementById('root'));window.commands=[];
    const activities=Array.from({length:11},(_,i)=>({id:'course-'+i,label:'긴 교육 과정 '+i+' '+('긴 이름 '.repeat(8)),...(i===1?{iconAssetId:'icon'}:{}),previewAttributes:Array.from({length:8},(_,n)=>({attributeId:'fee',label:'정보'+n,valueFormat:'긴정보'.repeat(20)+' {value}'}))}));
    const elements=[{id:'calendar',kind:'schedule',scheduleLayout:'calendar',x:10,y:10,width:500,height:430},{id:'menu',kind:'schedule',scheduleLayout:'menu',x:530,y:10,width:528,height:392,scheduleGroups:[{id:'education',label:'교육',activityIds:activities.map(a=>a.id)}]},{id:'portrait',kind:'image',imageSource:'portrait-center',x:100,y:80,width:400,height:600},{id:'fixed',kind:'image',imageSource:'asset',imageAssetId:'icon',x:800,y:500,width:100,height:100}];
    for(const [i,binding] of ['calendar-year','calendar-month','calendar-day','calendar-weekday','schedule-prompt'].entries())elements.push({id:binding,kind:'text',binding,x:10+i*170,y:700,width:160,height:40});
    const design={width:1100,height:750,widgetTheme:'ornate',screens:['title','home','schedule'].map(id=>({id,label:id,elements}))};
    const state={phase:'planning',stateRevision:1,periodIndex:0,attributes:{fee:7},relationships:{},gameplay:{screenId:'home'},presentation:{portraits:[{characterId:'hero',portraitId:'face',position:'center',scale:.2,offsetX:.1,offsetY:-.1}]}};
    let props={view:{generation:1,title:'Schedule',description:'',state,time:{year:1,month:1,periodInMonth:0},initialState:{attributes:{}},attributes:[{id:'fee',label:'비용',initial:7,min:0,max:100}],characters:[{id:'hero',portraits:{face:'icon'},relationships:[]}],activities,systems:null},design,busy:false,plan:activities.slice(0,3).map(a=>a.id),onPlan:plan=>{props.plan=plan;render();},act:a=>window.commands.push(a),newRun:()=>{},save:()=>{},load:()=>{},sound:()=>{},muted:true,preview:false,error:null,notice:null};
    function render(){root.render(<VisualPlayer {...props}/>)}window.update=p=>{props={...props,...p,view:{...props.view,...p.view}};render();};render();
  `,resolveDir:process.cwd(),loader:'tsx'},bundle:true,write:false,format:'iife',loader:{'.css':'empty'},logLevel:'silent'});
  const browser=await chromium.launch({headless:true,channel:process.env.PLAYWRIGHT_CHANNEL??'msedge'});t.after(()=>browser.close());const page=await browser.newPage({viewport:{width:1200,height:850}});
  await page.setContent('<div id="root"></div>');await page.addStyleTag({content:await readFile('apps/player/renderer/visual.css','utf8')});await page.addScriptTag({content:bundle.outputFiles[0].text});
  await page.getByRole('button',{name:'교육',exact:true}).click();
  assert.equal(await page.locator('[data-element-id="calendar-year"]').textContent(),'1');assert.equal(await page.locator('[data-element-id="calendar-month"]').textContent(),'1월');assert.equal(await page.locator('[data-element-id="calendar-day"]').textContent(),'1');assert.equal(await page.locator('[data-element-id="calendar-weekday"]').textContent(),'월요일');
  const cards=page.locator('[data-activity-id]');await cards.first().waitFor();
  const a=await cards.nth(0).boundingBox(),b=await cards.nth(1).boundingBox();assert.equal(a.y,b.y);assert.ok(b.x>a.x);
  assert.equal(await page.locator('.schedule-activity-title').first().evaluate(e=>e.clientHeight<=60&&e.clientWidth<=e.parentElement.clientWidth),true);
  assert.equal(await page.locator('.activity-preview').first().evaluate(e=>e.scrollWidth<=e.clientWidth),true);
  await cards.nth(10).scrollIntoViewIfNeeded();assert.ok(await page.locator('.schedule-menu').evaluate(e=>e.scrollTop)>0);await cards.nth(10).click();
  assert.ok((await page.locator('[data-calendar-day="1"]').textContent()).includes('긴 교육'));
  await page.getByRole('button',{name:'교육',exact:true}).click();await page.locator('[data-activity-id="course-1"]').click();
  assert.equal(await page.locator('[data-calendar-day="11"] img').count(),1);
  await page.getByRole('button',{name:'교육',exact:true}).click();await page.locator('[data-activity-id="course-2"]').click();
  await page.getByRole('button',{name:'첫 일정 시작',exact:true}).click();assert.deepEqual(await page.evaluate(()=>window.commands),[{type:'commitPlan',activityIds:['course-10','course-1','course-2']}]);
  assert.match(await page.locator('[data-element-id="schedule-prompt"]').textContent(),/첫 일정을 시작/);
  const portrait=await page.locator('[data-element-id="portrait"]').evaluate(e=>({x:e.style.left,y:e.style.top,w:e.style.width,h:e.style.height}));assert.deepEqual(portrait,{x:'300px',y:'500px',w:'80px',h:'120px'});
  const fixed=await page.locator('[data-element-id="fixed"]').evaluate(e=>e.style.width);assert.equal(fixed,'100px');
  await page.evaluate(()=>window.update({view:{generation:2,time:{year:1,month:2,periodInMonth:1},state:{phase:'ready',stateRevision:2,periodIndex:4,schedule:['course-10','course-1','course-2'],attributes:{fee:11},relationships:{},gameplay:{screenId:'home'},presentation:{portraits:[{characterId:'hero',portraitId:'face',position:'center'}]}}}}));
  await page.getByRole('button',{name:'다음 일정 진행',exact:true}).waitFor();assert.equal(await page.locator('[data-calendar-day="1"]').isDisabled(),true);assert.equal(await page.locator('[data-element-id="portrait"]').evaluate(e=>e.style.width),'400px');
  await mkdir('output/playwright',{recursive:true});await page.screenshot({path:'output/playwright/schedule-layout.png'});
  await page.evaluate(()=>window.update({plan:['course-0','course-1','course-2','course-3'],view:{generation:3,periodsPerMonth:4,schedulePlayback:'continuous',time:{year:1,month:4,periodInMonth:0},state:{phase:'planning',stateRevision:0,periodIndex:0,schedule:null,attributes:{fee:7},relationships:{},gameplay:{screenId:'home'},presentation:{portraits:[]}}}}));
  for(let slot=0;slot<4;slot++){
    assert.equal(await page.getByRole('button',{name:'월 일정 시작',exact:true}).count(),0);
    await page.getByRole('button',{name:'교육',exact:true}).click();
    await page.locator(`[data-activity-id="course-${slot}"]`).click();
  }
  for(const [day,slot] of [[1,0],[8,1],[15,2],[22,3],[30,3]])assert.equal(await page.locator(`[data-calendar-day="${day}"]`).getAttribute('data-slot'),String(slot));
  await page.getByRole('button',{name:'월 일정 시작',exact:true}).click();
  assert.deepEqual(await page.evaluate(()=>window.commands.at(-1)),{type:'commitPlan',activityIds:['course-0','course-1','course-2','course-3']});
  assert.match(await page.locator('[data-element-id="schedule-prompt"]').textContent(),/연속/);
  await page.screenshot({path:'output/playwright/schedule-four-weeks.png'});
});

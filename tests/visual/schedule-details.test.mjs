import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,mkdir} from 'node:fs/promises';
import {build} from 'esbuild';
import {chromium} from 'playwright';

test('detailed canvas schedules share complete prose and only explicit progression submits a command',async t=>{
  const bundle=await build({stdin:{contents:`
    import React,{useState} from 'react';import {createRoot} from 'react-dom/client';import {ScheduleWidget,useSchedulePlanner} from './apps/player/renderer/ScheduleWidget.tsx';
    const long='Requirements: Bring money for tuition and leave time for recovery. '.repeat(35)+'FINAL REQUIREMENT CLAUSE.';
    const activities=[{id:'study',label:'Study',description:long,costs:[{attributeId:'money',amount:12}],baseEffects:[{op:'add',target:{kind:'attribute',attributeId:'skill'},value:{op:'literal',value:5}}]},{id:'rest',label:'Rest',description:'Rest restores energy.'}];
    window.commands=[];window.width=880;window.plans=[];
    function App(){const [plan,onPlan]=useState(['study','rest','study','rest']);const [period,setPeriod]=useState(0),[flags,setFlags]=useState(true);window.updatePeriod=setPeriod;window.setFlags=setFlags;
      const context={activities,plan,period,phase:'planning',periodsPerMonth:4,locale:'en',stats:[{id:'money',label:'Money',value:42},{id:'skill',label:'Skill',value:0}]};const planning=useSchedulePlanner(context);window.plans=plan;
      return <><div className="visual-element visual-schedule" style={{width:880,height:480,color:'#273f3b',background:'#f5efdf'}}><ScheduleWidget element={{id:'schedule',kind:'schedule',x:0,y:0,width:880,height:480,showActivityRules:flags,showActivityDescription:flags}} context={context} planning={planning} groups={[]} assetUrl={()=>''} disabled={false} onPlan={(slot,id)=>onPlan(plan.map((p,i)=>i===slot?id:p))}/></div><button style={{position:'absolute',top:500}} onClick={()=>window.commands.push({type:'setSchedule',activityIds:plan})}>Confirm schedule</button></>;
    }createRoot(document.getElementById('root')).render(<App/>);
  `,resolveDir:process.cwd(),loader:'tsx'},bundle:true,write:false,format:'iife',loader:{'.css':'empty'},logLevel:'silent'});
  const browser=await chromium.launch({headless:true,channel:process.env.PLAYWRIGHT_CHANNEL??'chrome'});t.after(()=>browser.close());const page=await browser.newPage({viewport:{width:1000,height:720}});
  await page.setContent('<div id="root"></div>');await page.addStyleTag({content:await readFile('apps/player/renderer/visual.css','utf8')});await page.addScriptTag({content:bundle.outputFiles[0].text});
  const details=page.getByRole('region',{name:'Selected period details'});await details.waitFor();
  assert.equal(await page.locator('.schedule-detail-pane').count(),1);assert.equal(await page.locator('.schedule-detail-period').count(),4);
  assert.match(await details.textContent(),/FINAL REQUIREMENT CLAUSE/);assert.match(await details.textContent(),/Cost · Money 12/);assert.match(await details.textContent(),/Base effects · Skill \+5/);
  const metrics=await details.evaluate(e=>({w:e.clientWidth,h:e.clientHeight,scroll:e.scrollHeight,x:e.scrollWidth,children:[...e.children].map(c=>({h:c.clientHeight,s:c.scrollHeight}))}));
  assert.ok(metrics.w>800&&metrics.h>250);assert.ok(metrics.scroll>metrics.h);assert.ok(metrics.x<=metrics.w+1);assert.ok(metrics.children.every(c=>c.s<=c.h+1),'Prose children do not create hidden nested scrolling');
  await details.focus();await page.keyboard.press('End');await page.waitForTimeout(100);assert.ok(await details.evaluate(e=>e.scrollTop)>0,'Keyboard can reach later requirements');
  const before=await page.evaluate(()=>window.plans);
  await page.locator('.schedule-period-review').nth(1).focus();await page.keyboard.press('Enter');assert.match(await details.textContent(),/Rest restores energy/);assert.deepEqual(await page.evaluate(()=>window.plans),before);assert.deepEqual(await page.evaluate(()=>window.commands),[]);
  await page.getByRole('combobox').nth(1).selectOption('study');assert.match(await details.textContent(),/FINAL REQUIREMENT CLAUSE/);assert.equal(await details.evaluate(e=>e.scrollTop),0);assert.deepEqual(await page.evaluate(()=>window.plans),['study','study','study','rest']);assert.deepEqual(await page.evaluate(()=>window.commands),[]);
  await page.getByRole('combobox').nth(1).focus();await page.keyboard.press('r');await page.keyboard.press('Tab');assert.equal(await page.evaluate(()=>window.plans[1]),'rest','Native keyboard selection changes draft');
  await page.getByRole('button',{name:'Confirm schedule'}).click();assert.deepEqual(await page.evaluate(()=>window.commands),[{type:'setSchedule',activityIds:['study','rest','study','rest']}]);
  await page.evaluate(()=>window.updatePeriod(2));await page.waitForFunction(()=>document.querySelector('select').disabled);assert.equal(await page.getByRole('combobox').nth(0).isDisabled(),true);await page.locator('.schedule-period-review').nth(0).click();assert.match(await details.textContent(),/FINAL REQUIREMENT CLAUSE/,'Completed periods remain inspectable');
  await mkdir('.cache/schedule-details-web',{recursive:true});await page.screenshot({path:'.cache/schedule-details-web/schedule-details.png'});
  await page.evaluate(()=>window.setFlags(false));await page.locator('.schedule-detail-pane').waitFor({state:'detached'});assert.equal(await page.locator('.schedule-detail-pane').count(),0);assert.equal(await page.locator('.schedule-columns').count(),1,'Unopted schedules retain ordinary columns');
});

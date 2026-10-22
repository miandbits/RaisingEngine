import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {build} from 'esbuild';
import {chromium} from 'playwright';

test('browser roster rules follow draft activity and owner, stay opt-in, and do not alter state',async t=>{
  const bundle=await build({stdin:{contents:`import React from 'react';import {createRoot} from 'react-dom/client';import {RosterSchedule} from './apps/player/renderer/RosterSchedule.tsx';
    const root=createRoot(document.getElementById('root'));window.commands=[];window.state={stateRevision:1,roster:{assignments:{a:'work',b:'rest'},attributes:{a:{skill:10},b:{skill:3}}}};
    window.showRoster=(show,cost=4)=>root.render(<RosterSchedule showActivityRules={show} disabled={false}
      context={{phase:'ready',dataState:window.state,characters:[{id:'a',name:'One'},{id:'b',name:'Two'}],stats:[{id:'money',label:'Funds'},{id:'skill',label:'Craft'}],activities:[
        {id:'work',label:'Work',costs:[{attributeId:'money',amount:cost}],baseEffects:[{op:'add',target:{kind:'attribute',attributeId:'skill',owner:{kind:'actor'}},value:{op:'literal',value:4}}]},
        {id:'rest',label:'Rest',costs:[],baseEffects:[]}]}}
      onCommand={command=>window.commands.push(command)} onExecute={()=>window.commands.push('execute')}/>);`,resolveDir:process.cwd(),loader:'tsx'},bundle:true,write:false,format:'iife',logLevel:'silent'});
  const browser=await chromium.launch({headless:true,...(process.env.PLAYWRIGHT_CHANNEL?{channel:process.env.PLAYWRIGHT_CHANNEL}:{})});t.after(()=>browser.close());
  const page=await browser.newPage({viewport:{width:640,height:600}});
  await page.setContent('<div id="root" style="width:580px;height:500px"></div>');
  await page.addStyleTag({content:await readFile('apps/player/renderer/visual.css','utf8')});await page.addScriptTag({content:bundle.outputFiles[0].text});
  await page.evaluate(()=>window.showRoster(false));await page.getByRole('combobox',{name:'One 활동'}).waitFor();
  assert.equal(await page.locator('[id^="roster-rules"]').count(),0);
  const before=await page.evaluate(()=>JSON.stringify(window.state));await page.evaluate(()=>window.showRoster(true));await page.locator('#roster-rules-a').waitFor();
  assert.match(await page.locator('#roster-rules-a').innerText(),/공용 · Funds 4/);assert.match(await page.locator('#roster-rules-a').innerText(),/One · Craft \+4/);
  await page.getByRole('combobox',{name:'Two 활동'}).selectOption('work');assert.match(await page.locator('#roster-rules-b').innerText(),/Two · Craft \+4/);
  await page.evaluate(()=>window.showRoster(true,7));await page.waitForFunction(()=>document.querySelector('#roster-rules-a').textContent.includes('Funds 7'));
  assert.equal(await page.evaluate(()=>JSON.stringify(window.state)),before);assert.deepEqual(await page.evaluate(()=>window.commands),[]);
  await page.getByRole('button',{name:'활동 배정 적용',exact:true}).click();assert.deepEqual(await page.evaluate(()=>window.commands),[{type:'assignActivities',assignments:{a:'work',b:'work'}}]);
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {build} from 'esbuild';
import {chromium} from 'playwright';

test('roster activity descriptions follow each draft and remain opt-in',async t=>{
  const bundle=await build({stdin:{contents:`import React from 'react';import {createRoot} from 'react-dom/client';import {RosterSchedule} from './apps/player/renderer/RosterSchedule.tsx';
    const root=createRoot(document.getElementById('root'));window.commands=[];window.showRoster=(show,description)=>root.render(<RosterSchedule showActivityDescription={show} disabled={false}
      context={{phase:'ready',dataState:{stateRevision:1,roster:{assignments:{a:'work',b:'rest'}}},characters:[{id:'a',name:'One'},{id:'b',name:'Two'}],activities:[{id:'work',label:'Work',description},{id:'rest',label:'Rest',description:'Recover energy.'}]}}
      onCommand={command=>window.commands.push(command)} onExecute={()=>window.commands.push('execute')}/>);`,resolveDir:process.cwd(),loader:'tsx'},bundle:true,write:false,format:'iife',logLevel:'silent'});
  const browser=await chromium.launch({headless:true,...(process.env.PLAYWRIGHT_CHANNEL?{channel:process.env.PLAYWRIGHT_CHANNEL}:{})});t.after(()=>browser.close());
  const page=await browser.newPage({viewport:{width:640,height:600}});
  await page.setContent('<div id="root" style="width:580px"></div>');
  await page.addStyleTag({content:await readFile('apps/player/renderer/visual.css','utf8')});
  await page.addScriptTag({content:bundle.outputFiles[0].text});
  await page.evaluate(()=>window.showRoster(false,'Earn 12 coins.'));
  await page.getByRole('combobox',{name:'One 활동'}).waitFor();
  assert.equal(await page.locator('[id^="roster-description"]').count(),0);
  await page.evaluate(()=>window.showRoster(true,'Earn 12 coins. <b>literal</b>'));
  await page.locator('#roster-description-a').waitFor();
  assert.equal(await page.locator('#roster-description-a').textContent(),'Earn 12 coins. <b>literal</b>');
  assert.equal(await page.locator('#roster-description-a b').count(),0);
  await page.getByRole('combobox',{name:'One 활동'}).selectOption('rest');
  assert.equal(await page.locator('#roster-description-a').textContent(),'Recover energy.');
  assert.equal(await page.locator('#roster-description-b').textContent(),'Recover energy.');
  await page.getByRole('button',{name:'활동 배정 적용',exact:true}).click();
  assert.deepEqual(await page.evaluate(()=>window.commands),[{type:'assignActivities',assignments:{a:'rest',b:'rest'}}]);
  await page.getByRole('combobox',{name:'One 활동'}).selectOption('work');
  await page.evaluate(()=>window.showRoster(true,'Long explanation '.repeat(300)));
  const box=await page.locator('#roster-description-a').boundingBox();assert.ok(box.height<=112);
  assert.equal(await page.locator('#root').evaluate(e=>e.scrollWidth<=e.clientWidth),true);
  await page.getByRole('button',{name:'활동 배정 적용',exact:true}).click();
});

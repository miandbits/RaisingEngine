import test from 'node:test';
import assert from 'node:assert/strict';
import {build} from 'esbuild';
import {chromium} from 'playwright';

test('roster presentation keeps author order across sorted save reloads and targets the selected actor',async t=>{
  const bundle=await build({stdin:{contents:`import React from 'react';import {createRoot} from 'react-dom/client';import {RosterSchedule} from './apps/player/renderer/RosterSchedule.tsx';
    const root=createRoot(document.getElementById('root'));window.commands=[];
    window.showRoster=(order,revision)=>root.render(<RosterSchedule disabled={false}
      context={{phase:'ready',rosterCharacterIds:order,dataState:{stateRevision:revision,roster:{assignments:{friend:'rest',hero:'work'}}},characters:[{id:'friend',name:'Friend'},{id:'hero',name:'Hero'}],activities:[{id:'work',label:'Work'},{id:'rest',label:'Rest'}]}}
      onCommand={command=>window.commands.push(command)} onExecute={()=>{}}/>);`,resolveDir:process.cwd(),loader:'tsx'},bundle:true,write:false,format:'iife',logLevel:'silent'});
  const browser=await chromium.launch({headless:true,...(process.env.PLAYWRIGHT_CHANNEL?{channel:process.env.PLAYWRIGHT_CHANNEL}:{})});t.after(()=>browser.close());
  const page=await browser.newPage();await page.setContent('<div id="root"></div>');await page.addScriptTag({content:bundle.outputFiles[0].text});
  const labels=()=>page.getByRole('combobox').evaluateAll(nodes=>nodes.map(node=>node.getAttribute('aria-label')));
  await page.evaluate(()=>window.showRoster(['hero','friend'],1));await page.getByRole('combobox',{name:'Hero 활동'}).waitFor();
  assert.deepEqual(await labels(),['Hero 활동','Friend 활동']);
  await page.getByRole('combobox',{name:'Hero 활동'}).selectOption('rest');
  await page.getByRole('button',{name:'활동 배정 적용',exact:true}).click();
  assert.deepEqual(await page.evaluate(()=>window.commands),[{type:'assignActivities',assignments:{friend:'rest',hero:'rest'}}]);
  await page.evaluate(()=>window.showRoster(['hero','friend'],2));await page.waitForFunction(()=>document.querySelector('[aria-label="Hero 활동"]').value==='work');
  assert.deepEqual(await labels(),['Hero 활동','Friend 활동']);
  await page.evaluate(()=>window.showRoster(undefined,3));await page.waitForFunction(()=>document.querySelector('select').getAttribute('aria-label')==='Friend 활동');
  assert.deepEqual(await labels(),['Friend 활동','Hero 활동'],'legacy callers remain usable');
  await page.evaluate(()=>window.showRoster(['hero','missing','hero'],4));await page.waitForFunction(()=>document.querySelector('select').getAttribute('aria-label')==='Hero 활동');
  assert.deepEqual(await labels(),['Hero 활동','Friend 활동'],'missing/duplicate IDs never hide real assignments');
});

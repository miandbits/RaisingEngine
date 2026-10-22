import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
import {build} from 'esbuild';
import {chromium} from 'playwright';

test('roster lists wrap and scroll while actions remain reachable at narrow and wide sizes',async t=>{
  const bundle=await build({stdin:{contents:`import React from 'react';import {createRoot} from 'react-dom/client';import {RosterSchedule} from './apps/player/renderer/RosterSchedule.tsx';
    const root=createRoot(document.getElementById('root'));window.commands=[];window.showRoster=()=>root.render(<RosterSchedule showActivityDescription disabled={false}
      context={{phase:'ready',dataState:{stateRevision:1,roster:{assignments:Object.fromEntries(Array.from({length:6},(_,i)=>['a'+i,'work']))}},characters:Array.from({length:6},(_,i)=>({id:'a'+i,name:'Trainee '+i})),activities:[{id:'work',label:'Work',description:'A long description of costs and growth. '.repeat(8)},{id:'rest',label:'Rest',description:'Recover.'}]}}
      onCommand={command=>window.commands.push(command)} onExecute={()=>window.commands.push('execute')}/>);`,resolveDir:process.cwd(),loader:'tsx'},bundle:true,write:false,format:'iife',logLevel:'silent'});
  const browser=await chromium.launch({headless:true,...(process.env.PLAYWRIGHT_CHANNEL?{channel:process.env.PLAYWRIGHT_CHANNEL}:{})});t.after(()=>browser.close());
  const page=await browser.newPage({viewport:{width:1280,height:720}});
  await page.setContent('<div id="root" style="width:1100px;height:270px"></div><button id="outside">Outside</button>');
  await page.addStyleTag({content:await readFile('apps/player/renderer/visual.css','utf8')});
  await page.addScriptTag({content:bundle.outputFiles[0].text});await page.evaluate(()=>window.showRoster());
  await page.locator('.roster-schedule-actors').waitFor();
  for(const width of [1100,360]){
    await page.locator('#root').evaluate((e,w)=>e.style.width=w+'px',width);
    const actors=page.locator('.roster-schedule-actors'),actions=page.locator('.roster-schedule-actions');
    assert.ok(await actors.evaluate(e=>e.scrollHeight>e.clientHeight));
    assert.ok(await page.locator('#root').evaluate(e=>e.scrollWidth<=e.clientWidth));
    const outer=await page.locator('#root').boundingBox(),footer=await actions.boundingBox();
    assert.ok(footer.y+footer.height<=outer.y+outer.height+1);
    await actors.evaluate(e=>e.scrollTop=e.scrollHeight);
    const before=await actors.evaluate(e=>e.scrollTop);assert.ok(before>0);
    await page.getByRole('combobox',{name:'Trainee 5 활동'}).selectOption('rest');
    assert.equal(await actors.evaluate(e=>e.scrollTop),before);
    await actions.getByRole('button',{name:'활동 배정 적용',exact:true}).click();
    const command=await page.evaluate(()=>window.commands.at(-1));assert.equal(command.assignments.a5,'rest');assert.equal(Object.keys(command.assignments).length,6);
    await page.locator('#outside').click();
  }
});

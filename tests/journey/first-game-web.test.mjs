import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {chromium} from 'playwright';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {firstGameRoute} from '../qt-studio/first-game-route.mjs';
import {decodeSave} from '../../dist/packages/runtime-service/save.js';

test('exported Web starter: real WASM controls, both endings, download/upload and fresh save restoration',{timeout:180000},async t=>{
  await mkdir('.cache/first-game-web',{recursive:true});const out=await mkdtemp(path.resolve('.cache/first-game-web/run-'));
  const service=new QtStudioService();t.after(()=>service.releases.close());
  const call=(method,params={})=>service.handle({method,params:{revision:service.editor.revision,...params}});
  await call('create',{root:path.join(out,'project'),template:'my-first-raising-game'});
  const content=(await service.editor.snapshot(service.editor.revision)).snapshot;
  const exported=(await call('export',{target:'web',destination:path.join(out,'web'),releaseVersion:'0.1.0'})).exported;
  const {url}=(await call('releaseLaunch',{token:exported.launchToken})).launch;
  const browser=await chromium.launch({...(process.env.RAISING_BROWSER_CHANNEL?{channel:process.env.RAISING_BROWSER_CHANNEL}:{}),headless:true});t.after(()=>browser.close());
  const results=[];
  for(const accept of [true,false]) {
    const route=firstGameRoute(content,accept),page=await browser.newPage({viewport:{width:1280,height:800},acceptDownloads:true}),errors=[];page.on('pageerror',e=>errors.push(e.message));page.setDefaultTimeout(10000);
    await page.goto(url);await page.locator('.visual-stage').waitFor();
    let savedFile='',saves=0,checkpoints=0;
    const check=async(state,label)=>{await page.waitForFunction(()=>document.querySelector('.visual-player')?.getAttribute('aria-busy')==='false');const actual=await page.evaluate(async()=>(await window.player.view()).view);assert.equal(actual.runtimeBackend,'wasm');assert.deepEqual(actual.state,state,route.id+'/'+label);assert.equal(await page.locator('[role="alert"]').count(),0);++checkpoints;};
    for(const step of route.steps) {
      if(step.select)await page.locator('[data-element-id="schedule"] select').nth(Number(step.select.slice(5))).selectOption(step.value);
      if(step.click) {
        if(step.click.startsWith('choose-'))await page.getByRole('button',{name:route.choiceLabels[step.click.slice(7)],exact:true}).click();
        else await page.locator(`[data-element-id="${step.click}"]`).getByRole('button').click();
      }
      if(step.key==='F5') {
        const pending=page.waitForEvent('download');await page.getByRole('button',{name:/^(Save|저장)$/,exact:true}).click();savedFile=path.join(out,route.id+'-save-'+(++saves)+'.json');await(await pending).saveAs(savedFile);
        const decoded=await decodeSave(content,await readFile(savedFile,'utf8'));assert.equal(decoded.ok,true,JSON.stringify(decoded.error));await check(decoded.session.state,'download');
      }
      if(step.key==='F9') {const pending=page.waitForEvent('filechooser');await page.getByRole('button',{name:/^(Load|Continue a saved game|불러오기)$/,exact:true}).click();await(await pending).setFiles(savedFile);}
      if(step.checkpoint&&step.checkpoint!=='title')await check(route.expected[step.checkpoint],step.checkpoint);
    }
    await page.screenshot({path:path.join(out,route.id+'.png')});await page.reload();await page.locator('.visual-stage').waitFor();
    const pending=page.waitForEvent('filechooser');await page.getByRole('button',{name:/^(Load|Continue a saved game|불러오기)$/,exact:true}).click();await(await pending).setFiles(savedFile);await check(route.expected.ending,'fresh-ending');
    assert.deepEqual(errors,[]);results.push({ending:route.ending,checkpoints,freshRestore:true});await page.close();
  }
  await writeFile(path.join(out,'result.json'),JSON.stringify({ok:true,results},null,2));await writeFile('.cache/first-game-web/latest.json',JSON.stringify({out,results},null,2));
});

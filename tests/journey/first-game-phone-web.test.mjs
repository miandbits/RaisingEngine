import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {chromium} from 'playwright';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {firstGameRoute} from '../qt-studio/first-game-route.mjs';
import {decodeSave} from '../../dist/packages/runtime-service/save.js';

test('public starter completes both phone routes with guidance, touch planning and exact portable recovery',{timeout:180000},async t=>{
 await mkdir('.cache/first-game-phone-web',{recursive:true});const out=await mkdtemp(path.resolve('.cache/first-game-phone-web/run-'));
 const service=new QtStudioService();t.after(()=>service.releases.close());const call=(method,params={})=>service.handle({method,params:{revision:service.editor.revision,...params}});
 await call('create',{root:path.join(out,'project'),template:'my-first-raising-game'});const built=await service.editor.snapshot(service.editor.revision);assert.ok(built.ok,JSON.stringify(built.diagnostics));const content=built.snapshot;
 const reply=await call('export',{target:'web',destination:path.join(out,'web'),releaseVersion:'0.1.0'});assert.ok(reply.exported,JSON.stringify(reply.error));const {url}=(await call('releaseLaunch',{token:reply.exported.launchToken})).launch;
 const browser=await chromium.launch({...(process.env.RAISING_BROWSER_CHANNEL?{channel:process.env.RAISING_BROWSER_CHANNEL}:{}),headless:true});t.after(()=>browser.close());const results=[];
 for(const accept of [true,false]){
  const route=firstGameRoute(content,accept),page=await browser.newPage({viewport:{width:320,height:640},hasTouch:true,acceptDownloads:true}),errors=[];page.on('pageerror',e=>errors.push(e.message));page.setDefaultTimeout(12000);
  const id=name=>page.locator(`[data-reading-id="${name}"]`),action=name=>page.locator(`[data-element-id="${name}"] button`);
  const ready=()=>page.waitForFunction(()=>document.querySelector('.visual-player')?.getAttribute('aria-busy')==='false');
  const tap=async(locator,wait=true)=>{await locator.scrollIntoViewIfNeeded();await locator.tap();if(wait)await ready();};
  const authored=async(name,wait=true)=>{if(!await action(name).isVisible())await tap(id('engine-reading-menu'));await tap(action(name),wait);};
  const state=async()=>page.evaluate(async()=>(await window.player.view()).view.state);
  const geometry=async label=>{const data=await page.locator('.visual-readable').evaluate(root=>{const r=root.getBoundingClientRect(),main=root.querySelector('.readable-scroll'),a=root.querySelector('.readable-actions');return {width:r.width,overflow:root.scrollWidth-root.clientWidth,main:main.getBoundingClientRect().height,footer:a.getBoundingClientRect().height,controls:[...root.querySelectorAll('button')].filter(e=>e.getBoundingClientRect().width>0).map(e=>({label:e.textContent,width:e.getBoundingClientRect().width,height:e.getBoundingClientRect().height}))};});assert.ok(data.overflow<=1,label);for(const c of data.controls){assert.ok(c.width>=43.5&&c.height>=43.5,label+JSON.stringify(c));}assert.ok(data.main/(data.main+data.footer)>=.6,label+' reading space');};
  await page.goto(url);await page.locator('.visual-readable').waitFor();await tap(id('engine-reading-menu'));await tap(id('engine-reading-options'));await page.getByLabel('Text size',{exact:true}).selectOption('2');await page.locator('.player-quality dialog').getByRole('button',{name:'Close',exact:true}).click();await tap(id('engine-reading-menu-close'));await geometry('title');
  let savedFile='',saves=0,checkpoints=0,guideChecked=false;
  for(const step of route.steps){
   if(step.select){await tap(id('board-slot-'+step.select.slice(5)));await tap(id('board-activity-'+step.value));await tap(id('board-apply'));}
   if(step.click){
    if(step.click.startsWith('choose-'))await tap(page.getByRole('button',{name:route.choiceLabels[step.click.slice(7)],exact:true}));else await authored(step.click);
    if(step.click==='start'&&!guideChecked){await page.locator('[data-reading-dialog="engine-planning-guide"]').waitFor();const before=await state();await page.screenshot({path:path.join(out,route.id+'-guide.png')});await tap(id('engine-planning-guide-next'));await tap(id('engine-planning-guide-next'));await tap(id('engine-planning-guide-done'));assert.deepEqual(await state(),before);guideChecked=true;}
   }
   if(step.key==='F5'){const pending=page.waitForEvent('download');await authored('save-controls__save');savedFile=path.join(out,route.id+'-save-'+(++saves)+'.json');await(await pending).saveAs(savedFile);const decoded=await decodeSave(content,await readFile(savedFile,'utf8'));assert.ok(decoded.ok,JSON.stringify(decoded.error));assert.deepEqual(decoded.session.state,await state());}
   if(step.key==='F9'){const pending=page.waitForEvent('filechooser');await authored('save-controls__load',false);await(await pending).setFiles(savedFile);await ready();}
   if(step.checkpoint&&step.checkpoint!=='title'){await ready();assert.deepEqual(await state(),route.expected[step.checkpoint],route.id+'/'+step.checkpoint);await geometry(step.checkpoint);checkpoints++;}
  }
  await page.screenshot({path:path.join(out,route.id+'-ending.png')});await page.reload();await page.locator('.visual-readable').waitFor();const pending=page.waitForEvent('filechooser');await authored('load',false);await(await pending).setFiles(savedFile);await ready();assert.deepEqual(await state(),route.expected.ending);assert.deepEqual(errors,[]);results.push({route:route.id,ending:route.ending,checkpoints,guidancePreservesState:true,freshPortableRecovery:true});await page.close();
 }
 await writeFile(path.join(out,'result.json'),JSON.stringify({ok:true,results,viewport:{width:320,height:640},textScale:2,scope:'Chrome touch emulation; physical phone acceptance remains separate.'},null,2));await writeFile('.cache/first-game-phone-web/latest.json',JSON.stringify({ok:true,out,results},null,2));
});

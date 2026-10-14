// Verifies a project authored and saved by the Qt facilities GUI. No content is injected.
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { cp, mkdir, mkdtemp, readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { chromium } from 'playwright';
import { loadProject } from '../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../dist/packages/project-service/snapshot.js';
import { exportGame } from '../dist/packages/export-service/export.js';

const args=process.argv.slice(2),option=name=>args.includes(name)?args[args.indexOf(name)+1]:undefined;
const source=option('--project')??(args[0]?.startsWith('--')?undefined:args[0])??process.env.FACILITIES_PROJECT_ROOT;
assert.ok(source,'Usage: node scripts/verify-facilities.mjs <GUI-authored project directory>');
await mkdir('.cache/facilities',{recursive:true});
const work=await mkdtemp(path.resolve('.cache/facilities/run-')),root=path.join(work,'project');
await cp(source,root,{recursive:true});
const authored=option('--authored');if(authored)await cp(authored,path.join(root,'project.json'));
const built=await buildSnapshot(await loadProject(root));assert.ok(built.ok,JSON.stringify(built.diagnostics));
const content=built.snapshot,project=content.project,visual=project.visual,gameplay=project.gameplay;
assert.ok(visual&&gameplay);const shop=gameplay.shops.find(s=>s.sellOffers?.length),offer=shop?.offers[0],sale=shop?.sellOffers[0],service=gameplay.services?.[0],diet=gameplay.diets?.[0];
assert.ok(shop&&offer&&sale&&service&&diet,'GUI fixture needs buy/sell/service/diet definitions');
const shopScreens=new Set(),pendingScreens=[gameplay.locations.find(l=>l.id===shop.locationId)?.screenId];while(pendingScreens.length){const id=pendingScreens.pop();if(!id||shopScreens.has(id))continue;shopScreens.add(id);for(const e of visual.screens.find(s=>s.id===id)?.elements??[])if(e.action==='navigate')pendingScreens.push(e.screenId);}
const screenFor=kind=>{const screens=[...visual.screens].reverse();const screen=(kind==='shop'||kind==='sell'?screens.find(s=>s.elements.some(e=>e.kind===kind&&e.shopId===shop.id))??screens.find(s=>shopScreens.has(s.id)&&s.elements.some(e=>e.kind===kind)):null)??screens.find(s=>s.elements.some(e=>e.kind===kind));assert.ok(screen,`Missing ${kind} widget`);return screen.id;};
const buyScreen=screenFor('shop'),sellScreen=screenFor('sell'),serviceScreen=screenFor('services'),dietScreen=screenFor('diets');
const route=(from,to)=>{
  const queue=[{screen:from,buttons:[]}],seen=new Set();
  while(queue.length){const current=queue.shift();if(current.screen===to)return current.buttons;if(seen.has(current.screen))continue;seen.add(current.screen);
    for(const e of visual.screens.find(s=>s.id===current.screen)?.elements??[]){const destination=e.action==='navigate'?e.screenId:e.action==='visit'?gameplay.locations.find(l=>l.id===e.locationId)?.screenId:null;if(destination)queue.push({screen:destination,buttons:[...current.buttons,e.id]});}}
  throw new Error(`No authored button route ${from} → ${to}`);
};
const windows=path.join(work,'windows'),web=path.join(work,'web');
for(const [target,destination] of [['windows',windows],['web',web]])await exportGame({content,projectRoot:root,destination,target,runtimes:{windows:path.resolve('dist/native-player-app'),web:path.resolve('dist/web')}});
const screenshotRoot=path.resolve(source.includes('pm2')?'output/playwright/pm2-facilities':'output/playwright/facilities');await mkdir(screenshotRoot,{recursive:true});
const ids={buy:`buy-${shop.id}-${offer.itemId}`,sell:`sell-${shop.id}-${sale.itemId}`,service:`service-${service.id}`,diet:`diet-${diet.id}`};
const steps=[{waitFrames:5},{click:visual.screens.find(s=>s.id==='title').elements.find(e=>e.action==='start').id},{waitFrames:3}];
let nativeScreen=gameplay.homeScreenId??'home';
const nativeGo=target=>{steps.push(...route(nativeScreen,target).flatMap(click=>[{click},{waitFrames:3}]));nativeScreen=target;};
const checkpoint=name=>steps.push({waitFrames:3},{checkpoint:name});
const cancelNative=(id,tag)=>{checkpoint(`${tag}-before`);steps.push({click:id},{waitFrames:3},{screenshot:path.join(work,`${tag}-confirmation.png`)},{click:'cancel-purchase'});checkpoint(`${tag}-cancelled`);};
nativeGo(buyScreen);steps.push({click:`more-${ids.buy}`});cancelNative(ids.buy,'buy');steps.push({click:ids.buy},{click:'confirm-purchase'});checkpoint('bought');
nativeGo(sellScreen);cancelNative(ids.sell,'sell');steps.push({click:ids.sell},{click:'confirm-purchase'});checkpoint('sold');
nativeGo(serviceScreen);steps.push({click:`more-${ids.service}`});cancelNative(ids.service,'service');steps.push({click:ids.service},{click:'confirm-purchase'});checkpoint('treated');
nativeGo(dietScreen);cancelNative(ids.diet,'diet');steps.push({click:ids.diet},{click:'confirm-purchase'});checkpoint('diet-selected');steps.push({key:'F5'});checkpoint('saved');
nativeGo(serviceScreen);steps.push({click:`less-${ids.service}`},{click:ids.service},{click:'confirm-purchase'});checkpoint('changed');steps.push({key:'F9'});checkpoint('loaded');
await writeFile(path.join(work,'native-steps.json'),JSON.stringify(steps));
const native=spawnSync(path.join(windows,'RaisingPlayer.exe'),['--bundle',windows,'--save',path.join(work,'native-save.json'),'--hidden','--automation',path.join(work,'native-steps.json'),'--report',path.join(work,'native-report.json')],{windowsHide:true,encoding:'utf8',timeout:90000});
assert.equal(native.status,0,await readFile(path.join(work,'native-report.json'),'utf8').catch(()=>native.error?.message||native.stderr.slice(-1500)));
const report=JSON.parse(await readFile(path.join(work,'native-report.json'),'utf8')),nativeState=tag=>report.steps.find(s=>s.command.checkpoint===tag).ui.state;
for(const tag of ['buy','sell','service','diet'])assert.deepEqual(nativeState(`${tag}-before`),nativeState(`${tag}-cancelled`),`${tag} cancellation changed state`);
assert.equal(nativeState('buy-before').attributes.money-nativeState('bought').attributes.money,offer.price*2);
assert.equal(nativeState('sold').attributes.money-nativeState('sell-before').attributes.money,sale.price);
assert.equal(nativeState('service-before').attributes.money-nativeState('treated').attributes.money,service.costs.find(c=>c.attributeId==='money').amount*2);
assert.equal(nativeState('diet-selected').gameplay.dietId,diet.id);assert.deepEqual(nativeState('saved'),nativeState('loaded'));assert.notDeepEqual(nativeState('changed'),nativeState('saved'));

const mime={'.html':'text/html','.js':'text/javascript','.mjs':'application/javascript','.wasm':'application/wasm','.json':'application/json','.css':'text/css','.png':'image/png','.svg':'image/svg+xml'};
const server=createServer(async(req,res)=>{try{const pathname=decodeURIComponent(new URL(req.url,'http://127.0.0.1').pathname),file=path.resolve(web,`.${pathname==='/'?'/index.html':pathname}`);if(!file.startsWith(web+path.sep)){res.writeHead(403);res.end();return;}const bytes=await readFile(file);res.writeHead(200,{'Content-Type':mime[path.extname(file)]??'application/octet-stream','Cache-Control':'no-store'});res.end(bytes);}catch{res.writeHead(404);res.end();}});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));let browser;
try {
  browser=await chromium.launch({channel:'chrome',headless:true});const page=await browser.newPage({viewport:{width:1440,height:1080},acceptDownloads:true});page.setDefaultTimeout(10000);const errors=[];page.on('pageerror',e=>errors.push(e.message));
  const state=()=>page.evaluate(async()=> (await window.player.view()).view.state);
  const clickElement=id=>page.locator(`[data-element-id="${id}"] button`).click();
  await page.goto(`http://127.0.0.1:${server.address().port}`);await page.locator('.visual-stage').waitFor();
  await clickElement(visual.screens.find(s=>s.id==='title').elements.find(e=>e.action==='start').id);await page.locator('.visual-stage[data-screen=home]').waitFor();
  let browserScreen='home';const go=async target=>{for(const id of route(browserScreen,target)){await clickElement(id);await page.waitForFunction(()=>!document.querySelector('.visual-status')?.textContent?.includes('처리 중'));}browserScreen=target;await page.locator(`.visual-stage[data-screen="${target}"]`).waitFor();};
  const modal=()=>page.getByRole('dialog');
  const confirm=async definition=>{const revision=(await state()).stateRevision;const button=modal().getByRole('button',{name:definition.confirmation?.confirmLabel??'확인',exact:true});await button.evaluate(el=>{el.click();el.click();});await modal().waitFor({state:'hidden'});await page.waitForFunction(async before=>(await window.player.view()).view.state.stateRevision!==before||!!document.querySelector('[role=alert]'),revision);assert.equal(await page.getByRole('alert').count(),0,await page.locator('body').innerText());};
  const cancelWeb=async(open,definition,tag)=>{const before=await state();await open();await modal().waitFor();assert.ok((await modal().innerText()).includes(definition.confirmation?.title??'거래 확인'));await page.screenshot({path:path.join(screenshotRoot,`${tag}-confirmation.png`)});await modal().getByRole('button',{name:definition.confirmation?.cancelLabel??'취소',exact:true}).click();assert.deepEqual(await state(),before);};
  await go(buyScreen);await page.getByRole('button',{name:`${shop.id}-${offer.itemId} 수량 늘리기`,exact:true}).click();
  const buy=()=>page.getByRole('button',{name:`${gameplay.items.find(i=>i.id===offer.itemId).name} 구매`,exact:true}).click();await cancelWeb(buy,offer,'buy');await buy();await confirm(offer);const bought=await state();assert.equal(bought.attributes.money,nativeState('bought').attributes.money);
  await go(sellScreen);const sell=()=>page.getByTestId('sell-widget').getByRole('button',{name:'판매',exact:true}).first().click();await cancelWeb(sell,sale,'sell');await sell();await confirm(sale);assert.equal((await state()).attributes.money,nativeState('sold').attributes.money);
  await go(serviceScreen);await page.getByRole('button',{name:`${service.id} 수량 늘리기`,exact:true}).click();const treat=()=>page.getByTestId('services-widget').getByRole('button',{name:'이용',exact:true}).first().click();await cancelWeb(treat,service,'service');await treat();await confirm(service);assert.equal((await state()).attributes.money,nativeState('treated').attributes.money);
  await go(dietScreen);const selectDiet=()=>page.getByTestId('diets-widget').getByRole('button',{name:'식단 선택',exact:true}).first().click();await cancelWeb(selectDiet,diet,'diet');await selectDiet();await confirm(diet);assert.equal((await state()).gameplay.dietId,diet.id);
  const controlScreen=action=>[...visual.screens].sort((a,b)=>Number(b.id==='home')-Number(a.id==='home')).find(s=>{if(!s.elements.some(e=>e.action===action))return false;try{route('home',s.id);route(s.id,'home');return true;}catch{return false;}});
  const saveScreen=controlScreen('save'),loadScreen=controlScreen('load');assert.ok(saveScreen&&loadScreen,'Fixture needs reachable authored save/load controls');
  console.log('Web buy/sell/treatment/diet, cancel and duplicate confirm passed.');
  await go(saveScreen.id);const savedState=await state(),download=page.waitForEvent('download');await clickElement(saveScreen.elements.find(e=>e.action==='save').id);const saveFile=path.join(work,'web-save.json');await(await download).saveAs(saveFile);assert.deepEqual(JSON.parse(await readFile(saveFile,'utf8')).state,savedState);console.log('Web save downloaded.');
  await go(serviceScreen);const less=page.getByRole('button',{name:`${service.id} 수량 줄이기`,exact:true});if(await less.isEnabled())await less.click();await treat();await confirm(service);assert.notDeepEqual(await state(),savedState);
  await go(loadScreen.id);const chooser=page.waitForEvent('filechooser');await clickElement(loadScreen.elements.find(e=>e.action==='load').id);await(await chooser).setFiles(saveFile);await page.waitForFunction(()=>!document.querySelector('.visual-status')?.textContent?.includes('처리 중'));assert.deepEqual(await state(),savedState);
  assert.deepEqual(errors,[]);await writeFile(path.join(work,'web-result.json'),JSON.stringify({ok:true,runtime:'wasm',cancelPreservesState:true,duplicateConfirmOnce:true,saveLoad:true,savedState},null,2));
} finally {await browser?.close();server.closeAllConnections();await new Promise(resolve=>server.close(resolve));}
await writeFile('.cache/facilities/latest.json',JSON.stringify({ok:true,work,root,windows,web,screenshotRoot},null,2));console.log(JSON.stringify({ok:true,work,root,windows,web,cancelPreservesState:true,duplicateConfirmOnce:true,saveLoad:true}));

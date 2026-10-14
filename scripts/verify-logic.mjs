// Exercise the unchanged Qt-authored project through exported native and web players.
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {cp,mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {spawnSync} from 'node:child_process';
import {chromium} from 'playwright';
import {loadProject} from '../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../dist/packages/project-service/snapshot.js';
import {exportGame} from '../dist/packages/export-service/export.js';

const source=path.resolve(process.argv[2]??'projects/logic-lab');
await mkdir('.cache/logic',{recursive:true});
const work=await mkdtemp(path.resolve('.cache/logic/run-')),root=path.join(work,'project');
await cp(source,root,{recursive:true});
const loaded=await buildSnapshot(await loadProject(root));assert.ok(loaded.ok,JSON.stringify(loaded.diagnostics));
const content=loaded.snapshot,{gameplay,visual}=content.project;
assert.equal(gameplay.actions.length,10);assert.equal(gameplay.rules[0].trigger.type,'purchaseComplete');
const shop=gameplay.shops.find(s=>s.id==='florist'),flower=shop.offers.find(o=>o.itemId==='flower'),sword=shop.offers.find(o=>o.itemId==='training-sword');
const location=gameplay.locations.find(l=>l.id===shop.locationId),itemName=id=>gameplay.items.find(i=>i.id===id).name;
assert.ok(gameplay.locations.find(l=>l.id===gameplay.startLocationId||l.id==='home').connections.includes(location.id));
const windows=path.join(work,'windows'),web=path.join(work,'web');
for(const [target,destination] of [['windows',windows],['web',web]])await exportGame({content,projectRoot:root,destination,target,runtimes:{windows:path.resolve('dist/native-player-app'),web:path.resolve('dist/web')}});
const screenshotRoot=path.resolve('output/playwright/logic');await mkdir(screenshotRoot,{recursive:true});
const manifest={ok:false,source,work,root,windows,web,screenshotRoot};await writeFile('.cache/logic/latest.json',JSON.stringify(manifest,null,2));
const steps=[],click=id=>steps.push({click:id},{waitFrames:3}),checkpoint=name=>steps.push({checkpoint:name});
click(visual.screens.find(s=>s.id==='title').elements.find(e=>e.action==='start').id);checkpoint('started');
click(`visit-${location.id}`);checkpoint('market');
const nativeBuy=offer=>{click(`buy-${shop.id}-${offer.itemId}`);if(visual.confirmPurchases||offer.confirmation)click('confirm-purchase');};
nativeBuy(flower);checkpoint('purchased');steps.push({screenshot:path.join(work,'native-purchased.png')},{key:'F5'},{waitFrames:3});checkpoint('saved');
nativeBuy(sword);checkpoint('changed');steps.push({key:'F9'},{waitFrames:3});checkpoint('loaded');steps.push({screenshot:path.join(work,'native-loaded.png')});
await writeFile(path.join(work,'native-steps.json'),JSON.stringify(steps,null,2));
const native=spawnSync(path.join(windows,'RaisingPlayer.exe'),['--bundle',windows,'--save',path.join(work,'native-save.json'),'--hidden','--automation',path.join(work,'native-steps.json'),'--report',path.join(work,'native-report.json')],{windowsHide:true,encoding:'utf8',timeout:90000});
assert.equal(native.status,0,await readFile(path.join(work,'native-report.json'),'utf8').catch(()=>native.error?.message??native.stderr));
const nativeReport=JSON.parse(await readFile(path.join(work,'native-report.json'),'utf8')),nativeState=tag=>nativeReport.steps.find(s=>s.command.checkpoint===tag).ui.state;
const checkPurchase=(before,after,offer)=>{assert.equal(after.attributes.money,before.attributes.money-offer.price-5+3);assert.equal(after.stateRevision,before.stateRevision+1,'hook and branch belong to the one purchase command');assert.equal(after.gameplay.inventory[offer.itemId],(before.gameplay.inventory[offer.itemId]??0)+1);};
assert.equal(nativeState('started').attributes.money,60);checkPurchase(nativeState('market'),nativeState('purchased'),flower);assert.equal(nativeState('purchased').attributes.money,18);checkPurchase(nativeState('saved'),nativeState('changed'),sword);assert.deepEqual(nativeState('saved'),nativeState('loaded'));

const mime={'.html':'text/html','.js':'text/javascript','.mjs':'application/javascript','.wasm':'application/wasm','.json':'application/json','.css':'text/css','.png':'image/png','.svg':'image/svg+xml'};
const server=createServer(async(req,res)=>{try{const pathname=decodeURIComponent(new URL(req.url,'http://127.0.0.1').pathname),file=path.resolve(web,`.${pathname==='/'?'/index.html':pathname}`);if(!file.startsWith(web+path.sep)){res.writeHead(403);res.end();return;}const bytes=await readFile(file);res.writeHead(200,{'Content-Type':mime[path.extname(file)]??'application/octet-stream','Cache-Control':'no-store'});res.end(bytes);}catch{res.writeHead(404);res.end();}});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));let browser;
try{
  browser=await chromium.launch({channel:'chrome',headless:true});const page=await browser.newPage({viewport:{width:1440,height:1000},acceptDownloads:true});page.setDefaultTimeout(12000);const errors=[];page.on('pageerror',e=>errors.push(e.message));
  const state=()=>page.evaluate(async()=>(await window.player.view()).view.state);
  const idle=()=>page.waitForFunction(()=>!document.querySelector('.visual-status')?.textContent?.includes('처리 중'));
  const button=async id=>{await page.locator(`[data-element-id="${id}"] button`).click();await idle();};
  const buy=async offer=>{const before=await state();await page.getByRole('button',{name:`${itemName(offer.itemId)} 구매`,exact:true}).click();if(visual.confirmPurchases||offer.confirmation)await page.getByRole('dialog').getByRole('button',{name:offer.confirmation?.confirmLabel??'확인',exact:true}).click();await page.waitForFunction(async revision=>(await window.player.view()).view.state.stateRevision>revision,before.stateRevision);const after=await state();checkPurchase(before,after,offer);assert.equal(await page.getByRole('alert').count(),0);return after;};
  await page.goto(`http://127.0.0.1:${server.address().port}`);await page.locator('.visual-stage').waitFor();await button('start');await page.getByRole('button',{name:`${location.name} 방문`,exact:true}).click();await page.locator(`.visual-stage[data-screen="${location.screenId}"]`).waitFor();
  await page.screenshot({path:path.join(screenshotRoot,'before-purchase.png')});assert.equal((await state()).attributes.money,60);
  const purchased=await buy(flower);assert.equal(purchased.attributes.money,18);await page.screenshot({path:path.join(screenshotRoot,'purchase-chain.png')});
  const download=page.waitForEvent('download');await button('save');const saveFile=path.join(work,'web-save.json');await(await download).saveAs(saveFile);assert.deepEqual(JSON.parse(await readFile(saveFile,'utf8')).state,purchased);
  const changed=await buy(sword);assert.equal(changed.attributes.money,6);
  const chooser=page.waitForEvent('filechooser');await page.locator('[data-element-id="load"] button').click();await(await chooser).setFiles(saveFile);await page.waitForFunction(async expected=>JSON.stringify((await window.player.view()).view.state)===JSON.stringify(expected),purchased);assert.deepEqual(await state(),purchased);
  await page.screenshot({path:path.join(screenshotRoot,'loaded.png')});assert.deepEqual(errors,[]);
  await writeFile(path.join(work,'web-result.json'),JSON.stringify({ok:true,runtime:'cpp-wasm',guiSource:source,money:[60,purchased.attributes.money,changed.attributes.money,(await state()).attributes.money],oneRevisionPerPurchase:true,saveLoadExact:true,savedState:purchased},null,2));
}finally{await browser?.close();server.closeAllConnections();await new Promise(resolve=>server.close(resolve));}
assert.equal(await readFile(path.join(source,'project.json'),'utf8'),await readFile(path.join(root,'project.json'),'utf8'),'verification must not alter authored project');
await writeFile('.cache/logic/latest.json',JSON.stringify({...manifest,ok:true,nativePassed:true,webPassed:true},null,2));
console.log(JSON.stringify({...manifest,ok:true,nativePassed:true,webPassed:true}));

import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {mkdir,readFile,writeFile} from 'node:fs/promises';
import {resolve,extname,sep} from 'node:path';
import {chromium} from 'playwright';
import {makeGame,EVIDENCE} from './player-extension-fixture.mjs';

const INSTANCE='home/hero-anim';
const mime={'.html':'text/html','.js':'text/javascript','.mjs':'text/javascript','.css':'text/css','.json':'application/json','.wasm':'application/wasm','.png':'image/png','.jpg':'image/jpeg','.webp':'image/webp','.ttf':'font/ttf','.otf':'font/otf','.svg':'image/svg+xml'};
async function serve(t,root){
  const server=createServer(async(req,res)=>{
    try{
      const pathname=decodeURIComponent(new URL(req.url,'http://127.0.0.1').pathname),file=resolve(root,`.${pathname==='/'?'/index.html':pathname}`);
      if(!file.startsWith(`${root}${sep}`)){res.writeHead(403);res.end();return;}
      const bytes=await readFile(file);res.writeHead(200,{'Content-Type':mime[extname(file)]??'application/octet-stream','Cache-Control':'no-store'});res.end(bytes);
    }catch{res.writeHead(404);res.end();}
  });
  await new Promise(done=>server.listen(0,'127.0.0.1',done));t.after(()=>{server.closeAllConnections();server.close();});
  return `http://127.0.0.1:${server.address().port}/`;
}
async function open(t,f){
  const url=await serve(t,f.game);
  const browser=await chromium.launch({headless:true,channel:process.env.PLAYWRIGHT_CHANNEL??'chrome'});t.after(()=>browser.close());
  const page=await browser.newPage({viewport:{width:1280,height:760},acceptDownloads:true});
  const errors=[];page.on('pageerror',e=>errors.push(e.message));
  // Manual presentation clock (like the native automation clock) keeps assertions deterministic.
  await page.addInitScript(()=>{window.raisingExtensionManualClock=true;});
  await page.goto(url);await page.locator('[data-element-id="start"] button').click();
  await page.locator('[data-element-id="cheer"] button').waitFor();
  return {page,errors};
}
const describe=page=>page.evaluate(id=>{const d=window.raisingExtensions?.describe();return d&&{...d,instance:d.instances.find(i=>i.instance===id)};},INSTANCE);
const tick=(page,ms)=>page.evaluate(ms=>window.raisingExtensions.tick(ms),ms);
const track=(d,n)=>d.instance.state.tracks.find(t=>t.track===n);
const money=page=>page.locator('[data-testid="stat-money"]').first().textContent();
async function cheer(page){await page.locator('[data-element-id="cheer"] button').click();await page.locator('.transaction-dialog button').first().click();}

test('web player renders plugin items with matrix transforms, observes commands and restores clip state',{timeout:120000},async t=>{
  const f=await makeGame({target:'web',name:'web'});t.diagnostic(`fixture: ${f.root}`);
  const {page,errors}=await open(t,f);
  await page.locator('.visual-plugin-item').nth(2).waitFor();
  await tick(page,500);
  await page.waitForFunction(()=>document.querySelector('.visual-plugin-item')?.style.transform.replace(/\s/g,'')==='matrix(1.5,0,0,1.5,90,84)');
  const items=await page.locator('.visual-plugin-item').evaluateAll(list=>list.map(img=>({id:img.dataset.imageAssetId,transform:img.style.transform.replace(/\s/g,''),origin:getComputedStyle(img).transformOrigin,loaded:img.complete&&img.naturalWidth>0})));
  assert.deepEqual(items.map(i=>i.id),['anim-torso-summer','anim-face','anim-eyes-open'],'props.skin picks the outfit');
  assert.ok(items.every(i=>i.transform.startsWith('matrix(1.5,0,0,1.5,')&&i.origin==='0px 0px'));
  await page.waitForFunction(()=>[...document.querySelectorAll('.visual-plugin-item')].every(i=>i.complete&&i.naturalWidth>0));
  // Element-local placement: torso (80x120 at scale 1.5) starts 90px right of the element's left edge.
  const box=await page.evaluate(()=>{const el=document.querySelector('[data-element-id="hero-anim"]').getBoundingClientRect(),img=document.querySelector('.visual-plugin-item').getBoundingClientRect(),stage=document.querySelector('.visual-stage'),scale=stage.getBoundingClientRect().width/stage.offsetWidth;return {dx:(img.x-el.x)/scale,dy:(img.y-el.y)/scale,w:img.width/scale,h:img.height/scale};});
  for(const [a,b] of [[box.dx,90],[box.dy,84],[box.w,120],[box.h,180]])assert.ok(Math.abs(a-b)<0.5,JSON.stringify(box));
  let d=await describe(page);assert.equal(track(d,0).clip,'idle');assert.equal(track(d,0).timeMs,500);assert.equal(d.observations,0);
  const before=Number(await money(page));
  await cheer(page);
  await page.waitForFunction(()=>window.raisingExtensions.observations===1);
  d=await describe(page);assert.equal(track(d,0).clip,'joy');assert.equal(d.instance.state.skin,'winter');
  assert.deepEqual(d.lastObserve.plugins['cutout-animation'].matched,[`${INSTANCE}:cheer-joy`]);
  // The binding's skin swap shows the winter torso, decoded like every other part.
  await page.waitForFunction(()=>{const i=document.querySelector('.visual-plugin-item');return i?.dataset.imageAssetId==='anim-torso-winter'&&i.complete&&i.naturalWidth>0;});
  assert.equal(Number(await money(page)),before+1);
  await tick(page,300);d=await describe(page);assert.equal(track(d,0).timeMs,300);
  await mkdir(EVIDENCE,{recursive:true});await page.screenshot({path:resolve(EVIDENCE,'web-joy-300.png')});
  const download=page.waitForEvent('download');await page.locator('[data-element-id="save"] button').click();
  const savePath=resolve(f.root,'web-save.json');await (await download).saveAs(savePath);
  const saved=JSON.parse(await readFile(savePath,'utf8'));
  assert.equal(saved.playerPresentation.extensions['cutout-animation'].instances[INSTANCE].ref,'hero-main');
  assert.equal(saved.playerPresentation.timeline,null);
  await tick(page,700);d=await describe(page);assert.equal(track(d,0).clip,'idle','one-shot returns to the default clip');
  const chooser=page.waitForEvent('filechooser');await page.locator('[data-element-id="load"] button').click();await (await chooser).setFiles(savePath);
  await page.waitForFunction(id=>{const i=window.raisingExtensions.describe().instances.find(x=>x.instance===id);return i?.state?.tracks?.[0]?.clip==='joy';},INSTANCE);
  d=await describe(page);assert.equal(track(d,0).timeMs,300,'one-shot resumes at its saved time');
  assert.equal(saved.playerPresentation.extensions['cutout-animation'].instances[INSTANCE].state.skin,'winter');
  assert.equal(d.instance.state.skin,'winter','skin restored');
  assert.equal(await page.locator('.visual-plugin-item').first().getAttribute('data-image-asset-id'),'anim-torso-winter');
  assert.equal(Number(await money(page)),before+1);
  await page.screenshot({path:resolve(EVIDENCE,'web-loaded.png')});
  assert.deepEqual(errors,[]);
});

test('web player shows a placeholder when the plugin module is missing and stays playable',{timeout:120000},async t=>{
  const f=await makeGame({target:'web',name:'web-missing',runtime:false});t.diagnostic(`fixture: ${f.root}`);
  const {page,errors}=await open(t,f);
  await page.locator('.visual-plugin-placeholder').waitFor();
  assert.match(await page.locator('.visual-plugin-placeholder').textContent(),/플러그인 필요: cutout-animation/);
  assert.equal(await page.locator('.visual-plugin-item').count(),0);
  const before=Number(await money(page));
  await cheer(page);
  await page.waitForFunction(b=>Number(document.querySelector('[data-testid="stat-money"]').textContent)===b+1,before);
  await mkdir(EVIDENCE,{recursive:true});await page.screenshot({path:resolve(EVIDENCE,'web-placeholder.png')});
  assert.deepEqual(errors,[]);
  await writeFile(resolve(f.root,'ok'),'');
});

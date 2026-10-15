// Checks the exports produced by the Qt end-to-end scenario (StudioTests::cutoutEndToEnd):
//  - Windows: runs the exported RaisingPlayer.exe with automation (+ screenshots).
//  - Web: serves the exported folder and drives it in a real browser (Playwright).
// Then copies the authored project to projects/cutout-animation-demo.
//   node tests/plugins/cutout-e2e-exports.mjs [.cache/anim-e2e/latest.json]
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import {createServer} from 'node:http';
import {cp,mkdir,readFile,rm,writeFile} from 'node:fs/promises';
import {existsSync} from 'node:fs';
import {resolve,extname,sep} from 'node:path';
import {chromium} from 'playwright';

const manifest=JSON.parse(await readFile(resolve(process.argv[2]??'.cache/anim-e2e/latest.json'),'utf8'));
const {buttonId,instance,evidence}=manifest;
const imported=new Set(manifest.importedAssetIds);
// Exports pack the parts into atlas pages (plugin 1.9.0): items then name a page and its region.
const drawn=(id,region=true)=>imported.has(id)||(/^cutout-atlas-[0-9]+$/.test(id)&&region);
const results=[];
const check=(name,fn)=>{try{fn();results.push(`PASS ${name}`);}catch(e){results.push(`FAIL ${name}: ${e.message}`);throw e;}};
const track=(i,n)=>i?.state?.tracks?.find(t=>t.track===n);

// ---- Windows export ---------------------------------------------------------------------
{
  const work=resolve('.cache/anim-e2e/export-check');await mkdir(work,{recursive:true});
  const steps=[{click:'start'},{waitFrames:3},{expectScreen:'home'},{extensionAdvanceMs:500,extensionSteps:5},{checkpoint:'idle'},
    {screenshot:resolve(evidence,'export-windows-idle.png')},{click:buttonId},{waitFrames:3},{checkpoint:'joy'},
    {extensionAdvanceMs:400,extensionSteps:4},{checkpoint:'joy-400'},{screenshot:resolve(evidence,'export-windows-joy-400.png')},
    {extensionAdvanceMs:2000,extensionSteps:20},{checkpoint:'back-idle'},{screenshot:resolve(evidence,'export-windows-back-idle.png')}];
  const input=resolve(work,'steps.json'),report=resolve(work,'report.json');
  await writeFile(input,JSON.stringify(steps));await rm(report,{force:true});
  const child=spawnSync(resolve(manifest.windows,'RaisingPlayer.exe'),['--bundle',manifest.windows,'--save',resolve(work,'save.json'),'--seed','42','--hidden','--automation',input,'--report',report],{encoding:'utf8',timeout:120000,windowsHide:true});
  check('windows export player exits 0',()=>assert.equal(child.status,0,child.error?.message??child.stderr));
  const r=JSON.parse(await readFile(report,'utf8'));
  check('windows report ok',()=>assert.equal(r.ok,true,r.error));
  const ui=tag=>r.steps.find(s=>s.command.checkpoint===tag).ui;
  const inst=tag=>ui(tag).extensions.instances.find(i=>i.instance===instance);
  check('windows instance mounted with imported assets',()=>{const i=inst('idle');assert.equal(i.available,true,i.error);assert.ok(i.items.length>=8);for(const item of i.items)assert.ok(drawn(item.imageAssetId,!!item.region),item.imageAssetId);});
  check('windows idle + blink layer',()=>{assert.equal(track(inst('idle'),0).clip,'idle');assert.equal(track(inst('idle'),1).clip,'blink');});
  check('windows action -> joy',()=>{assert.equal(track(inst('joy'),0).clip,'joy');assert.deepEqual(ui('joy').extensions.lastObserve.plugins['cutout-animation'].matched,[`${instance}:cheer-joy`]);});
  check('windows joy at 400ms',()=>{assert.equal(track(inst('joy-400'),0).clip,'joy');assert.equal(track(inst('joy-400'),0).timeMs,400);});
  check('windows back to idle',()=>assert.equal(track(inst('back-idle'),0).clip,'idle'));
  check('windows money +5 exactly once',()=>{const m=t=>ui(t).state.attributes.money;assert.equal(m('joy'),m('idle')+5);assert.equal(m('back-idle'),m('joy'));});
}

// ---- Web export (real browser) ----------------------------------------------------------
{
  const mime={'.html':'text/html','.js':'text/javascript','.mjs':'text/javascript','.css':'text/css','.json':'application/json','.wasm':'application/wasm','.png':'image/png','.jpg':'image/jpeg','.webp':'image/webp','.ttf':'font/ttf','.otf':'font/otf','.svg':'image/svg+xml'};
  const root=resolve(manifest.web);
  const server=createServer(async(req,res)=>{
    try{const pathname=decodeURIComponent(new URL(req.url,'http://127.0.0.1').pathname),file=resolve(root,`.${pathname==='/'?'/index.html':pathname}`);
      if(!file.startsWith(`${root}${sep}`)){res.writeHead(403);res.end();return;}
      const bytes=await readFile(file);res.writeHead(200,{'Content-Type':mime[extname(file)]??'application/octet-stream','Cache-Control':'no-store'});res.end(bytes);}
    catch{res.writeHead(404);res.end();}
  });
  await new Promise(done=>server.listen(0,'127.0.0.1',done));
  const browser=await chromium.launch({headless:true,channel:process.env.PLAYWRIGHT_CHANNEL??'chrome'});
  try{
    const page=await browser.newPage({viewport:{width:1280,height:760}});
    const errors=[];page.on('pageerror',e=>errors.push(e.message));
    await page.addInitScript(()=>{window.raisingExtensionManualClock=true;});
    await page.goto(`http://127.0.0.1:${server.address().port}/`);
    await page.locator('[data-element-id="start"] button').click();
    const action=page.locator(`[data-element-id="${buttonId}"] button`);await action.waitFor();
    await page.waitForFunction(n=>document.querySelectorAll('.visual-plugin-item').length>=n,8);
    await page.waitForFunction(()=>[...document.querySelectorAll('.visual-plugin-item')].every(i=>i.complete&&i.naturalWidth>0));
    const describe=()=>page.evaluate(id=>{const d=window.raisingExtensions?.describe();return d&&{...d,instance:d.instances.find(i=>i.instance===id)};},instance);
    const tick=ms=>page.evaluate(ms=>window.raisingExtensions.tick(ms),ms);
    const ids=await page.locator('.visual-plugin-item').evaluateAll(list=>list.map(img=>img.dataset.imageAssetId));
    check('web renders the character from imported assets',()=>{assert.ok(ids.length>=8);for(const id of ids)assert.ok(drawn(id),id);});
    const box=await page.locator(`[data-element-id="${manifest.characterElement}"]`).boundingBox();
    check('web character element visible',()=>assert.ok(box&&box.width>100&&box.height>100,JSON.stringify(box)));
    await tick(500);
    let d=await describe();
    check('web idle + blink',()=>{assert.equal(track(d.instance,0).clip,'idle');assert.equal(track(d.instance,1).clip,'blink');assert.equal(d.observations,0);});
    await page.screenshot({path:resolve(evidence,'web-idle.png')});
    const money=async()=>{const l=page.locator('[data-testid="stat-money"]');return await l.count()?Number(await l.first().textContent()):null;};
    const before=await money();
    await action.click();
    await page.waitForFunction(()=>window.raisingExtensions.observations===1);
    d=await describe();
    check('web click -> joy',()=>{assert.equal(track(d.instance,0).clip,'joy');assert.deepEqual(d.lastObserve.plugins['cutout-animation'].matched,[`${instance}:cheer-joy`]);});
    await tick(400);
    const transforms=await page.locator('.visual-plugin-item').evaluateAll(list=>list.map(i=>i.style.transform));
    await page.screenshot({path:resolve(evidence,'web-joy-400.png')});
    d=await describe();
    check('web joy at 400ms',()=>assert.equal(track(d.instance,0).timeMs,400));
    await tick(2000);d=await describe();
    check('web back to idle',()=>assert.equal(track(d.instance,0).clip,'idle'));
    const rest=await page.locator('.visual-plugin-item').evaluateAll(list=>list.map(i=>i.style.transform));
    check('web joy pose differs from idle pose',()=>assert.notDeepEqual(transforms,rest));
    const after=await money();
    if(before!==null)check('web money +5',()=>assert.equal(after,before+5));
    await page.screenshot({path:resolve(evidence,'web-back-idle.png')});
    check('web no page errors',()=>assert.deepEqual(errors,[]));
  }finally{await browser.close();server.closeAllConnections();server.close();}
}

// ---- reusable demo project ----------------------------------------------------------------
{
  const target=resolve('projects/cutout-animation-demo');
  if(existsSync(target)){
    const marker=resolve(target,'.cutout-e2e-source');
    assert.ok(existsSync(marker),`${target} exists and was not written by this script; not overwriting`);
    await rm(target,{recursive:true,force:true});
  }
  await cp(manifest.projectRoot,target,{recursive:true,filter:src=>!/[\\/](\.raising|\.cache)([\\/]|$)/.test(src.slice(manifest.projectRoot.length))});
  await writeFile(resolve(target,'.cutout-e2e-source'),`${manifest.projectRoot}\n`);
  results.push(`COPIED ${manifest.projectRoot} -> ${target}`);
}
console.log(results.join('\n'));
await writeFile(resolve(evidence,'exports-check.txt'),results.join('\n')+'\n');

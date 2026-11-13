import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile,mkdir} from 'node:fs/promises';
import {resolve,extname,sep} from 'node:path';
import {chromium} from 'playwright';

const bundle=process.env.RAISING_VARIANTS_WEB_BUNDLE;
test('Qt-authored variants change in the real web player and survive download/upload without duplicating screens',{skip:!bundle,timeout:60000},async t=>{
  const root=resolve(bundle),mime={'.html':'text/html','.js':'text/javascript','.mjs':'text/javascript','.wasm':'application/wasm','.json':'application/json','.css':'text/css','.png':'image/png'};
  const server=createServer(async(req,res)=>{try{const pathname=new URL(req.url,'http://127.0.0.1').pathname,file=resolve(root,'.'+decodeURIComponent(pathname==='/'?'/index.html':pathname));if(!file.startsWith(root+sep)){res.writeHead(403);res.end();return;}const bytes=await readFile(file);res.writeHead(200,{'Content-Type':mime[extname(file)]??'application/octet-stream'});res.end(bytes);}catch{res.writeHead(404);res.end();}});
  await new Promise(r=>server.listen(0,'127.0.0.1',r));t.after(()=>{server.closeAllConnections();server.close();});
  const browser=await chromium.launch({channel:'chrome',headless:true});t.after(()=>browser.close());const page=await browser.newPage({viewport:{width:1280,height:800},acceptDownloads:true});const errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.goto(`http://127.0.0.1:${server.address().port}`);await page.locator('.visual-stage').waitFor();
  const beforeScreen=await page.locator('.visual-stage').getAttribute('data-screen');
  const click=name=>page.getByRole('button',{name,exact:true}).click();
  await click('처음 시작');
  const state=()=>page.evaluate(async()=>(await window.player.view()).view.state);
  assert.equal(await page.evaluate(async()=>(await window.player.view()).view.runtimeBackend),'wasm');
  assert.equal(await page.getByText('기본 표현',{exact:true}).count(),1);
  const selects=page.locator('.visual-schedule select');
  // Inspect live DOM rather than assuming the number or order of timetable controls.
  const count=await selects.count();assert.ok(count>0);
  for(let i=0;i<count;i++)await selects.nth(i).selectOption('rest');
  await click('일정 확정');if((await state()).phase==='ready')await click('활동 시작');
  await page.getByText('휴식 중 · 기운을 회복한다',{exact:true}).waitFor();assert.equal((await state()).phase,'activity-result');
  const expected=await state();const folder=resolve('output/playwright/presentation-variants');await mkdir(folder,{recursive:true});
  await page.screenshot({path:resolve(folder,'activity-result.png')});
  const download=page.waitForEvent('download');await click('저장');const save=resolve(folder,'web-save.json');await(await download).saveAs(save);
  await click('결과 확인 · 다음');await page.getByText('기본 표현',{exact:true}).waitFor();
  const chooser=page.waitForEvent('filechooser');await click('불러오기');await(await chooser).setFiles(save);
  await page.getByText('휴식 중 · 기운을 회복한다',{exact:true}).waitFor();assert.deepEqual(await state(),expected);assert.equal(await page.locator('.visual-stage').getAttribute('data-screen'),beforeScreen);
  await page.setViewportSize({width:800,height:600});await page.screenshot({path:resolve(folder,'restored.png')});assert.deepEqual(await state(),expected);assert.deepEqual(errors,[]);
});

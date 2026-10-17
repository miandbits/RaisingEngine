import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile,mkdir} from 'node:fs/promises';
import {resolve,extname,sep} from 'node:path';
import {chromium} from 'playwright';
import {resolveScreenElements} from '../../dist/packages/visual/components.js';

const bundle=process.env.RAISING_CONDITIONAL_ROUTING_WEB_BUNDLE??process.env.RAISING_CUSTOM_STATES_WEB_BUNDLE;
test('GUI-authored state conditions and event effects play in Wasm and survive actual download/upload',{skip:!bundle,timeout:90000},async t=>{
  const root=resolve(bundle),packed=JSON.parse(await readFile(resolve(root,'game.json'),'utf8')),content=packed.content??packed;
  const info=JSON.parse(await readFile(resolve('.cache/custom-states/authoring.json'),'utf8'));
  const routing=process.env.RAISING_CONDITIONAL_ROUTING_WEB_BUNDLE?JSON.parse(await readFile(resolve('.cache/conditional-routing/authoring.json'),'utf8')):null;
  const elements=resolveScreenElements(content.project.visual,'screen-1');
  const button=action=>elements.find(e=>e.action===action)?.id;
  const mime={'.html':'text/html','.js':'text/javascript','.mjs':'text/javascript','.wasm':'application/wasm','.json':'application/json','.css':'text/css','.png':'image/png'};
  const server=createServer(async(req,res)=>{try{const pathname=new URL(req.url,'http://127.0.0.1').pathname,file=resolve(root,'.'+decodeURIComponent(pathname==='/'?'/index.html':pathname));if(!file.startsWith(root+sep)){res.writeHead(403);res.end();return;}const bytes=await readFile(file);res.writeHead(200,{'Content-Type':mime[extname(file)]??'application/octet-stream'});res.end(bytes);}catch{res.writeHead(404);res.end();}});
  await new Promise(r=>server.listen(0,'127.0.0.1',r));t.after(()=>{server.closeAllConnections();server.close();});
  const browser=await chromium.launch({channel:'chrome',headless:true});t.after(()=>browser.close());const page=await browser.newPage({viewport:{width:1280,height:800},acceptDownloads:true});const errors=[];page.on('pageerror',e=>errors.push(e.message));
  const folder=resolve('output/playwright',routing?'conditional-routing':'custom-states');await mkdir(folder,{recursive:true});
  await page.goto(`http://127.0.0.1:${server.address().port}`);await page.getByRole('button',{name:'처음 시작',exact:true}).click();
  const state=()=>page.evaluate(async()=>(await window.player.view()).view.state);
  const click=action=>page.locator(`[data-element-id="${button(action)}"]`).click();
  const advance=async()=>{const before=await state();await click('advance');await page.waitForFunction(revision=>window.player.view().then(r=>r.view.state.stateRevision>revision),before.stateRevision);};
  assert.equal(await page.evaluate(async()=>(await window.player.view()).view.runtimeBackend),'wasm');
  assert.equal((await state()).customStates.shared[info.groupId],info.idle);await page.getByText('평상시 · 다음 행동을 고른다',{exact:true}).waitFor();
  const selects=page.locator('.visual-schedule select');for(let i=0;i<await selects.count();i++)await selects.nth(i).selectOption('rest');
  await advance();if((await state()).phase==='ready')await advance();
  await page.getByText('수련 · 기운을 가다듬는다',{exact:true}).waitFor();assert.equal((await state()).customStates.shared[info.groupId],info.training);
  if(routing)assert.equal(await page.locator('.visual-stage').getAttribute('data-screen'),routing.trainingScreen);
  const restore=async(label,move)=>{
    const before=await state(),beforeScreen=await page.locator('.visual-stage').getAttribute('data-screen'),save=resolve(folder,label+'.json');const download=page.waitForEvent('download');await click('save');await(await download).saveAs(save);
    await move();const chooser=page.waitForEvent('filechooser');await click('load');await(await chooser).setFiles(save);
    await page.waitForFunction(revision=>window.player.view().then(r=>r.view.state.stateRevision===revision),before.stateRevision);assert.deepEqual(await state(),before);
    await page.locator(`.visual-stage[data-screen="${beforeScreen}"]`).waitFor();
    await page.screenshot({path:resolve(folder,label+'.png')});
  };
  await restore('training-restored',advance);
  let selected=false;
  for(let i=0;i<30;i++){
    const s=await state();if(s.customStates.shared[info.groupId]===info.banquet)break;
    if(s.phase==='narrative'){
      const a=s.activeNarrative,n=content.events.find(e=>e.id===a.id)?.nodes.find(n=>n.id===a.nodeId);
      if(n?.type==='choice'){await page.getByRole('button',{name:n.choices[0].text,exact:true}).click();selected=true;continue;}
    }
    await advance();
  }
  assert.equal(selected,true);assert.equal((await state()).customStates.shared[info.groupId],info.banquet);
  await page.getByText('연회 · 인연을 쌓는다',{exact:true}).waitFor();await restore('banquet-restored',advance);
  if(routing)assert.equal(await page.locator('.visual-stage').getAttribute('data-screen'),routing.banquetScreen);
  await page.getByText('연회 · 인연을 쌓는다',{exact:true}).waitFor();assert.deepEqual(errors,[]);
});

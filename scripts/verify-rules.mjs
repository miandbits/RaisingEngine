// Verify a project authored through the Qt workspace; never inject game definitions.
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {cp,mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {chromium} from 'playwright';
import {loadProject} from '../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../dist/packages/project-service/snapshot.js';
import {exportGame} from '../dist/packages/export-service/export.js';

const source=path.resolve(process.argv[2]??'tests/fixtures/projects/rules-lab');
await mkdir('.cache/rules',{recursive:true});
const work=await mkdtemp(path.resolve('.cache/rules/run-')),root=path.join(work,'project');
await cp(source,root,{recursive:true});
const built=await buildSnapshot(await loadProject(root));assert.ok(built.ok,JSON.stringify(built.diagnostics));
const content=built.snapshot,project=content.project,visual=project.visual;
assert.equal(project.gameplay.policyGroups.length,3);assert.equal(project.gameplay.actions.length,7);
const windows=path.join(work,'windows'),web=path.join(work,'web');
for(const [target,destination] of [['windows',windows],['web',web]])await exportGame({content,projectRoot:root,destination,target,runtimes:{windows:path.resolve('dist/native-player-app'),web:path.resolve('dist/web')}});
const screenshotRoot=path.resolve('output/playwright/rules');await mkdir(screenshotRoot,{recursive:true});
const mime={'.html':'text/html','.js':'text/javascript','.mjs':'application/javascript','.wasm':'application/wasm','.json':'application/json','.css':'text/css','.png':'image/png','.svg':'image/svg+xml'};
const server=createServer(async(req,res)=>{try{const pathname=decodeURIComponent(new URL(req.url,'http://127.0.0.1').pathname),file=path.resolve(web,`.${pathname==='/'?'/index.html':pathname}`);if(!file.startsWith(web+path.sep)){res.writeHead(403);res.end();return;}const bytes=await readFile(file);res.writeHead(200,{'Content-Type':mime[path.extname(file)]??'application/octet-stream','Cache-Control':'no-store'});res.end(bytes);}catch{res.writeHead(404);res.end();}});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));let browser;
try{
  browser=await chromium.launch({channel:'chrome',headless:true});const page=await browser.newPage({viewport:{width:1440,height:1000},acceptDownloads:true});page.setDefaultTimeout(12000);const errors=[];page.on('pageerror',e=>errors.push(e.message));
  const state=()=>page.evaluate(async()=>(await window.player.view()).view.state);
  const idle=()=>page.waitForFunction(()=>!document.querySelector('.visual-status')?.textContent?.includes('처리 중'));
  const click=async id=>{await page.locator(`[data-element-id="${id}"] button`).click();await idle();};
  const screen=id=>page.locator(`.visual-stage[data-screen="${id}"]`).waitFor();
  const confirm=async()=>{const revision=(await state()).stateRevision;const button=page.getByRole('dialog').getByRole('button',{name:'Apply',exact:true});await button.evaluate(el=>{el.click();el.click();});await page.getByRole('dialog').waitFor({state:'hidden'});await page.waitForFunction(async before=>(await window.player.view()).view.state.stateRevision>before||!!document.querySelector('[role=alert]'),revision);assert.equal(await page.getByRole('alert').count(),0,await page.locator('body').innerText());assert.equal((await state()).stateRevision,revision+1,'double confirm must execute exactly once');};
  const policy=async(group,option)=>{await page.getByTestId(`policy__rules-1__policy-${group}__option-${option}`).click();await confirm();};
  const save=async filename=>{const expected=await state(),pending=page.waitForEvent('download');await click('save');const file=path.join(work,filename);await(await pending).saveAs(file);assert.deepEqual(JSON.parse(await readFile(file,'utf8')).state,expected);return{file,state:expected};};
  const load=async saved=>{const chooser=page.waitForEvent('filechooser');await page.locator('[data-element-id="load"] button').click();await(await chooser).setFiles(saved.file);await idle();await page.waitForFunction(async expected=>JSON.stringify((await window.player.view()).view.state)===JSON.stringify(expected),saved.state);assert.deepEqual(await state(),saved.state);};
  await page.goto(`http://127.0.0.1:${server.address().port}`);await page.locator('.visual-stage').waitFor();await click('start');await screen('home');await click('button-1');await screen('screen-1');
  const before=await state();await page.getByTestId('policy__rules-1__policy-1__option-1').click();await page.getByRole('dialog').getByRole('button',{name:'Cancel',exact:true}).click();assert.deepEqual(await state(),before);
  await policy(1,1);await policy(2,2);assert.equal(await page.getByTestId('policy__rules-1__policy-3__option-1').isEnabled(),false,'20h study + 8h sleep exceeds shared 24h budget');
  await policy(2,1);await policy(3,1);assert.deepEqual((await state()).gameplay.policySelections,{'policy-1':'option-1','policy-2':'option-1','policy-3':'option-1'});assert.equal((await state()).attributes.money,60);
  await page.screenshot({path:path.join(screenshotRoot,'policies.png')});await click('button-1');await screen('home');const policySave=await save('policies-save.json');
  await click('schedule-link');await screen('schedule');for(const select of await page.getByRole('combobox').all())await select.selectOption('rest');
  await click('advance');await page.waitForFunction(async()=>(await window.player.view()).view.state.phase==='ready');await click('advance');await page.waitForFunction(async()=>(await window.player.view()).view.state.phase==='activity-result');
  const afterActivity=await state();assert.equal(afterActivity.attributes.money,55);assert.equal(afterActivity.attributes.knowledge,4);assert.equal(afterActivity.attributes.stamina,96,'before nutrition +1, rest +30, after sleep +5');
  await page.screenshot({path:path.join(screenshotRoot,'trigger-result.png')});await load(policySave);await screen('home');
  await click('button-2');await screen('screen-2');const recovery='action__rules-1__action-4';await page.getByRole('button',{name:`${recovery} 수량 늘리기`,exact:true}).click();
  const beforeAction=await state();await page.getByTestId(recovery).click();await page.screenshot({path:path.join(screenshotRoot,'action-confirmation.png')});await page.getByRole('dialog').getByRole('button',{name:'Cancel',exact:true}).click();assert.deepEqual(await state(),beforeAction);
  await page.getByTestId(recovery).click();await confirm();assert.equal((await state()).attributes.money,52);assert.equal((await state()).attributes.stamina,80);
  await page.getByTestId('action__rules-1__action-5').click();await confirm();assert.equal((await state()).attributes.reputation,2);await click('button-1');await screen('home');const saved=await save('complete-save.json');
  await click('button-1');await screen('screen-1');await page.getByTestId('policy_clear__rules-1__policy-2').click();const clearDialog=page.getByRole('dialog');await clearDialog.getByRole('button',{name:'확인',exact:true}).click();await idle();assert.equal((await state()).gameplay.policySelections['policy-2'],null);assert.equal((await state()).gameplay.policySelections['policy-3'],'option-1');await click('button-1');await screen('home');await load(saved);
  assert.deepEqual(errors,[]);await writeFile(path.join(work,'web-result.json'),JSON.stringify({ok:true,guiSource:source,runtime:'cpp-wasm',policyCancelPreservesState:true,actionCancelPreservesState:true,duplicateConfirmOnce:true,sharedBudget:true,independentSelections:true,beforeAfterTriggers:afterActivity.attributes,saveLoad:true,savedState:saved.state},null,2));
}finally{await browser?.close();server.closeAllConnections();await new Promise(resolve=>server.close(resolve));}
await writeFile('.cache/rules/latest.json',JSON.stringify({ok:true,source,work,root,windows,web,screenshotRoot},null,2));
console.log(JSON.stringify({ok:true,source,work,windows,web,screenshotRoot}));


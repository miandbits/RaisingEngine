// Exercise the exported, Qt-authored HUD through real browser controls. Project data is read-only.
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {cp,mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import path from 'node:path';
import {chromium} from 'playwright';
import {loadProject} from '../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../dist/packages/project-service/snapshot.js';
import {exportGame} from '../dist/packages/export-service/export.js';
const source=path.resolve(process.argv[2]??'projects/battle-hud-lab');await mkdir('.cache/battle-hud',{recursive:true});
const work=await mkdtemp(path.resolve('.cache/battle-hud/run-')),root=path.join(work,'project'),web=path.join(work,'web');await cp(source,root,{recursive:true});
const built=await buildSnapshot(await loadProject(root));assert.ok(built.ok,JSON.stringify(built.diagnostics));const content=built.snapshot;
await exportGame({content,projectRoot:root,destination:web,target:'web',runtimes:{web:path.resolve('dist/web')}});
const screenshotRoot=path.resolve('output/playwright/battle-hud');await mkdir(screenshotRoot,{recursive:true});const manifest={ok:false,source,work,root,web,screenshotRoot};await writeFile('.cache/battle-hud/latest.json',JSON.stringify(manifest,null,2));
const mime={'.html':'text/html','.js':'text/javascript','.mjs':'text/javascript','.wasm':'application/wasm','.json':'application/json','.css':'text/css','.png':'image/png','.jpg':'image/jpeg','.svg':'image/svg+xml'};
const server=createServer(async(req,res)=>{try{const url=decodeURIComponent(new URL(req.url,'http://localhost').pathname),file=path.resolve(web,`.${url==='/'?'/index.html':url}`);if(!file.startsWith(web+path.sep)){res.writeHead(403);res.end();return;}const bytes=await readFile(file);res.writeHead(200,{'Content-Type':mime[path.extname(file)]??'application/octet-stream','Cache-Control':'no-store'});res.end(bytes);}catch{res.writeHead(404);res.end();}});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));let browser;
try{
  browser=await chromium.launch({channel:'chrome',headless:true});const page=await browser.newPage({viewport:{width:1440,height:1000},acceptDownloads:true});page.setDefaultTimeout(12000);const errors=[];page.on('pageerror',e=>errors.push(e.message));
  const view=()=>page.evaluate(async()=>(await window.player.view()).view);
  const wait=async predicate=>{const until=Date.now()+12000;while(Date.now()<until){const v=await view();if(predicate(v))return v;await page.waitForTimeout(50);}throw new Error('Browser player condition timed out');};
  const click=async id=>{await page.locator(`[data-element-id="${id}"] button`).click();};
  const clickAuthored=async action=>{const screen=await page.locator('.visual-stage').getAttribute('data-screen');const element=content.project.visual.screens.find(s=>s.id===screen).elements.find(e=>e.kind==='button'&&e.action===action);assert.ok(element,`${screen} has no ${action} button`);await click(element.id);};
  const collection=type=>content.project.visual.screens.find(s=>s.id==='battle').elements.find(e=>e.kind==='collection'&&e.source?.type===type);
  const selectRow=async(type,predicate)=>{const v=await view(),element=collection(type),row=v.dataUi.collections.battle[element.id].find(predicate);assert.ok(row,`Missing ${type} row`);const child=content.project.visual.components.find(c=>c.id===element.componentId).elements.find(e=>e.kind==='button');assert.ok(child);await click(`${element.id}__${row.id}__${child.id}`);};
  const selectAction=async action=>{await selectRow('battleActions',row=>row.values.battleAction===action);await wait(v=>v.battleSelection?.battleAction===action);};
  const selectTarget=async target=>{await selectRow('battleTargets',row=>row.values.battleTarget===target);await wait(v=>v.battleSelection?.target===target);};
  const confirm=async()=>{const revision=(await view()).state.stateRevision;await clickAuthored('confirmBattleAction');return wait(v=>v.state.stateRevision>revision);};
  await page.goto(`http://127.0.0.1:${server.address().port}`);await page.locator('.visual-stage').waitFor();await clickAuthored('start');await wait(v=>v.state?.phase==='planning');await click('schedule-link');for(const select of await page.getByRole('combobox').all())await select.selectOption('forest-trip');await clickAuthored('advance');await wait(v=>!!v.state.schedule);await clickAuthored('advance');await wait(v=>v.state.phase==='expedition');await page.getByRole('button',{name:'이동 · 슬라임 공터',exact:true}).click();await wait(v=>v.state.phase==='battle');
  const before=structuredClone((await view()).state);assert.equal(await page.locator('.visual-meter progress').count(),2);
  await selectAction('guard');await selectTarget('self');const pending=await view();assert.deepEqual(pending.state,before);await page.screenshot({path:path.join(screenshotRoot,'target-selection.png')});
  const download=page.waitForEvent('download');await clickAuthored('save');const saveFile=path.join(work,'midbattle.json');await(await download).saveAs(saveFile);const saved=JSON.parse(await readFile(saveFile,'utf8'));assert.deepEqual(saved.state,before);assert.deepEqual(saved.playerPresentation.battleSelection,pending.battleSelection);
  await clickAuthored('cancelBattleAction');await wait(v=>v.battleSelection===null);assert.deepEqual((await view()).state,before);
  const chooser=page.waitForEvent('filechooser');await clickAuthored('load');await(await chooser).setFiles(saveFile);await wait(v=>v.battleSelection?.battleAction==='guard'&&v.battleSelection.target==='self');assert.deepEqual((await view()).state,before);assert.deepEqual((await view()).battleSelection,pending.battleSelection);
  let current=await confirm();assert.equal(current.state.systems.battle.log.at(-1).action,'guard');assert.equal(current.battleSelection,null);
  await page.setViewportSize({width:1024,height:720});await page.screenshot({path:path.join(screenshotRoot,'responsive-battle.png')});
  for(let turn=0;current.state.phase==='battle'&&turn<40;turn++){await selectAction('attack');await selectTarget('enemy');current=await confirm();}
  assert.equal(current.state.systems.lastBattle.outcome,'victory');const final=structuredClone(current.state);assert.equal(final.gameplay.inventory.flower,(before.gameplay.inventory.flower??0)+1);assert.ok(final.systems.lastBattle.rewardChanges.length);await page.locator('.visual-stage[data-screen="expedition"]').waitFor();const logElement=content.project.visual.screens.find(s=>s.id==='expedition').elements.find(e=>e.source?.type==='battleLog');await page.locator(`[data-element-id="${logElement.id}"]`).hover();await page.mouse.wheel(0,1000);await page.waitForFunction(id=>document.querySelector(`[data-element-id="${id}"]`).scrollTop>0,logElement.id);await page.screenshot({path:path.join(screenshotRoot,'victory-rewards.png')});
  const completedDownload=page.waitForEvent('download');await clickAuthored('save');const completedFile=path.join(work,'completed.json');await(await completedDownload).saveAs(completedFile);const completedGeneration=(await view()).generation;const completedChooser=page.waitForEvent('filechooser');await clickAuthored('load');await(await completedChooser).setFiles(completedFile);await wait(v=>v.generation>completedGeneration&&v.state.stateRevision===final.stateRevision);assert.deepEqual((await view()).state,final);assert.deepEqual(errors,[]);
  await writeFile(path.join(work,'web-result.json'),JSON.stringify({ok:true,runtime:'cpp-wasm',uiOnlySelection:true,cancelPreservesState:true,midbattleSaveRestoresSelectionAndState:true,rewardsAppliedOnce:true,physicalBattleLogScroll:true,finalState:final,selectedState:pending.battleSelection},null,2));
}finally{await browser?.close();server.closeAllConnections();await new Promise(resolve=>server.close(resolve));}
assert.equal(await readFile(path.join(source,'project.json'),'utf8'),await readFile(path.join(root,'project.json'),'utf8'));await writeFile('.cache/battle-hud/latest.json',JSON.stringify({...manifest,ok:true},null,2));console.log(JSON.stringify({...manifest,ok:true}));

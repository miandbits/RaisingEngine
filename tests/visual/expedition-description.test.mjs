import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile, mkdir, mkdtemp, copyFile, writeFile, cp } from 'node:fs/promises';
import { resolve, basename } from 'node:path';
import { createHash } from 'node:crypto';
import { spawnSync } from 'node:child_process';
import { build } from 'esbuild';
import { chromium } from 'playwright';
import { getSystemsView } from '../../dist/packages/systems/service.js';
import { nativeRequest, nativeReplay, closeNativeRuntimes } from '../../dist/packages/native-bridge/index.js';
import { canonicalJson } from '../../dist/packages/project-service/snapshot.js';

test('web expedition descriptions wrap, preserve empty layout and leave movement buttons clickable', async t => {
  const bundle=await build({stdin:{contents:`import React from 'react';import {createRoot} from 'react-dom/client';import {SystemsWidget} from './apps/player/renderer/SystemsWidget.tsx';const root=createRoot(document.getElementById('root'));window.commands=[];window.renderTrip=description=>root.render(<SystemsWidget kind="expedition" busy={false} editing={false} systems={{expedition:{name:'Forest',nodeName:'Spring',nodeDescription:description,heroHp:80,stats:{maxHp:80},connections:[{id:'crossroad',name:'Crossroad',enabled:true}],canReturn:true}}} onCommand={command=>window.commands.push(command)}/>);`,resolveDir:process.cwd(),loader:'tsx'},bundle:true,write:false,format:'iife',logLevel:'silent'});
  const browser=await chromium.launch({headless:true,...(process.env.PLAYWRIGHT_CHANNEL?{channel:process.env.PLAYWRIGHT_CHANNEL}:{})});t.after(()=>browser.close());
  const page=await browser.newPage({viewport:{width:500,height:600}});
  await page.setContent('<div id="root" style="width:320px;max-height:420px;overflow:auto"></div>');
  await page.addStyleTag({content:await readFile('apps/player/renderer/visual.css','utf8')});
  await page.addScriptTag({content:bundle.outputFiles[0].text});
  const render=async description=>{await page.evaluate(value=>window.renderTrip(value),description);await page.locator('[data-testid="expedition-widget"]').waitFor();await page.waitForFunction(expected=>(document.querySelector('.expedition-description')?.textContent??'')===expected,description??'');};
  await render(undefined);const initial=await page.getByRole('button',{name:'이동 · Crossroad'}).boundingBox();
  await render('');assert.equal(await page.locator('.expedition-description').count(),0);assert.deepEqual(await page.getByRole('button',{name:'이동 · Crossroad'}).boundingBox(),initial);
  const description='맑은 물소리가 들린다. 갈림길로 돌아가거나 숲길을 더 살펴볼 수 있다.\n'+'아주긴설명'.repeat(180);
  await render(description);const box=await page.locator('.expedition-description').boundingBox();assert.ok(box.height>80);assert.ok(box.width<=320);
  assert.ok(await page.locator('#root').evaluate(e=>e.scrollWidth<=e.clientWidth));
  await page.getByRole('button',{name:'이동 · Crossroad'}).click();assert.deepEqual(await page.evaluate(()=>window.commands),[{type:'moveExpedition',nodeId:'crossroad'}]);
  await page.locator('#root').evaluate(e=>e.scrollTop=0);await mkdir('output/playwright',{recursive:true});await page.screenshot({path:'output/playwright/expedition-description.png'});
});

test('authored native expedition shows description, canonical projection agrees and buttons remain hit-testable', {skip:!process.env.RAISING_EXPEDITION_DESCRIPTION_BUNDLE,timeout:90000}, async t=>{
  t.after(closeNativeRuntimes);
  const source=resolve(process.env.RAISING_EXPEDITION_DESCRIPTION_BUNDLE),saved=resolve(process.env.RAISING_EXPEDITION_DESCRIPTION_SAVE);
  const {content}=JSON.parse(await readFile(resolve(source,'game.json'),'utf8')),saveData=JSON.parse(await readFile(saved,'utf8'));
  const stateBefore=structuredClone(saveData.state),trip=stateBefore.systems.expedition;
  const node=content.project.systems.expeditions.find(e=>e.id===trip.expeditionId).nodes.find(n=>n.id===trip.nodeId);
  const projection=nativeRequest({operation:'view',content,state:stateBefore});assert.equal(projection.ok,true);assert.deepEqual(projection.systems,getSystemsView(content,stateBefore));assert.equal(projection.systems.expedition.nodeDescription,node.description);assert.deepEqual(stateBefore,saveData.state);
  await mkdir('.cache/expedition-description-tests',{recursive:true});const root=await mkdtemp(resolve('.cache/expedition-description-tests/run-'));
  const save=resolve(root,'save.json');await copyFile(saved,save);const target=node.connections[0];
  await writeFile(resolve(root,'steps.json'),JSON.stringify([{key:'F9'},{waitFrames:8},{checkpoint:'description',screenshot:resolve(root,'description.png')},{clickWindow:`move-expedition-${target}`},{checkpoint:'moved'}]));
  const runtime=resolve(process.env.RAISING_EXPEDITION_DESCRIPTION_RUNTIME??'dist/native-player-expedition-copy','RaisingPlayer.exe');
  const result=spawnSync(runtime,['--bundle',source,'--save',save,'--hidden','--automation',resolve(root,'steps.json'),'--report',resolve(root,'report.json')],{encoding:'utf8',timeout:60000,windowsHide:true});assert.equal(result.status,0,result.error?.message??result.stderr);
  const report=JSON.parse(await readFile(resolve(root,'report.json'),'utf8'));assert.equal(report.ok,true);const initial=report.steps.find(s=>s.command.checkpoint==='description').ui;
  assert.deepEqual(initial.state,saveData.state);assert.ok(Object.values(initial.texts).some(text=>text.includes(node.description)));assert.equal(report.final.state.systems.expedition.nodeId,target);
  assert.deepEqual(JSON.parse(await readFile(save,'utf8')),saveData);t.diagnostic(`Evidence: ${root}`);
  // Isolated fixture variants use a fresh fingerprint and canonical command replay;
  // never modify the authored bundle or inject an inconsistent saved state.
  const variantRoot=resolve(root,'variants');await cp(source,variantRoot,{recursive:true,filter:path=>!['build','RaisingPlayer.exe'].includes(basename(path))});
  for(const [name,description] of [['long','맑은 물소리가 들린다. 숲길을 살펴보자. '.repeat(35)],['empty','']]){
    const bundle=JSON.parse(await readFile(resolve(source,'game.json'),'utf8'));const variant=bundle.content;
    variant.project.systems.expeditions.find(e=>e.id===trip.expeditionId).nodes.find(n=>n.id===trip.nodeId).description=description;
    delete variant.contentFingerprint;variant.contentFingerprint='sha256:'+createHash('sha256').update(canonicalJson(variant)).digest('hex');
    const replay=nativeReplay(variant,saveData.origin,saveData.commands);assert.equal(replay.ok,true,JSON.stringify(replay.error));
    const projection=nativeRequest({operation:'view',content:variant,state:replay.state});assert.deepEqual(projection.systems,getSystemsView(variant,replay.state));assert.equal(projection.systems.expedition.nodeDescription,description||undefined);
    await writeFile(resolve(variantRoot,'game.json'),JSON.stringify(bundle));await writeFile(save,JSON.stringify({...saveData,contentFingerprint:variant.contentFingerprint,state:replay.state}));
    await writeFile(resolve(root,'steps.json'),JSON.stringify([{key:'F9'},{waitFrames:8},{checkpoint:name,screenshot:resolve(root,`${name}.png`)},{clickWindow:`move-expedition-${target}`} ]));
    const result=spawnSync(runtime,['--bundle',variantRoot,'--save',save,'--hidden','--automation',resolve(root,'steps.json'),'--report',resolve(root,`${name}.json`)],{encoding:'utf8',timeout:60000,windowsHide:true});assert.equal(result.status,0,result.error?.message??result.stderr);
    const output=JSON.parse(await readFile(resolve(root,`${name}.json`),'utf8'));assert.equal(output.ok,true);const before=output.steps.find(s=>s.command.checkpoint===name).ui;
    const body=Object.values(before.texts).join('');assert.equal(body.includes('expedition-description'),!!description);assert.equal(output.final.state.systems.expedition.nodeId,target);
  }
});

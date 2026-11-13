import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {spawnSync} from 'node:child_process';
import path from 'node:path';
import {chromium} from 'playwright';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/session.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';

test('real native and browser players render revised UI while loading, continuing and resaving the original history',{timeout:180000},async t=>{
  await mkdir('.cache/presentation-update-players',{recursive:true});const out=await mkdtemp(path.resolve('.cache/presentation-update-players/run-'));
  const s=new QtStudioService();t.after(()=>s.releases.close());const call=(method,params={})=>s.handle({method,params:{revision:s.editor.revision,...params}});
  await call('create',{root:path.join(out,'project'),template:'my-first-raising-game'});
  const base=(await s.editor.snapshot(s.editor.revision)).snapshot;
  const baseline=(await call('export',{target:'web',destination:path.join(out,'baseline')})).exported;
  let session=createSession(base,42).session;
  session=dispatchSession(base,session,{type:'setSchedule',activityIds:Array(base.project.periodsPerMonth).fill('rest')},session.state.stateRevision).session;
  const original=await encodeSave(base,session);assert.equal(original.ok,true);
  const save=path.join(out,'original.json');await writeFile(save,original.text);
  const expected=dispatchSession(base,session,{type:'executeActivity'},session.state.stateRevision).session;
  const project=structuredClone(s.editor.documents.find(d=>d.kind==='project').data);
  Object.assign(project.visual.screens[0].elements.find(e=>e.id==='eyebrow'),{text:'A FAMILIAR FUTURE',color:'#f4c477',x:96});
  project.visual.screens.find(v=>v.id==='schedule').elements.find(e=>e.id==='hint').text='Revised presentation. Your original choices remain.';
  await call('edit',{edit:{type:'replace',file:'project.json',data:project}});await call('save');
  const published={};
  for(const target of ['web',desktopTarget()]){
    const report=(await call('presentationUpdateCheck',{target,previousBundle:path.join(baseline.directory,'game.json'),sampleSave:save})).presentationUpdate;
    published[target]=(await call('export',{target,destination:path.join(out,target),presentationCheck:report.checkId})).exported;
  }
  const target=desktopTarget(),native=published[target],input=path.join(out,'native-steps.json'),report=path.join(out,'native-report.json'),nativeSave=path.join(out,'native-save.json');await writeFile(nativeSave,original.text);
  await writeFile(input,JSON.stringify([{checkpoint:'title',screenshot:path.join(out,'native-title.png')},{key:'F9'},{checkpoint:'loaded'},{click:'advance'},{key:'F5'},{checkpoint:'continued',screenshot:path.join(out,'native-continued.png')}]));
  const run=(steps,output)=>spawnSync(playerExecutable(target,native.directory),['--bundle',gameDirectory(target,native.directory),'--automation',steps,'--report',output,'--save',nativeSave,'--hidden','--report-checkpoints-only'],{encoding:'utf8',timeout:60000});
  let process=run(input,report);assert.equal(process.status,0,process.stderr||process.error?.message);
  const proof=JSON.parse(await readFile(report,'utf8')),at=id=>proof.steps.find(v=>v.command.checkpoint===id).ui;
  assert.match(at('title').texts.eyebrow,/A FAMILIAR FUTURE/);assert.match(at('loaded').texts.hint,/Revised presentation/);
  assert.deepEqual(at('loaded').state,session.state);assert.deepEqual(at('continued').state,expected.state);
  assert.equal((await decodeSave(base,await readFile(nativeSave,'utf8'))).ok,true);
  await writeFile(input,JSON.stringify([{key:'F9'},{checkpoint:'fresh'}]));process=run(input,path.join(out,'native-fresh.json'));assert.equal(process.status,0,process.stderr);
  assert.deepEqual(JSON.parse(await readFile(path.join(out,'native-fresh.json'),'utf8')).steps[0].ui.state,expected.state);

  const browser=await chromium.launch({headless:true,...(globalThis.process.env.RAISING_BROWSER_CHANNEL?{channel:globalThis.process.env.RAISING_BROWSER_CHANNEL}:{})});t.after(()=>browser.close());
  const page=await browser.newPage({viewport:{width:1280,height:800},acceptDownloads:true});const errors=[];page.on('pageerror',e=>errors.push(e.message));
  const {url}=(await call('releaseLaunch',{token:published.web.launchToken})).launch;await page.goto(url);await page.locator('[data-element-id="eyebrow"]').waitFor();
  assert.match(await page.locator('[data-element-id="eyebrow"]').innerText(),/A FAMILIAR FUTURE/);
  const load=async file=>{const chooser=page.waitForEvent('filechooser');await page.getByRole('button',{name:/^(Load|Continue a saved game|불러오기)$/}).click();await(await chooser).setFiles(file);await page.waitForFunction(()=>document.querySelector('.visual-player')?.getAttribute('aria-busy')==='false');};
  await load(save);assert.deepEqual((await page.evaluate(()=>window.player.view())).view.state,session.state);
  assert.match(await page.locator('[data-element-id="hint"]').innerText(),/Revised presentation/);
  await page.locator('[data-element-id="advance"]').getByRole('button').click();
  assert.deepEqual((await page.evaluate(()=>window.player.view())).view.state,expected.state);
  const download=page.waitForEvent('download');await page.getByRole('button',{name:/^(Save|저장)$/}).click();const webSave=path.join(out,'web-save.json');await(await download).saveAs(webSave);
  assert.equal((await decodeSave(base,await readFile(webSave,'utf8'))).ok,true);
  await page.screenshot({path:path.join(out,'web-continued.png')});await page.reload();await page.locator('[data-element-id="eyebrow"]').waitFor();await load(webSave);
  assert.deepEqual((await page.evaluate(()=>window.player.view())).view.state,expected.state);assert.deepEqual(errors,[]);
  // Tampered functional fields are rejected at native startup, before any command.
  const bundleFile=path.join(gameDirectory(target,native.directory),'game.json'),bundle=JSON.parse(await readFile(bundleFile,'utf8'));
  bundle.presentationRevision.visual.screens[0].elements.find(e=>e.id==='start').action='load';await writeFile(bundleFile,JSON.stringify(bundle));
  process=run(input,path.join(out,'tampered.json'));assert.notEqual(process.status,0);assert.match(process.stderr,/PRESENTATION_CONTENT_CHANGED/);
  await writeFile(path.join(out,'result.json'),JSON.stringify({ok:true,baselineFingerprint:base.contentFingerprint,nativeFreshRestore:true,webFreshRestore:true,renderedOverlay:true,tamperedOverlayRejected:true},null,2));
  await writeFile('.cache/presentation-update-players/latest.json',JSON.stringify({ok:true,out},null,2));
});

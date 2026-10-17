import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile} from 'node:fs/promises';
import {resolve,extname,sep} from 'node:path';
import {createServer} from 'node:http';
import {chromium} from 'playwright';
import {spawnSync} from 'node:child_process';
import {makeGame,controller} from './player-extension-fixture.mjs';

async function fixture(target){
  const f=await makeGame({target,name:'presentation-variants-'+target}),file=resolve(f.game,'game.json');
  const packed=JSON.parse(await readFile(file,'utf8')),c=packed.content,home=c.project.visual.screens.find(s=>s.id==='home');
  const money=c.project.initialState.attributes.money??c.attributes.find(a=>a.id==='money').initial;
  const hero=home.elements.find(e=>e.id==='hero-anim');hero.variants=[{label:'action-dependent motion',when:{op:'compare',comparator:'gt',left:{op:'read',target:{kind:'attribute',attributeId:'money'}},right:{op:'literal',value:money}},values:{extensionRef:'hero-training'}}];
  packed.extensions.plugins[0].documents.push({type:'controller',id:'hero-training',data:{...controller,defaultClip:'joy',bindings:[]}});
  c.project.gameplay.actions.push({id:'calm',name:'진정하기',costs:[],effects:[{op:'add',target:{kind:'attribute',attributeId:'money'},value:{op:'literal',value:-1}}]});
  home.elements.push({id:'calm',kind:'button',x:370,y:600,width:250,height:48,text:'진정하기',action:'executeAction',actionId:'calm'});
  await writeFile(file,JSON.stringify(packed));return f;
}
test('one placed plugin switches controller by a real action and restores the saved animation cursor',{timeout:120000},async()=>{
  const f=await fixture('windows');
  const steps=[{click:'start'},{checkpoint:'base'},{click:'cheer'},{click:'confirm-purchase'},{extensionAdvanceMs:250},{checkpoint:'changed'},{key:'F5'},{click:'calm'},{click:'confirm-purchase'},{checkpoint:'base-again'},{key:'F9'},{checkpoint:'restored'}];
  const script=resolve(f.root,'steps.json'),reportFile=resolve(f.root,'report.json'),save=resolve(f.root,'save.json');await writeFile(script,JSON.stringify(steps));
  const run=spawnSync(resolve(f.game,'RaisingPlayer.exe'),['--bundle',f.game,'--save',save,'--seed','42','--hidden','--automation',script,'--report',reportFile],{encoding:'utf8',timeout:90000,windowsHide:true});
  const report=JSON.parse(await readFile(reportFile,'utf8'));assert.equal(run.status,0,JSON.stringify(report.error??run.stderr));assert.equal(report.ok,true);
  const ui=name=>report.steps.find(s=>s.command.checkpoint===name).ui;
  const animation=name=>ui(name).extensions.instances.find(i=>i.instance==='home/hero-anim');
  for(const name of ['base','changed','base-again','restored']){assert.equal(ui(name).screenId,'home');assert.equal(animation(name).available,true,animation(name).error);}
  assert.equal(animation('base').state.tracks[0].clip,'idle');assert.equal(animation('changed').state.tracks[0].clip,'joy');assert.equal(animation('changed').state.tracks[0].timeMs,250);
  assert.equal(animation('base-again').state.tracks[0].clip,'idle');assert.deepEqual(animation('restored').state,animation('changed').state);assert.deepEqual(ui('restored').state,ui('changed').state);
  assert.equal(JSON.parse(await readFile(save,'utf8')).playerPresentation.extensions['cutout-animation'].instances['home/hero-anim'].ref,'hero-training');
  console.log('Animation switch evidence: '+f.root);
});

test('web controller switch preserves every animation layer on actual save upload',{timeout:60000},async t=>{
  const f=await fixture('web'),mime={'.html':'text/html','.js':'text/javascript','.mjs':'text/javascript','.wasm':'application/wasm','.css':'text/css','.json':'application/json','.png':'image/png'};
  const server=createServer(async(req,res)=>{try{const pathname=new URL(req.url,'http://127.0.0.1').pathname,file=resolve(f.game,'.'+decodeURIComponent(pathname==='/'?'/index.html':pathname));if(!file.startsWith(f.game+sep)){res.writeHead(403);res.end();return;}const bytes=await readFile(file);res.writeHead(200,{'Content-Type':mime[extname(file)]??'application/octet-stream'});res.end(bytes);}catch{res.writeHead(404);res.end();}});
  await new Promise(r=>server.listen(0,'127.0.0.1',r));t.after(()=>{server.closeAllConnections();server.close();});const browser=await chromium.launch({channel:'chrome',headless:true});t.after(()=>browser.close());const page=await browser.newPage({acceptDownloads:true});
  await page.addInitScript(()=>{window.raisingExtensionManualClock=true;});await page.goto(`http://127.0.0.1:${server.address().port}`);
  const click=id=>page.locator(`[data-element-id="${id}"] button`).click();
  const animation=()=>page.evaluate(()=>window.raisingExtensions.describe().instances.find(i=>i.instance==='home/hero-anim'));
  const state=()=>page.evaluate(async()=>(await window.player.view()).view.state);
  const waitRef=ref=>page.waitForFunction(ref=>window.raisingExtensions.describe().instances.some(i=>i.instance==='home/hero-anim'&&i.element.ref===ref),ref);
  await click('start');await waitRef('hero-main');await click('cheer');await page.locator('.transaction-dialog button').first().click();await waitRef('hero-training');
  await page.evaluate(()=>window.raisingExtensions.tick(250));const expected=await animation(),expectedState=await state();assert.equal(expected.state.tracks[1].timeMs,250);
  const download=page.waitForEvent('download');await click('save');const save=resolve(f.root,'save.json');await(await download).saveAs(save);
  await click('calm');await page.locator('.transaction-dialog button').first().click();await waitRef('hero-main');
  const chooser=page.waitForEvent('filechooser');await click('load');await(await chooser).setFiles(save);await waitRef('hero-training');await page.evaluate(()=>window.raisingExtensions.tick(0));
  assert.deepEqual((await animation()).state,expected.state);assert.deepEqual(await state(),expectedState);assert.equal(await page.locator('.visual-stage').getAttribute('data-screen'),'home');
});

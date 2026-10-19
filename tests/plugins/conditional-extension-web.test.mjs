import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile,writeFile} from 'node:fs/promises';
import {resolve,extname,sep} from 'node:path';
import {chromium} from 'playwright';
import {makeGame,rig,controller,pluginElement} from './player-extension-fixture.mjs';

test('actual React result mounts observe once, later navigation is not replayed, and exact restore wins',{timeout:120000},async t=>{
 const f=await makeGame({target:'web',name:'conditional-mount',documents:{rig,controller:{...controller,bindings:[{id:'activity-joy',signal:'activityResult',match:{},clip:'joy',then:'default'},{id:'action-joy',signal:'actionResult',match:{actionId:'cheer'},clip:'joy',then:'default'}]}}});
 t.diagnostic(`fixture: ${f.root}`);
 const file=resolve(f.game,'game.json'),game=JSON.parse(await readFile(file,'utf8'));
 const button=(id,text,action,x,extra={})=>({id,kind:'button',x,y:620,width:150,height:50,text,action,...extra});
 game.content.events=[{id:'flash',label:'Flash',trigger:'interaction',priority:10,window:{fromPeriod:0,throughPeriod:35},requirements:{op:'always'},entryNodeId:'effect',nodes:[{id:'effect',type:'effect',effects:[],next:'dialogue'},{id:'dialogue',type:'dialogue',text:'Settled dialogue',next:'end'},{id:'end',type:'end'}]}];
 game.content.project.gameplay.actions[0].eventId='flash';game.content.project.schedulePlayback='manual';delete game.content.project.activityTimelines;
 game.content.project.visual={width:1280,height:720,routing:{defaultScreenId:'home',phases:[{phase:'title',screenId:'title'}]},screens:[
  {id:'title',label:'Title',elements:[button('start','Start','start',20)]},
  {id:'home',label:'Home',elements:[{id:'plan',kind:'schedule',x:20,y:20,width:1100,height:100},
   {...pluginElement,x:250,y:150},{...pluginElement,id:'result-anim',x:800,y:150,visibleWhen:'result'},{...pluginElement,id:'dialogue-anim',x:800,y:150,visibleWhen:'dialogue'},
   button('advance','Next','advance',20),button('save','Save','save',200),button('load','Load','load',380),button('details','Details','navigate',560,{screenId:'details'}),button('trigger','Trigger','executeAction',740,{actionId:'cheer'})]},
  {id:'details',label:'Details',elements:[{...pluginElement,id:'late',x:250,y:150},button('load','Load','load',20),button('save','Save','save',200)]},
 ]};
 await writeFile(file,JSON.stringify(game));
 const mime={'.html':'text/html','.js':'text/javascript','.mjs':'text/javascript','.css':'text/css','.json':'application/json','.wasm':'application/wasm','.png':'image/png','.svg':'image/svg+xml'};
 const server=createServer(async(req,res)=>{try{const file=resolve(f.game,'.'+(new URL(req.url,'http://localhost').pathname==='/'?'/index.html':new URL(req.url,'http://localhost').pathname));if(!file.startsWith(f.game+sep)){res.writeHead(403).end();return;}const data=await readFile(file);res.writeHead(200,{'Content-Type':mime[extname(file)]??'application/octet-stream'}).end(data);}catch{res.writeHead(404).end();}});
 await new Promise(r=>server.listen(0,'127.0.0.1',r));t.after(()=>{server.closeAllConnections();server.close();});
 const browser=await chromium.launch({channel:'chrome',headless:true});t.after(()=>browser.close());
 const page=await browser.newPage({viewport:{width:1280,height:850}}),errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.addInitScript(()=>{window.raisingExtensionManualClock=true;});await page.goto(`http://127.0.0.1:${server.address().port}`);
 const click=async id=>page.locator(`[data-element-id="${id}"] button`).click();
 const describe=()=>page.evaluate(()=>window.raisingExtensions.describe());
 const track=(d,id,n=0)=>d.instances.find(i=>i.instance===id)?.state?.tracks.find(t=>t.track===n);
 const tick=ms=>page.evaluate(ms=>window.raisingExtensions.tick(ms),ms);
 const save=async name=>{const waiting=page.waitForEvent('download');await click('save');const destination=resolve(f.root,name+'.json');await(await waiting).saveAs(destination);return {destination,value:JSON.parse(await readFile(destination,'utf8'))};};
 const load=async saved=>{const waiting=page.waitForEvent('filechooser');await click('load');await(await waiting).setFiles(saved.destination);await page.locator('[data-instance="home/result-anim"]').waitFor();};
 await click('start');await page.locator('[data-instance="home/hero-anim"]').waitFor();
 assert.equal((await describe()).instances.some(i=>i.instance==='home/result-anim'),false);
 await click('advance');await click('advance');await page.locator('[data-instance="home/result-anim"]').waitFor();
 await page.waitForFunction(()=>!window.raisingExtensions.presentationPending);await tick(0);
 let d=await describe();assert.equal(track(d,'home/result-anim').clip,'joy','new result-only mount receives the activity result');
 assert.equal(track(d,'home/hero-anim').clip,'joy');assert.equal(d.observations,2);
 assert.deepEqual(d.lastObserve.plugins['cutout-animation'].matched.sort(),['home/hero-anim:activity-joy','home/result-anim:activity-joy']);
 await tick(250);const saved=await save('result-250');const ext=saved.value.playerPresentation.extensions;
 assert.equal(ext['cutout-animation'].instances['home/result-anim'].state.tracks[0].current.timeMs,250);
 await click('details');await page.locator('[data-instance="details/late"]').waitFor();await tick(0);d=await describe();assert.equal(track(d,'details/late').clip,'idle','navigation never replays the previous activity result');assert.equal(d.observations,3);
 await load(saved);await tick(0);d=await describe();assert.equal(track(d,'home/result-anim').clip,'joy');assert.equal(track(d,'home/result-anim').timeMs,250);assert.equal(d.observations,3,'load does not replay command observations');
 assert.deepEqual((await save('restored-250')).value.playerPresentation.extensions,ext,'restored plugin snapshot wins over new mount defaults');
 await tick(750);await tick(237);const loop=await save('idle-exact');d=await describe();const live=track(d,'home/result-anim');assert.equal(live.clip,'idle');
 assert.ok(live.timeMs>0,'loop advanced beyond its default start');
 assert.equal(loop.value.playerPresentation.extensions['cutout-animation'].instances['home/result-anim'].state.tracks[0].current.timeMs,live.timeMs,'loop playback saves exactly without presentation variants');
 await tick(321);await load(loop);await tick(0);assert.deepEqual((await save('idle-restored')).value.playerPresentation.extensions,loop.value.playerPresentation.extensions);
 // A single action settles an effect node before React renders its final dialogue.
 // The newly visible dialogue element must still receive the original actionResult.
 await page.reload();await click('start');await click('trigger');await page.locator('.transaction-dialog button').first().click();
 await page.locator('[data-instance="home/dialogue-anim"]').waitFor();await page.waitForFunction(()=>!window.raisingExtensions.presentationPending);await tick(0);
 d=await describe();assert.equal(track(d,'home/dialogue-anim').clip,'joy');assert.equal(track(d,'home/hero-anim').clip,'joy');assert.equal(d.observations,2,'action and automatic effect advance are each observed once');
 assert.equal(d.lastObserve.command.type,'advanceNarrative');
 assert.deepEqual(errors,[]);
});

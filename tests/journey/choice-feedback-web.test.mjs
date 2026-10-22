import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile,mkdtemp,writeFile} from 'node:fs/promises';
import {resolve,extname,sep} from 'node:path';
import {chromium} from 'playwright';
import {actorStoryFixture} from '../native/actor-story-fixture.mjs';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('exported choice cards show authored skins and restore results through browser download/upload',{timeout:90000},async t=>{
 const {built,root}=await actorStoryFixture(t);assert.ok(built.ok);const c=structuredClone(built.snapshot);
 c.attributes.find(a=>a.id==='skill').max=11;
 const e=c.events.find(e=>e.id==='personal-choice');e.trigger='period-start';delete e.scope;
 e.entryNodeId='setup';e.nodes.push({id:'setup',type:'scene',scene:{portraits:[]},next:'ask'});
 e.nodes.find(n=>n.id==='ask').choices.forEach(ch=>{ch.next='reply';ch.consequenceText='A later meeting at the workshop.';for(const effect of ch.effects)effect.owner={kind:'character',characterId:'apprentice'};});
 e.nodes.find(n=>n.id==='ask').choices[0].effects.push({op:'add',target:{kind:'attribute',attributeId:'skill',owner:{kind:'character',characterId:'apprentice'}},value:{op:'literal',value:10}});
 e.nodes.push({id:'reply',type:'dialogue',speakerId:null,text:'The promise is remembered.',next:'end'});
 const b=(id,action,x,y,extra={})=>({id,kind:'button',x,y,width:100,height:48,fontSize:20,action,text:id,...extra});
 c.project.visual={...c.project.visual,width:1280,height:720,routing:{defaultScreenId:'home',useLocationScreens:false,phases:[{phase:'title',screenId:'title'},{phase:'choice',screenId:'story'},{phase:'dialogue',screenId:'story'}]},screens:[
  {id:'title',label:'Title',elements:[b('start','start',20,20)]},
  {id:'home',label:'Home',elements:[]},{id:'schedule',label:'Schedule',elements:[]},{id:'ending',label:'Ending',elements:[]},
  {id:'story',label:'Story',elements:[
   {id:'cards',kind:'choices',x:50,y:60,width:1000,height:300,fontSize:20,choiceLayout:'cards',showChoiceEffects:true,rowHeight:110,gap:14,fill:'#263c36',color:'#fff8e8',borderWidth:2,borderColor:'#b19c72',radius:6},
   {id:'result',kind:'text',x:50,y:400,width:1100,height:150,binding:'choice-result',fontSize:22,color:'#ffffff'},
   b('save','save',50,600),b('load','load',180,600,{text:'불러오기'}),b('next','advance',310,600)
  ]}]};
 const out=await mkdtemp(resolve('.cache/choice-feedback-web-')),destination=resolve(out,'web');
 await exportGame({content:c,projectRoot:root,destination,target:'web',runtimes:{web:resolve('dist/web')}});
 const mime={'.html':'text/html','.js':'text/javascript','.mjs':'text/javascript','.wasm':'application/wasm','.json':'application/json','.css':'text/css'};
 const server=createServer(async(req,res)=>{try{const path=decodeURIComponent(new URL(req.url,'http://127.0.0.1').pathname),file=resolve(destination,'.'+(path==='/'?'/index.html':path));if(!file.startsWith(destination+sep)){res.writeHead(403).end();return;}const data=await readFile(file);res.writeHead(200,{'Content-Type':mime[extname(file)]??'application/octet-stream'});res.end(data);}catch{res.writeHead(404).end();}});
 await new Promise(r=>server.listen(0,'127.0.0.1',r));t.after(()=>{server.closeAllConnections();server.close();});
 const browser=await chromium.launch({channel:process.env.PLAYWRIGHT_CHANNEL??'msedge',headless:true});t.after(()=>browser.close());
 const page=await browser.newPage({viewport:{width:1280,height:800},acceptDownloads:true}),errors=[];page.on('pageerror',e=>errors.push(e.message));
 const el=id=>page.locator(`[data-element-id="${id}"]`),click=id=>el(id).getByRole('button').click();
 const state=async()=>page.evaluate(async()=>(await window.player.view()).view.state);
 await page.goto(`http://127.0.0.1:${server.address().port}`);await click('start');
 await page.getByRole('button',{name:/Return.*A later meeting/}).waitFor();
 assert.equal((await state()).activeNarrative.nodeId,'ask','startup settles scene but stops at choice');
 const card=el('cards').getByRole('button').first();const skin=await card.evaluate(e=>{const s=getComputedStyle(e);return{fill:s.backgroundColor,color:s.color,border:s.borderWidth,radius:s.borderRadius}});
 assert.deepEqual(skin,{fill:'rgb(38, 60, 54)',color:'rgb(255, 248, 232)',border:'2px',radius:'6px'});
 assert.match(await card.innerText(),/01/);assert.match(await card.innerText(),/\+10/);assert.doesNotMatch(await card.innerText(),/personal-promise/);
 const loadSize=await el('load').getByRole('button').evaluate(e=>({height:e.scrollHeight,box:e.clientHeight}));assert.ok(loadSize.height<=loadSize.box,'four Korean characters fit authored button height');
 await page.screenshot({path:resolve(out,'cards.png')});await card.click();
 await page.waitForTimeout(100);
 await writeFile(resolve(out,'after-choice.json'),JSON.stringify({view:await page.evaluate(async()=>(await window.player.view()).view),text:await el('result').innerText(),errors},null,2));
 assert.match(await el('result').innerText(),/10 → 11 \(\+1\)/);
 const expected=await state(),text=await el('result').innerText();
 const pending=page.waitForEvent('download');await click('save');const save=resolve(out,'result.json');await(await pending).saveAs(save);
 const envelope=JSON.parse(await readFile(save,'utf8'));assert.deepEqual(envelope.state,expected);
 await page.reload();const chooser=page.waitForEvent('filechooser');
 // Reload returns to the title; start again only to expose the authored load button.
 await click('start');await click('load');await(await chooser).setFiles(save);
 await page.waitForFunction(()=>document.querySelector('[data-element-id="result"]')?.textContent?.includes('10 → 11 (+1)'));
 assert.deepEqual(await state(),expected);assert.equal(await el('result').innerText(),text);
 await page.screenshot({path:resolve(out,'restored.png')});assert.deepEqual(errors,[]);
 await writeFile(resolve(out,'verification.json'),JSON.stringify({skin,save,restoredExactly:true,errors},null,2));
});

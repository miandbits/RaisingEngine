import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {mkdir,mkdtemp,cp,readFile,writeFile} from 'node:fs/promises';
import {resolve,extname,sep} from 'node:path';
import {chromium} from 'playwright';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('exported web player renders nested scopes, responsive anchors and clips real pointer input', {timeout:120000},async t=>{
 await mkdir('.cache/hierarchy-web',{recursive:true});await mkdir('output/playwright',{recursive:true});const root=await mkdtemp(resolve('.cache/hierarchy-web/run-')),projectRoot=resolve(root,'project'),destination=resolve(root,'web');await cp(resolve('examples/academy-journey'),projectRoot,{recursive:true});
 const file=resolve(projectRoot,'project.json'),p=JSON.parse(await readFile(file,'utf8'));p.visual.viewportMode='responsive';p.visual.components??=[];p.visual.components.push({id:'group-card',label:'Grouped card',width:200,height:100,elements:[{id:'group',kind:'group',x:0,y:0,width:200,height:100,clipChildren:true,anchorX:'stretch'},{id:'choose',parentId:'group',kind:'button',x:10,y:10,width:160,height:40,text:'Disabled row',action:'navigate',screenId:'schedule'}]});
 p.visual.dataTables=[{id:'rows',name:'Rows',columns:[{id:'enabled',name:'Enabled',type:'boolean'}],rows:[{id:'one',values:{enabled:false}}]}];
 p.visual.screens.find(s=>s.id==='home').elements=[
  {id:'outer',kind:'group',x:100,y:100,width:300,height:200,anchorX:'stretch',clipChildren:true,opacity:.5,fill:'#335566'},
  {id:'clipped',parentId:'outer',kind:'button',x:-100,y:20,width:160,height:40,text:'Clipped schedule',action:'navigate',screenId:'schedule'},
  {id:'anchored',parentId:'outer',kind:'button',x:200,y:100,width:80,height:40,anchorX:'right',text:'Anchored',action:'navigate',screenId:'schedule'},
  {id:'hidden',kind:'group',x:800,y:50,width:200,height:100,hidden:true},
  {id:'hidden-button',parentId:'hidden',kind:'button',x:0,y:0,width:160,height:40,text:'Hidden child',action:'sound'},
  {id:'disabled',kind:'group',x:800,y:200,width:200,height:100,enabledCondition:{op:'not',condition:{op:'always'}}},
  {id:'disabled-button',parentId:'disabled',kind:'button',x:0,y:0,width:160,height:40,text:'Disabled child',action:'navigate',screenId:'schedule'},
  {id:'disabled-schedule',parentId:'disabled',kind:'schedule',x:0,y:60,width:200,height:150},
  {id:'rows',kind:'collection',componentId:'group-card',source:{type:'table',tableId:'rows'},x:600,y:400,width:300,height:100,bindings:[{elementId:'group',property:'enabled',field:'enabled'}]},
 ];await writeFile(file,JSON.stringify(p));const built=await buildSnapshot(await loadProject(projectRoot));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));await exportGame({content:built.snapshot,projectRoot,destination,target:'web',runtimes:{web:resolve('dist/web'),windows:resolve('dist/native-player-app')}});
 const mime={'.html':'text/html','.js':'text/javascript','.mjs':'text/javascript','.wasm':'application/wasm','.css':'text/css','.json':'application/json','.png':'image/png'};
 const server=createServer(async(req,res)=>{try{const path=decodeURIComponent(new URL(req.url,'http://127.0.0.1').pathname),file=resolve(destination,`.${path==='/'?'/index.html':path}`);if(!file.startsWith(destination+sep)){res.writeHead(403);res.end();return;}const bytes=await readFile(file);res.writeHead(200,{'Content-Type':mime[extname(file)]??'application/octet-stream'});res.end(bytes);}catch{res.writeHead(404);res.end();}});await new Promise(r=>server.listen(0,'127.0.0.1',r));t.after(()=>{server.closeAllConnections();server.close();});
 const browser=await chromium.launch({channel:'chrome',headless:true});t.after(()=>browser.close());const page=await browser.newPage({viewport:{width:1280,height:800}}),errors=[];page.on('pageerror',e=>errors.push(e.message));await page.goto(`http://127.0.0.1:${server.address().port}`);await page.getByRole('button',{name:'새 게임 시작',exact:true}).click();await page.locator('.visual-stage[data-screen="home"]').waitFor();
 assert.equal(await page.getByRole('button',{name:'Hidden child',exact:true}).count(),0);assert.equal(await page.getByRole('button',{name:'Disabled child',exact:true}).isDisabled(),true);assert.equal(await page.getByRole('button',{name:'Disabled row',exact:true}).isDisabled(),true);for(const select of await page.locator('[data-element-id=disabled-schedule] select').all())assert.equal(await select.isDisabled(),true);
 const clipped=page.locator('[data-element-id="clipped"]');assert.equal(await clipped.evaluate(e=>getComputedStyle(e).opacity),'0.5');assert.match(await clipped.evaluate(e=>getComputedStyle(e).clipPath),/100px/);
 const bounds=await clipped.boundingBox();await page.mouse.click(bounds.x+bounds.width/2,bounds.y+bounds.height/2);assert.equal(await page.locator('.visual-stage').getAttribute('data-screen'),'home');
 await page.setViewportSize({width:1800,height:800});await page.waitForFunction(()=>parseFloat(document.querySelector('[data-element-id="outer"]').style.width)>300);const geometry=await page.evaluate(()=>{const g=document.querySelector('[data-element-id="outer"]'),b=document.querySelector('[data-element-id="anchored"]');return {x:parseFloat(b.style.left),expected:parseFloat(g.style.left)+200+parseFloat(g.style.width)-300};});assert.equal(geometry.x,geometry.expected);
 await page.screenshot({path:resolve('output/playwright/hierarchy-web-wide.png')});await page.getByRole('button',{name:'Anchored',exact:true}).click();await page.locator('.visual-stage[data-screen="schedule"]').waitFor();assert.deepEqual(errors,[]);
 await writeFile(resolve(root,'proof.json'),JSON.stringify({checks:['nested clipping blocks outside click','parent conditions disable buttons','group row binding disables descendant','responsive parent anchors','actual navigation'],geometry,screenshot:resolve('output/playwright/hierarchy-web-wide.png')},null,2));
});

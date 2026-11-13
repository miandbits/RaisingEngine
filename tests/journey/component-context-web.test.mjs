import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {readFile,writeFile,mkdir,mkdtemp} from 'node:fs/promises';
import {resolve,extname,sep} from 'node:path';
import {chromium} from 'playwright';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/index.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';
import {resolveScreenElements} from '../../dist/packages/visual/components.js';
import {widgetCommand} from '../../dist/packages/visual/commands.js';
import {dataBindingText} from '../../dist/packages/visual/data-binding.js';
import {resolvePresentation,presentationActivity} from '../../dist/packages/visual/variants.js';
import {nativeRequest} from '../../dist/packages/native-bridge/index.js';

test('exported component cards: browser/native parity, actual buttons, downloads/uploads and ending', {timeout:120000},async t=>{
  const info=JSON.parse(await readFile(process.env.RAISING_COMPONENT_CONTEXT??new URL('../fixtures/component-context.json',import.meta.url),'utf8'));
  const built=await buildSnapshot(await loadProject(info.root));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));const c=built.snapshot;
  await mkdir('.cache/web-component-context',{recursive:true});await mkdir('output/playwright',{recursive:true});
  const out=await mkdtemp(resolve('.cache/web-component-context/run-')),destination=resolve(out,'web');
  const runtime=resolve(process.env.RAISING_WEB_RUNTIME??'dist/web');
  await exportGame({content:c,projectRoot:info.root,destination,target:'web',runtimes:{web:runtime,windows:''}});
  const mime={'.html':'text/html','.js':'text/javascript','.mjs':'text/javascript','.wasm':'application/wasm','.json':'application/json','.css':'text/css'};
  const server=createServer(async(req,res)=>{try{
    const url=new URL(req.url,'http://127.0.0.1'),file=resolve(destination,`.${url.pathname==='/'?'/index.html':decodeURIComponent(url.pathname)}`);
    if(!file.startsWith(destination+sep)){res.writeHead(403);res.end();return;}
    const body=await readFile(file);res.writeHead(200,{'Content-Type':mime[extname(file)]??'application/octet-stream'});res.end(body);
  }catch{res.writeHead(404);res.end();}});
  await new Promise(r=>server.listen(0,'127.0.0.1',r));t.after(()=>{server.closeAllConnections();server.close();});
  const browser=await chromium.launch({channel:'chrome',headless:process.env.RAISING_HEADED!=='1'});t.after(()=>browser.close());
  const page=await browser.newPage({viewport:{width:1280,height:800},acceptDownloads:true}),errors=[],checkpoints=[];
  page.on('pageerror',error=>errors.push(error.message));page.setDefaultTimeout(10000);
  const url=`http://127.0.0.1:${server.address().port}`;await page.goto(url);await page.locator('.visual-stage').waitFor();
  const elements=resolveScreenElements(c.project.visual,info.screen),cards=elements.filter(e=>e.id.startsWith('instance-'));
  const actions=cards.filter(e=>e.action==='executeActorAction'),assign=cards.filter(e=>e.action==='assignActivity');
  const element=id=>page.locator(`[data-element-id="${id}"]`);
  const button=(action,when)=>elements.find(e=>e.kind==='button'&&e.action===action&&(!when||e.visibleWhen===when)).id;
  const click=id=>element(id).getByRole('button').click();
  let session=createSession(c,42,{mode:'full'}).session;
  const node=()=>{const a=session.state.activeNarrative;return a?(a.kind==='event'?c.events:c.endings).find(e=>e.id===a.id).nodes.find(n=>n.id===a.nodeId):null;};
  const send=command=>{const r=dispatchSession(c,session,command,session.state.stateRevision);assert.equal(r.ok,true,JSON.stringify(r.error));session=r.session;};
  const settle=()=>{for(let i=0;i<100;i++){
    if(session.state.phase==='period-end')send({type:'resolvePeriodEnd'});
    else if(session.state.phase==='narrative'&&!['dialogue','choice'].includes(node()?.type))send({type:'advanceNarrative'});
    else return;
  }throw Error('Unbounded settle');};
  const checkpoint=async label=>{
    // Observe the rendered completion signal; an async predicate can finish the wait before file.text()/load completes.
    await page.waitForFunction(()=>document.querySelector('.visual-player')?.getAttribute('aria-busy')==='false');
    const actual=await page.evaluate(async()=>(await window.player.view()).view);
    assert.equal(actual.runtimeBackend,'wasm');assert.deepEqual(actual.state,session.state,label);
    for(const card of cards){
      const projected=resolvePresentation(card,presentationActivity(session.state,card),condition=>nativeRequest({operation:'evaluate',content:c,state:session.state,condition}).value===true);
      let expected;
      if(card.kind==='text'&&card.binding==='data-value')expected=dataBindingText(card,{stats:c.attributes.map(a=>({...a,value:session.state.attributes[a.id]})),dataState:session.state});
      else if(card.kind==='text'&&card.variants)expected=projected.text;
      else if(card.binding==='context-name'){const rows=card.contextField==='actorId'?c.characters:card.contextField==='actionId'?c.project.gameplay.actions:c.activities;const row=rows.find(x=>x.id===card.uiContext[card.contextField]);expected=row.name??row.label;}
      if(expected!==undefined)assert.equal(await element(card.id).innerText(),expected,label+'/'+card.id);
    }
    assert.equal(await page.locator('[role="alert"]').count(),0);checkpoints.push({label,revision:actual.state.stateRevision,phase:actual.state.phase});
  };
  const download=async name=>{
    const pending=page.waitForEvent('download');await click(button('save'));const file=resolve(out,name+'.json');await (await pending).saveAs(file);
    const decoded=await decodeSave(c,await readFile(file,'utf8'));assert.equal(decoded.ok,true,JSON.stringify(decoded.error));assert.deepEqual(decoded.session.state,session.state);return file;
  };
  const upload=async(file,saved,label)=>{const chooser=page.waitForEvent('filechooser');await click(button('load'));await (await chooser).setFiles(file);session=structuredClone(saved);await checkpoint(label);};
  t.after(async()=>{if(!page.isClosed())await page.screenshot({path:resolve('output/playwright/component-context-web-final.png')});await writeFile(resolve(out,'report.json'),JSON.stringify({destination,checkpoints,errors},null,2));});
  for(const card of assign)assert.equal(await element(card.id).getByRole('button').isDisabled(),true);
  await click(button('start','title'));await checkpoint('initial');
  assert.equal(await element(actions[0].id).getByRole('button').isDisabled(),true);assert.equal(await element(actions[1].id).getByRole('button').isDisabled(),false);
  await page.screenshot({path:resolve('output/playwright/component-context-web-initial.png')});
  await click(button('advance'));send({type:'assignActivities',assignments:{...session.state.roster.assignments}});await checkpoint('default-assignment');
  await page.getByRole('combobox',{name:'소운 활동',exact:true}).selectOption('rest');
  assert.equal(await page.getByRole('button',{name:'배정한 활동 함께 실행',exact:true}).isDisabled(),true);
  const pendingAssignments={...session.state.roster.assignments,[info.actors[0]]:'rest'};
  await page.getByRole('button',{name:'활동 배정 적용',exact:true}).click();send({type:'assignActivities',assignments:pendingAssignments});await checkpoint('manual-assignment');
  const overflow=await page.locator('[data-element-id="schedule-1"]').evaluate(e=>({height:e.clientHeight,content:e.scrollHeight,inner:e.firstElementChild.scrollHeight}));
  assert.ok(overflow.content<=overflow.height+1&&overflow.inner<=overflow.height+1,JSON.stringify(overflow));
  await click(actions[1].id);send(widgetCommand(actions[1],session.state));await checkpoint('second-action');
  const middle=structuredClone(session),middleFile=await download('middle');
  await click(actions[1].id);send(widgetCommand(actions[1],session.state));await checkpoint('second-again');
  await upload(middleFile,middle,'middle-restored');await page.reload();await page.locator('.visual-stage').waitFor();await upload(middleFile,middle,'reload-middle');
  // The standard save format is interchangeable with native, not a browser-only state blob.
  const nativeFile=resolve(out,'native-middle.json'),encoded=await encodeSave(c,middle);assert.equal(encoded.ok,true);await writeFile(nativeFile,encoded.text);
  await upload(nativeFile,middle,'native-save-import');
  let acted=false,choices=0;
  for(let i=0;session.state.phase!=='finished'&&i<120;i++){
    if(['planning','ready'].includes(session.state.phase)){
      if(!acted&&session.state.periodIndex>0){assert.equal(await element(actions[0].id).getByRole('button').isDisabled(),false);await click(actions[0].id);send(widgetCommand(actions[0],session.state));acted=true;await checkpoint('first-action');}
      for(const card of assign){await click(card.id);send(widgetCommand(card,session.state));await checkpoint('assigned-'+session.state.periodIndex+'-'+card.actorId);}
      for(const card of assign)assert.equal(await page.getByRole('combobox',{name:`${c.characters.find(a=>a.id===card.actorId).name} 활동`,exact:true}).inputValue(),card.activityId);
      await page.getByRole('button',{name:'배정한 활동 함께 실행',exact:true}).click();send({type:'executeActivity'});settle();await checkpoint('result-'+session.state.periodIndex);
      if(session.state.periodIndex===0){assert.match(await element('result-1').innerText(),/소운/);assert.match(await element('result-1').innerText(),/연화/);await page.screenshot({path:resolve('output/playwright/component-context-web-result.png')});}
    }else if(session.state.phase==='activity-result'){
      if(session.state.periodIndex===0){const saved=structuredClone(session),file=await download('result');await click(button('advance'));send({type:'acknowledgeResult'});settle();await checkpoint('past-result');await upload(file,saved,'result-restored');}
      await click(button('advance'));send({type:'acknowledgeResult'});settle();await checkpoint('advanced-'+session.state.periodIndex);
    }else if(session.state.phase==='narrative'){
      if(node().type==='choice'){const choice=node().choices[0];await page.getByRole('button',{name:choice.text,exact:true}).click();send({type:'choose',choiceId:choice.id});++choices;}
      else {await click(button('advance'));send({type:'advanceNarrative'});}
      settle();await checkpoint('narrative-'+i);
    }else throw Error(session.state.phase);
  }
  assert.equal(session.state.phase,'finished');assert.equal(session.state.endingId,'certified');assert.ok(acted&&choices>0);
  const ending=structuredClone(session),endingFile=await download('ending');await click(button('start','finished'));
  // Restart is asynchronous. Wait for the new run before checking a load against the old ending revision.
  await page.waitForFunction(()=>document.querySelector('.visual-player')?.getAttribute('aria-busy')==='false');
  assert.equal((await page.evaluate(async()=>(await window.player.view()).view.state)).stateRevision,0);
  await upload(endingFile,ending,'ending-restored');
  await page.reload();await page.locator('.visual-stage').waitFor();await upload(endingFile,ending,'reload-ending');assert.deepEqual(errors,[]);
  await writeFile(resolve('.cache/web-component-context/evidence.json'),JSON.stringify({out,destination,checkpoints:checkpoints.length,ending:session.state.endingId,errors},null,2));
  console.log(`Browser/native component context evidence: ${out}`);
});

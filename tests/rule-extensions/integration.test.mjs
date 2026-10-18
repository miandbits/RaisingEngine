import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir,mkdtemp} from 'node:fs/promises';
import {resolve,sep,extname} from 'node:path';
import {spawnSync} from 'node:child_process';
import {createServer} from 'node:http';
import {chromium} from 'playwright';
import {baseProject,json} from './helpers.mjs';
import {StudioEditor} from '../../dist/packages/studio-service/editor.js';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/index.js';
import {encodeSave,decodeSave} from '../../dist/packages/runtime-service/save.js';
import {runGoalRoute} from '../../dist/packages/analysis-service/goal-routes.js';

test('offscreen Qt authoring → shared save → hidden Windows and headless Wasm → regression',{timeout:240000},async t=>{
  const root=await baseProject();await mkdir('.cache/rule-extension-verification',{recursive:true});const out=await mkdtemp(resolve('.cache/rule-extension-verification/run-'));
  const qt=resolve(process.env.RULE_QT_OUT??'dist/qt-studio-rule-extensions');
  const env={...process.env,QT_QPA_PLATFORM:'offscreen',QT_QPA_PLATFORM_PLUGIN_PATH:'C:/Qt/6.10.1/mingw_64/plugins/platforms',PATH:`C:/Qt/6.10.1/mingw_64/bin;C:/Qt/Tools/mingw1310_64/bin;${process.env.PATH}`};
  const author=spawnSync(resolve(qt,'qt-rule-extensions-dialog-test.exe'),[root,resolve('examples/rule-extension-lab/dojo.raiserules'),out],{env,windowsHide:true,encoding:'utf8',timeout:60000});
  assert.equal(author.status,0,author.stdout+'\n'+author.stderr);
  const authored=await json(resolve(out,'authored-project.json'));
  const editor=new StudioEditor();await editor.open(root);editor.edit({type:'replace',file:'project.json',data:authored},editor.revision);
  const validation=await editor.validate(editor.revision);assert.equal(validation.view.diagnostics.filter(d=>d.severity==='error').length,0,JSON.stringify(validation.view.diagnostics));
  await editor.save(editor.revision);await editor.open(root);
  const built=await buildSnapshot(await loadProject(root));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));const content=built.snapshot;
  const windows=resolve(out,'windows'),web=resolve(out,'web');const runtimes={windows:resolve('dist/native-player-authoring-preview'),web:resolve('dist/web')};
  await exportGame({content,projectRoot:root,destination:windows,target:'windows',runtimes});
  await exportGame({content,projectRoot:root,destination:web,target:'web',runtimes});
  const created=createSession(content,42,{mode:'full'});assert.equal(created.ok,true);let session=created.session;
  const commands=[],steps=[{click:'start'}],expected={},choiceLabels={};
  const settle=()=>{for(let i=0;i<50;i++){
    const a=session.state.activeNarrative,node=a?(a.kind==='event'?content.events:content.endings).find(x=>x.id===a.id).nodes.find(n=>n.id===a.nodeId):null;
    if(session.state.phase==='period-end')send({type:'resolvePeriodEnd'});
    else if(session.state.phase==='narrative'&&!['dialogue','choice'].includes(node.type))send({type:'advanceNarrative'});
    else return;
  }throw Error('unbounded settle');};
  const send=c=>{const r=dispatchSession(content,session,c,session.state.stateRevision);assert.equal(r.ok,true,JSON.stringify(r.error));session=r.session;commands.push(c);};
  const check=id=>{expected[id]=structuredClone(session.state);steps.push({checkpoint:id});};
  check('start');
  steps.push({click:'action__rule-actions-flow-1__rx-flow-1-prepare'},{click:'cancel-purchase'});check('cancelled');
  const act=name=>{send({type:'executeAction',actionId:'rx-flow-1-'+name,quantity:1});steps.push({click:'action__rule-actions-flow-1__rx-flow-1-'+name},{click:'confirm-purchase'});check(name);};
  const unavailable=dispatchSession(content,session,{type:'executeAction',actionId:'rx-flow-1-complete',quantity:1},session.state.stateRevision);assert.equal(unavailable.ok,false);
  act('prepare');act('practice');
  steps.push({key:'F5'});const saved=structuredClone(session);check('saved');act('complete');
  steps.push({key:'F9'});session=saved;check('restored');act('complete');
  assert.equal(session.state.attributes.skill,12);assert.equal(session.state.attributes.money,25);assert.equal(session.state.attributes.stress,2);
  // An ordinary action uses the condition and effect configured in real Qt widgets.
  send({type:'executeAction',actionId:'celebrate',quantity:1});
  // Keep actual player commands aligned: expose the authored action through a normal button.
  steps.push({click:'celebrate'},{click:'confirm-purchase'});check('celebrate');assert.equal(session.state.attributes.skill,16);
  while(session.state.phase!=='finished') {
    const phase=session.state.phase;
    if(phase==='planning'){const ids=['rest','rest','rest'];send({type:'setSchedule',activityIds:ids});steps.push(...ids.map((value,i)=>({select:'plan-'+i,value})),{click:'advance'});}
    else if(phase==='ready'){send({type:'executeActivity'});steps.push({click:'advance'});}
    else if(phase==='activity-result'){send({type:'acknowledgeResult'});steps.push({click:'advance'});}
    else if(phase==='narrative'){const a=session.state.activeNarrative;const node=(a.kind==='event'?content.events:content.endings).find(x=>x.id===a.id).nodes.find(n=>n.id===a.nodeId);if(node.type==='choice'){const choice=node.choices[0];choiceLabels[choice.id]=choice.text;send({type:'choose',choiceId:choice.id});steps.push({click:'choose-'+choice.id});}else{send({type:'advanceNarrative'});steps.push({click:'advance'});}}
    else throw Error('unexpected '+phase);
    settle();if(commands.length>120)throw Error('route did not finish');
  }
  check('ending');steps.push({key:'F5'},{screenshot:resolve(out,'windows.png')});
  // Save/load restored the earlier branch: discard its abandoned complete command.
  const routeCommands=commands.filter((c,i)=>!(i===3&&c.actionId==='rx-flow-1-complete'));
  const route={version:1,id:'dojo-breakthrough',name:'수련 돌파와 엔딩',gameId:content.project.gameId,recordedFingerprint:content.contentFingerprint,origin:created.session.origin,commands:routeCommands,endingId:session.state.endingId,checkpoints:[2,3],checkpointEvery:3};
  const regression=await runGoalRoute(content,route);assert.equal(regression.ok,true,JSON.stringify(regression));
  const changed=await loadProject(root);changed.documents.find(d=>d.kind==='project').data.ruleExtensions.progressions[0].args.gain=1;
  const changedSnapshot=await buildSnapshot(changed);assert.equal(changedSnapshot.ok,true,JSON.stringify(changedSnapshot.diagnostics));
  const caught=await runGoalRoute(changedSnapshot.snapshot,route);assert.equal(caught.ok,false);assert.equal(caught.failure.command.actionId,'rx-flow-1-complete','content change blocks the recorded breakthrough');
  const save=await encodeSave(content,session);assert.equal(save.ok,true);const load=await decodeSave(content,save.text);assert.equal(load.ok,true);assert.deepEqual(load.session,session);
  const script=resolve(out,'steps.json'),report=resolve(out,'native.json'),nativeSave=resolve(out,'native-save.json');await writeFile(script,JSON.stringify(steps));
  const native=spawnSync(resolve(windows,'RaisingPlayer.exe'),['--bundle',windows,'--seed','42','--save',nativeSave,'--automation',script,'--report',report,'--hidden'],{windowsHide:true,encoding:'utf8',timeout:90000});
  const result=await json(report);assert.equal(native.status,0,JSON.stringify(result.error??native.stderr));assert.equal(result.ok,true,JSON.stringify(result));
  for(const s of result.steps)if(s.command.checkpoint)assert.deepEqual(s.ui.state,expected[s.command.checkpoint],s.command.checkpoint);
  const freshSteps=resolve(out,'fresh.json'),freshReport=resolve(out,'fresh-native.json');await writeFile(freshSteps,JSON.stringify([{key:'F9'},{checkpoint:'ending'}]));
  const fresh=spawnSync(resolve(windows,'RaisingPlayer.exe'),['--bundle',windows,'--save',nativeSave,'--automation',freshSteps,'--report',freshReport,'--hidden'],{windowsHide:true,encoding:'utf8',timeout:30000});assert.equal(fresh.status,0,fresh.stderr);assert.deepEqual((await json(freshReport)).steps.find(s=>s.command.checkpoint).ui.state,expected.ending);
  const mime={'.html':'text/html','.js':'application/javascript','.mjs':'application/javascript','.wasm':'application/wasm','.json':'application/json','.css':'text/css'};
  const server=createServer(async(req,res)=>{try{const u=new URL(req.url,'http://localhost'),file=resolve(web,`.${u.pathname==='/'?'/index.html':decodeURIComponent(u.pathname)}`);if(!file.startsWith(web+sep)){res.writeHead(403);res.end();return;}res.setHeader('Content-Type',mime[extname(file)]??'application/octet-stream');res.end(await readFile(file));}catch{res.writeHead(404);res.end();}});
  await new Promise(r=>server.listen(0,'127.0.0.1',r));t.after(()=>{server.closeAllConnections();server.close();});
  const browser=await chromium.launch({channel:'chrome',headless:true});t.after(()=>browser.close());const page=await browser.newPage({viewport:{width:1280,height:800},acceptDownloads:true});const errors=[];page.on('pageerror',e=>errors.push(e.message));page.setDefaultTimeout(10000);await page.goto(`http://127.0.0.1:${server.address().port}`);await page.locator('.visual-stage').waitFor();
  let webSave='';const labels={prepare:'수련 준비',practice:'호흡 수련',complete:'경지 돌파',restart:'다음 수련 준비'};
  const webCheck=async state=>{await page.waitForFunction(()=>document.querySelector('.visual-status')?.textContent?.includes('플레이 중'));const view=(await page.evaluate(()=>window.player.view())).view;assert.equal(view.runtimeBackend,'wasm');assert.deepEqual(view.state,state);};
  for(const step of steps) {
    if(step.select)await page.locator('[data-element-id="schedule"] select').nth(Number(step.select.slice(5))).selectOption(step.value);
    if(step.click){if(step.click.startsWith('action__'))await page.getByRole('button',{name:labels[step.click.split('-').at(-1)],exact:true}).click();else if(step.click==='confirm-purchase')await page.getByRole('button',{name:'확인',exact:true}).click();else if(step.click==='cancel-purchase')await page.getByRole('button',{name:'취소',exact:true}).click();else if(step.click.startsWith('choose-'))await page.getByRole('button',{name:choiceLabels[step.click.slice(7)],exact:true}).click();else await page.locator(`[data-element-id="${step.click}"]`).getByRole('button').click();}
    if(step.key==='F5'){const download=page.waitForEvent('download');await page.getByRole('button',{name:'저장',exact:true}).click();webSave=resolve(out,'web-save-'+Date.now()+'.json');await(await download).saveAs(webSave);}
    if(step.key==='F9'){const chooser=page.waitForEvent('filechooser');await page.getByRole('button',{name:'불러오기',exact:true}).click();await(await chooser).setFiles(webSave);}
    if(step.checkpoint)await webCheck(expected[step.checkpoint]);
  }
  await page.screenshot({path:resolve(out,'web.png')});await page.reload();await page.locator('.visual-stage').waitFor();const chooser=page.waitForEvent('filechooser');await page.getByRole('button',{name:'불러오기',exact:true}).click();await(await chooser).setFiles(webSave);await webCheck(expected.ending);assert.deepEqual(errors,[]);
  await writeFile(resolve(out,'goal-route.json'),JSON.stringify(route,null,2));
  const evidence={ok:true,root,out,qt:author.stdout.trim(),windows,web,nativeCheckpoints:result.steps.filter(s=>s.command.checkpoint).length,regression,changedContentRegression:caught,webErrors:errors,backgroundOnly:true};await writeFile(resolve(out,'verification.json'),JSON.stringify(evidence,null,2));await writeFile('.cache/rule-extension-verification/latest.json',JSON.stringify(evidence,null,2));
});

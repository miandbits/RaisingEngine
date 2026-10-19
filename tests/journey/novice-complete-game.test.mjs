import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir,mkdtemp} from 'node:fs/promises';
import {resolve,sep,extname} from 'node:path';
import {spawnSync} from 'node:child_process';
import {createServer} from 'node:http';
import {chromium} from 'playwright';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {createSession,dispatchSession} from '../../dist/packages/runtime-service/index.js';
import {decodeSave} from '../../dist/packages/runtime-service/save.js';
import {resolveScreenElements} from '../../dist/packages/visual/components.js';

// Content is copied unchanged from the independent creator's GUI-authored project.
// These commands operate real player controls; the parallel session is only an oracle.
function scenario(content,accept) {
  const flow=content.project.authoring.storyFlows[0];
  const branch=flow.choices.find(c=>accept?c.delta>0:c.delta<0);
  const plans=accept
    ?[[flow.activityId,'study','rest'],['study','study','rest'],['rest','rest','rest']]
    :[[flow.activityId,'rest','rest'],['rest','rest','rest'],['rest','rest','rest']];
  let session=createSession(content,42,{mode:'full'}).session;
  const steps=[{checkpoint:'title'},{click:'start'}],expected={},commands=[],choiceLabels={};
  const node=()=>{const a=session.state.activeNarrative;return a?(a.kind==='event'?content.events:content.endings).find(n=>n.id===a.id).nodes.find(n=>n.id===a.nodeId):null;};
  const send=command=>{const r=dispatchSession(content,session,command,session.state.stateRevision);assert.equal(r.ok,true,JSON.stringify(r.error));session=r.session;commands.push(command);};
  const settle=()=>{for(let i=0;i<100;i++){
    if(session.state.phase==='period-end')send({type:'resolvePeriodEnd'});
    else if(session.state.phase==='narrative'&&!['dialogue','choice'].includes(node()?.type))send({type:'advanceNarrative'});
    else return;
  }throw Error('Narrative did not settle');};
  const checkpoint=label=>{steps.push({checkpoint:label});expected[label]=structuredClone(session.state);};
  const act=(command,ui)=>{steps.push(...ui);send(command);settle();};
  let narratives=0;
  const story=()=>{while(session.state.phase==='narrative'){
    assert.ok(++narratives<100,'bounded story');
    if(node().type==='choice'){
      const choice=node().choices.find(c=>c.id===branch.id)??node().choices[0];
      choiceLabels[choice.id]=choice.text;
      act({type:'choose',choiceId:choice.id},[{click:'choose-'+choice.id}]);
    }else act({type:'advanceNarrative'},[{click:'advance'}]);
    checkpoint('story-'+narratives);
  }};
  checkpoint('start');
  for(let month=0;month<3;month++){
    assert.equal(session.state.phase,'planning');
    act({type:'setSchedule',activityIds:plans[month]},[...plans[month].map((value,i)=>({select:'plan-'+i,value})),{click:'advance'}]);
    checkpoint('plan-'+month);
    for(let period=0;period<3;period++){
      act({type:'executeActivity'},[{click:'advance'}]);checkpoint(`result-${month}-${period}`);
      if(month===1&&period===0){
        const saved=structuredClone(session);steps.push({key:'F5'});checkpoint('middle-saved');
        act({type:'acknowledgeResult'},[{click:'advance'}]);checkpoint('past-middle');
        steps.push({key:'F9'});session=saved;checkpoint('middle-restored');
      }
      act({type:'acknowledgeResult'},[{click:'advance'}]);story();checkpoint(`period-${month}-${period}`);
    }
  }
  assert.equal(session.state.phase,'finished');
  assert.equal(session.state.endingId,accept?'certified':'ordinary');
  assert.equal(session.state.relationships[flow.characterId][flow.relationshipId],accept?20:-10);
  const followup=`${flow.id}-${flow.choices.find(c=>c.followupEnabled).id}-next`;
  assert.equal(session.state.completedEvents.includes(followup),accept,'follow-up only after the experienced accepting branch');
  assert.ok(commands.some(c=>c.type==='choose'&&c.choiceId===branch.id),'authored branch exercised');
  assert.deepEqual(expected['middle-saved'],expected['middle-restored']);
  checkpoint('ending');steps.push({key:'F5'});
  return {id:accept?'trust-and-survive':'refuse-and-farewell',steps,expected,choiceLabels,commands,ending:session.state.endingId};
}

test('novice GUI game: both endings, Windows/web controls, middle and fresh ending restoration',{timeout:240000},async t=>{
  const project=resolve(process.env.RAISING_NOVICE_PROJECT??'tests/fixtures/projects/novice-three-month-trial');
  const built=await buildSnapshot(await loadProject(project));assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const content=built.snapshot;assert.equal(content.project.durationMonths,3);assert.equal(content.project.periodsPerMonth,3);
  const named=Object.fromEntries(content.project.visual.screens.map(screen=>[screen.id,resolveScreenElements(content.project.visual,screen.id).filter(e=>e.kind==='text'&&e.binding==='context-name').map(e=>{
    const id=e.uiContext?.[e.contextField]??(e.contextField==='actorId'?content.project.protagonistId:'');
    const definition=(e.contextField==='activityId'?content.activities:content.characters).find(x=>x.id===id);
    assert.ok(definition,'authored name binding has a target');return {id:e.id,text:definition.name??definition.label};
  })]));
  await mkdir('.cache/novice-game-verification',{recursive:true});
  const out=await mkdtemp(resolve('.cache/novice-game-verification/run-'));
  const cases=[scenario(content,true),scenario(content,false)],results=[];
  const runtimes={windows:resolve(process.env.RAISING_NOVICE_RUNTIME??'dist/native-player-authoring-preview'),web:resolve('dist/web')};
  const windows=resolve(out,'windows'),web=resolve(out,'web');
  await exportGame({content,projectRoot:project,destination:windows,target:'windows',runtimes});
  await exportGame({content,projectRoot:project,destination:web,target:'web',runtimes});
  for(const route of cases){
    const script=resolve(out,route.id+'-steps.json'),report=resolve(out,route.id+'-native.json'),save=resolve(out,route.id+'-native-save.json');
    await writeFile(script,JSON.stringify([...route.steps,{screenshot:resolve(out,route.id+'-native.png')}]));
    const run=spawnSync(resolve(windows,'RaisingPlayer.exe'),['--bundle',windows,'--seed','42','--save',save,'--automation',script,'--report',report,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:90000});
    const result=JSON.parse(await readFile(report,'utf8'));assert.equal(run.status,0,JSON.stringify(result.error??run.stderr));assert.equal(result.ok,true);
    for(const step of result.steps)if(step.command.checkpoint)for(const name of named[step.ui.screenId]??[])assert.equal(step.ui.texts[name.id],name.text,'native bound name '+name.id);
    for(const [id,state] of Object.entries(route.expected))assert.deepEqual(result.steps.find(s=>s.command.checkpoint===id).ui.state,state,route.id+'/'+id);
    const freshSteps=resolve(out,route.id+'-fresh-steps.json'),freshReport=resolve(out,route.id+'-fresh-native.json');
    await writeFile(freshSteps,JSON.stringify([{key:'F9'},{checkpoint:'fresh'}]));
    const fresh=spawnSync(resolve(windows,'RaisingPlayer.exe'),['--bundle',windows,'--save',save,'--automation',freshSteps,'--report',freshReport,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:30000});
    assert.equal(fresh.status,0,fresh.stderr);const restored=JSON.parse(await readFile(freshReport,'utf8'));
    assert.deepEqual(restored.steps.find(s=>s.command.checkpoint==='fresh').ui.state,route.expected.ending);
    results.push({route:route.id,ending:route.ending,nativeCheckpoints:Object.keys(route.expected).length,nativeFreshEnding:true});
  }
  const mime={'.html':'text/html','.js':'application/javascript','.mjs':'application/javascript','.wasm':'application/wasm','.json':'application/json','.css':'text/css','.svg':'image/svg+xml'};
  const server=createServer(async(req,res)=>{try{
    const url=new URL(req.url,'http://127.0.0.1'),file=resolve(web,`.${url.pathname==='/'?'/index.html':decodeURIComponent(url.pathname)}`);
    if(!file.startsWith(web+sep)){res.writeHead(403);res.end();return;}
    const body=await readFile(file);res.writeHead(200,{'Content-Type':mime[extname(file)]??'application/octet-stream'});res.end(body);
  }catch{res.writeHead(404);res.end();}});
  await new Promise(r=>server.listen(0,'127.0.0.1',r));t.after(()=>{server.closeAllConnections();server.close();});
  const browser=await chromium.launch({channel:'chrome',headless:process.env.RAISING_HEADED!=='1'});t.after(()=>browser.close());
  for(const [index,route] of cases.entries()){
    const page=await browser.newPage({viewport:{width:1280,height:800},acceptDownloads:true}),errors=[];page.setDefaultTimeout(10000);page.on('pageerror',e=>errors.push(e.message));
    await page.goto(`http://127.0.0.1:${server.address().port}`);await page.locator('.visual-stage').waitFor();
    let savedFile='',saves=0,checkpoints=0;
    const idle=()=>page.waitForFunction(()=>document.querySelector('.visual-status')?.textContent?.includes('플레이 중'));
    const check=async(state,label)=>{await idle();const actual=await page.evaluate(async()=>(await window.player.view()).view);assert.equal(actual.runtimeBackend,'wasm');assert.deepEqual(actual.state,state,route.id+'/'+label);assert.equal(await page.locator('[role="alert"]').count(),0);++checkpoints;};
    for(const step of route.steps){
      if(step.select)await page.locator('[data-element-id="schedule"] select').nth(Number(step.select.slice(5))).selectOption(step.value);
      if(step.click){
        if(step.click.startsWith('choose-'))await page.getByRole('button',{name:route.choiceLabels[step.click.slice(7)],exact:true}).click();
        else await page.locator(`[data-element-id="${step.click}"]`).getByRole('button').click();
      }
      if(step.key==='F5'){
        const pending=page.waitForEvent('download');await page.getByRole('button',{name:'저장',exact:true}).click();
        savedFile=resolve(out,`${route.id}-web-save-${++saves}.json`);await (await pending).saveAs(savedFile);
        const decoded=await decodeSave(content,await readFile(savedFile,'utf8'));assert.equal(decoded.ok,true,JSON.stringify(decoded.error));
        await check(decoded.session.state,'download-'+saves);
      }
      if(step.key==='F9'){
        const chooser=page.waitForEvent('filechooser');await page.getByRole('button',{name:'불러오기',exact:true}).click();await (await chooser).setFiles(savedFile);
      }
      if(step.checkpoint){
        if(step.checkpoint!=='title')await check(route.expected[step.checkpoint],step.checkpoint);
        for(const name of Object.values(named).flat()){
          const element=page.locator(`[data-element-id="${name.id}"]`);if(await element.count())assert.equal(await element.innerText(),name.text,'web bound name '+name.id);
        }
      }
    }
    await page.screenshot({path:resolve(out,route.id+'-web.png')});
    await page.reload();await page.locator('.visual-stage').waitFor();
    const chooser=page.waitForEvent('filechooser');await page.getByRole('button',{name:'불러오기',exact:true}).click();await (await chooser).setFiles(savedFile);
    await check(route.expected.ending,'fresh-ending');assert.deepEqual(errors,[]);
    Object.assign(results[index],{webCheckpoints:checkpoints,webFreshEnding:true,errors});await page.close();
  }
  const evidence={ok:true,project,windows,web,results};await writeFile(resolve(out,'verification.json'),JSON.stringify(evidence,null,2));
  await writeFile(resolve('.cache/novice-game-verification/latest.json'),JSON.stringify({...evidence,out},null,2));
});

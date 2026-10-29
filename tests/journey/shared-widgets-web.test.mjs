import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve,join,extname,sep} from 'node:path';
import {chromium} from 'playwright';
import {sharedWidgetsFixture,sharedWidgetIds as ids} from '../fixtures/shared-widgets.mjs';
import {loadProject,buildSnapshot} from '../../dist/packages/project-service/index.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('exported shared widgets retain contextual planning, forecast, growth and portable saves on desktop and phone', {timeout:180000}, async t => {
  await mkdir('.cache/shared-widgets/web',{recursive:true});
  const out=await mkdtemp(resolve('.cache/shared-widgets/web/run-'));
  const fixture=await sharedWidgetsFixture(join(out,'project'));
  // Synthetic fixture authoring is separate from the editor-only private-game acceptance.
  const projectFile=join(fixture.root,'project.json');
  const project=JSON.parse(await readFile(projectFile,'utf8'));
  const title=project.visual.screens.find(s=>s.id==='title');
  title.elements.push({id:'restore',kind:'button',action:'load',text:'Restore saved game',x:280,y:640,width:240,height:60});
  title.readableLayout.actions.push('restore');
  await writeFile(projectFile,JSON.stringify(project));
  const built=await buildSnapshot(await loadProject(fixture.root));
  assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const web=join(out,'web');
  await exportGame({content:built.snapshot,projectRoot:fixture.root,target:'web',destination:web,runtimes:{web:resolve('dist/web')}});
  const server=createServer(async(req,res)=>{
    try {
      const name=new URL(req.url,'http://localhost').pathname;
      const file=resolve(web,'.'+(name==='/'?'/index.html':name));
      if(!file.startsWith(web+sep))throw Error('path');
      res.setHeader('Content-Type',({'.html':'text/html','.js':'text/javascript','.mjs':'text/javascript','.css':'text/css','.json':'application/json','.wasm':'application/wasm'})[extname(file)]??'application/octet-stream');
      res.end(await readFile(file));
    } catch {res.writeHead(404);res.end();}
  });
  await new Promise(r=>server.listen(0,'127.0.0.1',r));
  t.after(()=>{server.closeAllConnections();server.close();});
  const browser=await chromium.launch({headless:true,channel:process.env.RAISING_BROWSER_CHANNEL??process.env.PLAYWRIGHT_CHANNEL??'chromium'});
  t.after(()=>browser.close());
  const cases=[];
  for(const phone of [false,true]) {
    const context=await browser.newContext({viewport:phone?{width:390,height:844}:{width:1280,height:800},hasTouch:phone,acceptDownloads:true});
    if(phone)await context.addInitScript(()=>localStorage.setItem('raising-player-preferences-v1',JSON.stringify({readingLayout:'auto',textScale:2})));
    const page=await context.newPage(),errors=[];
    page.on('pageerror',error=>errors.push(error.message));
    page.setDefaultTimeout(15000);
    const name=phone?'phone':'desktop';
    const action=id=>page.locator(`[data-element-id="${id}"] button`);
    const control=suffix=>page.locator(`[id="${ids.schedule}__board-${suffix}"]`);
    const idle=()=>page.waitForFunction(()=>document.querySelector('.visual-player')?.getAttribute('aria-busy')==='false');
    const snapshot=()=>page.evaluate(()=>({state:window.player.session.state,commands:window.player.session.commands}));
    try {
      await page.goto('http://127.0.0.1:'+server.address().port);
      await action('start').waitFor();await idle();await action('start').click();await idle();
      const initial=await snapshot();
      await action('open-planner').click();await idle();
      const guide=page.locator('[data-reading-id="engine-planning-guide-skip"]');
      await guide.waitFor();await guide.click();
      await page.waitForFunction(id=>document.getElementById(id)?.getAttribute('aria-pressed')==='true',ids.schedule+'__board-activity-study');
      assert.equal(await control('activity-study').getAttribute('aria-pressed'),'true','Nested contextual target selects the requested activity');
      const entered=await snapshot();
      assert.deepEqual(entered.commands,[...initial.commands,{type:'navigate',screenId:'schedule'}]);
      assert.deepEqual(entered.state,{...initial.state,stateRevision:initial.state.stateRevision+1,uiScreenId:'schedule'},'Context entry records only navigation, without advancing time or resources');
      await control('cancel').click();
      const plan=['study','rest','study','rest'];
      for(const [slot,activity] of plan.entries()) {
        await control('slot-'+slot).click();await control('activity-'+activity).click();await control('apply').click();
      }
      assert.deepEqual(await snapshot(),entered,'All four shared-widget Apply actions edit only the draft');
      await page.locator(`[data-reading-id="phase-forecast-${ids.schedule}"]`).click();
      await page.getByText('End of this phase',{exact:false}).first().waitFor();
      assert.deepEqual(await snapshot(),entered,'Forecast is read-only');
      await page.locator('[data-reading-id="engine-phase-forecast-close"]').click();
      await page.screenshot({path:join(out,name+'-plan.png')});
      await action('advance').click();await idle();
      // This fixture uses continuous playback: confirming the plan schedules
      // its first execution. A second click can wait through the presentation
      // and acknowledge that result instead of starting the first activity.
      await page.waitForFunction(()=>window.player.session.state.phase==='activity-result');
      const growth=page.locator(`[data-element-id="${ids.result}"] [data-growth-summary="activity"]`);
      await growth.waitFor();await idle();
      await page.locator('[data-testid="timeline-overlay"]').waitFor({state:'detached'});
      const result=await snapshot();
      assert.equal(result.state.phase,'activity-result');
      assert.equal(result.state.periodIndex,0);
      assert.equal(result.state.lastActivityResult.activityId,'study');
      assert.deepEqual(result.commands,[...entered.commands,{type:'setSchedule',activityIds:plan},{type:'executeActivity'}],'Continuous confirmation executes exactly one activity without acknowledging its result');
      assert.equal(result.state.attributes.skill,10);
      assert.match(await growth.innerText(),/\+10/);
      assert.equal(await page.locator(`[data-element-id="${ids.stats}"] [data-testid="stat-skill"]`).innerText(),'10');
      assert.equal(await page.locator('[data-testid="timeline-overlay"]').count(),0);
      // Outlast both schedule-driver delays (150/700 ms). Reveal-result must
      // hold the same canonical result until the player's explicit Continue.
      await page.waitForTimeout(850);
      assert.deepEqual(await snapshot(),result,'Shared growth review remains stable during continuous playback');
      assert.match(await growth.innerText(),/\+10/);
      const download=page.waitForEvent('download');await action('save').click();
      const save=join(out,name+'-save.json');await(await download).saveAs(save);
      await page.screenshot({path:join(out,name+'-growth.png')});
      await page.reload();await action('restore').waitFor();await idle();
      const choose=page.waitForEvent('filechooser');await action('restore').click();await(await choose).setFiles(save);
      await growth.waitFor();await idle();
      assert.deepEqual(await snapshot(),result,'A fresh page restores canonical state and history under shared widgets');
      assert.equal(await page.locator('[data-testid="timeline-overlay"]').count(),0,'Completed execution is not replayed after load');
      if(phone) {
        const geometry=await page.locator('.readable-scroll').evaluate(e=>({width:e.clientWidth,scrollWidth:e.scrollWidth}));
        assert.ok(geometry.scrollWidth<=geometry.width+1,JSON.stringify(geometry));
      }
      await action('advance').click();await idle();
      assert.equal((await snapshot()).commands.filter(c=>c.type==='acknowledgeResult').length,1);
      assert.deepEqual(errors,[]);
      cases.push({phone,plan,canonicalDraftOnly:true,forecastReadOnly:true,growthSkill:10,portableRestore:true});
    } finally {
      await page.screenshot({path:join(out,name+'-last.png')}).catch(()=>{});
      await writeFile(join(out,name+'-last.txt'),await page.locator('body').innerText().catch(()=>''));
      await context.close();
    }
  }
  await writeFile(join(out,'proof.json'),JSON.stringify({ok:true,contentFingerprint:built.snapshot.contentFingerprint,cases},null,2));
});

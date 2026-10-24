import test from 'node:test';
import assert from 'node:assert/strict';
import {cp,mkdir,mkdtemp,readFile,writeFile,rm} from 'node:fs/promises';
import path from 'node:path';
import {spawnSync} from 'node:child_process';
import {chromium} from 'playwright';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';
import {decodeSave} from '../../dist/packages/runtime-service/save.js';

test('custom calendar: native and Web show phase 13, four weeks, fixed grid and restored year boundary',{timeout:180000},async t=>{
 await mkdir('.cache/calendar',{recursive:true});const out=await mkdtemp(path.resolve('.cache/calendar/players-'));const browser=await chromium.launch({headless:true,...(process.env.RAISING_BROWSER_CHANNEL?{channel:process.env.RAISING_BROWSER_CHANNEL}:{})});t.after(()=>browser.close());
 for(const layout of ['columns','calendar']){
  const root=path.join(out,layout);await cp('examples/my-first-raising-game',root,{recursive:true});const p=JSON.parse(await readFile(path.join(root,'project.json'),'utf8'));
  Object.assign(p,{startYear:7,startMonth:13,durationMonths:2,periodsPerMonth:4,calendar:{monthsPerYear:13,daysPerMonth:28,yearLabel:'Year',monthLabel:'Phase',periodLabel:'Week',dayLabel:'Day',weekdayLabels:['First','Second','Third','Fourth','Fifth','Sixth','Seventh'],startWeekday:0}});
  const screen=p.visual.screens.find(x=>x.id==='schedule');screen.elements.find(x=>x.id==='schedule').scheduleLayout=layout;const dateElement=screen.elements.find(x=>x.id==='calendar');for(const [i,binding] of ['calendar-month','calendar-day','calendar-weekday'].entries())screen.elements.push({...dateElement,id:binding+'-proof',binding,x:32+i*220,y:104,width:210,height:30,fontSize:16});
  await writeFile(path.join(root,'project.json'),JSON.stringify(p));await rm(path.join(root,'events'),{recursive:true});await mkdir(path.join(root,'events'));await rm(path.join(root,'scenarios'),{recursive:true});await mkdir(path.join(root,'scenarios'));
  const service=new QtStudioService();t.after(()=>service.releases.close());await service.handle({method:'open',params:{root}});const built=await service.editor.snapshot(service.editor.revision);assert.equal(built.ok,true,JSON.stringify(built.diagnostics));
  const publish=async target=>{const destination=path.join(out,layout+'-'+target);const r=await service.handle({method:'export',params:{revision:service.editor.revision,target,destination}});assert.ok(r.exported,JSON.stringify(r));return {destination,exported:r.exported};};
  const native=await publish(desktopTarget()),directory=gameDirectory(desktopTarget(),native.destination),executable=playerExecutable(desktopTarget(),native.destination),steps=[{click:'start'},{checkpoint:'first'}];
  if(layout==='columns'){steps.push(...Array.from({length:4},(_,i)=>({select:'plan-'+i,value:'rest'})),{click:'advance'});for(let i=0;i<4;i++){if(i===3)steps.push({checkpoint:'week-four'});steps.push({click:'advance'},{click:'advance'});}steps.push({checkpoint:'next-year'},{key:'F5'},{key:'F9'},{checkpoint:'restored'});}
  steps.push({screenshot:path.join(out,layout+'-native.png')});const automation=path.join(out,layout+'-steps.json'),report=path.join(out,layout+'-native.json'),save=path.join(out,layout+'-save.json');await writeFile(automation,JSON.stringify(steps));
  const run=spawnSync(executable,['--bundle',directory,'--automation',automation,'--report',report,'--save',save,'--hidden'],{encoding:'utf8',timeout:60000});assert.equal(run.status,0,run.error?.message??run.stderr);const proof=JSON.parse(await readFile(report,'utf8'));assert.equal(proof.ok,true,JSON.stringify(proof));const first=proof.steps.find(x=>x.command.checkpoint==='first').ui;
  assert.match(first.texts.calendar,/Year 7 · Phase 13 · Week 1/);assert.match(first.texts['calendar-month-proof'],/Phase 13/);assert.equal(first.texts['calendar-day-proof'],'1');assert.match(first.texts['calendar-weekday-proof'],/First/);
  if(layout==='columns'){assert.match(first.texts.schedule,/Week 4/);assert.equal(proof.steps.find(x=>x.command.checkpoint==='week-four').ui.texts['calendar-day-proof'],'22');const next=proof.steps.find(x=>x.command.checkpoint==='next-year').ui;assert.equal(next.state.periodIndex,4);assert.match(next.texts.calendar,/Year 8 · Phase 1 · Week 1/);assert.deepEqual(proof.steps.find(x=>x.command.checkpoint==='restored').ui.state,next.state);}
  else {assert.equal((first.texts.schedule.match(/id="calendar-day-/g)??[]).length,28);assert.match(first.texts.schedule,/First/);assert.match(first.texts.schedule,/Seventh/);assert.doesNotMatch(first.texts.schedule,/calendar-day-29/);}
  const web=await publish('web'),launch=await service.handle({method:'releaseLaunch',params:{token:web.exported.launchToken}}),page=await browser.newPage({viewport:{width:1280,height:800},acceptDownloads:true});const errors=[];page.on('pageerror',e=>errors.push(e.message));await page.goto(launch.launch.url);await page.locator('[data-element-id="start"]').getByRole('button').click();
  await page.waitForFunction(async()=>!!(await window.player.view()).view?.state);assert.match(await page.locator('[data-element-id="calendar"]').innerText(),/Year 7 · Phase 13 · Week 1/);
  if(layout==='columns'){
   assert.match(await page.locator('[data-element-id="schedule"]').innerText(),/Week 4/);for(const select of await page.locator('[data-element-id="schedule"] select').all())await select.selectOption('rest');
   const advance=async()=>{const before=await page.evaluate(async()=>(await window.player.view()).view.state.stateRevision);await page.locator('[data-element-id="advance"]').getByRole('button').click();await page.waitForFunction(async r=>(await window.player.view()).view.state.stateRevision>r,before);};
   await advance();for(let i=0;i<4;i++){if(i===3)assert.equal((await page.locator('[data-element-id="calendar-day-proof"]').innerText()).trim(),'22');await advance();await advance();}assert.match(await page.locator('[data-element-id="calendar"]').innerText(),/Year 8 · Phase 1 · Week 1/);
   const pending=page.waitForEvent('download');await page.getByRole('button',{name:'Save',exact:true}).click();const file=path.join(out,'web-save.json');await(await pending).saveAs(file);const decoded=await decodeSave(built.snapshot,await readFile(file,'utf8'));assert.equal(decoded.ok,true,JSON.stringify(decoded));assert.equal(decoded.session.state.periodIndex,4);
   await page.reload();await page.locator('.visual-stage').waitFor();const chooser=page.waitForEvent('filechooser');await page.getByRole('button',{name:'Continue a saved game',exact:true}).click();await(await chooser).setFiles(file);await page.waitForFunction(async()=>(await window.player.view()).view?.state?.periodIndex===4);assert.match(await page.locator('[data-element-id="calendar"]').innerText(),/Year 8 · Phase 1 · Week 1/);
  }else {assert.equal(await page.locator('[data-calendar-day]').count(),28);assert.match(await page.locator('.schedule-calendar-week').innerText(),/First/);assert.match(await page.locator('.schedule-calendar-week').innerText(),/Seventh/);}
  await page.screenshot({path:path.join(out,layout+'-web.png')});assert.deepEqual(errors,[]);await page.close();
 }
 await writeFile('.cache/calendar/players-latest.json',JSON.stringify({ok:true,out},null,2));
});

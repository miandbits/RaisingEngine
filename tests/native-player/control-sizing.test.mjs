import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {phaseForecastFixture} from '../fixtures/phase-forecast-ui.mjs';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';
const at=(r,n)=>r.steps.find(s=>s.command.checkpoint===n).ui;
const control=(ui,id)=>{const c=ui.readingLayout.controls.find(c=>c.id===id);assert.ok(c,`Missing window-point control ${id}`);return c;};
const node=(ui,id)=>{const n=ui.nodes.find(n=>n.id===id);assert.ok(n,`Missing rendered node ${id}`);return n;};
const click=id=>({clickWindow:id,waitFrames:3});
function minimum(ui,id){const c=control(ui,id);assert.ok(c.width>=43.5&&c.height>=43.5,`${id} must occupy at least 44 window points: ${JSON.stringify(c)}`);return c;}
function separated(ui){
 const review=minimum(ui,'plan-review-0'),select=minimum(ui,'plan-0'),forecast=minimum(ui,'phase-forecast-schedule');
 assert.ok(review.y+review.height<=select.y+.5,JSON.stringify({review,select}));
 assert.ok(select.y+select.height<=forecast.y+.5,JSON.stringify({select,forecast}));
 const detail=node(ui,'plan-details-schedule'),launch=node(ui,'phase-forecast-schedule');
 assert.ok(launch.y+launch.height<=detail.y+.5,JSON.stringify({launch,detail}));
 assert.ok(detail.clientHeight>=40&&detail.scrollWidth<=detail.clientWidth+1,JSON.stringify(detail));
 assert.equal(node(ui,'plan-review-0').color.toLowerCase(),'#203a3b','Bare authored themes retain their readable ink');
 const period=node(ui,'plan-periods-schedule');assert.ok(period.scrollWidth<=period.clientWidth+1,'Four fitting periods do not gain a horizontal scrollbar');
 const planner=node(ui,'schedule');assert.ok(detail.y+detail.height<=planner.y+planner.height+.5,'Detail flow stays inside authored schedule rectangle');
 for(const id of ['plan-review-0','phase-forecast-schedule'])assert.equal(node(ui,id).clickableAtCenter,true,`${id} has a real unobscured hit area`);
}
test('generated planner controls reserve 44 window points through canvas fitting, focus and live resizing',{timeout:180000},async()=>{
 await mkdir('.cache/native-control-sizing',{recursive:true});const out=await mkdtemp(resolve('.cache/native-control-sizing/run-'));
 const target=desktopTarget(),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app');
 for(const toolbar of [true,false]){
  const name=toolbar?'toolbar':'plain',f=await phaseForecastFixture(join(out,name+'-project'));f.content=structuredClone(f.content);
  if(!toolbar){delete f.content.project.visual.readableLayout;for(const s of f.content.project.visual.screens)delete s.readableLayout;}
  const destination=join(out,name+'-game');await exportGame({content:f.content,projectRoot:f.root,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
  const steps=[{resize:{width:1280,height:720}},click('start'),{checkpoint:'desktop',screenshot:join(out,name+'-desktop.png')},click('plan-review-0'),{checkpoint:'focus'},
   {resize:{width:1024,height:680}},{waitFrames:5},{checkpoint:'smaller',screenshot:join(out,name+'-smaller.png')},click('plan-0'),{checkpoint:'dropdown'},click('plan-0-option-rest'),{checkpoint:'edited'},click('phase-forecast-schedule'),{checkpoint:'forecast'},{key:'Escape'},{checkpoint:'returned'},
   {resize:{width:1600,height:1000}},{waitFrames:5},{checkpoint:'larger'}, {resize:{width:1280,height:720}},{waitFrames:5},{checkpoint:'restored'}];
  const input=join(out,name+'.json'),report=join(out,name+'-report.json');await writeFile(input,JSON.stringify(steps));
  const child=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',join(out,name+'-save.json'),'--automation',input,'--report',report,'--report-checkpoints-only','--locale','en','--hidden'],{encoding:'utf8',timeout:90000});
  assert.equal(child.status,0,child.stderr||child.error?.message);const result=JSON.parse(await readFile(report,'utf8'));assert.equal(result.ok,true,result.error);
  const initial=at(result,'desktop');for(const n of ['desktop','focus','smaller','edited','returned','larger','restored']){const ui=at(result,n);if(toolbar){const c=minimum(ui,'engine-reading-options'),w=n==='larger'?1600:['smaller','edited','returned'].includes(n)?1024:1280,h=n==='larger'?1000:['smaller','edited','returned'].includes(n)?680:720;assert.ok(c.x>=0&&c.y>=0&&c.x+c.width<=w+.5&&c.y+c.height<=h+.5,JSON.stringify({c,w,h}));}assert.equal(ui.readingLayout.active,false);separated(ui);assert.deepEqual(ui.state,initial.state);assert.deepEqual(ui.commands,initial.commands);}
  assert.deepEqual(control(at(result,'focus'),'plan-review-0'),control(initial,'plan-review-0'),'Focus border reserves space without resizing or displacing the control');
  assert.ok(at(result,'smaller').documentGeneration>initial.documentGeneration,'Plain fit layouts also regenerate control minima after resize');
  assert.equal(at(result,'edited').scheduleDraft.activities[0],'rest','Real dropdown pointer choice edits the draft');
  assert.ok(at(result,'dropdown').options.filter(o=>o.id.startsWith('plan-0-option-')).every(o=>o.visible),'Dropdown options are not clipped by the period strip');
  assert.deepEqual(at(result,'forecast').state,at(result,'edited').state);assert.deepEqual(at(result,'forecast').commands,at(result,'edited').commands);assert.deepEqual(at(result,'forecast').scheduleDraft,at(result,'edited').scheduleDraft);
  assert.deepEqual(at(result,'returned').scheduleDraft,at(result,'edited').scheduleDraft);assert.equal(at(result,'returned').focusId,'phase-forecast-schedule');
  assert.equal(node(at(result,'restored'),'plan-review-0').height,node(initial,'plan-review-0').height,'Growing and shrinking recomputes from the authored canvas, without accumulating scale');
 }
 await writeFile(join(out,'proof.json'),JSON.stringify({ok:true,checks:['actual window-point minima including toolbar','plain fit resize invalidation','focus border geometry','pointer dropdown and forecast','no extra commands or state mutation','flow separation and no spurious horizontal scrollbar']},null,2));
 await writeFile('.cache/native-control-sizing/latest.json',JSON.stringify({ok:true,out},null,2));console.log('Control sizing evidence: '+out);
});

test('plain twelve-period columns and narrow calendars preserve target width with real scrolling',{timeout:180000},async()=>{
 await mkdir('.cache/native-control-sizing',{recursive:true});const out=await mkdtemp(resolve('.cache/native-control-sizing/narrow-'));
 const target=desktopTarget(),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app');
 for(const layout of ['columns','calendar']){
  const f=await phaseForecastFixture(join(out,layout+'-project'),{layout,opted:layout==='calendar'});f.content=structuredClone(f.content);const planner=f.content.project.visual.screens.find(s=>s.id==='schedule').elements.find(e=>e.kind==='schedule');
  Object.assign(planner,{width:layout==='calendar'?340:540,showActivityDescription:false,showActivityRules:false});
  if(layout==='columns')f.content.project.periodsPerMonth=12;
  const destination=join(out,layout+'-game');await exportGame({content:f.content,projectRoot:f.root,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
  const steps=[{resize:{width:1024,height:680}},click('start'),{checkpoint:'before',screenshot:join(out,layout+'-before.png')}];
  if(layout==='calendar')steps.push({touchDrag:{fromX:255,fromY:320,toX:135,toY:225}},{waitFrames:3},{checkpoint:'swiped',screenshot:join(out,layout+'-swiped.png')},{scrollIntoView:'calendar-day-28'},{checkpoint:'last-visible'},click('calendar-day-28'),{checkpoint:'selected'});
  else steps.push({scrollIntoView:'plan-11'},click('plan-11'),{checkpoint:'dropdown'},click('plan-11-option-rest'),{checkpoint:'selected',screenshot:join(out,layout+'-last.png')});
  const input=join(out,layout+'.json'),report=join(out,layout+'-report.json');await writeFile(input,JSON.stringify(steps));
  const run=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',join(out,layout+'-save.json'),'--automation',input,'--report',report,'--report-checkpoints-only','--hidden'],{encoding:'utf8',timeout:90000});assert.equal(run.status,0,run.stderr||run.error?.message);const r=JSON.parse(await readFile(report,'utf8'));assert.equal(r.ok,true,r.error);const before=at(r,'before'),selected=at(r,'selected');
  assert.deepEqual(selected.state,before.state);assert.deepEqual(selected.commands,before.commands);
  if(layout==='calendar'){
   for(let day=1;day<=28;day++)minimum(before,'calendar-day-'+day);
   const swiped=at(r,'swiped');assert.deepEqual(swiped.scheduleDraft,before.scheduleDraft,'Dragging a day scrolls without selecting it');assert.deepEqual(swiped.commands,before.commands);assert.deepEqual(swiped.state,before.state);
   assert.ok(node(swiped,'calendar-day-7').x<node(before,'calendar-day-7').x-20,'Horizontal touch reaches narrow calendar columns');
   assert.ok(node(swiped,'calendar-day-28').y<node(before,'calendar-day-28').y-20,'Vertical touch reaches lower calendar rows');
   assert.equal(selected.scheduleDraft.slot,3);assert.equal(node(at(r,'last-visible'),'calendar-day-28').clickableAtCenter,true);
  }else{
   for(let period=0;period<12;period++)minimum(before,'plan-'+period);
   const strip=node(before,'plan-periods-schedule');assert.ok(strip.scrollWidth>strip.clientWidth,'Twelve minimum-width controls reserve real scrollable width');
   assert.ok(node(selected,'plan-periods-schedule').scrollLeft>0);assert.equal(selected.scheduleDraft.activities[11],'rest');
   assert.ok(at(r,'dropdown').options.filter(o=>o.id.startsWith('plan-11-option-')).every(o=>o.visible));
  }
 }
 console.log('Narrow control evidence: '+out);
});

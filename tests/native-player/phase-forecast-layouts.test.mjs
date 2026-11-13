import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {phaseForecastFixture} from '../fixtures/phase-forecast-ui.mjs';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';
const at=(r,n)=>r.steps.find(s=>s.command.checkpoint===n).ui;
const element=(ui,id)=>{const value=ui.nodes.find(e=>e.id===id&&e.kind==='runtime-button');assert.ok(value,`Missing rendered ${id}`);return value;};
const click=id=>({scrollIntoView:id,clickWindow:id,waitFrames:2});

test('native forecast launcher reserves space in calendar and menu without changing unopted layouts',{timeout:180000},async()=>{
 await mkdir('.cache/native-phase-forecast-layouts',{recursive:true});const out=await mkdtemp(resolve('.cache/native-phase-forecast-layouts/run-'));
 const runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app'),target=desktopTarget(),reports={};
 for(const layout of ['calendar','menu'])for(const opted of [false,true]){
  const name=layout+(opted?'-forecast':'-original'),f=await phaseForecastFixture(join(out,name+'-project'),{layout,opted}),destination=join(out,name+'-game');
  await exportGame({content:f.content,projectRoot:f.root,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
  const steps=[{resize:{width:1280,height:720}},click('start'),{checkpoint:'before',screenshot:join(out,name+'.png')}];
  if(opted)steps.push(click('phase-forecast-schedule'),{checkpoint:'incomplete'}, {key:'Escape'});
  if(layout==='calendar')steps.push({scrollIntoView:'calendar-day-28',waitFrames:2},{checkpoint:'last-visible',screenshot:join(out,name+'-last-day.png')},{clickWindow:'calendar-day-28',waitFrames:2},{checkpoint:'last-day'});
  else steps.push(click('plan-group-all'),{checkpoint:'activities',screenshot:join(out,name+'-activities.png')},click('plan-activity-study'),{checkpoint:'selected'});
  if(layout==='menu'&&opted)steps.push(click('phase-forecast-schedule'),{checkpoint:'partial'}, {key:'Escape'});
  const input=join(out,name+'-steps.json'),report=join(out,name+'-report.json');await writeFile(input,JSON.stringify(steps));
  const child=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',join(out,name+'-save.json'),'--automation',input,'--report',report,'--report-checkpoints-only','--locale','en','--hidden'],{encoding:'utf8',timeout:60000});
  assert.equal(child.status,0,child.stderr||child.error?.message);const result=JSON.parse(await readFile(report,'utf8'));assert.equal(result.ok,true,result.error);reports[name]=result;
  const before=at(result,'before');
  if(opted){
   const forecast=at(result,'incomplete');assert.deepEqual(forecast.state,before.state);assert.deepEqual(forecast.commands,before.commands);assert.deepEqual(forecast.scheduleDraft,before.scheduleDraft);
   assert.equal(forecast.phaseForecast.result.forecast.status,'blocked');assert.equal(forecast.phaseForecast.result.forecast.rows.length,0);assert.equal(forecast.phaseForecast.result.forecast.issues[0].code,'FORECAST_INCOMPLETE');
   const launcher=element(before,'phase-forecast-schedule');assert.ok(launcher.height>=43.5);
   const content=layout==='calendar'?element(before,'calendar-day-1'):element(at(result,'activities'),'plan-activity-study');
   assert.ok(content.y>=launcher.y+launcher.height+7,JSON.stringify({layout,launcher,content}));
   if(layout==='calendar')for(const day of before.nodes.filter(e=>e.kind==='runtime-button'&&e.id.startsWith('calendar-day-')))assert.ok(day.height>=43.5&&day.width>=43.5,JSON.stringify(day));
  }else assert.equal(before.elements.some(e=>e.id==='phase-forecast-schedule'),false);
  const after=at(result,layout==='calendar'?'last-day':'selected');assert.deepEqual(after.state,before.state);assert.deepEqual(after.commands,before.commands);
  if(layout==='calendar'){
   assert.equal(after.scheduleDraft.slot,3,'Last calendar period remains pointer-accessible');
   if(opted){const initialLast=element(before,'calendar-day-28'),last=element(at(result,'last-visible'),'calendar-day-28');assert.ok(initialLast.visibleHitbox.height<initialLast.height,JSON.stringify(initialLast));assert.ok(last.visibleHitbox.height>=last.height-.5,JSON.stringify(last));}
  }
  else {assert.equal(after.scheduleDraft.activities[0],'study');assert.equal(after.scheduleDraft.selected,1);
   if(opted){const partial=at(result,'partial');assert.deepEqual(partial.state,after.state);assert.deepEqual(partial.commands,after.commands);assert.deepEqual(partial.scheduleDraft,after.scheduleDraft);assert.equal(partial.phaseForecast.result.forecast.issues[0].code,'FORECAST_INCOMPLETE');assert.equal(partial.phaseForecast.result.forecast.issues[0].periodIndex,1);assert.match(partial.texts['engine-phase-forecast-panel'],/Week 2.*Choose an activity/s);}
  }
 }
 const original=element(at(reports['calendar-original'],'before'),'calendar-day-1'),opted=element(at(reports['calendar-forecast'],'before'),'calendar-day-1');
 assert.ok(opted.y-original.y>=51.5,JSON.stringify({original,opted}));
 await writeFile(join(out,'proof.json'),JSON.stringify({ok:true,checks:['opt-in calendar/menu launcher separation','44 canvas-unit calendar targets (window size measured separately by diagnostics)','last calendar period reached by real pointer','menu activity edits draft only','incomplete placeholder drafts blocked','absent flag retains original layout']},null,2));
 await writeFile('.cache/native-phase-forecast-layouts/latest.json',JSON.stringify({ok:true,root:out}));
});

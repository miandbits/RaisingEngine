import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {phaseForecastFixture} from '../fixtures/phase-forecast-ui.mjs';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';
const at=(r,n)=>r.steps.find(s=>s.command.checkpoint===n).ui,touch=id=>({scrollIntoView:id,touchWindow:id,waitFrames:2});
const unchanged=(a,b)=>{assert.deepEqual(a.state,b.state);assert.deepEqual(a.commands,b.commands);assert.deepEqual(a.scheduleDraft,b.scheduleDraft);};
function fit(ui){const r=ui.readingLayout,main=r['reading-main'],footer=r['reading-actions'];assert.ok(main.height>0);assert.ok(main.scrollWidth<=main.clientWidth+1);assert.ok(main.y+main.height<=footer.y+1);for(const c of r.controls)assert.ok(c.width>=43.5&&c.height>=43.5,JSON.stringify(c));}

test('native forecast review preserves live state, saves and draft; cached results refresh after editing at phone 200%',{timeout:180000},async()=>{
 await mkdir('.cache/native-phase-forecast',{recursive:true});const out=await mkdtemp(resolve('.cache/native-phase-forecast/run-')),f=await phaseForecastFixture(join(out,'project'));
 const runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app'),target=desktopTarget(),destination=join(out,'game');await exportGame({content:f.content,projectRoot:f.root,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
 const run=async(name,steps,extra=[])=>{const input=join(out,name+'.json'),report=join(out,name+'-report.json');await writeFile(input,JSON.stringify(steps));const child=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',join(out,'save.json'),'--automation',input,'--report',report,'--report-checkpoints-only','--hidden',...extra],{encoding:'utf8',timeout:90000});assert.equal(child.status,0,child.stderr||child.error?.message);const result=JSON.parse(await readFile(report,'utf8'));assert.equal(result.ok,true,result.error);return result;};
 const choose=(slot,activity)=>[touch('board-slot-'+slot),touch('board-activity-'+activity),touch('board-apply')];
 await run('seed',[{clickWindow:'start'},{key:'F5'}]);const savedBytes=await readFile(join(out,'save.json'));
 const r=await run('phone',[{resize:{width:390,height:844}},{waitFrames:4},touch('start'),...choose(0,'study'),...choose(1,'rest'),...choose(2,'study'),...choose(3,'rest'),{checkpoint:'before'},touch('phase-forecast-schedule'),{checkpoint:'opened',screenshot:join(out,'phone-200.png')},{key:'End'},{waitFrames:4},{checkpoint:'keyboard-end'},{key:'Home'},{waitFrames:4},{key:'F5'},{key:'F9'},{checkpoint:'shortcuts'},{resize:{width:844,height:390}},{checkpoint:'landscape',screenshot:join(out,'landscape-200.png')},touch('engine-phase-forecast-close'),{resize:{width:390,height:844}},{checkpoint:'closed'},touch('phase-forecast-schedule'),{checkpoint:'cached'},touch('engine-phase-forecast-close'),...choose(0,'rest'),{checkpoint:'edited'},touch('phase-forecast-schedule'),{checkpoint:'updated'},touch('engine-phase-forecast-close'),...choose(0,'costly'),{checkpoint:'blocked-draft'},touch('phase-forecast-schedule'),{checkpoint:'blocked'},touch('engine-phase-forecast-close'),{checkpoint:'saved'}],['--text-scale','2','--locale','en']);
 assert.deepEqual(await readFile(join(out,'save.json')),savedBytes,'review and modal save/load shortcuts preserve save bytes');
 const scrolled=at(r,'keyboard-end').readingLayout['reading-main'];assert.ok(scrolled.scrollTop>0&&scrolled.scrollTop+scrolled.clientHeight>=scrolled.scrollHeight-2,'real End reaches the final forecast row');
 const before=at(r,'before');for(const n of ['opened','shortcuts','landscape','closed','cached'])unchanged(at(r,n),before);
 for(const n of ['opened','shortcuts','landscape','cached','updated','blocked']){assert.equal(at(r,n).phaseForecast.open,true);fit(at(r,n));}
 assert.equal(at(r,'opened').phaseForecast.result.forecast.status,'exact');assert.equal(at(r,'opened').phaseForecast.result.forecast.attributes.find(a=>a.attributeId==='money').after,18);
 assert.equal(at(r,'cached').phaseForecast.requests,at(r,'opened').phaseForecast.requests);assert.equal(at(r,'updated').phaseForecast.requests,at(r,'opened').phaseForecast.requests+1);
 unchanged(at(r,'updated'),at(r,'edited'));assert.equal(at(r,'updated').phaseForecast.result.forecast.attributes.find(a=>a.attributeId==='money').after,23);
 unchanged(at(r,'blocked'),at(r,'blocked-draft'));assert.equal(at(r,'blocked').phaseForecast.result.forecast.status,'blocked');assert.match(at(r,'blocked').texts['engine-phase-forecast-panel'],/Week 1.*Expensive workshop.*not enough resources/s);
 assert.equal(at(r,'closed').focusId,'phase-forecast-schedule');assert.deepEqual(at(r,'saved').state,before.state);assert.deepEqual(at(r,'saved').commands,before.commands);
 const saved=JSON.parse(await readFile(join(out,'save.json'),'utf8'));assert.deepEqual(saved.commands,before.commands);
 const desktop=await run('desktop',[{clickWindow:'start'},...['study','rest','study','rest'].map((value,i)=>({select:'plan-'+i,value})),{checkpoint:'before'},{clickWindow:'phase-forecast-schedule'},{checkpoint:'open',screenshot:join(out,'desktop.png')},{key:'Escape'},{checkpoint:'closed'}]);
 unchanged(at(desktop,'open'),at(desktop,'before'));unchanged(at(desktop,'closed'),at(desktop,'before'));assert.equal(at(desktop,'closed').focusId,'phase-forecast-schedule');
 const detail=at(desktop,'before').nodes.find(n=>n.kind==='schedule-details');assert.ok(detail.clientHeight>=80);assert.ok(detail.scrollWidth<=detail.clientWidth+1);
 await writeFile(join(out,'proof.json'),JSON.stringify({ok:true,checks:['real touch and desktop opener','200% portrait and landscape','exact costs/recovery/upkeep','draft edits remain uncommitted','cache reuse and refresh','blocked activity context','F5/F9 modal isolation','focus restoration']},null,2));
});


test('native story forecasts preserve uncertainty, hide story spoilers and localize standard periods',{timeout:120000},async()=>{
 await mkdir('.cache/native-phase-forecast',{recursive:true});const out=await mkdtemp(resolve('.cache/native-phase-forecast/story-')),f=await phaseForecastFixture(join(out,'project'),{story:true,calendar:false}),target=desktopTarget(),destination=join(out,'game'),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app');
 await exportGame({content:f.content,projectRoot:f.root,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
 for(const[locale,horizon,assumption]of[['en','During Period 1','Assumes no story effects'],['ja','予定 1の途中まで','物語の効果'],['ko','일정 1 진행 중','이야기 효과']]){
  const input=join(out,locale+'.json'),report=join(out,locale+'-report.json'),steps=[{resize:{width:390,height:844}},{waitFrames:4},touch('start'),...[0,1,2,3].flatMap(i=>[touch('board-slot-'+i),touch('board-activity-study'),touch('board-apply')]),{checkpoint:'before'},touch('phase-forecast-schedule'),{checkpoint:'open',screenshot:join(out,locale+'-200.png')},{key:'End'},{waitFrames:3},{checkpoint:'end'},{key:'Escape'},{checkpoint:'closed'}];await writeFile(input,JSON.stringify(steps));
  const run=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',join(out,locale+'-save.json'),'--automation',input,'--report',report,'--report-checkpoints-only','--hidden','--locale',locale,'--text-scale','2'],{encoding:'utf8',timeout:60000});assert.equal(run.status,0,run.stderr||run.error?.message);const r=JSON.parse(await readFile(report,'utf8'));assert.equal(r.ok,true);const ui=at(r,'open');unchanged(ui,at(r,'before'));unchanged(at(r,'closed'),at(r,'before'));fit(ui);assert.equal(ui.phaseForecast.result.forecast.status,'partial');assert.equal(ui.phaseForecast.result.baseline.status,'projected');const text=ui.texts['engine-phase-forecast-panel'];assert.ok(text.includes(horizon),text);assert.ok(text.includes(assumption),text);assert.doesNotMatch(text,/HIDDEN STORY/);const pane=at(r,'end').readingLayout['reading-main'];assert.ok(pane.scrollTop>0&&pane.scrollTop+pane.clientHeight>=pane.scrollHeight-2);
 }
});

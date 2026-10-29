import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {sharedWidgetsFixture,sharedWidgetIds} from '../fixtures/shared-widgets.mjs';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {desktopTarget,gameDirectory,playerExecutable} from '../../dist/packages/export-service/targets.js';
const at=(r,k)=>r.steps.find(s=>s.command.checkpoint===k)?.ui;
const hit=id=>[{scrollIntoView:id,waitFrames:2},{clickWindow:id,waitFrames:2}];
const bid=suffix=>sharedWidgetIds.schedule+'__board-'+suffix;
async function fixture(options){
 await mkdir('.cache/shared-widgets/native',{recursive:true});const out=await mkdtemp(resolve('.cache/shared-widgets/native/run-')),f=await sharedWidgetsFixture(join(out,'project'),options),target=desktopTarget(),destination=join(out,'game'),runtime=resolve(process.env.NATIVE_PLAYER_TEST_RUNTIME??process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app');
 await exportGame({content:f.content,projectRoot:f.root,target,destination,runtimes:{windows:runtime,macos:runtime,web:runtime}});
 return {...f,out,run:async(steps)=>{const input=join(out,'input.json'),report=join(out,'report.json');await writeFile(input,JSON.stringify(steps.flat()));const run=spawnSync(playerExecutable(target,destination),['--bundle',gameDirectory(target,destination),'--save',join(out,'save.json'),'--automation',input,'--report',report,'--report-checkpoints-only','--hidden','--locale','en'],{encoding:'utf8',timeout:90000});assert.equal(run.status,0,run.stderr||run.error?.message);const result=JSON.parse(await readFile(report));assert.equal(result.ok,true,result.error);return result;}};
}

test('native nested shared widgets play contextual planning, forecast, presentation, growth and saved phone result',{timeout:180000},async()=>{
 const f=await fixture({reading:true}),steps=[hit('start'),{checkpoint:'outings'},hit('open-planner'),{checkpoint:'inspected'},hit(bid('apply'))];
 for(let i=1;i<4;i++)steps.push(hit(bid('slot-'+i)),hit(bid('activity-rest')),hit(bid('apply')));
 steps.push({checkpoint:'draft'},hit('phase-forecast-'+sharedWidgetIds.schedule),{checkpoint:'forecast'},hit('engine-phase-forecast-close'),hit('advance'),{checkpoint:'committed'},hit('advance'),{checkpoint:'performance'},hit('timeline-skip'),{checkpoint:'growth'},{resize:{width:390,height:844}},{checkpoint:'phone'},{key:'F5'},{key:'F9'},{checkpoint:'restored'});
 const r=await f.run(steps),start=at(r,'outings'),inspection=at(r,'inspected'),draft=at(r,'draft'),forecast=at(r,'forecast'),committed=at(r,'committed'),growth=at(r,'growth');
 assert.equal(start.screenId,'outings');assert.equal(inspection.screenId,'schedule');assert.equal(inspection.activityBoard.activityId,'study');assert.equal(inspection.activityBoard.elementId,sharedWidgetIds.schedule);
 assert.deepEqual(inspection.commands,[{type:'navigate',screenId:'schedule'}]);assert.deepEqual(inspection.state.attributes,start.state.attributes);assert.deepEqual(draft.state,inspection.state);assert.deepEqual(draft.commands,inspection.commands);assert.deepEqual(draft.scheduleDraft.activities,['study','rest','rest','rest']);
 assert.equal(forecast.phaseForecast.open,true);assert.deepEqual(forecast.state,draft.state);assert.deepEqual(forecast.commands,draft.commands);
 assert.deepEqual(committed.commands,[{type:'navigate',screenId:'schedule'},{type:'setSchedule',activityIds:['study','rest','rest','rest']}]);assert.equal(at(r,'performance').state.phase,'activity-result');
 assert.equal(growth.state.attributes.skill,10);assert.equal(growth.state.attributes.money,35);assert.match(growth.texts[sharedWidgetIds.result],/Skill/);assert.match(growth.texts[sharedWidgetIds.stats],/35/);
 for(const tag of ['phone','restored']){assert.equal(at(r,tag).readingLayout.active,true);assert.deepEqual(at(r,tag).state,growth.state);assert.deepEqual(at(r,tag).commands,growth.commands);assert.match(at(r,tag).texts[sharedWidgetIds.result],/Skill/);}
 const ids=inspection.nodes.map(n=>n.id);assert.equal(ids.filter(id=>id===bid('apply')).length,1);console.log('Shared native widget evidence: '+f.out);
});

test('two shared columns planners have distinct controls while editing one synchronizes the common draft and detail text',{timeout:180000},async()=>{
 const f=await fixture({layout:'columns',reading:false,two:true}),one=sharedWidgetIds.schedule,two='other-section__content__schedule';
 const r=await f.run([hit('start'),{checkpoint:'start'},hit(one+'__plan-0'),{clickWindow:one+'__plan-0-option-study',waitFrames:2},{checkpoint:'one'},{select:two+'__plan-1',value:'rest'},{checkpoint:'two'},hit(one+'__plan-review-1'),{checkpoint:'detail'},hit('advance'),{checkpoint:'committed'}]);
 for(const tag of ['one','two','detail'])assert.deepEqual(at(r,tag).commands,[]);
 assert.equal(at(r,'one').scheduleDraft.activities[0],'study');assert.equal(at(r,'two').scheduleDraft.activities[1],'rest');
 for(const owner of [one,two]){assert.match(at(r,'one').texts[owner],/Practice carefully/);assert.match(at(r,'detail').texts[owner],/Recover energy/);assert.equal(at(r,'one').controls.find(n=>n.id===owner+'__plan-0').value,'study');assert.ok(at(r,'start').elements.some(n=>n.id===owner+'__plan-review-1'));}
 assert.equal(at(r,'committed').commands[0].type,'setSchedule');assert.deepEqual(at(r,'committed').commands[0].activityIds,at(r,'two').scheduleDraft.activities);console.log('Shared columns evidence: '+f.out);
});

test('shared compact phone schedule retains its owner and restores focus after applying and cancelling a picker',{timeout:180000},async()=>{
 const f=await fixture({layout:'columns',reading:true}),owner=sharedWidgetIds.schedule;
 const r=await f.run([{resize:{width:390,height:844}},hit('start'),hit('engine-planning-guide-skip'),{checkpoint:'start'},hit(owner+'__board-slot-0'),hit('board-activity-study'),{checkpoint:'inspection'},hit('board-apply'),{checkpoint:'applied'},hit(owner+'__board-slot-1'),hit('board-activity-rest'),hit('board-cancel'),{checkpoint:'cancelled'}]);
 const start=at(r,'start'),inspection=at(r,'inspection'),applied=at(r,'applied'),cancelled=at(r,'cancelled');
 assert.equal(start.readingLayout.active,true);assert.deepEqual(inspection.state,start.state);assert.deepEqual(inspection.scheduleDraft,start.scheduleDraft);
 assert.equal(applied.scheduleDraft.activities[0],'study');assert.equal(applied.focusedId,owner+'__board-slot-0');assert.deepEqual(cancelled.scheduleDraft,applied.scheduleDraft);assert.equal(cancelled.focusedId,owner+'__board-slot-1');assert.deepEqual(cancelled.commands,[]);
 console.log('Shared compact phone evidence: '+f.out);
});

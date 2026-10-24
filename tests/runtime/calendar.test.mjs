import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,writeFile} from 'node:fs/promises';
import {content,doc,loaded,replaceEvents,event} from './helpers.mjs';
import {buildSnapshot} from '../../dist/packages/project-service/index.js';
import {calendarDate,calendarGrid,calendarText,calendarDaySlot} from '../../dist/packages/core/calendar.js';
import {calendarTime} from '../../dist/packages/core/evaluation.js';
import {createRun,dispatchCommand} from '../../dist/packages/core/run.js';
import {nativeCreateRun,nativeReplay,nativeRequest} from '../../dist/packages/native-bridge/index.js';
import {encodeSave,decodeSave,MAX_SAVE_BYTES} from '../../dist/packages/runtime-service/save.js';
import {requiredPlayerFeatures,missingPlayerFeatures} from '../../dist/packages/export-service/compatibility.js';
import {extractMessages,localizeContent} from '../../dist/packages/localization/catalog.js';
const calendar={monthsPerYear:13,daysPerMonth:28,yearLabel:'Year',monthLabel:'Phase',periodLabel:'Week',dayLabel:'Day',weekdayLabels:['A','B','C','D','E','F','G'],startWeekday:2};
const configured={startYear:1,startMonth:1,periodsPerMonth:4,calendar};
test('calendar vectors preserve legacy rollover and fixed calendars including all four weekly slots',()=>{
 assert.deepEqual(calendarDate({startYear:4,startMonth:12,periodsPerMonth:3},3),{year:5,month:1,periodInMonth:0,periodIndex:3});
 for(const [index,year,month,period] of [[0,1,1,0],[3,1,1,3],[4,1,2,0],[48,1,13,0],[51,1,13,3],[52,2,1,0],[207,4,13,3],[415,8,13,3]]){
  const d=calendarDate(configured,index);assert.deepEqual(d,{year,month,periodInMonth:period,periodIndex:index});const grid=calendarGrid(configured,d);assert.equal(grid.days,28);assert.equal(grid.offset,2);assert.equal(grid.day,period*7+1);assert.equal(grid.rows,5);
 }
 assert.equal(calendarText(calendarDate(configured,415),calendar),'Year 8 · Phase 13 · Week 4');
 assert.equal(calendarDate({...configured,startYear:16,startMonth:13},4).year,17);
 for(const count of [3,4,12])for(const days of [28,29,31,366])for(let slot=0;slot<count;slot++){const p={...configured,periodsPerMonth:count,calendar:{...calendar,daysPerMonth:days}};const grid=calendarGrid(p,calendarDate(p,slot));assert.equal(calendarDaySlot(grid.day,count,days,true),slot);}
});
test('schema, semantics and raw runtimes reject inconsistent calendar settings',async()=>{
 const good=await content(p=>{Object.assign(doc(p,'project'),configured,{durationMonths:104});});
 for(const patch of [{calendar:{...calendar,monthsPerYear:0}},{calendar:{...calendar,monthsPerYear:61}},{calendar:{...calendar,daysPerMonth:3}},{calendar:{...calendar,daysPerMonth:367}},{startMonth:14},{calendar:{...calendar,weekdayLabels:['one']}},{calendar:{...calendar,monthLabel:' '}}]){
  const p=structuredClone(loaded);p.documents=p.documents.filter(d=>d.kind!=='scenario');Object.assign(doc(p,'project'),configured,patch);const built=await buildSnapshot(p);assert.equal(built.ok,false,JSON.stringify(patch));
  const c=structuredClone(good);Object.assign(c.project,patch);assert.equal(createRun(c,42,{mode:'full'}).ok,false);assert.equal(nativeCreateRun(c,42,{mode:'full'}).ok,false);
 }
 const p=structuredClone(loaded);doc(p,'project').startMonth=13;assert.equal((await buildSnapshot(p)).ok,false);
 const c=structuredClone(good);c.project.gameplay={ruleCalendar:{daysPerMonth:30}};assert.equal(createRun(c,42,{mode:'full'}).ok,false);assert.equal(nativeCreateRun(c,42,{mode:'full'}).ok,false);
});
test('custom calendars are gated, and labels including weekday names translate without changing IDs',async()=>{
 const c=await content(p=>Object.assign(doc(p,'project'),configured));assert.ok(requiredPlayerFeatures(c).includes('calendar-v1'));assert.deepEqual(missingPlayerFeatures(['calendar-v1'],['monthly-schedule-v1']),['calendar-v1']);
 const legacy=structuredClone(c);delete legacy.project.calendar;assert.ok(!requiredPlayerFeatures(legacy).includes('calendar-v1'));
 const same=structuredClone(c);same.project.calendar.monthsPerYear=12;assert.ok(requiredPlayerFeatures(same).includes('calendar-v1'));
 const sources=extractMessages(c),entry=sources.find(x=>x.key==='/project/calendar/weekdayLabels/0');assert.equal(entry.source,'A');
 const messages=[{...entry,translations:{ja:'第一'}},{key:'/project/calendar/monthLabel',source:'Phase',translations:{ja:'節'}}];const translated=localizeContent(c,{defaultLocale:'en',fallbackLocale:'en',locales:[{id:'en',label:'English'},{id:'ja',label:'Japanese'}],entries:messages},'ja');
 assert.equal(translated.project.calendar.weekdayLabels[0],'第一');assert.equal(translated.project.calendar.monthLabel,'節');assert.equal(translated.project.gameId,c.project.gameId);
});
test('104 phases / 416 weeks finish with queued scenes, year conditions and exact saved native/TS replay', {timeout:120000}, async()=>{
 const c=await content(p=>{
  Object.assign(doc(p,'project'),configured,{durationMonths:104,periodFlow:{allPeriodEndEvents:true}});
  doc(p,'flags').flags.push({id:'promise',label:'Promise',initial:false});
  const events=[];
  for(let phase=0;phase<104;phase++){
   const id='phase-'+String(phase+1).padStart(3,'0'),window={fromPeriod:phase*4+3,throughPeriod:phase*4+3};
   events.push(event(id,[{id:'choice',type:'choice',prompt:'Keep the promise?',choices:[{id:'keep',text:'Keep it',requirements:{op:'always'},effects:[{op:'setFlag',flagId:'promise',value:true}],next:'end'},{id:'decline',text:'Decline',requirements:{op:'always'},effects:[{op:'setFlag',flagId:'promise',value:false}],next:'end'}]},{id:'end',type:'end'}],{priority:50,window}));
   events.push(event(id+'-callback',[{id:'end',type:'end'}],{trigger:'after-event',priority:60,window,requirements:{op:'all',conditions:[{op:'eventCompleted',eventId:id},{op:'flag',flagId:'promise',equals:true}]}}));
   events.push(event(id+'-civic',[{id:'end',type:'end'}],{priority:10,window,requirements:{op:'compare',comparator:'eq',left:{op:'time',field:'year'},right:{op:'literal',value:Math.floor(phase/13)+1}}}));
  }replaceEvents(p,events);
 });
 const origin={seed:42,mode:'full',initialOverrides:{attributes:{},flags:{},relationships:{}}};let result=createRun(c,42,origin);assert.equal(result.ok,true);let state=result.state;assert.deepEqual(nativeCreateRun(c,42,origin).state,state);const commands=[],checkpoints=[];
 const checkpoint=async()=>{assert.deepEqual(nativeReplay(c,origin,commands).state,state);assert.deepEqual(nativeRequest({operation:'view',content:c,state}).time,calendarTime(c,state));const start=performance.now();const saved=await encodeSave(c,{origin,commands,state});assert.equal(saved.ok,true,JSON.stringify(saved));const restored=await decodeSave(c,saved.text);assert.equal(restored.ok,true,JSON.stringify(restored));assert.deepEqual(restored.session.state,state);assert.ok(Buffer.byteLength(saved.text)<MAX_SAVE_BYTES);checkpoints.push({week:state.periodIndex,phase:state.phase,commands:commands.length,bytes:Buffer.byteLength(saved.text),roundTripMs:performance.now()-start});};
 const seen=new Set();
 while(state.phase!=='finished'&&commands.length<10000){
  let command;
  if(state.phase==='planning')command={type:'setSchedule',activityIds:['rest','rest','rest','rest']};
  else if(state.phase==='ready')command={type:'executeActivity'};
  else if(state.phase==='activity-result')command={type:'acknowledgeResult'};
  else if(state.phase==='period-end')command={type:'resolvePeriodEnd'};
  else {const cur=state.activeNarrative;const def=(cur.kind==='event'?c.events:c.endings).find(x=>x.id===cur.id),node=def.nodes.find(x=>x.id===cur.nodeId);command=node.type==='choice'?{type:'choose',choiceId:node.choices[0].id}:{type:'advanceNarrative'};}
  const next=dispatchCommand(c,state,command,state.stateRevision);assert.equal(next.ok,true,JSON.stringify(next));state=next.state;commands.push(command);
  const key=state.activeNarrative?.id;if(['phase-001-callback','phase-052-callback','phase-104-callback'].includes(key)&&!seen.has(key)){seen.add(key);await checkpoint();}
 }
 assert.equal(state.phase,'finished');assert.equal(state.periodIndex,415);assert.equal(state.completedEvents.length,312);assert.equal(calendarText(calendarTime(c,state),calendar),'Year 8 · Phase 13 · Week 4');await checkpoint();
 await mkdir('.cache/calendar',{recursive:true});await writeFile('.cache/calendar/long-campaign.json',JSON.stringify({ok:true,weeks:416,phases:104,events:state.completedEvents.length,ending:state.endingId,checkpoints},null,2));
});

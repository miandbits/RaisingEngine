import {PhaseForecast} from './PhaseForecast.js';
import type {PhaseForecastReply} from '../../../packages/player-service/phase-forecast.js';
import type {PlanningControl} from './PlanningGuide.js';
import type {LastCommittedPlan} from '../../../packages/player-service/plan-reuse.js';
import type {RunState} from '../../../packages/core/model.js';
import {CompactSchedule} from './CompactSchedule.js';
import {calendarText,calendarGrid,calendarDaySlot,calendarLabels,type CalendarProject} from '../../../packages/core/calendar.js';
import { periodLabel } from '../../../packages/player-service/schedule.js';
import React, {useEffect, useState} from 'react';
import type {Element} from '../../../packages/visual/model.js';
import {activityRuleLines} from '../../../packages/visual/activity-rules.js';
import type {ContentSnapshot} from '../../../packages/core/model.js';
import {activityPreview} from '../../../packages/visual/activity-summary.js';

export interface ScheduleActivity {
  id:string; label:string; iconAssetId?:string; description?:string;
  baseEffects?:ContentSnapshot['activities'][number]['baseEffects'];
  successCheck?:NonNullable<ContentSnapshot['activities'][number]['successCheck']>;
  costs?:readonly {attributeId:string;amount:number}[];
  previewAttributes?:readonly {attributeId:string;label?:string;valueFormat?:string}[];
}
export interface ScheduleContext {
  forecastPhase?:((activityIds:string[])=>Promise<PhaseForecastReply|null>)|undefined;
  forecastMask?:number;
  gameId?:string;generation?:number;lastCommittedPlan?:LastCommittedPlan;
  dataState?:Pick<RunState,'phase'|'periodIndex'|'schedule'|'activityIntermission'|'stateRevision'>|null;
  activities:readonly ScheduleActivity[]; plan:readonly string[]; period:number; phase:string;
  stats:readonly {id:string;label:string;value:number}[];
  periodsPerMonth?:number; schedulePlayback?:string;
  calendarProject?:CalendarProject;locale?:string;
  calendarTime?:{year:number;month:number}; planningKey?:string; scheduleCommitted?:boolean;
}
export function useSchedulePlanner(context:ScheduleContext){
  const full=(1<<(context.periodsPerMonth??3))-1;
  const key=context.planningKey??`${context.calendarTime?.year??1}:${context.calendarTime?.month??1}`;
  const [planner,set]=useState({key,mask:context.scheduleCommitted?full:0,slot:context.period,groupId:''});
  useEffect(()=>{set(previous=>previous.key!==key
    ?{key,mask:context.scheduleCommitted?full:0,slot:context.period,groupId:''}
    :context.scheduleCommitted?{...previous,mask:full,slot:Math.max(context.period,previous.slot)}:previous);
  },[key,context.scheduleCommitted,full]);
  return {planner,set};
}
export type SchedulePlanner=ReturnType<typeof useSchedulePlanner>;
export function ScheduleWidget({element,context,planning,groups,assetUrl,disabled,onPlan,onPlanReplace,onCommit,readable=false,compact=false,guidance=false,planReuse=false,ownerKey,planningControl}: {
  ownerKey?:string;planReuse?:boolean;guidance?:boolean;planningControl?:PlanningControl|undefined;
  readable?:boolean;compact?:boolean;
  element:Element;context:ScheduleContext;planning:SchedulePlanner;
  groups:readonly {id:string;label:string;activityIds:readonly string[]}[];
  assetUrl:(id:string)=>string;disabled:boolean;
  onPlanReplace?:((ids:string[])=>void)|undefined;onPlan?:((slot:number,id:string)=>void)|undefined;onCommit?:(()=>void)|undefined;
}){
  const count=context.periodsPerMonth??3,full=(1<<count)-1;
  const labels=Array.from({length:count},(_,i)=>periodLabel(i,count,context.calendarProject?.calendar,context.locale));
  const {planner,set}=planning,editable=!disabled&&['planning','ready'].includes(context.phase);
  const chooseSlot=(slot:number)=>{if(!editable||slot<context.period)return;set({...planner,slot,mask:planner.mask&~(1<<slot),groupId:''});};
  const chooseActivity=(id:string,slot=planner.slot)=>{
    if(!editable||slot<context.period)return;
    onPlan?.(slot,id);const mask=planner.mask|(1<<slot);
    let next=slot;for(let n=1;n<=count;n++){const candidate=(slot+n)%count;if(candidate>=context.period&&!(mask&(1<<candidate))){next=candidate;break;}}
    set({...planner,mask,slot:next,groupId:''});
  };
  const selectionDriven=!readable&&['calendar','menu'].includes(element.scheduleLayout??'');
  const forecastContext=selectionDriven?{...context,forecastMask:planner.mask,plan:context.plan.map((id,index)=>index>=context.period&&!(planner.mask&(1<<index))?'':id)}:context;
  const forecast=element.showPhaseForecast?<PhaseForecast id={element.id} context={forecastContext} disabled={disabled}/>:null;
  if(readable&&compact)return <>{forecast}<CompactSchedule id={element.id} context={context} labels={labels} groups={groups} disabled={disabled} chooseActivity={chooseActivity} guidance={guidance} planReuse={planReuse} selectionKey={JSON.stringify([ownerKey,planner])} applyPlan={onPlanReplace?ids=>{if(editable){onPlanReplace(ids);set({...planner,mask:full,slot:context.period,groupId:''});}}:undefined} control={planningControl??(element.scheduleLayout==='menu'&&onCommit?{label:context.locale?.startsWith('en')?'Confirm schedule':context.locale?.startsWith('ja')?'予定を確定':'일정 확정',effect:'confirm-and-execute'}:undefined)}/>{element.scheduleLayout==='menu'&&<button className="readable-schedule-commit" disabled={!editable} onClick={onCommit}>{context.locale?.startsWith('en')?'Confirm schedule':context.locale?.startsWith('ja')?'予定を確定':'일정 확정'}</button>}</>;
  if(!readable&&element.scheduleLayout==='calendar'){
    const {year,month}=context.calendarTime??{year:1,month:1};
    const project=context.calendarProject??{startYear:year,startMonth:month,periodsPerMonth:count};const grid=calendarGrid(project,{year,month,periodInMonth:context.period,periodIndex:0});const first=grid.offset,days=grid.days,custom=project.calendar,words=custom?calendarLabels(custom,context.locale):null;
    return <div className="schedule-calendar" data-testid="schedule-calendar">
      <div className="schedule-calendar-title">{forecast}{custom?calendarText({year,month,periodInMonth:context.period,periodIndex:0},custom,context.locale,false):`${year}년 ${month}월`}</div>
      <div className="schedule-calendar-body"><div className="schedule-calendar-week">{(words?.weekdays??['일','월','화','수','목','금','토']).map(day=><span key={day}>{day}</span>)}</div>
      <div className="schedule-calendar-days" style={{gridTemplateRows:`repeat(${grid.rows},minmax(var(--canvas-control-min,44px),1fr))`,minHeight:`calc(${grid.rows} * var(--canvas-control-min,44px))`}}>{Array.from({length:grid.rows*7},(_,index)=>{
        const day=index-first+1;if(day<1||day>days)return <span key={index}/>;
        const slot=calendarDaySlot(day,count,days,!!custom),activity=context.activities.find(a=>a.id===context.plan[slot]);
        const selected=!!(planner.mask&(1<<slot));const chars=Array.from(activity?.label??'');
        return <button key={index} type="button" data-calendar-day={day} data-slot={slot} aria-label={`${words?words.day+' '+day:day+'일'} · ${selected?activity?.label??'':'미선택'}`} title={activity?.label} className={slot===planner.slot?'selected-slot':''} disabled={!editable||slot<context.period} onClick={()=>chooseSlot(slot)}>
          <span>{day}</span>{selected&&activity&&(activity.iconAssetId?<img src={assetUrl(activity.iconAssetId)} alt=""/>:<small className="calendar-activity-name">{chars.slice(0,4).join('')}{chars.length>4?'…':''}</small>)}
        </button>;
      })}</div></div>
    </div>;
  }
  if(!readable&&element.scheduleLayout==='menu'){
    const group=groups.find(g=>g.id===planner.groupId);
    const available=Math.max(1,element.width-2*(element.padding??0)-2*(element.borderWidth??0)-16);
    const columns=Math.max(1,Math.floor((available+16)/236));
    const frame=element.frameAssetId?{backgroundImage:`url(${JSON.stringify(assetUrl(element.frameAssetId))})`,backgroundSize:'100% 100%'}:undefined;
    const entryFrame=element.entryFrameAssetId?{backgroundImage:`url(${JSON.stringify(assetUrl(element.entryFrameAssetId))})`,backgroundSize:'100% 100%'}:undefined;
    return <div className="schedule-menu" data-testid="schedule-menu">{forecast}
      {group?<><div className="schedule-menu-heading">{group.label} · {labels[planner.slot]}</div><div className="schedule-activity-grid" style={{gridTemplateColumns:`repeat(${columns},minmax(0,1fr))`}}>
        {group.activityIds.flatMap(id=>{const activity=context.activities.find(a=>a.id===id);if(!activity)return [];const summary=activityPreview(activity,context.stats);
          const costs=(activity.costs??[]).map(cost=>`${context.stats.find(s=>s.id===cost.attributeId)?.label??cost.attributeId} ${cost.amount}`).join(' · ');
          return [<button key={id} type="button" data-activity-id={id} disabled={!editable} onClick={()=>chooseActivity(id)} style={entryFrame}>
            {activity.iconAssetId&&<img src={assetUrl(activity.iconAssetId)} alt=""/>}<span className="schedule-activity-title" title={activity.label}>{activity.label}</span>
            {costs&&<small>{costs}</small>}{summary&&<small className="activity-preview">{summary}</small>}
          </button>];})}
      </div><button type="button" className="schedule-menu-back" disabled={!editable} onClick={()=>set({...planner,groupId:''})}>분류 목록</button></>:
      <div className="schedule-categories" style={frame}>
        <div>{labels[planner.slot]} 일정</div>
        {planner.mask===full?<><button type="button" disabled={!editable} onClick={onCommit}>{context.schedulePlayback==='continuous'?'월 일정 시작':context.period===0?'첫 일정 시작':'다음 일정 진행'}</button><button type="button" disabled={!editable} onClick={()=>set({...planner,mask:(1<<context.period)-1,slot:context.period,groupId:''})}>남은 일정 변경</button></>:
        groups.map(value=><button key={value.id} type="button" disabled={!editable} onClick={()=>set({...planner,groupId:value.id})}>{value.label}</button>)}
        {!groups.length&&<p>일정 분류가 없습니다.</p>}
      </div>}
    </div>;
  }
  // Detailed canvas schedules keep every period editable while presenting prose
  // only once, at full width. Readable/roster planners retain their own contract.
  if(!readable&&(element.showActivityRules||element.showActivityDescription||element.showPhaseForecast)){
    const slot=Math.max(0,Math.min(count-1,planner.slot));
    const activity=context.activities.find(a=>a.id===context.plan[slot]);
    const summary=activity?activityPreview(activity,context.stats):'';
    const words=(ko:string,en:string,ja:string)=>context.locale?.startsWith('en')?en:context.locale?.startsWith('ja')?ja:ko;
    const detailLabels=context.calendarProject?.calendar?labels:labels.map((_,index)=>words(`일정 ${index+1}`,`Period ${index+1}`,`予定 ${index+1}`));
    const inspect=(index:number)=>set({...planner,slot:index});
    const edit=(index:number,id:string)=>{if(!editable||index<context.period)return;onPlan?.(index,id);set({...planner,slot:index,mask:planner.mask|(1<<index),groupId:''});};
    return <div className="schedule-detail-planner">
      <div className="schedule-detail-periods">{detailLabels.map((label,index)=><div className="schedule-detail-period" key={index}>
        <button type="button" className="schedule-period-review" aria-pressed={slot===index} aria-controls={`plan-details-${element.id}`} onClick={()=>inspect(index)}>{label}{words(' · 자세히',' · Details',' · 詳細')}</button>
        <select aria-label={`${label} ${words('활동','activity','活動')}`} value={context.plan[index]??''} disabled={!editable||index<context.period} onChange={event=>edit(index,event.target.value)}>
          {context.activities.map(value=><option key={value.id} value={value.id}>{value.label}</option>)}
        </select>
      </div>)}</div>
      <div className={`schedule-draft-note${element.showPhaseForecast?' has-forecast':''}`}>{forecast}<span>{words('확정할 때까지 변경은 초안에 남습니다.',"Changes stay in your draft until you confirm.",'確定するまで変更は下書きに保存されます。')}</span></div>
      <section key={`${slot}:${activity?.id}`} id={`plan-details-${element.id}`} className="schedule-detail-pane" tabIndex={0} aria-label={words('선택한 일정 상세','Selected period details','選択した予定の詳細')}>
        <strong className="schedule-detail-title">{detailLabels[slot]} · {activity?.label}</strong>
        {element.showActivityDescription&&activity?.description&&<div className="schedule-detail-text activity-description">{activity.description}</div>}
        {element.showActivityRules&&activity&&<div className="schedule-detail-text activity-rules">{activityRuleLines(activity,{attributes:context.stats,...(context.locale?{locale:context.locale}:{})}).map((line,index)=><div key={index}>{line}</div>)}</div>}
        {summary&&<div className="schedule-detail-text activity-preview">{summary}</div>}
      </section>
    </div>;
  }
  return <>{forecast}<div className="schedule-columns">{labels.map((period,index)=>{
    const activity=context.activities.find(a=>a.id===context.plan[index]);const summary=activity?activityPreview(activity,context.stats):'';
    return <label key={period}>{period}{readable&&activity&&<div className="readable-selected-activity">{activity.label}</div>}<select aria-label={`${period} 활동`} value={context.plan[index]??''} disabled={!editable||index<context.period} onChange={event=>chooseActivity(event.target.value,index)}>
      {context.activities.map(value=><option key={value.id} value={value.id}>{value.label}{value.previewAttributes?.length?` · ${activityPreview(value,context.stats)}`:''}</option>)}
    </select>{element.showActivityRules&&activity&&<small className="activity-preview activity-rules">{activityRuleLines(activity,{attributes:context.stats,...(context.locale?{locale:context.locale}:{})}).map((line,index)=><div key={index}>{line}</div>)}</small>}{summary&&<small className="activity-preview">{summary}</small>}{element.showActivityDescription&&activity?.description&&<small className="activity-preview activity-description">{activity.description}</small>}</label>;
  })}</div>{readable&&element.scheduleLayout==='menu'&&<button className="readable-schedule-commit" disabled={!editable} onClick={onCommit}>{context.locale?.startsWith('en')?'Confirm schedule':context.locale?.startsWith('ja')?'予定を確定':'일정 확정'}</button>}</>;
}

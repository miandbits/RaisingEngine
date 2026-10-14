import { periodLabel } from '../../../packages/player-service/schedule.js';
import React, {useEffect, useState} from 'react';
import type {Element} from '../../../packages/visual/model.js';
import {activityPreview} from '../../../packages/visual/activity-summary.js';

export interface ScheduleActivity {
  id:string; label:string; iconAssetId?:string;
  costs?:readonly {attributeId:string;amount:number}[];
  previewAttributes?:readonly {attributeId:string;label?:string;valueFormat?:string}[];
}
export interface ScheduleContext {
  activities:readonly ScheduleActivity[]; plan:readonly string[]; period:number; phase:string;
  stats:readonly {id:string;label:string;value:number}[];
  periodsPerMonth?:number; schedulePlayback?:string;
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
export function ScheduleWidget({element,context,planning,groups,assetUrl,disabled,onPlan,onCommit}: {
  element:Element;context:ScheduleContext;planning:SchedulePlanner;
  groups:readonly {id:string;label:string;activityIds:readonly string[]}[];
  assetUrl:(id:string)=>string;disabled:boolean;
  onPlan?:((slot:number,id:string)=>void)|undefined;onCommit?:(()=>void)|undefined;
}){
  const count=context.periodsPerMonth??3,full=(1<<count)-1;
  const labels=Array.from({length:count},(_,i)=>periodLabel(i,count));
  const {planner,set}=planning,editable=!disabled&&['planning','ready'].includes(context.phase);
  const chooseSlot=(slot:number)=>{if(!editable||slot<context.period)return;set({...planner,slot,mask:planner.mask&~(1<<slot),groupId:''});};
  const chooseActivity=(id:string,slot=planner.slot)=>{
    if(!editable||slot<context.period)return;
    onPlan?.(slot,id);const mask=planner.mask|(1<<slot);
    let next=slot;for(let n=1;n<=count;n++){const candidate=(slot+n)%count;if(candidate>=context.period&&!(mask&(1<<candidate))){next=candidate;break;}}
    set({...planner,mask,slot:next,groupId:''});
  };
  if(element.scheduleLayout==='calendar'){
    const {year,month}=context.calendarTime??{year:1,month:1};
    const date=new Date(0);date.setUTCHours(0,0,0,0);date.setUTCFullYear(year,month-1,1);
    const first=date.getUTCDay();date.setUTCMonth(month,0);const days=date.getUTCDate();
    return <div className="schedule-calendar" data-testid="schedule-calendar">
      <div className="schedule-calendar-title">{year}년 {month}월</div>
      <div className="schedule-calendar-week">{['일','월','화','수','목','금','토'].map(day=><span key={day}>{day}</span>)}</div>
      <div className="schedule-calendar-days">{Array.from({length:42},(_,index)=>{
        const day=index-first+1;if(day<1||day>days)return <span key={index}/>;
        const slot=Math.min(count-1,Math.floor((day-1)/(count===3?10:Math.floor(days/count)))),activity=context.activities.find(a=>a.id===context.plan[slot]);
        const selected=!!(planner.mask&(1<<slot));const chars=Array.from(activity?.label??'');
        return <button key={index} type="button" data-calendar-day={day} data-slot={slot} aria-label={`${day}일 · ${selected?activity?.label??'':'미선택'}`} title={activity?.label} className={slot===planner.slot?'selected-slot':''} disabled={!editable||slot<context.period} onClick={()=>chooseSlot(slot)}>
          <span>{day}</span>{selected&&activity&&(activity.iconAssetId?<img src={assetUrl(activity.iconAssetId)} alt=""/>:<small className="calendar-activity-name">{chars.slice(0,4).join('')}{chars.length>4?'…':''}</small>)}
        </button>;
      })}</div>
    </div>;
  }
  if(element.scheduleLayout==='menu'){
    const group=groups.find(g=>g.id===planner.groupId);
    const available=Math.max(1,element.width-2*(element.padding??0)-2*(element.borderWidth??0)-16);
    const columns=Math.max(1,Math.floor((available+16)/236));
    const frame=element.frameAssetId?{backgroundImage:`url(${JSON.stringify(assetUrl(element.frameAssetId))})`,backgroundSize:'100% 100%'}:undefined;
    const entryFrame=element.entryFrameAssetId?{backgroundImage:`url(${JSON.stringify(assetUrl(element.entryFrameAssetId))})`,backgroundSize:'100% 100%'}:undefined;
    return <div className="schedule-menu" data-testid="schedule-menu">
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
  return <div className="schedule-columns">{labels.map((period,index)=>{
    const activity=context.activities.find(a=>a.id===context.plan[index]);const summary=activity?activityPreview(activity,context.stats):'';
    return <label key={period}>{period}<select aria-label={`${period} 활동`} value={context.plan[index]??''} disabled={!editable||index<context.period} onChange={event=>chooseActivity(event.target.value,index)}>
      {context.activities.map(value=><option key={value.id} value={value.id}>{value.label}{value.previewAttributes?.length?` · ${activityPreview(value,context.stats)}`:''}</option>)}
    </select>{summary&&<small className="activity-preview">{summary}</small>}</label>;
  })}</div>;
}

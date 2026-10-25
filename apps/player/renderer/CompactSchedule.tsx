import React,{useEffect,useState,useRef} from 'react';
import {slotWords} from '../../../packages/player-service/save-slots.js';
import {activityRuleLines} from '../../../packages/visual/activity-rules.js';
import {ReadingDialog} from './ReadingDialog.js';
import type {ScheduleActivity,ScheduleContext} from './ScheduleWidget.js';

export function CompactSchedule({id,context,labels,groups,disabled,chooseActivity}:{id:string;context:ScheduleContext;labels:string[];groups:readonly {id:string;label:string;activityIds:readonly string[]}[];disabled:boolean;chooseActivity:(id:string,slot:number)=>void}) {
  const details=useRef<HTMLHeadingElement>(null);
  const [draft,setDraft]=useState<{slot:number;activityId:string;groupId:string}|null>(null);
  const words=(ko:string,en:string,ja:string)=>slotWords(context.locale,ko,en,ja);
  const editable=!disabled&&['planning','ready'].includes(context.phase),key=context.planningKey??`${context.calendarTime?.year}:${context.calendarTime?.month}`;
  useEffect(()=>setDraft(null),[key,context.phase]);
  const selected=context.activities.find(activity=>activity.id===draft?.activityId);
  const group=groups.find(group=>group.id===draft?.groupId);
  const shown=group?context.activities.filter(activity=>group.activityIds.includes(activity.id)):context.activities;
  const choose=(activity:ScheduleActivity)=>{setDraft(previous=>previous?{...previous,activityId:activity.id}:null);requestAnimationFrame(()=>{details.current?.scrollIntoView({block:'start'});details.current?.focus({preventScroll:true});});};
  return <div className="compact-schedule" data-testid="compact-schedule">
    <div className="compact-schedule-slots">{labels.map((label,slot)=>{const activity=context.activities.find(activity=>activity.id===context.plan[slot]);return <button key={slot} type="button" data-reading-id={`board-slot-${slot}`} disabled={!editable||slot<context.period} aria-haspopup="dialog" onClick={()=>setDraft({slot,activityId:activity?.id??'',groupId:''})}>
      <strong>{label}</strong><span>{activity?.label??words('활동 선택','Choose an activity','活動を選択')}</span>
    </button>;})}</div>
    {draft&&<ReadingDialog id={`${id}-picker`} title={words('활동 선택','Choose an activity','活動を選択')+' · '+labels[draft.slot]} onClose={()=>setDraft(null)}>
      <div className="reading-dialog-body">
        {groups.length>0&&<div className="compact-schedule-groups" aria-label={words('분류','Categories','分類')}>
          <button type="button" data-reading-id="board-group-all" aria-pressed={!draft.groupId} onClick={()=>setDraft({...draft,groupId:''})}>{words('전체','All','すべて')}</button>
          {groups.map(group=><button type="button" key={group.id} data-reading-id={`board-group-option-${group.id}`} aria-pressed={draft.groupId===group.id} onClick={()=>setDraft({...draft,groupId:group.id})}>{group.label}</button>)}
        </div>}
        <div className="compact-schedule-activities" aria-label={words('활동','Activities','活動')}>{shown.map(activity=><button type="button" key={activity.id} data-reading-id={`board-activity-${activity.id}`} aria-pressed={draft.activityId===activity.id} disabled={!editable} onClick={()=>choose(activity)}>{activity.label}</button>)}</div>
        {selected&&<section className="compact-schedule-detail" aria-label={words('선택한 활동','Selected activity','選択した活動')}>
          <h3 ref={details} tabIndex={-1} data-reading-id="board-details">{selected.label}</h3>
          {activityRuleLines(selected,{attributes:context.stats,...(context.locale?{locale:context.locale}:{})}).map((line,index)=><p key={index}>{line}</p>)}{selected.description&&<p>{selected.description}</p>}
        </section>}
      </div>
      <div className="reading-dialog-actions"><button type="button" data-reading-id="board-apply" disabled={!editable||draft.slot<context.period||!selected} onClick={()=>{if(selected){chooseActivity(selected.id,draft.slot);setDraft(null);}}}>{words('적용','Apply','適用')}</button><button type="button" data-reading-id="board-cancel" onClick={()=>setDraft(null)}>{words('취소','Cancel','キャンセル')}</button></div>
    </ReadingDialog>}
  </div>;
}

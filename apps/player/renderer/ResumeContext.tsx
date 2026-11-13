import React,{createContext,useContext} from 'react';
import type {PlayerView} from '../../../packages/player-service/controller.js';
import {slotWords} from '../../../packages/player-service/save-slots.js';
import {resolveResumeContext,type ResumeContextSnapshot} from '../../../packages/visual/resume-context.js';
import {numericBinding,userFieldBinding} from '../../../packages/visual/bindings.js';
import {calendarGrid,calendarLabels} from '../../../packages/core/calendar.js';
import {readableDefaults} from '../../../packages/visual/readable-layout.js';
import {readingHistoryDate} from './ReadingHistory.js';
import {ReadingDialog} from './ReadingDialog.js';
export const ResumeContextContext=createContext<{enabled:boolean;active:boolean;open:()=>void}|null>(null);
export const useResumeContext=()=>useContext(ResumeContextContext);
export function resumeContextForView(view:PlayerView):ResumeContextSnapshot {
  if(!view.visual||!view.state)return {entries:[],utf8Bytes:0,omittedEntries:0,truncated:false};
  const state=view.state,phase=view.narrative?.node.type==='choice'?'choice':view.narrative?.node.type==='dialogue'?'dialogue':state.phase;
  const values={stats:view.attributes.map(a=>({...a,value:state.attributes[a.id]??a.initial})),relationships:view.characters.flatMap(c=>c.relationships.map(r=>({characterId:c.id,relationshipId:r.id,name:c.name,label:r.label,value:state.relationships[c.id]?.[r.id]??r.initial,delta:view.relationshipChanges?.[c.id]?.[r.id]??0}))),items:view.gameplayDefinition?.items??[],gameplay:view.gameplay};
  return resolveResumeContext(view.visual,{state,phase,evaluate:condition=>view.dataUi?.conditions[JSON.stringify(condition)]===true,text:element=>{
    const bound=userFieldBinding(element,state.userValues)??numericBinding(element,values);if(bound!==null)return bound;
    const binding=element.binding;if(binding==='location')return view.gameplay?.location.name??'';
    if(binding==='calendar')return readingHistoryDate(view.time,view.calendarProject?.calendar,view.locale);
    if(binding?.startsWith('calendar-')){
      const date=view.time;if(!date)return '';
      if(binding==='calendar-year')return String(date.year);
      const project=view.calendarProject??{startYear:date.year,startMonth:date.month,periodsPerMonth:view.periodsPerMonth??3};
      const grid=calendarGrid(project,date),labels=calendarLabels(project.calendar??{monthsPerYear:12,daysPerMonth:28},view.locale);
      if(binding==='calendar-month')return `${labels.month} ${date.month}`;
      if(binding==='calendar-day')return String(grid.day);
      return labels.weekdays[(grid.offset+grid.day-1)%7]??'';
    }
    return element.text??'';
  }});
}
export function CurrentReminders({snapshot,onClose,locale,date,phase,textScale=1,background=readableDefaults.background}:{snapshot:ResumeContextSnapshot;onClose:()=>void;locale?:string|undefined;date:string;phase:string;textScale?:number;background?:string|undefined}){
  const words=(ko:string,en:string,ja:string)=>slotWords(locale,ko,en,ja),rgb=parseInt(background.slice(1),16),ink=.2126*((rgb>>16)&255)+.7152*((rgb>>8)&255)+.0722*(rgb&255)>145?'#263c36':'#f5efdf';
  return <div className="resume-context-host" style={{fontSize:18*textScale,'--readable-background':background,'--readable-ink':ink} as React.CSSProperties}>
    <ReadingDialog id="engine-resume-context-panel" title={words('현재 상황','Current reminders','現在のメモ')} onClose={onClose} initialFocus="heading">
      <div className="reading-dialog-body resume-context-body">
        <p data-reading-id="engine-resume-context-current">{[date,phase].filter(Boolean).join(' · ')}</p>
        {!snapshot.entries.length&&<p data-reading-id="engine-resume-context-empty">{words('현재 진행에 표시할 메모가 없습니다.','There are no reminders to show at this point.','現在の進行に表示するメモはありません。')}</p>}
        {snapshot.entries.map((entry,index)=><p key={entry.elementId} data-reading-id={`engine-resume-context-entry-${index}`} data-reminder-element={entry.elementId}>{entry.text}</p>)}
        {snapshot.truncated&&<p data-reading-id="engine-resume-context-truncated">{words('일부 메모를 줄이거나 생략했습니다.','Some reminders were shortened or omitted.','一部のメモを短縮または省略しました。')}</p>}
        <p className="resume-context-notice">{words('현재 진행에 맞춘 안내입니다.','Notes for your current progress.','現在の進行に合わせたメモです。')}</p>
      </div>
      <div className="reading-dialog-actions"><button data-reading-id="engine-resume-context-close" onClick={onClose}>{words('계속하기','Continue','続ける')}</button></div>
    </ReadingDialog>
  </div>;
}

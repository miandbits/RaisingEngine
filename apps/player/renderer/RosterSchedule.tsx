import React, {useEffect, useState} from 'react';
import type {VisualContext} from './VisualSurface.js';
import type {WidgetCommand} from './JourneyWidgets.js';

/** A roster uses actor assignments, not the legacy monthly schedule command. */
export function RosterSchedule({context, disabled, onCommand, onExecute, showActivityDescription=false}: {
  context: VisualContext; disabled: boolean;
  showActivityDescription?: boolean;
  onCommand: ((command: WidgetCommand) => void) | undefined; onExecute: () => void;
}) {
  const state=context.dataState, assignments=state?.roster?.assignments??{};
  const [draft,setDraft]=useState({...assignments});
  const revision=state?.stateRevision, key=JSON.stringify(assignments);
  useEffect(()=>{setDraft(JSON.parse(key));},[key,revision,context.planningKey]);
  const editable=!disabled&&['planning','ready'].includes(context.phase);
  const changed=Object.keys(assignments).some(id=>draft[id]!==assignments[id]);
  return <div className="roster-schedule">
    <div>이번 기간 · 캐릭터별 활동 (함께 정산)</div>
    <div className="roster-schedule-actors">{Object.keys(assignments).map(id=><label key={id}>
      <span>{context.characters?.find(c=>c.id===id)?.name??id}</span>
      <select aria-label={`${context.characters?.find(c=>c.id===id)?.name??id} 활동`} value={draft[id]??assignments[id]} disabled={!editable}
        onChange={event=>setDraft(previous=>({...previous,[id]:event.target.value}))}>
        {context.activities.map(activity=><option key={activity.id} value={activity.id}>{activity.label}</option>)}
      </select>
      {showActivityDescription&&<small id={`roster-description-${id}`} style={{display:'block',maxHeight:100,overflow:'auto',whiteSpace:'normal',lineHeight:'25px',paddingTop:10}}>
        {context.activities.find(activity=>activity.id===(draft[id]??assignments[id]))?.description??''}
      </small>}
    </label>)}</div>
    <div className="roster-schedule-actions">
      <button disabled={!editable||!onCommand} onClick={()=>onCommand?.({type:'assignActivities',assignments:{...draft}})}>활동 배정 적용</button>
      <button disabled={!editable||changed||context.phase!=='ready'} onClick={onExecute}>배정한 활동 함께 실행</button>
    </div>
  </div>;
}

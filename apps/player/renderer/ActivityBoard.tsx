import React,{useEffect,useRef,useState} from 'react';
import {calendarLabels} from '../../../packages/core/calendar.js';
import {activityRequirementText} from '../../../packages/visual/activity-requirements.js';
import {activityRuleLines} from '../../../packages/visual/activity-rules.js';
import {slotWords} from '../../../packages/player-service/save-slots.js';
import type {ScheduleDraft} from '../../../packages/player-service/save-metadata.js';
import {firstDraftSlot} from '../../../packages/player-service/activity-board.js';
import type {Element} from '../../../packages/visual/model.js';
import type {ScheduleContext,SchedulePlanner} from './ScheduleWidget.js';
import {PhaseForecast} from './PhaseForecast.js';
import {PlanReuse} from './PlanReuse.js';
import {PlanningGuide,type PlanningControl} from './PlanningGuide.js';
import {ReadingDialog} from './ReadingDialog.js';
import {usePlayerPreferences} from './PlayerSettings.js';
import {planningGuidanceSeen,acknowledgePlanningGuidance,claimPlanningGuidance,releasePlanningGuidance} from './planning-guidance.js';

type Review={token:string;slot:number;activityId:string;groupId:string};
/** The inspected activity is ephemeral. Only Apply publishes an assignment to the draft. */
export function ActivityBoard({element,context,planning,assetUrl,ownerKey='',disabled,readable,guidance,planReuse,control,onPlanReplace}:{element:Element;context:ScheduleContext;planning:SchedulePlanner;assetUrl:(id:string)=>string;ownerKey?:string;disabled:boolean;readable:boolean;guidance:boolean;planReuse:boolean;control?:PlanningControl|undefined;onPlanReplace?:((ids:string[])=>void)|undefined}) {
  const words=(ko:string,en:string,ja:string)=>slotWords(context.locale,ko,en,ja),count=context.periodsPerMonth??3;
  const id=(suffix:string)=>`${element.id}__board-${suffix}`,attrs=(suffix:string)=>({id:id(suffix),'data-reading-id':id(suffix)});
  const first=context.dataState?firstDraftSlot(context.dataState,count):context.period;
  const editable=!disabled&&['planning','ready'].includes(context.phase)&&first<count;
  const draft:ScheduleDraft=context.scheduleDraft??{month:Math.floor((context.dataState?.periodIndex??context.period)/count),slot:planning.planner.slot,selected:planning.planner.mask,activities:[...context.plan]};
  const groups=element.scheduleGroups??[],labels=Array.from({length:count},(_,i)=>`${context.calendarProject?.calendar?calendarLabels(context.calendarProject.calendar,context.locale).period:words('일정','Period','予定')} ${i+1}`);
  const token=JSON.stringify([ownerKey,element.id,context.gameId,context.generation,context.dataState?.stateRevision,context.phase,context.dataState?.periodIndex,context.dataState?.activityIntermission,context.dataState?.schedule,draft,context.activities,context.locale,groups,element.activityIcons,element.entryFrameAssetId,disabled]);
  const latest=useRef(token);latest.current=token;
  const [review,setReview]=useState<Review|null>(null),[guide,setGuide]=useState<number|null>(null),guideOwner=useRef(Symbol('activity-board-guide')),details=useRef<HTMLDivElement>(null),preferences=usePlayerPreferences();
  const current=review?.token===token&&editable?review:null,slot=current?.slot??Math.min(count-1,Math.max(first,draft.slot));
  const pendingId=current?.activityId??((draft.selected&(1<<slot))?draft.activities[slot]:'');
  const selected=context.activities.find(activity=>activity.id===pendingId),group=groups.find(group=>group.id===current?.groupId);
  const shown=group?context.activities.filter(activity=>group.activityIds.includes(activity.id)):context.activities;
  const close=()=>setReview(null),closeGuide=()=>{acknowledgePlanningGuidance(context.gameId);releasePlanningGuidance(guideOwner.current);setGuide(null);};
  const openGuide=()=>{if(!document.querySelector('dialog[open],.authored-slot-modal')&&claimPlanningGuidance(guideOwner.current)){close();setGuide(0);}};
  useEffect(()=>{setReview(previous=>previous?.token===token&&editable?previous:null);setGuide(null);releasePlanningGuidance(guideOwner.current);},[token,editable]);
  useEffect(()=>()=>releasePlanningGuidance(guideOwner.current),[]);
  useEffect(()=>{if(guidance&&editable&&context.gameId&&!preferences?.readingOverlayActive&&!planningGuidanceSeen(context.gameId))openGuide();},[guidance,editable,context.gameId,context.planningKey,preferences?.readingOverlayActive]);
  useEffect(()=>{const clear=()=>{close();setGuide(null);releasePlanningGuidance(guideOwner.current);};window.addEventListener('raising-open-reading-history',clear);window.addEventListener('raising-open-resume-context',clear);return()=>{window.removeEventListener('raising-open-reading-history',clear);window.removeEventListener('raising-open-resume-context',clear);};},[]);
  const inspectPeriod=(index:number)=>{if(!editable||index<first||document.querySelector('dialog[open],.authored-slot-modal'))return;setReview({token,slot:index,activityId:draft.selected&(1<<index)?draft.activities[index]??'':'',groupId:''});};
  const inspectActivity=(activityId:string)=>{if(!editable||latest.current!==token)return;setReview({token,slot,activityId,groupId:current?.groupId??''});requestAnimationFrame(()=>{if(readable)details.current?.scrollIntoView({block:'start'});if(details.current)details.current.scrollTop=0;details.current?.focus({preventScroll:true});});};
  const publish=(next:ScheduleDraft)=>{
    if(!editable||latest.current!==token)return;
    if(context.onScheduleDraft)context.onScheduleDraft(next,{screenId:ownerKey,elementId:element.id});
    else {onPlanReplace?.([...next.activities]);planning.set(previous=>({...previous,slot:next.slot,mask:next.selected,groupId:''}));}
    close();
  };
  const apply=()=>{if(current&&current.token===latest.current&&selected&&current.slot>=first){const next={...draft,slot:current.slot,selected:draft.selected|(1<<current.slot),activities:[...draft.activities]};next.activities[current.slot]=selected.id;publish(next);}};
  const icon=(activityId:string)=>element.activityIcons?.find(icon=>icon.activityId===activityId)?.imageAssetId;
  const frame=element.entryFrameAssetId?{backgroundImage:`url(${JSON.stringify(assetUrl(element.entryFrameAssetId))})`,backgroundSize:'100% 100%'}:undefined;
  const artwork=(activityId:string)=>{const image=icon(activityId);return image?<img className="activity-board-image" src={assetUrl(image)} alt="" draggable={false}/>:null;};
  const content=<>
    <div className="activity-board-categories" role="group" aria-label={words('분류','Categories','分類')}>
      <button type="button" {...attrs('group-all')} disabled={!editable} aria-pressed={!current?.groupId} onClick={()=>setReview({token,slot,activityId:pendingId??'',groupId:''})}>{words('전체','All','すべて')}</button>
      {groups.map(value=><button type="button" key={value.id} {...attrs('group-option-'+value.id)} disabled={!editable} aria-pressed={current?.groupId===value.id} onClick={()=>setReview({token,slot,activityId:pendingId??'',groupId:value.id})}>{value.iconAssetId&&<img src={assetUrl(value.iconAssetId)} alt=""/>}{value.label}</button>)}
    </div>
    <div className="activity-board-workspace">
      <div {...attrs('cards')} className="activity-board-cards" role="group" aria-label={words('활동','Activities','活動')}>{shown.map(activity=><button type="button" key={activity.id} {...attrs('activity-'+activity.id)} className="activity-board-card" style={frame} disabled={!editable} aria-pressed={pendingId===activity.id} aria-controls={id('details')} onClick={()=>inspectActivity(activity.id)}>{artwork(activity.id)}<strong>{activity.label}</strong></button>)}</div>
      <section {...attrs('details')} className="activity-board-details" aria-label={words('활동 살펴보기','Inspect activity','活動の確認')}>
        <div ref={details} tabIndex={0} className="activity-board-detail-body"><div className="activity-board-detail-title">{labels[slot]}{selected?' · '+selected.label:''}</div>
        {selected?<>{element.showActivityDescription&&selected.description&&<p>{selected.description}</p>}{element.showActivityRules&&<div className="activity-board-rules"><p>{activityRequirementText(selected.requirements,context.stats,context.locale)}</p>{activityRuleLines(selected,{attributes:context.stats,...(context.locale?{locale:context.locale}:{})}).map((line,index)=><p key={index}>{line}</p>)}</div>}</>:<p>{words('활동 카드를 눌러 설명, 비용과 효과를 확인하세요.','Choose an activity card to inspect its description, costs and effects.','活動カードを選び、説明・費用・効果を確認してください。')}</p>}
        <p className="activity-board-note">{words('적용은 초안만 바꿉니다. 시간과 자원은 활동 실행 시 변합니다.','Apply changes the draft only. Time and resources change when activities run.','適用は下書きだけを変更します。時間と資源は活動の実行時に変わります。')}</p></div>
        {!readable&&<div className="activity-board-actions"><button type="button" {...attrs('apply')} disabled={!current||!selected} onClick={apply}>{words('초안에 적용','Apply to draft','下書きに適用')}</button><button type="button" {...attrs('cancel')} disabled={!current} onClick={close}>{words('취소','Cancel','キャンセル')}</button></div>}
      </section>
    </div>
  </>;
  return <div className={`activity-board${readable?' activity-board-readable':''}`} data-testid="activity-board" onKeyDown={event=>{if(event.key==='Escape'&&current){event.preventDefault();event.stopPropagation();close();}}}>
    <div {...attrs('periods')} className="activity-board-periods" role="group" aria-label={words('기간별 초안','Period draft','期間ごとの下書き')}>{labels.map((label,index)=>{const assignment=draft.selected&(1<<index)?context.activities.find(activity=>activity.id===draft.activities[index]):null;return <button type="button" key={index} {...attrs('slot-'+index)} className="activity-board-ticket" disabled={!editable||index<first} aria-pressed={slot===index} aria-haspopup={readable?'dialog':undefined} onClick={()=>inspectPeriod(index)}><span className="activity-board-period">{label} · {index<first?words('진행 완료','Already progressed','進行済み'):words('초안','Draft','下書き')}</span><span className="activity-board-assignment">{assignment&&artwork(assignment.id)}<strong>{assignment?.label??words('활동 선택','Choose activity','活動を選択')}</strong></span></button>;})}</div>
    <div className="activity-board-tools">{element.showPhaseForecast&&<PhaseForecast id={element.id} context={{...context,forecastMask:draft.selected,plan:draft.activities.map((activity,index)=>draft.selected&(1<<index)?activity:'')}} disabled={disabled}/>}<button type="button" {...attrs('help')} disabled={!editable} onClick={openGuide}>{words('계획 세우는 방법','How planning works','計画の立て方')}</button>{planReuse&&<PlanReuse compact controlPrefix={element.id} id={element.id} context={context} labels={labels} disabled={!editable} selectionKey={token} apply={ids=>publish({...draft,slot:Math.min(first,count-1),selected:(1<<count)-1,activities:ids})}/>}</div>
    {!readable&&content}
    {readable&&current&&<ReadingDialog initialFocus="heading" id={id('review')} title={labels[slot]??''} onClose={close}><div className="reading-dialog-body activity-board-review" tabIndex={0}>{content}</div><div className="reading-dialog-actions activity-board-actions"><button type="button" {...attrs('apply')} disabled={!selected} onClick={apply}>{words('초안에 적용','Apply to draft','下書きに適用')}</button><button type="button" {...attrs('cancel')} onClick={close}>{words('취소','Cancel','キャンセル')}</button></div></ReadingDialog>}
    {guide!==null&&<PlanningGuide step={guide} onStep={setGuide} onClose={closeGuide} labels={labels} period={first} hasCategories={groups.length>0} control={control} locale={context.locale}/>}
  </div>;
}

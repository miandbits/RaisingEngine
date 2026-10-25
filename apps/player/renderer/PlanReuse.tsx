import React,{useEffect,useState} from 'react';
import {planReuseDraft} from '../../../packages/player-service/plan-reuse.js';
import {slotWords} from '../../../packages/player-service/save-slots.js';
import {activityRuleLines} from '../../../packages/visual/activity-rules.js';
import {ReadingDialog} from './ReadingDialog.js';
import type {ScheduleContext} from './ScheduleWidget.js';

/** A review snapshot is presentation state only. Every Apply rechecks its full owner identity. */
export function PlanReuse({id,context,labels,disabled,selectionKey,apply}:{id:string;context:ScheduleContext;labels:string[];disabled:boolean;selectionKey:string;apply:(ids:string[])=>void}) {
  const words=(ko:string,en:string,ja:string)=>slotWords(context.locale,ko,en,ja);
  const candidate=planReuseDraft(context.lastCommittedPlan,context.dataState,labels.length,context.activities.map(a=>a.id));
  const token=JSON.stringify([id,context.planningKey,context.generation,context.dataState?.stateRevision,context.phase,context.dataState?.periodIndex,context.dataState?.activityIntermission,context.dataState?.schedule,context.lastCommittedPlan,context.plan,context.activities,labels,selectionKey,disabled]);
  const [review,setReview]=useState<{token:string;activityIds:string[];firstEditableSlot:number}|null>(null);
  const editable=!disabled&&['planning','ready'].includes(context.phase),current=review?.token===token&&editable&&candidate.status==='ready';
  useEffect(()=>{setReview(previous=>previous?.token===token&&editable?previous:null);},[token,editable]);
  const unavailable=candidate.status==='unavailable'?candidate.reason:null;
  const reason=unavailable==='none'?words('아직 확정한 계획이 없습니다.','No confirmed plan yet.','確定した計画はまだありません。')
    :unavailable==='finished'?words('이번 기간에 변경할 일정이 남아 있지 않습니다.','No remaining periods to edit.','この期間に変更できる予定は残っていません。')
    :unavailable==='phase'||!editable?words('계획을 편집할 때 사용할 수 있습니다.','Available while editing a plan.','計画の編集中に利用できます。')
    :words('마지막 계획을 이 일정에 사용할 수 없습니다.','The last confirmed plan is unavailable for this schedule.','最後に確定した計画はこの予定には使えません。');
  return <div className="plan-reuse">
    <button type="button" data-reading-id="board-reuse" aria-haspopup="dialog" aria-describedby={candidate.status!=='ready'||!editable?`${id}-reuse-reason`:undefined} disabled={!editable||candidate.status!=='ready'} onClick={()=>{if(candidate.status==='ready'&&editable&&!document.querySelector('dialog[open],.authored-slot-modal'))setReview({token,activityIds:[...candidate.activityIds],firstEditableSlot:candidate.firstEditableSlot});}}>{words('마지막 확정 계획 사용','Use last confirmed plan','最後に確定した計画を使う')}</button>
    {(candidate.status!=='ready'||!editable)&&<p id={`${id}-reuse-reason`} data-reading-id="board-reuse-notice">{reason}</p>}
    {review&&current&&<ReadingDialog id="board-reuse-review" title={words('계획 검토','Review plan','計画を確認')} onClose={()=>setReview(null)}>
      <div className="reading-dialog-body plan-reuse-body">
        <p>{words('초안만 변경합니다.','Changes the draft only.','下書きだけを変更します。')}</p>
        {review.activityIds.map((activityId,index)=>{const activity=context.activities.find(a=>a.id===activityId)!;return <section key={index} data-reading-id={`board-reuse-slot-${index}`}><h3>{labels[index]} · {activity.label}</h3>{index<review.firstEditableSlot?<p>{words('완료한 배정 유지','Completed assignment kept','完了した割り当てを維持')}</p>:activityRuleLines(activity,{attributes:context.stats,...(context.locale?{locale:context.locale}:{})}).map((line,i)=><p key={i}>{line}</p>)}</section>;})}
        <p data-reading-id="board-reuse-notice">{words('가장 최근에 확정한 배정입니다. 이번 기간에 확정한 계획일 수도 있습니다. 적용하면 남은 일정의 초안만 바뀌며, 시간이나 자원은 변하지 않습니다. 활동을 실행할 때 기존 조건과 비용이 적용됩니다.','These are the most recently confirmed assignments, which may be from this phase. Applying changes only the remaining draft, not time or resources. Existing requirements and costs still apply when activities run.','直近に確定した割り当てです。今の期間に確定した計画の場合もあります。適用しても残りの予定の下書きだけが変わり、時間や資源は変わりません。活動の実行時には既存の条件と費用が適用されます。')}</p>
        {review.activityIds.some((value,i)=>i>=review.firstEditableSlot&&value!==context.plan[i])&&<p>{words('남은 일정의 현재 초안을 표시된 배정으로 바꿉니다.','This replaces your remaining draft with the assignments shown here.','残りの予定の下書きをここに表示された割り当てに置き換えます。')}</p>}
      </div>
      <div className="reading-dialog-actions plan-reuse-actions"><button type="button" data-reading-id="board-reuse-apply" disabled={!current} onClick={()=>{if(current&&review.token===token){apply([...review.activityIds]);setReview(null);}}}>{words('초안에 적용','Apply to draft','下書きに適用')}</button><button type="button" data-reading-id="board-reuse-cancel" onClick={()=>setReview(null)}>{words('취소','Cancel','キャンセル')}</button></div>
    </ReadingDialog>}
  </div>;
}

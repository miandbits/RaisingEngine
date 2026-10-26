import {periodLabel} from '../../../packages/player-service/schedule.js';
import React,{useEffect,useRef,useState} from 'react';
import type {PhaseForecastProjection,PhaseForecastReply} from '../../../packages/player-service/phase-forecast.js';
import {slotWords} from '../../../packages/player-service/save-slots.js';
import type {ScheduleContext} from './ScheduleWidget.js';
import {ReadingDialog} from './ReadingDialog.js';

export function forecastIssueText(code:string,locale?:string):string {
  const words=(ko:string,en:string,ja:string)=>slotWords(locale,ko,en,ja);
  switch(code){
    case 'FORECAST_RANDOM':return words('확률에 따라 결과가 달라집니다.','Random outcomes can change what happens next.','確率によって結果が変わります。');
    case 'FORECAST_NARRATIVE':return words('이야기가 진행된 뒤 다시 확인하세요.','Check again after the story continues.','物語が進んだ後にもう一度確認してください。');
    case 'FORECAST_INTERMISSION':return words('활동 사이의 선택에 따라 이후 결과가 달라집니다.','Choices between activities can change later results.','活動の間の選択によって、その後の結果が変わります。');
    case 'FORECAST_TIMED_ACTIVITY':return words('진행 중 활동의 남은 결과는 아직 정해지지 않았습니다.','This activity has unresolved steps.','この活動には未確定の段階があります。');
    case 'FORECAST_EXPEDITION':return words('탐험 중 선택과 전투 결과는 미리 정할 수 없습니다.','Expedition choices and battles are not determined yet.','探索中の選択や戦闘の結果はまだ決まっていません。');
    case 'FORECAST_ROSTER':return words('여러 인물의 일정 예측은 아직 지원하지 않습니다.','Forecasts for multiple characters are not supported yet.','複数の人物の予定予測にはまだ対応していません。');
    case 'FORECAST_INCOMPLETE':return words('남은 일정의 활동을 모두 선택하세요.','Choose an activity for every remaining period.','残りの予定すべてに活動を選んでください。');
    case 'INSUFFICIENT_COST':return words('이 계획을 진행할 자원이 부족합니다.','There are not enough resources to continue this plan.','この計画を進めるための資源が足りません。');
    case 'REQUIREMENTS_NOT_MET':return words('활동의 조건을 충족하지 못했습니다.','An activity requirement is not met.','活動の条件を満たしていません。');
    case 'COMPLETED_SLOT':return words('이미 진행한 일정은 바꿀 수 없습니다.','Periods already played cannot be changed.','進行済みの予定は変更できません。');
    case 'UNKNOWN_ACTIVITY':return words('계획에 사용할 수 없는 활동이 있습니다.','The plan contains an unavailable activity.','計画に利用できない活動が含まれています。');
    default:return words('현재 계획의 이후 결과는 아직 확인할 수 없습니다.','Later results cannot be determined from the current plan yet.','現在の計画では、その後の結果をまだ確認できません。');
  }
}
const signed=(value:number)=>`${value>0?'+':''}${value}`;
export function PhaseForecast({id,context,disabled=false}:{id:string;context:ScheduleContext;disabled?:boolean}) {
  const words=(ko:string,en:string,ja:string)=>slotWords(context.locale,ko,en,ja);
  const available=!disabled&&['planning','ready'].includes(context.dataState?.phase??context.phase);
  const period=(index:number)=>{const slot=index%(context.periodsPerMonth??3);return context.calendarProject?.calendar?periodLabel(slot,context.periodsPerMonth??3,context.calendarProject.calendar,context.locale):words('일정 '+(slot+1),'Period '+(slot+1),'予定 '+(slot+1));};
  const key=JSON.stringify([context.gameId,context.generation,context.dataState?.stateRevision,context.dataState?.phase,context.dataState?.schedule,context.plan,context.forecastMask,context.locale]);
  const current=useRef(key);current.current=key;
  const serial=useRef(0),cache=useRef<{key:string;reply:PhaseForecastReply|null}|null>(null);
  const [open,setOpen]=useState(false),[reply,setReply]=useState<PhaseForecastReply|null>(null),[loading,setLoading]=useState(false);
  useEffect(()=>{serial.current++;setOpen(false);setReply(null);setLoading(false);},[key]);
  useEffect(()=>()=>{serial.current++;},[]);
  const launch=async()=>{
    if(!available||document.querySelector('dialog[open],.authored-slot-modal'))return;
    const request=++serial.current;setOpen(true);
    if(cache.current?.key===key){setReply(cache.current.reply);setLoading(false);return;}
    setReply(null);setLoading(true);
    let result:PhaseForecastReply|null=null;try{result=await context.forecastPhase?.([...context.plan])??null;}catch{}
    if(request!==serial.current||current.current!==key)return;
    cache.current={key,reply:result};setReply(result);setLoading(false);
  };
  const close=()=>{serial.current++;setOpen(false);setLoading(false);};
  const projection=(value:PhaseForecastProjection,baseline=false)=>{
    const title=baseline?words('활동만 고려한 예상','Activity-only estimate','活動のみの見積もり'):value.complete?words('이번 일정 종료 시','End of this phase','今期の終了時'):words('확인된 변화','Known changes','確認できる変化');
    const changed=value.attributes.filter(row=>row.delta!==0||row.costs!==0||row.effects!==0||row.automatic!==0);
    return <section className="phase-forecast-section" data-forecast-basis={value.basis} data-forecast-status={value.status} key={value.basis}>
      <h3>{title}</h3>
      {baseline&&<p className="phase-forecast-assumptions">{words('이야기 효과와 활동 사이의 선택, 추가 행동을 제외하고 현재 정책을 유지한다고 가정합니다. 실제 결과와 다를 수 있습니다.','Assumes no story effects, between-activity choices, or optional actions, with current policies unchanged. Actual results may differ.','物語の効果、活動の間の選択、追加の行動を除き、現在の方針を維持すると仮定します。実際の結果とは異なる場合があります。')}</p>}
      {!value.complete&&<p>{baseline?words('예상은 여기까지입니다. 이후 결과는 포함하지 않습니다.','The estimate stops here. Later results are not included.','見積もりはここまでです。その後の結果は含まれていません。'):words('여기까지 확인된 변화만 표시합니다. 이번 일정의 최종 수치가 아닙니다.','Only the known part of the plan is shown. These are not end-of-phase values.','確認できる部分だけを表示しています。今期の最終値ではありません。')}</p>}
      {value.lastProjectedPeriodIndex!==null&&<p>{value.rows.at(-1)?.boundaryComplete?words(period(value.lastProjectedPeriodIndex)+'까지','Through '+period(value.lastProjectedPeriodIndex),period(value.lastProjectedPeriodIndex)+'まで'):words(period(value.lastProjectedPeriodIndex)+' 진행 중','During '+period(value.lastProjectedPeriodIndex),period(value.lastProjectedPeriodIndex)+'の途中まで')}</p>}
      {value.rows.length>0&&changed.map(row=><div className="phase-forecast-attribute" key={row.attributeId} data-forecast-attribute={row.attributeId}>
        <p><strong>{context.stats.find(a=>a.id===row.attributeId)?.label??row.attributeId}</strong><span>{row.before} → {row.after} ({signed(row.delta)})</span></p>
        <p className="phase-forecast-breakdown">{words('비용','Costs','費用')} {signed(row.costs)} · {words('활동 효과','Activity effects','活動の効果')} {signed(row.effects)} · {words('자동 규칙','Automatic rules','自動ルール')} {signed(row.automatic)}</p>
      </div>)}
      {value.rows.length>0&&!changed.length&&<p>{words('확인된 능력·자원 변화가 없습니다.','No determined attribute changes.','確認できる能力・資源の変化はありません。')}</p>}
      {value.issues.map((issue,index)=>{const slot=issue.periodIndex%(context.periodsPerMonth??3),activity=context.activities.find(a=>a.id===context.plan[slot]);return <p className="phase-forecast-issue" key={index}>{issue.periodIndex>=0?period(issue.periodIndex)+(activity?' · '+activity.label:'')+': ':''}{forecastIssueText(issue.code,context.locale)}</p>;})}
    </section>;
  };
  return <><button type="button" className="phase-forecast-launcher" data-reading-id={`phase-forecast-${id}`} disabled={!available} aria-haspopup="dialog" onClick={()=>void launch()}>{words('일정 예상','Phase forecast','今期の見通し')}</button>
    {open&&<ReadingDialog id="engine-phase-forecast-panel" title={words('일정 예상','Phase forecast','今期の見通し')} onClose={close} initialFocus="heading">
      <div className="reading-dialog-body" tabIndex={0} role="region" aria-labelledby="engine-phase-forecast-panel-title" data-reading-id="engine-phase-forecast-summary">
        <p>{words('현재 초안을 검토합니다. 시간이나 자원을 사용하지 않습니다.','Reviewing the current draft uses no time or resources.','現在の下書きを確認します。時間や資源は消費しません。')}</p>
        {loading?<p role="status">{words('예상 결과 확인 중…','Checking the forecast…','見通しを確認中…')}</p>:reply?.ok?<>{projection(reply.forecast)}{!reply.forecast.complete&&(reply.baseline.rows.length>0||reply.baseline.complete)&&projection(reply.baseline,true)}</>:<p>{words('이 화면에서는 예상 결과를 확인할 수 없습니다.','A forecast is unavailable for this view.','この画面では見通しを確認できません。')}</p>}
      </div>
      <div className="reading-dialog-actions"><button type="button" data-reading-id="engine-phase-forecast-close" onClick={close}>{words('계획으로 돌아가기','Back to plan','計画に戻る')}</button></div>
    </ReadingDialog>}
  </>;
}

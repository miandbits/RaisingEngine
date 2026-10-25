import React,{useEffect,useRef} from 'react';
import {slotWords} from '../../../packages/player-service/save-slots.js';
import {ReadingDialog} from './ReadingDialog.js';
export interface PlanningControl {label:string;effect:'confirm'|'execute'|'acknowledge'|'confirm-and-execute'}
export function PlanningGuide({step,onStep,onClose,labels,period,hasCategories,control,locale}:{step:number;onStep:(step:number)=>void;onClose:()=>void;labels:readonly string[];period:number;hasCategories:boolean;control?:PlanningControl|undefined;locale?:string|undefined}) {
  const words=(ko:string,en:string,ja:string)=>slotWords(locale,ko,en,ja),instruction=useRef<HTMLParagraphElement>(null),first=labels[period]??labels[0]??'',remaining=Math.max(0,labels.length-period);
  const instructions=[words(`남은 ${remaining}개 기간의 계획을 확인하세요. ${first} 같은 기간을 누르면 활동 목록이 열립니다.`,`Your plan has ${remaining} remaining periods. Open a period such as ${first} to choose an activity.`,`残り${remaining}期間の計画を確認しましょう。${first}などの期間を選ぶと、活動の一覧が開きます。`),
    (hasCategories?words('분류로 활동을 찾아볼 수 있습니다. ','Categories help you browse activities. ','分類から活動を探せます。'):'')+words('활동의 설명과 비용, 효과를 확인하세요. 적용은 해당 기간의 계획만 바꿉니다. 취소하면 이전 계획이 유지됩니다. 적용만으로는 시간이 흐르지 않습니다.','Review the description, costs and effects. Apply changes only that period in your draft plan. Cancel keeps the previous assignment. Apply does not advance time.','説明・費用・効果を確認しましょう。適用はその期間の下書きだけを変更します。キャンセルすると元の予定が残ります。適用だけでは時間は進みません。'),
    !control?words('모든 기간을 검토한 뒤 게임의 진행 버튼을 사용하세요. 활동에 적용을 누르는 것과 전체 계획을 확정하거나 실행하는 것은 다릅니다.','Review every period, then use the game’s schedule or continue control. Applying an activity is separate from confirming or running the whole plan.','すべての期間を確認してから、ゲームの予定・進行ボタンを使います。活動の適用と、計画の確定・実行は別の操作です。'):
    control.effect==='confirm'?words(`계획을 검토한 뒤 “${control.label}”을 누르면 전체 일정을 확정합니다. 이 단계에서는 아직 활동을 실행하지 않습니다.`,`After reviewing the plan, “${control.label}” confirms the schedule. That step does not execute an activity yet.`,`計画を確認してから「${control.label}」で予定を確定します。この操作では、まだ活動は実行されません。`):
    control.effect==='execute'?words(`현재 계획은 이미 확정되어 있습니다. “${control.label}”을 누르면 다음 활동을 실행합니다.`,`The current plan is already confirmed. “${control.label}” executes the next activity.`,`現在の計画は確定済みです。「${control.label}」で次の活動を実行します。`):
    control.effect==='acknowledge'?words(`현재 계획은 이미 확정되어 있습니다. “${control.label}”을 누르면 현재 결과 다음으로 진행합니다.`,`The current plan is already confirmed. “${control.label}” continues after the current result.`,`現在の計画は確定済みです。「${control.label}」で現在の結果の先に進みます。`):
    words(`계획을 검토한 뒤 “${control.label}”을 누르면 일정을 확정하고 다음 활동을 시작합니다.`,`After reviewing the plan, “${control.label}” confirms the schedule and starts the next activity.`,`計画を確認してから「${control.label}」を押すと、予定を確定して次の活動を始めます。`)];
  useEffect(()=>{instruction.current?.focus({preventScroll:true});instruction.current?.parentElement?.scrollTo(0,0);},[step]);
  return <ReadingDialog id="engine-planning-guide" title={words('계획 세우는 방법','How planning works','計画の立て方')} onClose={onClose}>
    <div className="reading-dialog-body planning-guide-body"><p role="status">{words(`${step+1} / 3 단계`,`Step ${step+1} of 3`,`${step+1} / 3 ステップ`)}</p><p ref={instruction} tabIndex={-1}>{instructions[step]}</p></div>
    <div className="reading-dialog-actions"><button data-reading-id="engine-planning-guide-back" disabled={step===0} onClick={()=>onStep(step-1)}>{words('이전','Back','戻る')}</button><button data-reading-id="engine-planning-guide-skip" onClick={onClose}>{words('건너뛰기','Skip','スキップ')}</button>{step<2?<button data-reading-id="engine-planning-guide-next" onClick={()=>onStep(step+1)}>{words('다음','Next','次へ')}</button>:<button data-reading-id="engine-planning-guide-done" onClick={onClose}>{words('완료','Done','完了')}</button>}</div>
  </ReadingDialog>;
}

import type {Element} from '../../../packages/visual/model.js';
import {slotWords} from '../../../packages/player-service/save-slots.js';
import type {VisualContext} from './VisualSurface.js';

type ProgressionContext=Pick<VisualContext,'dataState'|'phase'|'plan'|'locale'>;

/** Follow the command boundary, not a stale authored label or presentation phase. */
export function progressionButtonLabel(element:Element,context:ProgressionContext):string {
  if(element.autoLabel===false)return element.text??'';
  if(element.action!=='advance')return element.text??'버튼';
  const words=(ko:string,en:string,ja:string)=>slotWords(context.locale,ko,en,ja);
  const state=context.dataState,phase=state?.phase??context.phase;
  const confirm=()=>words('일정 확정','Confirm schedule','予定を確定');
  if(phase==='planning')return confirm();
  if(phase==='ready'){
    // Roster assignments have their own draft; their ready action executes.
    const committed=!!state?.roster||!state||!!state.schedule&&state.schedule.length===context.plan.length&&context.plan.every((id,index)=>state.schedule![index]===id);
    if(!committed)return confirm();
    return state?.activityIntermission?words('결과 확인 · 다음','Continue','次へ'):words('활동 시작','Start activity','活動を開始');
  }
  if(phase==='activity-result')return words('결과 확인 · 다음','Continue','次へ');
  if(['narrative','dialogue','choice'].includes(phase))return words('이야기 계속','Continue story','物語を続ける');
  return words('다음','Next','次へ');
}

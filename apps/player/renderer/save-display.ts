import {slotWords} from '../../../packages/player-service/save-slots.js';
export function savedTime(value:string|undefined,locale?:string):{text:string;original:string} {
  if(!value)return {text:'',original:''};
  // Native v1 records contain local wall time without a zone. Never reinterpret it.
  if(/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/.test(value))return {text:value+' · '+slotWords(locale,'시간대 정보 없음','time zone not recorded','タイムゾーン未記録'),original:value};
  if(/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?(Z|[+-]\d{2}:\d{2})$/.test(value)&&Number.isFinite(Date.parse(value))){
    try{return {text:new Intl.DateTimeFormat(locale??'ko',{year:'numeric',month:'short',day:'numeric',hour:'numeric',minute:'2-digit',second:'2-digit',timeZoneName:'short'}).format(new Date(value)),original:value};}catch{/* Unknown locale: retain the explicit original timestamp. */}
  }
  return {text:value,original:value};
}
export function savedPhase(phase:string|undefined,locale?:string):string {
  const labels:Record<string,[string,string,string]>={planning:['계획 중','Planning','計画中'],ready:['일정 준비됨','Schedule ready','予定の準備完了'],narrative:['이야기','Story','物語'],'activity-result':['활동 결과','Activity result','活動結果'],'period-end':['기간 완료','Period complete','期間完了'],finished:['게임 완료','Game finished','ゲーム終了']};
  const label=phase?labels[phase]:undefined;return label?slotWords(locale,...label):'';
}

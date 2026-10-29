import type {ContentSnapshot} from '../core/model.js';

type Activity = Pick<ContentSnapshot['activities'][number], 'costs' | 'baseEffects' | 'successCheck'>;
export interface ActivityRuleContext {
  locale?: string;
  attributes: readonly {id:string;label:string}[];
  characters?: readonly {id:string;name:string}[];
  actorId?: string;
  localAttributeIds?: readonly string[];
}

/** Authored rules only. Does not evaluate expressions, roll outcomes or mutate run state. */
export function activityRuleLines(activity:Partial<Activity>, context:ActivityRuleContext):string[] {
  const language=context.locale?.startsWith('en')?'en':context.locale?.startsWith('ja')?'ja':'ko';
  const words=(ko:string,en:string,ja:string)=>language==='en'?en:language==='ja'?ja:ko;
  const name=(id:string)=>context.attributes.find(a=>a.id===id)?.label??id;
  const actor=(id:string)=>context.characters?.find(c=>c.id===id)?.name??id;
  const scoped=(id:string,owner?:{kind:string;characterId?:string})=>{
    const ownerId=owner?.kind==='character'?owner.characterId:owner?.kind==='actor'?context.actorId:
      context.localAttributeIds?.includes(id)?context.actorId:undefined;
    const prefix=ownerId?actor(ownerId):context.actorId?words('공용','Shared','共有'):'';
    return `${prefix?prefix+' · ':''}${name(id)}`;
  };
  const costs=(activity.costs??[]).map(c=>`${scoped(c.attributeId)} ${c.amount}`).join(' / ');
  const effects:string[]=[];let other=false;
  for(const effect of activity.baseEffects??[]){
    if((effect.op==='add'||effect.op==='set')&&effect.target.kind==='attribute'){
      const value=effect.value.op==='literal'?`${effect.op==='set'?'= ':effect.value.value>=0?'+':''}${effect.value.value}`:words('실행 시 계산','Calculated when performed','実行時に計算');
      effects.push(`${scoped(effect.target.attributeId,effect.target.owner)} ${value}`);
    } else other=true; // Do not expose internal flags or silently claim an exhaustive summary.
  }
  const lines=[`${words('비용','Cost','費用')} · ${costs||words('없음','None','なし')}`,`${words('기본 효과','Base effects','基本効果')} · ${effects.join(' / ')||words('수치 변화 없음','No numeric changes','数値の変化なし')}`];
  if(other)lines.push(words('그 밖의 상태·관계·물품 규칙이 있습니다.','Other state, relationship or item rules also apply.','ほかに状態・関係・アイテムのルールがあります。'));
  if(activity.successCheck)lines.push(words('성공·실패에 따른 추가 효과가 있습니다.','Success or failure can add further effects.','成功・失敗による追加効果があります。'));
  lines.push(words('규칙 요약 · 최종 결과 예측 아님','Authored rules · not a prediction of the final result','ルールの要約 · 最終結果の予測ではありません'));
  return lines;
}

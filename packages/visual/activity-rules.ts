import type {ContentSnapshot} from '../core/model.js';

type Activity = Pick<ContentSnapshot['activities'][number], 'costs' | 'baseEffects' | 'successCheck'>;
export interface ActivityRuleContext {
  attributes: readonly {id:string;label:string}[];
  characters?: readonly {id:string;name:string}[];
  actorId?: string;
  localAttributeIds?: readonly string[];
}

/** Authored rules only. Does not evaluate expressions, roll outcomes or mutate run state. */
export function activityRuleLines(activity:Partial<Activity>, context:ActivityRuleContext):string[] {
  const name=(id:string)=>context.attributes.find(a=>a.id===id)?.label??id;
  const actor=(id:string)=>context.characters?.find(c=>c.id===id)?.name??id;
  const scoped=(id:string,owner?:{kind:string;characterId?:string})=>{
    const ownerId=owner?.kind==='character'?owner.characterId:owner?.kind==='actor'?context.actorId:
      context.localAttributeIds?.includes(id)?context.actorId:undefined;
    const prefix=ownerId?actor(ownerId):context.actorId?'공용':'';
    return `${prefix?prefix+' · ':''}${name(id)}`;
  };
  const costs=(activity.costs??[]).map(c=>`${scoped(c.attributeId)} ${c.amount}`).join(' / ');
  const effects:string[]=[];let other=false;
  for(const effect of activity.baseEffects??[]){
    if((effect.op==='add'||effect.op==='set')&&effect.target.kind==='attribute'){
      const value=effect.value.op==='literal'?`${effect.op==='set'?'= ':effect.value.value>=0?'+':''}${effect.value.value}`:'실행 시 계산';
      effects.push(`${scoped(effect.target.attributeId,effect.target.owner)} ${value}`);
    } else other=true; // Do not expose internal flags or silently claim an exhaustive summary.
  }
  const lines=[`비용 · ${costs||'없음'}`,`기본 효과 · ${effects.join(' / ')||'수치 변화 없음'}`];
  if(other)lines.push('그 밖의 상태·관계·물품 규칙이 있습니다.');
  if(activity.successCheck)lines.push('성공·실패에 따른 추가 효과가 있습니다.');
  lines.push('규칙 요약 · 최종 결과 예측 아님');
  return lines;
}

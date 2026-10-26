import type {ContentSnapshot} from '../core/model.js';
import {slotWords} from '../player-service/save-slots.js';
type Condition=ContentSnapshot['activities'][number]['requirements'];
type Expression=Extract<Condition,{op:'compare'}>['left'];
/** Describes authored conditions without predicting eligibility in a future period. */
export function activityRequirementText(condition:Condition|undefined,attributes:readonly {id:string;label:string;value?:number}[],locale?:string):string {
  const w=(ko:string,en:string,ja:string)=>slotWords(locale,ko,en,ja);
  const expression=(value:Expression):string=>{
    if(value.op==='literal')return String(value.value);
    if(value.op==='time')return ({year:w('연도','Year','年'),month:w('월','Month','月'),periodIndex:w('전체 기간 번호 (0부터)','Overall period index (from 0)','通算期間番号（0から）'),periodInMonth:w('단계 내 기간 번호 (0부터)','Period index in phase (from 0)','段階内の期間番号（0から）')})[value.field];
    if(value.op==='read'){
      if(value.target.kind==='attribute'){const target=value.target,attribute=attributes.find(a=>a.id===target.attributeId);return (attribute?.label??target.attributeId)+(attribute?.value!==undefined?` (${w('현재','current','現在')} ${attribute.value})`:'');}
      if(value.target.kind==='relationship')return w('관계 수치','Relationship value','関係値');
      return w('물품 수량','Item quantity','アイテム数');
    }
    if(value.op==='clamp')return `${w('범위 제한','Clamped','範囲制限')} (${expression(value.value)}, ${value.min}…${value.max})`;
    return `(${expression(value.left)} ${{add:'+',subtract:'−',multiply:'×'}[value.op]} ${expression(value.right)})`;
  };
  const describe=(value:Condition):string=>{
    if(value.op==='always')return w('없음','None','なし');
    if(value.op==='compare')return `${expression(value.left)} ${{eq:'=',ne:'≠',gt:'>',gte:'≥',lt:'<',lte:'≤'}[value.comparator]} ${expression(value.right)}`;
    if(value.op==='all'||value.op==='any')return value.conditions.map(c=>`(${describe(c)})`).join(value.op==='all'?w(' 그리고 ',' and ',' かつ '):w(' 또는 ',' or ',' または '));
    if(value.op==='not')return `${w('제외','Not','除外')}: (${describe(value.condition)})`;
    return w('추가 이야기·상태 조건이 있습니다.','Additional story or state conditions apply.','追加の物語・状態条件があります。');
  };
  return `${w('필요 조건','Requirements','必要条件')} · ${describe(condition??{op:'always'})}`;
}

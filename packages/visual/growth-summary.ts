import type {ActivityResult,DeepReadonly,NumericTarget} from '../core/model.js';

export interface GrowthRow {key:string;target:DeepReadonly<NumericTarget>;before:number;after:number;delta:number}
/** Complete structured identity, including actor/owner metadata; field order is irrelevant. */
function identity(value:unknown):string {
  if(Array.isArray(value))return '['+value.map(identity).join(',')+']';
  if(value&&typeof value==='object')return '{'+Object.entries(value).sort(([a],[b])=>a<b?-1:a>b?1:0).map(([key,item])=>JSON.stringify(key)+':'+identity(item)).join(',')+'}';
  return JSON.stringify(value)??'null';
}
/** One committed activity only. No prediction, state sampling, or phase accumulation. */
export function activityGrowthRows(result:DeepReadonly<ActivityResult>):GrowthRow[] {
  const rows=new Map<string,GrowthRow>();
  for(const change of result.changes){
    if(change.kind!=='numeric')continue;
    const key=identity(change.target),prior=rows.get(key),before=prior?.before??change.before;
    rows.set(key,{key,target:structuredClone(change.target),before,after:change.after,delta:change.after-before});
  }
  return [...rows.values()].filter(row=>row.delta!==0);
}
export function growthWords(locale='ko') {
  if(locale.startsWith('en'))return {attribute:'Attribute',scope:'This activity',before:'Before',after:'After',change:'Net change',none:'No net numeric changes'};
  if(locale.startsWith('ja'))return {attribute:'項目',scope:'今回の活動',before:'前',after:'後',change:'差分',none:'数値の差分なし'};
  return {attribute:'항목',scope:'이번 활동',before:'이전',after:'이후',change:'최종 변화',none:'최종 수치 변화 없음'};
}
export function growthOwnerId(target:DeepReadonly<NumericTarget>):string|undefined {
  const t=target as {actorId?:string;owner?:{kind?:string;characterId?:string}};
  return t.actorId??(t.owner?.kind==='character'?t.owner.characterId:undefined);
}

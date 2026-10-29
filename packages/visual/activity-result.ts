import type {ActivityResult,NumericTarget,DeepReadonly} from '../core/model.js';

/** Engine summary copy only; authored labels and timeline text are never translated here. */
export function activityResultWords(locale='ko') {
  if(locale.startsWith('en'))return {complete:'complete',success:'Success',failure:'Failure',noChanges:'No numeric changes'};
  if(locale.startsWith('ja'))return {complete:'完了',success:'成功',failure:'失敗',noChanges:'数値の変化なし'};
  return {complete:'완료',success:'성공',failure:'실패',noChanges:'수치 변화 없음'};
}

/** Format committed changes without altering the result or looking ahead in the simulation. */
export function activityResultText(result:DeepReadonly<ActivityResult>,activity:string,targetName:(target:DeepReadonly<NumericTarget>)=>string,locale?:string,actor?:string):string {
  const words=activityResultWords(locale),heading=actor===undefined?`${activity} ${words.complete}`:`${actor} · ${activity}`;
  const outcome=result.success?(result.success.passed?words.success:words.failure):'';
  const changes=result.changes.filter(change=>change.kind==='numeric'&&change.delta!==0).map(change=>change.kind==='numeric'?`${targetName(change.target)} ${change.delta>0?'+':''}${change.delta}`:'');
  const summary=changes.length?changes.join(actor===undefined?' · ':'\n'):words.noChanges;
  return heading+(outcome?(actor===undefined?' · ':'\n')+outcome:'')+'\n'+summary;
}

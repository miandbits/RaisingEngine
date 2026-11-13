import type {ContentSnapshot,RunState} from '../core/model.js';

export type LastCommittedPlan = {status:'none'} | {status:'unavailable';reason:'length'|'activity'}
  | {status:'ready';commandIndex:number;activityIds:readonly string[]};
export type PlanReuseDraft = {status:'ready';activityIds:string[];firstEditableSlot:number}
  | {status:'unavailable';reason:'none'|'phase'|'finished'|'length'|'activity'|'prefix'};

export function hasPlanReuse(content:Pick<ContentSnapshot,'project'>):boolean {
  return !content.project.roster&&!!content.project.visual?.screens.some(screen=>screen.readableLayout?.items.some(item=>item.planReuse==='last-committed'));
}
/** Accepted history only. Never substitute an older plan for an invalid newest one. */
export function deriveLastCommittedPlan(commands:readonly unknown[],count:number,knownActivityIds:readonly string[]):LastCommittedPlan {
  for(let i=commands.length-1;i>=0;i--){
    const command=commands[i];if(!command||typeof command!=='object'||(command as {type?:unknown}).type!=='setSchedule')continue;
    const ids=(command as {activityIds?:unknown}).activityIds;
    if(!Number.isInteger(count)||count<1||!Array.isArray(ids)||ids.length!==count)return {status:'unavailable',reason:'length'};
    const known=new Set(knownActivityIds);if(ids.some(id=>typeof id!=='string'||!known.has(id)))return {status:'unavailable',reason:'activity'};
    return {status:'ready',commandIndex:i,activityIds:[...ids]};
  }
  return {status:'none'};
}
/** Presentation draft only. Canonical confirmation/execution remains authoritative. */
export function planReuseDraft(source:LastCommittedPlan|undefined,state:Pick<RunState,'phase'|'periodIndex'|'schedule'|'activityIntermission'>|null|undefined,count:number,knownActivityIds:readonly string[]):PlanReuseDraft {
  if(!state||!['planning','ready'].includes(state.phase))return {status:'unavailable',reason:'phase'};
  if(!source||source.status==='none')return {status:'unavailable',reason:'none'};
  if(source.status==='unavailable')return source;
  const checked=deriveLastCommittedPlan([{type:'setSchedule',activityIds:source.activityIds}],count,knownActivityIds);
  if(checked.status!=='ready')return checked.status==='none'?{status:'unavailable',reason:'none'}:checked;
  if(!Number.isInteger(state.periodIndex)||state.periodIndex<0)return {status:'unavailable',reason:'phase'};
  const firstEditableSlot=Math.min(count,state.periodIndex%count+(state.activityIntermission?1:0));
  if(firstEditableSlot>=count)return {status:'unavailable',reason:'finished'};
  const activityIds=[...checked.activityIds],known=new Set(knownActivityIds);
  for(let i=0;i<firstEditableSlot;i++){
    const id=state.schedule?.[i];if(typeof id!=='string'||!known.has(id))return {status:'unavailable',reason:'prefix'};
    activityIds[i]=id;
  }
  return {status:'ready',activityIds,firstEditableSlot};
}

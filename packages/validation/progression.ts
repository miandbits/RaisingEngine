import type { SemanticContext } from './context.js';
import { isObject, type ProjectDocument, type JsonObject } from './model.js';
export function checkProgression(ctx: SemanticContext, root: ProjectDocument, project: JsonObject): void {
  const p=project.progression;if(!isObject(p))return;
  const ap=p.actionPoints as JsonObject;
  if((ap.initial as number)>(ap.max as number))ctx.report(root,'/progression/actionPoints/initial','ACTION_POINTS_RANGE','초기 행동력이 최대 행동력을 초과합니다.');
  const gameplay=project.gameplay as JsonObject|undefined,actions=(gameplay?.actions??[]) as JsonObject[];
  const seen=new Set<string>();
  for(const [i,rule] of (p.actions as JsonObject[]).entries()){
    const path=`/progression/actions/${i}`,id=rule.actionId as string,action=actions.find(a=>a.id===id);
    if(!action)ctx.report(root,path+'/actionId','UNKNOWN_ACTION','소요 시간을 설정할 공통 행동이 없습니다.');
    if(action?.expeditionId)ctx.report(root,path,'TIMED_EXPEDITION_UNSUPPORTED','탐험 진입 행동의 시간 세션 중첩은 지원하지 않습니다.');
    if(seen.has(id))ctx.report(root,path+'/actionId','DUPLICATE_TIMING','행동별 시간 설정이 중복되었습니다.');seen.add(id);
    if((rule.actionPointCost as number)>(ap.max as number))ctx.report(root,path+'/actionPointCost','ACTION_POINTS_RANGE','행동 비용이 최대 행동력을 초과합니다.');
  }
  const ids=new Set<string>();const roster=project.roster as JsonObject|undefined;
  for(const [i,a] of (p.appointments as JsonObject[]).entries()){
    const path=`/progression/appointments/${i}`;if(ids.has(a.id as string))ctx.report(root,path+'/id','DUPLICATE_APPOINTMENT','예약 ID가 중복되었습니다.');ids.add(a.id as string);
    const event=ctx.resolve(ctx.events,a.eventId as string,root,path+'/eventId','예약 사건');
    if(event&&event.data.trigger!=='interaction')ctx.report(root,path+'/eventId','APPOINTMENT_EVENT_SCOPE','예약 사건은 직접 상호작용 사건이어야 합니다.');
    if(event?.data.scope==='actor'){
      if(roster?.stateModel!=='character-v1'||!(roster.members as JsonObject[]).some(m=>m.characterId===a.actorId))ctx.report(root,path+'/actorId','APPOINTMENT_ACTOR','개인 예약 사건에는 육성 대상이 필요합니다.');
    }else if(a.actorId!==undefined)ctx.report(root,path+'/actorId','APPOINTMENT_ACTOR','공용 사건에는 실행자를 지정하지 않습니다.');
  }
}

import type { SemanticContext } from './context.js';
import { isObject, type ProjectDocument, type JsonObject } from './model.js';
export function checkProgression(ctx: SemanticContext, root: ProjectDocument, project: JsonObject): void {
  const p=project.progression;if(!isObject(p))return;
  const ap=p.actionPoints as JsonObject;
  const unified=p.model==='unified-v2',rosterConfig=project.roster as JsonObject|undefined;
  const members=(rosterConfig?.members??[{characterId:project.protagonistId}]) as JsonObject[];
  const pools=(p.actorPools??[]) as JsonObject[];
  if(!unified&&['ticksPerPeriod','parallel','actorPools','activities'].some(key=>p[key]!==undefined))ctx.report(root,'/progression/model','PROGRESSION_MODEL','개인·병렬·일정 시간 설정에는 unified-v2 모델이 필요합니다.');
  if(unified&&p.ticksPerPeriod===undefined)ctx.report(root,'/progression/ticksPerPeriod','PERIOD_TIME_REQUIRED','통합 시간에는 일정 한 칸의 시간이 필요합니다.');
  if(unified&&Number(p.ticksPerPeriod)*Number(project.durationMonths)*Number(project.periodsPerMonth)>2147483647)ctx.report(root,'/progression/ticksPerPeriod','CLOCK_OVERFLOW','전체 달력 시간이 정수 범위를 초과합니다.');
  const poolIds=new Set<string>();
  for(const [i,pool]of pools.entries()){
    const path=`/progression/actorPools/${i}`,id=pool.actorId as string;
    if(!members.some(m=>m.characterId===id))ctx.report(root,path+'/actorId','UNKNOWN_ACTOR','육성 대상이 아닌 행동력 소유자입니다.');
    if(poolIds.has(id))ctx.report(root,path+'/actorId','DUPLICATE_ACTOR_POOL','개인 행동력 설정이 중복되었습니다.');poolIds.add(id);
    if(Number(pool.initial)>Number(pool.max))ctx.report(root,path+'/initial','ACTION_POINTS_RANGE','초기 행동력이 최대값을 초과합니다.');
  }
  const activityIds=new Set<string>();
  for(const [i,rule]of ((p.activities??[])as JsonObject[]).entries()){
    const path=`/progression/activities/${i}`,id=rule.activityId as string;
    ctx.resolve(ctx.activities,id,root,path+'/activityId','일정 활동');
    if(activityIds.has(id))ctx.report(root,path+'/activityId','DUPLICATE_TIMING','활동별 행동력이 중복되었습니다.');activityIds.add(id);
    for(const member of members){const settings=pools.find(x=>x.actorId===member.characterId)??ap;if(Number(rule.actionPointCost)>Number(settings.max))ctx.report(root,path+'/actionPointCost','ACTION_POINTS_RANGE','활동 행동력이 육성 대상의 최대 행동력을 초과합니다.');}
  }
  if(unified)for(const entries of ctx.activities.values())for(const entry of entries)if(entry.data.expeditionId)ctx.report(entry.doc,'/expeditionId','TIMED_EXPEDITION_UNSUPPORTED','통합 일정에는 탐험 진입 활동 대신 별도 탐험 행동을 사용하세요.');
  if((ap.initial as number)>(ap.max as number))ctx.report(root,'/progression/actionPoints/initial','ACTION_POINTS_RANGE','초기 행동력이 최대 행동력을 초과합니다.');
  const gameplay=project.gameplay as JsonObject|undefined,actions=(gameplay?.actions??[]) as JsonObject[];
  const seen=new Set<string>();
  for(const [i,rule] of (p.actions as JsonObject[]).entries()){
    const path=`/progression/actions/${i}`,id=rule.actionId as string,action=actions.find(a=>a.id===id);
    if(!action)ctx.report(root,path+'/actionId','UNKNOWN_ACTION','소요 시간을 설정할 공통 행동이 없습니다.');
    if(action?.expeditionId)ctx.report(root,path,'TIMED_EXPEDITION_UNSUPPORTED','탐험 진입 행동의 시간 세션 중첩은 지원하지 않습니다.');
    if(seen.has(id))ctx.report(root,path+'/actionId','DUPLICATE_TIMING','행동별 시간 설정이 중복되었습니다.');seen.add(id);
    if((rule.actionPointCost as number)>Math.max(Number(ap.max),...pools.map(x=>Number(x.max))))ctx.report(root,path+'/actionPointCost','ACTION_POINTS_RANGE','행동 비용이 최대 행동력을 초과합니다.');
    if(unified)for(const member of members)if((member.actionIds as string[]|undefined)?.includes(id)){
      const settings=pools.find(x=>x.actorId===member.characterId)??ap;
      if(Number(rule.actionPointCost)>Number(settings.max))ctx.report(root,path+'/actionPointCost','ACTION_POINTS_RANGE',`${member.characterId}: 행동 비용이 개인 최대 행동력을 초과합니다.`);
    }
    if(unified&&Number(rule.duration)>Number(p.ticksPerPeriod))ctx.report(root,path+'/duration','PERIOD_TIME_EXCEEDED','행동 소요 시간이 일정 한 칸보다 깁니다.');
  }
  const ids=new Set<string>();const roster=project.roster as JsonObject|undefined;
  for(const [i,a] of (p.appointments as JsonObject[]).entries()){
    const path=`/progression/appointments/${i}`;if(ids.has(a.id as string))ctx.report(root,path+'/id','DUPLICATE_APPOINTMENT','예약 ID가 중복되었습니다.');ids.add(a.id as string);
    if(a.repeatEvery!==undefined||a.repeatCount!==undefined){
      if(!unified||a.repeatEvery===undefined||a.repeatCount===undefined)ctx.report(root,path,'REPEAT_APPOINTMENT_CONFIG','반복 예약에는 통합 모델, 간격과 횟수가 모두 필요합니다.');
      if(Number(a.atTick)+(Number(a.repeatCount)-1)*Number(a.repeatEvery)>2147483647)ctx.report(root,path+'/repeatCount','CLOCK_OVERFLOW','마지막 반복 시각이 정수 범위를 초과합니다.');
    }
    const event=ctx.resolve(ctx.events,a.eventId as string,root,path+'/eventId','예약 사건');
    if(event&&event.data.trigger!=='interaction')ctx.report(root,path+'/eventId','APPOINTMENT_EVENT_SCOPE','예약 사건은 직접 상호작용 사건이어야 합니다.');
    if(event?.data.scope==='actor'){
      if(roster?.stateModel!=='character-v1'||!(roster.members as JsonObject[]).some(m=>m.characterId===a.actorId))ctx.report(root,path+'/actorId','APPOINTMENT_ACTOR','개인 예약 사건에는 육성 대상이 필요합니다.');
    }else if(a.actorId!==undefined)ctx.report(root,path+'/actorId','APPOINTMENT_ACTOR','공용 사건에는 실행자를 지정하지 않습니다.');
  }
  if((p.appointments as JsonObject[]).reduce((sum,a)=>sum+Number(a.repeatCount??1),0)>4096)ctx.report(root,'/progression/appointments','APPOINTMENT_LIMIT','반복을 포함해 최대 4096회까지 예약할 수 있습니다.');
}

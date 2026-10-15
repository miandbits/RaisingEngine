import type { Command, ContentSnapshot, RunData } from './model.js';
import type { ProgressionState } from './progression.js';
import { requireCore } from './util.js';
import { eventCompleted } from './roster.js';
import { evaluateCondition } from './evaluation.js';
import { enterNarrative } from './narrative.js';

export interface TimeSession {
  id: string; kind: 'action' | 'calendar' | 'wait'; remaining: number;
  command: NonNullable<ProgressionState['pending']>['command'];
}
export const unified = (c: ContentSnapshot): boolean => c.project.progression?.model === 'unified-v2';
export function timeActors(c: ContentSnapshot): string[] { return c.project.roster?.members.map(m=>m.characterId) ?? [c.project.protagonistId]; }
export function pool(c: ContentSnapshot,id: string) { const cfg=c.project.progression!;return cfg.actorPools?.find(p=>p.actorId===id) ?? cfg.actionPoints; }
export function syncPending(p: ProgressionState): void { const first=p.sessions![0];p.pending=first?{remaining:first.remaining,command:first.command}:null; }
export function createUnified(c: ContentSnapshot,p: ProgressionState): void {
  p.sessions=[];p.actors=Object.fromEntries(timeActors(c).map(id=>[id,{elapsed:0,actionPoints:pool(c,id).initial}]));
}
export function unifiedGuard(s: RunData,command: Command): void {
  const sessions=s.progression!.sessions!;
  if(!sessions.length)return;
  const allowed=['advanceSession','cancelSession','cancelActorSession','advanceNarrative','choose','navigate'];
  requireCore(allowed.includes(command.type)||command.type==='executeActorAction','SESSION_BUSY','진행 중인 세션을 마치거나 다른 캐릭터의 시간 행동을 시작하세요.','/progression/sessions');
}
export function fitPeriod(c: ContentSnapshot,s: RunData,duration: number): void {
  requireCore(s.progression!.tick+duration<=(s.periodIndex+1)*c.project.progression!.ticksPerPeriod!,
    'PERIOD_TIME_EXCEEDED','이번 일정 칸의 남은 시간을 초과합니다. 월 일정을 실행·완료해 다음 칸으로 진행하세요.','/progression/tick');
}
export function reserveAction(c: ContentSnapshot,s: RunData,cmd: NonNullable<TimeSession['command']>,duration:number,cost:number): void {
  const p=s.progression!,cfg=c.project.progression!,id=cmd.actorId??'$shared',sessions=p.sessions!;
  requireCore(!sessions.length||(cfg.parallel===true&&!!cmd.actorId&&sessions.every(x=>x.kind==='action'&&x.command?.actorId&&x.id!==id)),
    'ACTOR_SESSION_BUSY','이 캐릭터 또는 공용 시간이 이미 사용 중입니다.','/progression/sessions');
  fitPeriod(c,s,duration);
  const account=cmd.actorId?p.actors![cmd.actorId]:p;
  requireCore(account,'ACTOR_REQUIRED','시간을 사용할 육성 대상이 없습니다.','/command/actorId');
  requireCore(account.actionPoints>=cost,'INSUFFICIENT_ACTION_POINTS','행동력이 부족합니다.','/progression/actionPoints');
  account.actionPoints-=cost;sessions.push({id,kind:'action',remaining:duration,command:cmd});sessions.sort((a,b)=>a.id<b.id?-1:a.id>b.id?1:0);syncPending(p);
}
export function startCalendar(c:ContentSnapshot,s:RunData):boolean {
  if(!unified(c))return false;
  requireCore(s.phase==='ready'&&s.schedule,'WRONG_PHASE','실행할 일정이 준비되지 않았습니다.','/phase');
  const p=s.progression!,cfg=c.project.progression!;
  requireCore(!p.sessions!.length,'SESSION_BUSY','다른 세션을 먼저 완료하세요.','/progression/sessions');
  const remaining=(s.periodIndex+1)*cfg.ticksPerPeriod!-p.tick;
  requireCore(remaining>=0,'PERIOD_TIME_EXCEEDED','일정과 게임 시간이 일치하지 않습니다.','/progression/tick');
  for(const id of timeActors(c)){
    const activityId=s.roster?.assignments[id]??s.schedule[s.periodIndex%c.project.periodsPerMonth]!;
    const cost=cfg.activities?.find(a=>a.activityId===activityId)?.actionPointCost??0;
    requireCore(p.actors![id]!.actionPoints>=cost,'INSUFFICIENT_ACTION_POINTS',`${id}: 일정 행동력이 부족합니다.`,'/progression/actors/'+id);
    p.actors![id]!.actionPoints-=cost;
  }
  p.sessions!.push({id:'$calendar',kind:'calendar',remaining,command:null});syncPending(p);return true;
}
export function advanceUnified(c:ContentSnapshot,s:RunData,command:Command,sounds:string[],execute:(cmd:TimeSession['command'])=>void,calendar:()=>void):void {
  const p=s.progression!,cfg=c.project.progression!,sessions=p.sessions!;
  requireCore(['planning','ready'].includes(s.phase),'WRONG_PHASE','대본이나 진행 결과를 먼저 마치세요.','/phase');
  if(command.type==='waitTime'){
    requireCore(!sessions.length,'SESSION_BUSY','진행 중인 세션을 먼저 마치세요.','/progression/sessions');fitPeriod(c,s,command.ticks);
    sessions.push({id:'$wait',kind:'wait',remaining:command.ticks,command:null});syncPending(p);return;
  }
  requireCore(sessions.length,'NO_PROGRESS_SESSION','이어갈 진행 세션이 없습니다.','/progression/sessions');
  if(command.type==='cancelSession'){p.sessions=[];syncPending(p);return;}
  if(command.type==='cancelActorSession'){
    const index=sessions.findIndex(x=>x.command?.actorId===command.actorId);requireCore(index>=0,'NO_ACTOR_SESSION','이 캐릭터의 진행 세션이 없습니다.','/command/actorId');sessions.splice(index,1);syncPending(p);return;
  }
  const horizon=p.tick+Math.min(...sessions.map(x=>x.remaining));
  const candidates=cfg.appointments.flatMap(a=>{
    const occurrence=p.handled.filter(h=>h.id===a.id).length;
    const atTick=a.atTick+occurrence*(a.repeatEvery??0);
    return occurrence<(a.repeatCount??1)&&atTick<=horizon?[{...a,occurrence,atTick}]:[];
  }).sort((a,b)=>a.atTick-b.atTick||(a.id<b.id?-1:a.id>b.id?1:0));
  const next=candidates[0],delta=next?Math.max(0,next.atTick-p.tick):horizon-p.tick;
  requireCore(p.tick+delta<=2147483647,'CLOCK_OVERFLOW','게임 시간 한도를 초과합니다.','/progression/tick');
  p.tick+=delta;p.actionPoints=Math.min(cfg.actionPoints.max,p.actionPoints+delta*cfg.actionPoints.recoveryPerTick);
  for(const id of timeActors(c)){
    const account=p.actors![id]!,settings=pool(c,id);account.actionPoints=Math.min(settings.max,account.actionPoints+delta*settings.recoveryPerTick);
    if(sessions.some(x=>x.kind==='calendar'||x.command?.actorId===id))account.elapsed+=delta;
  }
  for(const session of sessions)session.remaining-=delta;
  syncPending(p);
  if(next){
    const event=c.events.find(e=>e.id===next.eventId)!;
    if(next.actorId)s.roster!.actorId=next.actorId;
    const eligible=(!!next.repeatEvery||!eventCompleted(c,s,event.id))&&s.periodIndex>=event.window.fromPeriod&&s.periodIndex<=event.window.throughPeriod&&evaluateCondition(c,s,event.requirements,`/events/${event.id}/requirements`);
    p.handled.push({id:next.id,occurrence:next.occurrence,atTick:next.atTick,outcome:eligible?'fired':'skipped'});
    if(eligible){const phase=s.phase,presentation=structuredClone(s.presentation);enterNarrative(c,s,'event',event.id);s.activeNarrative!.returnPhase=phase as 'planning'|'ready';s.activeNarrative!.returnPresentation=presentation;}
    return;
  }
  // One completion per command: deterministic actor order and an exact save boundary.
  const index=sessions.findIndex(x=>x.remaining===0),done=sessions.splice(index,1)[0]!;syncPending(p);
  if(done.kind==='calendar')calendar();else execute(done.command);
}

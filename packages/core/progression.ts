import type { Command, ContentSnapshot, RunData } from './model.js';
import { requireCore } from './util.js';
import { applyGameplay } from '../gameplay/service.js';
import { beginActorAction, eventCompleted } from './roster.js';
import { enterNarrative } from './narrative.js';
import { evaluateCondition } from './evaluation.js';
import { unified, createUnified, unifiedGuard, reserveAction, advanceUnified, type TimeSession } from './unified-time.js';

export interface ProgressionState {
  tick: number;
  actionPoints: number;
  handled: { id: string; outcome: 'fired' | 'skipped'; occurrence?:number; atTick?:number }[];
  sessions?: TimeSession[];
  actors?: Record<string,{elapsed:number;actionPoints:number}>;
  pending: null | { remaining: number; command: { type: 'executeAction'; actionId: string; quantity: number; actorId?: string } | null };
}
export function createProgression(c: ContentSnapshot, s: RunData): void {
  if (c.project.progression) s.progression = { tick: 0, actionPoints: c.project.progression.actionPoints.initial, handled: [], pending: null };
  if (unified(c)) createUnified(c,s.progression!);
}
export function guardProgression(s: RunData, command: Command): void {
  if(s.progression?.sessions){unifiedGuard(s,command);return;}
  requireCore(!s.progression?.pending || ['advanceSession','cancelSession','advanceNarrative','choose'].includes(command.type),
    'SESSION_BUSY', '진행 중인 행동을 이어가거나 취소하세요.', '/progression/pending');
}
function execute(c: ContentSnapshot, s: RunData, cmd: NonNullable<ProgressionState['pending']>['command'], sounds: string[]): void {
  if (!cmd) return;
  if (cmd.actorId) beginActorAction(c, s, cmd.actorId, cmd.actionId);
  applyGameplay(c, s, { type:'executeAction', actionId:cmd.actionId, quantity:cmd.quantity }, sounds);
}
/** Probe eligibility without spending RNG or mutating the real transaction. */
export function startTimedAction(c: ContentSnapshot, s: RunData, command: Extract<Command,{type:'executeAction'|'executeActorAction'}>): boolean {
  const rule = c.project.progression?.actions.find(a => a.actionId === command.actionId);
  if (!rule) {requireCore(!s.progression?.sessions?.length,'SESSION_BUSY','병렬 진행에는 시간 설정된 행동이 필요합니다.','/progression/sessions');return false;}
  requireCore(s.mode==='full' && ['planning','ready'].includes(s.phase), 'WRONG_PHASE', '행동을 시작할 수 있는 상태가 아닙니다.', '/phase');
  const p=s.progression!, cost=rule.actionPointCost*command.quantity, duration=rule.duration*command.quantity;
  if(!unified(c))requireCore(p.actionPoints>=cost,'INSUFFICIENT_ACTION_POINTS','행동력이 부족합니다.','/progression/actionPoints');
  const cmd={type:'executeAction' as const,actionId:command.actionId,quantity:command.quantity,...(command.type==='executeActorAction'?{actorId:command.actorId}:{})};
  requireCore(duration<=2147483647,'DURATION_OVERFLOW','행동 소요 시간 한도를 초과합니다.','/command/quantity');
  execute(c,structuredClone(s),cmd,[]);
  if(unified(c)){reserveAction(c,s,cmd,duration,cost);return true;}
  p.actionPoints-=cost;p.pending={remaining:duration,command:cmd};return true;
}
export function progressSession(c: ContentSnapshot, s: RunData, command: Extract<Command,{type:'advanceSession'|'cancelSession'|'cancelActorSession'|'waitTime'}>, sounds: string[], calendar:()=>void): void {
  const config=c.project.progression,p=s.progression;
  requireCore(config&&p,'PROGRESSION_DISABLED','시간·진행 세션 설정이 없습니다.','/project/progression');
  if(unified(c)){advanceUnified(c,s,command,sounds,cmd=>execute(c,s,cmd,sounds),calendar);return;}
  requireCore(command.type!=='cancelActorSession','PROGRESSION_MODEL','개인 세션 취소에는 통합 시간 모드가 필요합니다.','/project/progression/model');
  requireCore(s.mode==='full'&&['planning','ready'].includes(s.phase),'WRONG_PHASE','대본이나 진행 결과를 먼저 마치세요.','/phase');
  if(command.type==='waitTime'){p.pending={remaining:command.ticks,command:null};return;}
  requireCore(p.pending,'NO_PROGRESS_SESSION','이어갈 진행 세션이 없습니다.','/progression/pending');
  if(command.type==='cancelSession'){p.pending=null;return;}
  const pending=p.pending;
  const next=[...config.appointments].filter(a=>!p.handled.some(h=>h.id===a.id)&&a.atTick<=p.tick+pending.remaining).sort((a,b)=>a.atTick-b.atTick||(a.id<b.id?-1:a.id>b.id?1:0))[0];
  const delta=next?Math.max(0,next.atTick-p.tick):pending.remaining;
  requireCore(p.tick+delta<=2147483647,'CLOCK_OVERFLOW','게임 시간 한도를 초과합니다.','/progression/tick');
  p.tick+=delta;pending.remaining-=delta;p.actionPoints=Math.min(config.actionPoints.max,p.actionPoints+delta*config.actionPoints.recoveryPerTick);
  if(next){
    const event=c.events.find(e=>e.id===next.eventId)!;
    if(next.actorId)s.roster!.actorId=next.actorId;
    const eligible=!eventCompleted(c,s,event.id)&&s.periodIndex>=event.window.fromPeriod&&s.periodIndex<=event.window.throughPeriod&&evaluateCondition(c,s,event.requirements,`/events/${event.id}/requirements`);
    p.handled.push({id:next.id,outcome:eligible?'fired':'skipped'});
    if(eligible){const phase=s.phase,presentation=structuredClone(s.presentation);enterNarrative(c,s,'event',event.id);s.activeNarrative!.returnPhase=phase as 'planning'|'ready';s.activeNarrative!.returnPresentation=presentation;}
    return;
  }
  // Effects and ordinary costs settle once, only at completion. A failed settlement
  // leaves the prior session intact, so the player may cancel without duplicating effects.
  p.pending=null;execute(c,s,pending.command,sounds);
}

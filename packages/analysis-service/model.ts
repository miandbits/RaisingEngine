import {validatePriorityProfile,type PriorityProfile} from './priorities.js';
import type {AnalysisBalance,TraceBalance} from './balance.js';
import type {RoutePoint,RouteGraph} from './routes.js';
import type { Command, ContentSnapshot, RunState } from '../core/model.js';
export interface AnalysisOptions { workers?: number; runs: number; seed: number; strategy: 'random' | 'focus' | 'conserve' | 'explore' | 'priorities'; priorityProfile?:PriorityProfile; balance?:'sampled-v1'; attributeId?: string; maxCommands: number; timeoutMs: number; target?: { kind: 'event' | 'ending' | 'choice'; id: string; nodeId?: string; choiceId?: string }; beamWidth?: number; maxStates?: number }
export interface Trace { balance?:TraceBalance; route?:RoutePoint[]; run: number; seed: number; commands: Command[]; state: RunState; outcome: 'finished' | 'blocked' | 'limit' | 'error' | 'target'; reason: string; events: string[]; choices: string[]; nodes: string[]; eventOrder: string[] }
export interface AnalysisResult { balance?:AnalysisBalance; routeGraph?:RouteGraph; completed: number; requested: number; outcomes: Record<string, number>; events: Record<string, number>; choices: Record<string, number>; nodes: Record<string, number>; endings: Record<string, number>; blocked: Record<string, number>; attributes: Record<string, { min: number; max: number; sum: number }>; traces: Trace[]; targetStatus: 'not-requested' | 'found' | 'not-found' | 'impossible'; proof: string | null; exploredStates: number; stopReason: 'completed' | 'cancelled' | 'time-limit' | 'state-limit' }
export interface AnalysisJob { effectiveWorkers?: number; snapshotHash?:string; id: string; root: string; sourceVersion: string; revision: number | null; fingerprint: string; createdAt: string; status: 'running' | 'completed' | 'cancelled' | 'error'; options: AnalysisOptions; result: AnalysisResult; error: string | null }
export interface StoredAnalysis { job: AnalysisJob; content: ContentSnapshot }
export function validateOptions(input: AnalysisOptions, content: ContentSnapshot): AnalysisOptions {
  const o = structuredClone(input); o.workers ??= 1;
  const integer = (x: number, min: number, max: number) => Number.isSafeInteger(x) && x >= min && x <= max;
  if (!integer(o.workers, 1, 32)) throw new Error('작업 스레드 수는 1~32를 입력하세요.');
  if (!integer(o.runs, 1, 10000) || !integer(o.seed, 0, 0xffffffff) || !integer(o.maxCommands, 1, 10000) || !integer(o.timeoutMs, 100, 3600000)
    || !['random', 'focus', 'conserve', 'explore', 'priorities'].includes(o.strategy)) throw new Error('실행 횟수 1~10000, seed uint32, 명령 1~10000, 시간 100~3600000ms를 입력하세요.');
  if (o.strategy === 'focus' && !content.attributes.some(a => a.id === o.attributeId)) throw new Error('집중할 능력·자원을 선택하세요.');
  if(o.balance!==undefined&&o.balance!=='sampled-v1')throw new Error('Unsupported balance measurement version.');
  if(o.priorityProfile!==undefined)o.priorityProfile=validatePriorityProfile(o.priorityProfile,content);
  if(o.strategy==='priorities'){
    if(!o.priorityProfile)throw new Error('Choose a player-priority profile.');
    if(content.project.roster)throw new Error('Player priorities currently support single-character schedules only.');
    if(o.target)throw new Error('Player priorities are sampled play, not target search.');
    o.balance='sampled-v1';
  }
  if (o.target) {
    if (!['event', 'ending', 'choice'].includes(o.target.kind)) throw new Error('지원하지 않는 목표 종류입니다.');
    const target = (o.target.kind === 'ending' ? content.endings : content.events).find(e => e.id === o.target!.id);
    if (!target) throw new Error('목표 사건/엔딩이 없습니다.');
    if (o.target.kind === 'choice' && !target.nodes.some(n => n.type === 'choice' && n.id === o.target!.nodeId && n.choices.some(c => c.id === o.target!.choiceId))) throw new Error('목표 선택지가 없습니다.');
    if (!integer(o.beamWidth ?? 12, 1, 64) || !integer(o.maxStates ?? 5000, 1, 100000)) throw new Error('탐색 폭 1~64, 상태 수 1~100000을 입력하세요.');
  }
  return o;
}
export function emptyResult(runs: number): AnalysisResult { return { routeGraph:{nodes:[],edges:[],runCount:0}, completed: 0, requested: runs, outcomes: {}, events: {}, choices: {}, nodes: {}, endings: {}, blocked: {}, attributes: {}, traces: [], targetStatus: 'not-requested', proof: null, exploredStates: 0, stopReason: 'completed' }; }

import { isDeepStrictEqual } from 'node:util';
import type { ContentSnapshot } from '../core/model.js';
import { createSession, dispatchSession, type RunOrigin, type RunSession } from '../runtime-service/session.js';
import { encodeSave, decodeSave } from '../runtime-service/save.js';
import { stateInvariants } from './assurance.js';

/** Authoring tests are deliberately separate from content/save fingerprints. */
export type GoalTarget = { kind: 'ending' | 'event'; id: string };
export interface GoalRoute {
  version: 1; id: string; name: string; gameId: string;
  recordedFingerprint: string; origin: RunOrigin; commands: unknown[];
  endingId?: string; target?: GoalTarget; sourceRouteId?: string; checkpoints: number[]; checkpointEvery: number;
}
export function routeTarget(route: GoalRoute): GoalTarget { return route.target ?? { kind: 'ending', id: route.endingId! }; }
export function targetReached(state: RunSession['state'], target: GoalTarget): boolean {
  return target.kind === 'ending' ? state.phase === 'finished' && state.endingId === target.id
    : state.completedEvents.includes(target.id) || state.activeNarrative?.kind === 'event' && state.activeNarrative.id === target.id;
}
const idPattern = /^[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}$/;
function object(value: unknown): value is Record<string, any> { return !!value && typeof value === 'object' && !Array.isArray(value); }
function fail(message: string): never { throw new Error(message); }
export function validateGoalRoute(input: unknown): GoalRoute {
  if (!object(input) || input.version !== 1) fail('지원하지 않는 목표 경로 형식입니다.');
  if (typeof input.id !== 'string' || !idPattern.test(input.id)) fail('경로 ID는 영문·숫자·하이픈·밑줄 1~64자입니다.');
  for (const key of ['name', 'gameId', 'recordedFingerprint'])
    if (typeof input[key] !== 'string' || !input[key].trim() || input[key].length > 256) fail(`목표 경로 ${key}가 올바르지 않습니다.`);
  const target = input.target ?? { kind: 'ending', id: input.endingId };
  if (!object(target) || !['ending','event'].includes(target.kind) || typeof target.id !== 'string' || !target.id.trim() || target.id.length > 256) fail('목표 엔딩 또는 사건을 선택하세요.');
  if (input.target && input.endingId !== undefined && (target.kind !== 'ending' || input.endingId !== target.id)) fail('목표와 기존 엔딩 ID가 다릅니다.');
  if (input.sourceRouteId !== undefined && (typeof input.sourceRouteId !== 'string' || !idPattern.test(input.sourceRouteId))) fail('원본 경로 ID가 올바르지 않습니다.');
  const o = input.origin;
  if (!object(o) || o.mode !== 'full' || !Number.isSafeInteger(o.seed) || o.seed < 0 || o.seed > 0xffffffff || !object(o.initialOverrides)) fail('기록의 시작 정보가 올바르지 않습니다.');
  if (Object.keys(o).some(k => !['seed', 'mode', 'initialOverrides', 'userValues'].includes(k))) fail('알 수 없는 시작 정보가 있습니다.');
  if (!Array.isArray(input.commands) || input.commands.length < 1 || input.commands.length > 10000 || input.commands.some(c => !object(c) || typeof c.type !== 'string')) fail('경로에는 1~10,000개의 명령이 필요합니다.');
  if (!Number.isSafeInteger(input.checkpointEvery) || input.checkpointEvery < 1 || input.checkpointEvery > 1000) fail('저장 간격은 1~1,000 명령입니다.');
  if (!Array.isArray(input.checkpoints) || input.checkpoints.length > 10000 || input.checkpoints.some(n => !Number.isSafeInteger(n) || n < 0 || n > input.commands.length)) fail('저장 검사 위치가 기록 범위를 벗어났습니다.');
  if (Buffer.byteLength(JSON.stringify(input)) > 4 * 1024 * 1024) fail('목표 경로는 4 MiB를 넘을 수 없습니다.');
  return structuredClone(input) as GoalRoute;
}
export function routeFromRecording(record: unknown, metadata: { id: string; name: string; endingId?: string; target?: GoalTarget; checkpointEvery?: number }): GoalRoute {
  if (!object(record) || record.recordingVersion !== 1 || !object(record.state)) fail('플레이어가 기록한 경로 파일을 선택하세요.');
  return validateGoalRoute({ version: 1, ...metadata, gameId: record.gameId, recordedFingerprint: record.contentFingerprint,
    origin: record.origin, commands: record.commands, ...(metadata.target ? {target:metadata.target} : {endingId: metadata.endingId || record.state.endingId}),
    checkpoints: record.checkpoints ?? [], checkpointEvery: metadata.checkpointEvery ?? 10 });
}

export interface GoalRouteReport {
  id: string; name: string; ok: boolean; status: string; contentFingerprint: string;
  recordedFingerprint: string; contentChanged: boolean; expectedEnding: string | undefined; target: GoalTarget; actualEnding: string | null;
  commands: number; executed: number; saves: number; checkpoints: number[];
  failure?: { step: number; command: unknown; code: string; message: string; path?: string };
}
/** Replay original commands on the CURRENT snapshot, never inject old recorded states. */
export async function runGoalRoute(content: ContentSnapshot, input: unknown,
  options: { timeoutMs?: number; cancelled?: () => boolean } = {}): Promise<GoalRouteReport> {
  const route = validateGoalRoute(input), target = routeTarget(route), started = Date.now();
  const report: GoalRouteReport = { id: route.id, name: route.name, ok: false, status: 'running',
    contentFingerprint: content.contentFingerprint, recordedFingerprint: route.recordedFingerprint,
    contentChanged: content.contentFingerprint !== route.recordedFingerprint, expectedEnding: target.kind === 'ending' ? target.id : undefined, target,
    actualEnding: null, commands: route.commands.length, executed: 0, saves: 0, checkpoints: [] };
  const reject = (step: number, command: unknown, code: string, message: string, path?: string) => {
    report.status = code === 'CANCELLED' ? 'cancelled' : code === 'TIMEOUT' ? 'incomplete' : 'failed';
    report.failure = { step, command, code, message, ...(path ? { path } : {}) }; return report;
  };
  if (route.gameId !== content.project.gameId) return reject(0, null, 'GAME_MISMATCH', '다른 게임에서 기록한 경로입니다.');
  if (!(target.kind === 'ending' ? content.endings : content.events).some(e => e.id === target.id)) return reject(0, null, target.kind === 'ending' ? 'UNKNOWN_ENDING' : 'UNKNOWN_EVENT', '목표가 삭제되었거나 ID가 변경되었습니다.', '/target');
  const created = createSession(content, route.origin.seed, route.origin);
  if (!created.ok) return reject(0, null, created.error.code, created.error.message, created.error.path);
  let baseline: RunSession = created.session, restored: RunSession = created.session, reached = targetReached(created.session.state, target);
  const checkpoints = new Set([0, ...route.checkpoints, route.commands.length]);
  for (let n = route.checkpointEvery; n < route.commands.length; n += route.checkpointEvery) checkpoints.add(n);
  for (let index = 0; index <= route.commands.length; ++index) {
    if (index % 16 === 0) await new Promise<void>(resolve => setImmediate(resolve));
    if (options.cancelled?.()) return reject(index, route.commands[index] ?? null, 'CANCELLED', '검증을 취소했습니다.');
    if (Date.now() - started > (options.timeoutMs ?? 60000)) return reject(index, route.commands[index] ?? null, 'TIMEOUT', '시간 제한으로 검사를 끝내지 못했습니다.');
    if (checkpoints.has(index)) {
      const saved = await encodeSave(content, restored);
      if (!saved.ok) return reject(index, null, 'SAVE_FAILED', saved.error.message);
      const loaded = await decodeSave(content, saved.text);
      if (!loaded.ok) return reject(index, null, 'LOAD_FAILED', loaded.error.message);
      if (!isDeepStrictEqual(loaded.session, baseline)) return reject(index, null, 'SAVE_DIVERGED', '중간 저장 복원 결과가 저장 없이 진행한 세션과 다릅니다.');
      restored = loaded.session; report.saves++; report.checkpoints.push(index);
    }
    if (index === route.commands.length) break;
    const command = route.commands[index];
    const next = dispatchSession(content, baseline, command, baseline.state.stateRevision);
    if (!next.ok) return reject(index + 1, command, next.error.code, next.error.message, next.error.path);
    const replay = dispatchSession(content, restored, command, restored.state.stateRevision);
    if (!replay.ok) return reject(index + 1, command, 'RESTORE_REPLAY_FAILED', replay.error.message, replay.error.path);
    if (!isDeepStrictEqual(next.session, replay.session)) return reject(index + 1, command, 'REPLAY_DIVERGED', '저장 복원 이후의 진행/난수 결과가 원래 진행과 다릅니다.');
    const violations = stateInvariants(content, next.session.state);
    if (violations.length) return reject(index + 1, command, 'STATE_INVALID', JSON.stringify(violations));
    baseline = next.session; restored = replay.session; report.executed++; reached ||= targetReached(baseline.state, target);
  }
  report.actualEnding = baseline.state.endingId;
  if (!reached)
    return reject(route.commands.length, null, 'TARGET_NOT_REACHED', `목표 ${target.kind}:${target.id}, 실제 ${baseline.state.endingId ?? baseline.state.phase}. 기록된 경로로 목표에 도달하지 못했습니다.`);
  report.ok = true; report.status = 'passed'; return report;
}

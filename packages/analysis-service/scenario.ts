import type { ContentSnapshot, Command } from '../core/model.js';
import type { Trace } from './model.js';
import { createRun, dispatchCommand } from '../native-bridge/backend.js';
import { replayTrace } from './runner.js';
import { runScenario } from '../runtime-service/scenario.js';
import type { Json } from '../validation/model.js';

export async function traceScenario(content: ContentSnapshot, trace: Trace, id: string): Promise<Json> {
  if (!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(id) || id.length > 64) throw new Error('시나리오 ID 형식이 올바르지 않습니다.');
  replayTrace(content, trace);
  if (trace.state.phase !== 'finished' || !trace.state.endingId) throw new Error('회귀 시나리오 등록은 끝까지 완료한 경로만 지원합니다. 사건 목표는 재현 기록으로 보관됩니다.');
  if (trace.commands.some(c => ['navigate', 'visit', 'buy', 'interact'].includes(c.type))) {
    const scenario: Json = { formatVersion: '0.1.0', id, seed: trace.seed,
      initialOverrides: { attributes: {}, flags: {}, relationships: {} },
      commands: trace.commands.map(command => ({ type: 'dispatch', command: structuredClone(command) as unknown as Json })),
      assertions: [{ kind: 'ending', endingId: trace.state.endingId }], stepLimit: 100000 };
    const verified = await runScenario(content, scenario);
    if (!verified.ok || JSON.stringify(verified.session!.state) !== JSON.stringify(trace.state))
      throw new Error('생활 행동 경로를 시나리오로 변환한 뒤 재실행 결과가 다릅니다.');
    return scenario;
  }
  const start = createRun(content, trace.seed, { mode: 'full' }); if (!start.ok) throw new Error(start.error.message);
  let state = start.state;
  const periods: { activity: string; choices: { eventId: string; nodeId: string; choiceId: string }[] }[] = [];
  for (const command of trace.commands) {
    if (command.type === 'executeActivity') periods[state.periodIndex] = { activity: state.schedule![state.periodIndex % content.project.periodsPerMonth]!, choices: [] };
    if (command.type === 'choose') {
      if (state.activeNarrative?.kind !== 'event') throw new Error('현재 시나리오 계약은 엔딩 내부 선택지 등록을 지원하지 않습니다. 재현 기록은 유지됩니다.');
      periods[state.periodIndex]!.choices.push({ eventId: state.activeNarrative.id, nodeId: state.activeNarrative.nodeId, choiceId: command.choiceId });
    }
    const next = dispatchCommand(content, state, command as Command, state.stateRevision); if (!next.ok) throw new Error(next.error.message); state = next.state;
  }
  const commands: Json[] = [];
  for (let i = 0; i < periods.length; i++) {
    if (i % content.project.periodsPerMonth === 0) commands.push({ type: 'planMonth', activityIds: periods.slice(i, i + content.project.periodsPerMonth).map(p => p.activity) });
    commands.push({ type: 'runPeriod', choices: periods[i]!.choices });
  }
  const scenario: Json = { formatVersion: '0.1.0', id, seed: trace.seed, initialOverrides: { attributes: {}, flags: {}, relationships: {} }, commands,
    assertions: [{ kind: 'ending', endingId: trace.state.endingId }], stepLimit: 100000 };
  const verified = await runScenario(content, scenario); if (!verified.ok) throw new Error('시나리오 변환 후 재실행에 실패했습니다.');
  const normalize = (s: typeof state) => ({ ...s, stateRevision: 0 });
  if (JSON.stringify(normalize(verified.session!.state)) !== JSON.stringify(normalize(trace.state))) throw new Error('시나리오 변환 후 최종 상태가 다릅니다.');
  return scenario;
}

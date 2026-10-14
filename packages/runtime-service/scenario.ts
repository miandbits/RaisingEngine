import type { Assertion, ScenarioSpecification } from '../schema/generated/scenario.js';
import type { ContentSnapshot, CoreError, DeepReadonly, Transition } from '../core/model.js';
import { evaluateCondition, getNarrativeView } from '../core/index.js';
import { CoreFault, requireCore } from '../core/util.js';
import { validateStructure } from '../validation/structure.js';
import type { Json } from '../validation/model.js';
import { canonicalJson } from '../project-service/snapshot.js';
import { createSession, dispatchSession, type RunSession, type SessionResult } from './session.js';

export interface AssertionCheck { path: string; passed: boolean; assertion: Assertion }
export type ScenarioResult = {
  scenarioId: string; steps: number; assertions: AssertionCheck[]; transitions: DeepReadonly<Transition>[];
} & ({ ok: true; session: RunSession } | { ok: false; session: RunSession | null; error: CoreError });
export interface ScenarioOptions {
  /** Optional persistence checkpoint after initial creation and every accepted core command. */
  checkpoint?: (session: RunSession) => Promise<SessionResult>;
}

export async function runScenario(content: ContentSnapshot, input: unknown, options: ScenarioOptions = {}): Promise<ScenarioResult> {
  // Detach before the first await so validation and execution see the same scenario.
  const candidate = structuredClone(input);
  const issues = await validateStructure({ kind: 'scenario', file: '<scenario>', data: candidate as Json, version: '', locations: new Map() });
  if (issues.length) return { ok: false, scenarioId: '<invalid>', steps: 0, assertions: [], transitions: [], session: null,
    error: { code: 'SCENARIO_SCHEMA', message: issues[0]!.message, path: issues[0]!.pointer } };
  const scenario = candidate as ScenarioSpecification;
  let session: RunSession | null = null;
  const currentState = () => session!.state;
  let steps = 0;
  const transitions: DeepReadonly<Transition>[] = [];
  const assertions: AssertionCheck[] = [];
  const report = () => ({ scenarioId: scenario.id, steps, assertions, transitions });
  const checkpoint = async () => {
    if (!options.checkpoint) return;
    const result = await options.checkpoint(session!);
    if (!result.ok) throw new CoreFault(result.error.code, result.error.message, result.error.path);
    requireCore(canonicalJson(result.session) === canonicalJson(session), 'CHECKPOINT_MISMATCH', '저장·복원 후 실행 세션이 달라졌습니다.', '/checkpoint');
    session = result.session;
  };
  const send = async (command: unknown, location: string) => {
    requireCore(steps < scenario.stepLimit, 'SCENARIO_STEP_LIMIT', `실행 한도 ${scenario.stepLimit}에 도달했습니다.`, location);
    const result = dispatchSession(content, session!, command, session!.state.stateRevision);
    if (!result.ok) throw new CoreFault(result.error.code, result.error.message, result.error.path);
    session = result.session; steps++; transitions.push(result.transition);
    await checkpoint();
  };
  const checkAssertions = (items: Assertion[], location: string) => {
    for (const [i, assertion] of items.entries()) {
      const pointer = `${location}/${i}`;
      const passed = assertion.kind === 'ending'
        ? session!.state.phase === 'finished' && session!.state.endingId === assertion.endingId
        : evaluateCondition(content, session!.state, assertion.condition, `${pointer}/condition`);
      assertions.push({ path: pointer, assertion, passed });
    }
    requireCore(assertions.every(a => a.passed), 'SCENARIO_ASSERTION_FAILED', '시나리오 기대값을 만족하지 않습니다.', assertions.find(a => !a.passed)?.path ?? location);
  };
  try {
    const created = createSession(content, scenario.seed, { mode: 'full', initialOverrides: scenario.initialOverrides });
    if (!created.ok) throw new CoreFault(created.error.code, created.error.message, created.error.path);
    session = created.session;
    await checkpoint();
    for (const [i, command] of scenario.commands.entries()) {
      const location = `/commands/${i}`;
      if (command.type === 'dispatch') { await send(command.command, `${location}/command`); continue; }
      if (command.type === 'planMonth') {
        requireCore(session!.state.phase === 'planning', 'SCENARIO_PHASE', 'planMonth는 월초 일정 대기 상태에서만 가능합니다.', location);
        await send({ type: 'setSchedule', activityIds: command.activityIds }, location);
        continue;
      }
      requireCore(currentState().phase === 'ready', 'SCENARIO_PHASE', 'runPeriod는 준비된 활동부터 시작해야 합니다.', location);
      const period = session!.state.periodIndex;
      let choiceIndex = 0;
      await send({ type: 'executeActivity' }, location);
      while (session!.state.periodIndex === period && session!.state.phase !== 'finished') {
        switch (session!.state.phase) {
          case 'activity-result': await send({ type: 'acknowledgeResult' }, location); break;
          case 'period-end': await send({ type: 'resolvePeriodEnd' }, location); break;
          case 'narrative': {
            const view = getNarrativeView(content, session!.state)!;
            if (view.node.type !== 'choice') { await send({ type: 'advanceNarrative' }, location); break; }
            requireCore(view.cursor.kind === 'event', 'SCENARIO_ENDING_CHOICE', '현재 시나리오 계약은 엔딩 선택 주소를 지원하지 않습니다.', location);
            const selected = command.choices[choiceIndex];
            requireCore(selected, 'SCENARIO_CHOICE_MISSING', `선택이 필요합니다: ${view.cursor.id}/${view.cursor.nodeId}`, `${location}/choices/${choiceIndex}`);
            requireCore(selected.eventId === view.cursor.id && selected.nodeId === view.cursor.nodeId,
              'SCENARIO_CHOICE_TARGET', `기대 선택 위치는 ${view.cursor.id}/${view.cursor.nodeId}입니다.`, `${location}/choices/${choiceIndex}`);
            await send({ type: 'choose', choiceId: selected.choiceId }, location); choiceIndex++;
            break;
          }
          default: throw new CoreFault('SCENARIO_PHASE', '기간 처리 중 예상하지 않은 상태입니다.', location);
        }
      }
      requireCore(choiceIndex === command.choices.length, 'SCENARIO_CHOICE_UNUSED', '사용하지 않은 선택 입력이 남았습니다.', `${location}/choices/${choiceIndex}`);
      checkAssertions(command.assertions ?? [], `${location}/assertions`);
    }
    checkAssertions(scenario.assertions, '/assertions');
    return { ...report(), ok: true, session: session! };
  } catch (error) {
    if (error instanceof CoreFault) return { ...report(), ok: false, session, error: error.diagnostic() };
    throw error;
  }
}

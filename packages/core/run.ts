import { calendarProblem } from './calendar.js';
import { createCustomStates } from './custom-states.js';
import { createProgression, guardProgression, startTimedAction, progressSession } from './progression.js';
import { startCalendar } from './unified-time.js';
import { resolveUserValues } from './user-fields.js';
import { createRoster, assignActivities, executeRoster, beginActorAction } from './roster.js';
import type { Overrides } from '../schema/generated/project.js';
import { CORE_VERSION, type Command, type ContentSnapshot, type CreateResult, type DispatchResult,
  type RunData, type RunState, type Values, type ActivityResult, type RunMode, type NarrativeStep, type EventDecision } from './model.js';
import { applyEffects, calendarTime, evaluateCondition, evaluateNumber } from './evaluation.js';
import { seedRandom, sampleBelow } from './random.js';
import { CoreFault, freeze, int32, object, requireCore } from './util.js';
import { applyScene, finishPeriod, resolvePeriodEnd, stepNarrative, startAppointmentEvents } from './narrative.js';
import { applyDiet, applyGameplay, createGameplay } from '../gameplay/service.js';
import { applyPolicies, beginRuleCommand, emitRules, emitRelationshipRules, runBoundaryRules, runDayRules } from '../gameplay/rules.js';
import { applySystems, createSystems, startExpedition, syncRelationshipStages } from '../systems/service.js';

function checkContent(content: ContentSnapshot): void {
  const problem=calendarProblem(content.project);requireCore(!problem,'INCOMPATIBLE_CONTENT',problem?.message??'', '/project'+(problem?.path??''));
  requireCore(!content.project.periodFlow || (!content.project.roster && !content.project.progression),
    'PERIOD_FLOW_MODE', '기간 단계는 현재 단일 대상의 월 일정에서 사용할 수 있습니다.', '/project/periodFlow');
  requireCore(content.snapshotVersion === '1' && /^sha256:[0-9a-f]{64}$/.test(content.contentFingerprint),
    'INCOMPATIBLE_CONTENT', '지원하는 buildSnapshot 결과가 필요합니다.', '/content');
  requireCore(Number.isInteger(content.project.periodsPerMonth) && content.project.periodsPerMonth >= 1 && content.project.periodsPerMonth <= 12,
    'INCOMPATIBLE_CONTENT', '월별 일정 칸 수는 1..12 정수여야 합니다.', '/project/periodsPerMonth');
}

function mergeOverrides(content: ContentSnapshot, values: Values, overrides: unknown, path: string): void {
  requireCore(object(overrides) && Object.keys(overrides).length === 3
    && object(overrides.attributes) && object(overrides.flags) && object(overrides.relationships),
  'INVALID_OVERRIDES', 'attributes·flags·relationships 객체가 필요합니다.', path);
  for (const [id, value] of Object.entries(overrides.attributes)) {
    const definition = content.attributes.find(a => a.id === id);
    requireCore(definition, 'UNKNOWN_TARGET', `알 수 없는 능력치: ${id}`, `${path}/attributes/${id}`);
    requireCore(typeof value === 'number' && Number.isInteger(value) && value >= definition.min && value <= definition.max,
      'INITIAL_RANGE', `초기 값은 ${definition.min}..${definition.max} 정수여야 합니다.`, `${path}/attributes/${id}`);
    values.attributes[id] = value;
  }
  for (const [id, value] of Object.entries(overrides.flags)) {
    requireCore(Object.hasOwn(values.flags, id), 'UNKNOWN_FLAG', `알 수 없는 플래그: ${id}`, `${path}/flags/${id}`);
    requireCore(typeof value === 'boolean', 'INITIAL_TYPE', '플래그 초기 값은 boolean이어야 합니다.', `${path}/flags/${id}`);
    values.flags[id] = value;
  }
  for (const [characterId, relationships] of Object.entries(overrides.relationships)) {
    const character = content.characters.find(c => c.id === characterId);
    requireCore(character, 'UNKNOWN_TARGET', `알 수 없는 인물: ${characterId}`, `${path}/relationships/${characterId}`);
    requireCore(object(relationships), 'INVALID_OVERRIDES', '관계 초기 값은 객체여야 합니다.', `${path}/relationships/${characterId}`);
    for (const [id, value] of Object.entries(relationships)) {
      const definition = character.relationships.find(r => r.id === id);
      const location = `${path}/relationships/${characterId}/${id}`;
      requireCore(definition, 'UNKNOWN_TARGET', `알 수 없는 관계: ${id}`, location);
      requireCore(typeof value === 'number' && Number.isInteger(value) && value >= definition.min && value <= definition.max,
        'INITIAL_RANGE', `초기 값은 ${definition.min}..${definition.max} 정수여야 합니다.`, location);
      values.relationships[characterId]![id] = value;
    }
  }
}

export function createRun(content: ContentSnapshot, seed: number,
  options: { mode: RunMode; initialOverrides?: Overrides; userValues?: Record<string, string> }): CreateResult {
  try {
    checkContent(content);
    requireCore(options?.mode === 'activities-only' || options?.mode === 'full', 'UNSUPPORTED_MODE', 'activities-only 또는 full 모드를 명시해야 합니다.', '/mode');
    const userValues = resolveUserValues(content.project.userFields, options.userValues);
    const state: RunData = {
      ...(userValues ? { userValues } : {}),
      coreVersion: CORE_VERSION, contentFingerprint: content.contentFingerprint, mode: options.mode,
      stateRevision: 0, periodIndex: 0, phase: 'planning', schedule: null, rng: seedRandom(seed),
      attributes: Object.fromEntries(content.attributes.map(a => [a.id, a.initial])),
      flags: Object.fromEntries(content.flags.map(f => [f.id, f.initial])),
      relationships: Object.fromEntries(content.characters.map(c => [c.id, Object.fromEntries(c.relationships.map(r => [r.id, r.initial]))])),
      completedEvents: [], lastActivityResult: null,
      activeNarrative: null, endingId: null, presentation: {
        backgroundAssetId: content.project.visual?.defaultScene?.backgroundAssetId ?? null,
        musicAssetId: content.project.visual?.defaultScene?.musicAssetId ?? null,
        portraits: (content.project.visual?.defaultScene?.portraits ?? []).map(p=>({...p})),
      },
    };
    mergeOverrides(content, state, content.project.initialState, '/project/initialState');
    if (content.project.gameplay) {
      state.gameplay = createGameplay(content.project.gameplay);
      const presentation = content.project.gameplay.locations.find(x => x.id === state.gameplay!.locationId)?.presentation;
      if (presentation) applyScene(state, presentation, []);
    }
    if (content.project.systems) state.systems = createSystems();
    if (options.initialOverrides !== undefined) mergeOverrides(content, state, options.initialOverrides, '/initialOverrides');
    createRoster(content, state);
    createCustomStates(content, state);
    createProgression(content, state);
    startAppointmentEvents(content, state, 'period-start');
    return { ok: true, state: freeze(state) };
  } catch (error) {
    if (error instanceof CoreFault) return { ok: false, error: error.diagnostic() };
    throw error;
  }
}

function parseCommand(input: unknown, periods: number): Command {
  requireCore(object(input), 'INVALID_COMMAND', '명령은 객체여야 합니다.', '/command');
  if(input.type==='tacticsUnequip'||input.type==='tacticsAction'||input.type==='tacticsEquip'||input.type==='tacticsFlee'){
    const fields=input.type==='tacticsAction'?['unitId','skillId','targetId']:input.type==='tacticsEquip'?['unitId','equipmentId']:input.type==='tacticsUnequip'?['unitId','slot']:[];requireCore(Object.keys(input).length===fields.length+1&&fields.every(k=>typeof input[k]==='string'&&String(input[k]).length>0),'INVALID_COMMAND','전투 명령의 대상과 스킬을 확인하세요.','/command');
  } else if (['acceptQuest', 'claimQuest', 'equip', 'unequip', 'moveExpedition'].includes(input.type as string)) {
    const field = input.type === 'equip' ? 'equipmentId' : input.type === 'unequip' ? 'slot' : input.type === 'moveExpedition' ? 'nodeId' : 'questId';
    requireCore(Object.keys(input).length === 2 && typeof input[field] === 'string' && (input[field] as string).length > 0,
      'INVALID_COMMAND', '시스템 명령의 ID가 필요합니다.', `/command/${field}`);
    if (input.type === 'unequip') requireCore(['weapon', 'armor', 'accessory'].includes(input.slot as string), 'INVALID_COMMAND', '장비 슬롯이 올바르지 않습니다.', '/command/slot');
  } else if (input.type === 'battleAction') {
    requireCore(['attack', 'guard', 'flee', 'useItem'].includes(input.action as string) && Object.keys(input).length === (input.action === 'useItem' ? 3 : 2)
      && (input.action !== 'useItem' || typeof input.itemId === 'string' && input.itemId.length > 0), 'INVALID_COMMAND', '전투 행동과 물품 ID를 확인해 주세요.', '/command');
  } else if (input.type === 'returnFromExpedition' || input.type === 'startNodeBattle') {
    requireCore(Object.keys(input).length === 1, 'INVALID_COMMAND', '귀환 명령에 추가 필드를 사용할 수 없습니다.', '/command');
  } else if (input.type === 'assignActivities') {
    requireCore(Object.keys(input).length === 2 && object(input.assignments) && Object.values(input.assignments).every(id => typeof id === 'string'), 'INVALID_COMMAND', '대상별 활동 배정이 필요합니다.', '/command/assignments');
  } else if (input.type === 'setSchedule') {
    requireCore(Object.keys(input).length === 2 && Array.isArray(input.activityIds) && input.activityIds.length === periods
      && Array.from(input.activityIds).every(id => typeof id === 'string'),
    'INVALID_COMMAND', '월별 일정 칸 수와 같은 개수의 활동 ID가 필요합니다.', '/command/activityIds');
  } else if (input.type === 'navigate' || input.type === 'visit' || input.type === 'interact') {
    const field = input.type === 'navigate' ? 'screenId' : input.type === 'visit' ? 'locationId' : 'interactionId';
    requireCore(Object.keys(input).length === 2 && typeof input[field] === 'string' && (input[field] as string).length > 0,
      'INVALID_COMMAND', `${input.type}에는 ${field}가 필요합니다.`, `/command/${field}`);
  } else if (input.type === 'selectPolicy') {
    requireCore(Object.keys(input).length === 3 && typeof input.groupId === 'string' && input.groupId.length > 0 && (input.optionId === null || typeof input.optionId === 'string' && input.optionId.length > 0), 'INVALID_COMMAND', '정책 그룹 ID와 선택 ID 또는 null이 필요합니다.', '/command');
  } else if (input.type === 'cancelActorSession') {
    requireCore(Object.keys(input).length===2&&typeof input.actorId==='string'&&input.actorId.length>0,'INVALID_COMMAND','취소할 실행자가 필요합니다.','/command/actorId');
  } else if (input.type === 'waitTime') {
    requireCore(Object.keys(input).length===2 && typeof input.ticks==='number' && Number.isInteger(input.ticks) && input.ticks>0 && input.ticks<=1000000,'INVALID_COMMAND','대기 시간은 1..1000000 정수입니다.','/command/ticks');
  } else if (input.type === 'executeActorAction') {
    requireCore(Object.keys(input).length===4 && typeof input.actorId==='string' && input.actorId.length>0 && typeof input.actionId==='string' && input.actionId.length>0 && typeof input.quantity==='number' && Number.isInteger(input.quantity) && input.quantity>0 && input.quantity<=2147483647,'INVALID_COMMAND','실행자·행동 ID와 양의 정수 수량이 필요합니다.','/command');
  } else if (input.type === 'executeAction') {
    requireCore(Object.keys(input).length === 3 && typeof input.actionId === 'string' && input.actionId.length > 0 && typeof input.quantity === 'number' && Number.isInteger(input.quantity) && input.quantity > 0 && input.quantity <= 2147483647, 'INVALID_COMMAND', '행동 ID와 양의 정수 수량이 필요합니다.', '/command');
  } else if (input.type === 'setDiet') {
    requireCore(Object.keys(input).length === 2 && (input.dietId === null || typeof input.dietId === 'string' && input.dietId.length > 0), 'INVALID_COMMAND', '식단 ID 또는 null이 필요합니다.', '/command/dietId');
  } else if (input.type === 'useService') {
    requireCore(Object.keys(input).length === 3 && typeof input.serviceId === 'string' && input.serviceId.length > 0 && typeof input.quantity === 'number' && Number.isInteger(input.quantity) && input.quantity > 0 && input.quantity <= 2147483647, 'INVALID_COMMAND', '서비스 ID와 양의 정수 수량이 필요합니다.', '/command');
  } else if (input.type === 'buy' || input.type === 'sell') {
    requireCore(Object.keys(input).length === 4 && typeof input.shopId === 'string' && input.shopId.length > 0
      && typeof input.itemId === 'string' && input.itemId.length > 0 && typeof input.quantity === 'number'
      && Number.isInteger(input.quantity) && input.quantity > 0 && input.quantity <= 2147483647,
      'INVALID_COMMAND', 'buy에는 shopId, itemId와 양의 정수 quantity가 필요합니다.', '/command');
  } else if (input.type === 'choose') {
    requireCore(Object.keys(input).length === 2 && typeof input.choiceId === 'string' && input.choiceId.length > 0,
      'INVALID_COMMAND', 'choose에는 choiceId가 필요합니다.', '/command/choiceId');
  } else {
    requireCore(Object.keys(input).length === 1 && ['advanceSession', 'cancelSession', 'executeActivity', 'acknowledgeResult', 'completePeriodWithoutEvents', 'resolvePeriodEnd', 'advanceNarrative'].includes(input.type as string),
      'INVALID_COMMAND', '지원하지 않는 명령 또는 추가 필드입니다.', '/command/type');
  }
  return structuredClone(input) as Command;
}

function executeActivity(content: ContentSnapshot, state: RunState, draft: RunData, individual = false): ActivityResult {
  requireCore(state.phase === 'ready' && state.schedule, 'WRONG_PHASE', '실행할 일정이 준비되지 않았습니다.', '/phase');
  const activityId = state.schedule[state.periodIndex % content.project.periodsPerMonth]!;
  const activity = content.activities.find(a => a.id === activityId);
  requireCore(activity, 'UNKNOWN_ACTIVITY', `알 수 없는 활동: ${activityId}`, '/schedule');
  const path = `/activities/${activityId}`;
  const result: ActivityResult = { activityId, time: calendarTime(content, state), changes: [], success: null };
  if (!individual) {
    if (draft.periodPreparation) result.changes.push(...structuredClone(draft.periodPreparation.changes));
    else {
      runBoundaryRules(content, draft, 'periodStart', result.changes);
      applyPolicies(content, draft, result.changes, 'beforeActivity');
    }
    if (content.project.roster) {
      const batch = executeRoster(content, draft, executeActivity);
      batch.changes.unshift(...result.changes);
      applyPolicies(content, draft, batch.changes, 'afterActivity');
      runDayRules(content, draft, batch.changes);
      return batch;
    }
  }
  // Generic before triggers are part of this transaction and feed its checks.
  // Snapshot before legacy diet and activity costs preserves older projects' rolls.
  const activityInput = structuredClone(draft);
  requireCore(evaluateCondition(content, activityInput, activity.requirements, `${path}/requirements`),
    'REQUIREMENTS_NOT_MET', `활동 조건을 만족하지 않습니다: ${activityId}`, `${path}/requirements`);
  if (!individual && !draft.periodPreparation) applyDiet(content, draft, result.changes);
  for (const [i, cost] of activity.costs.entries()) {
    const definition = content.attributes.find(a => a.id === cost.attributeId)!;
    requireCore(draft.attributes[cost.attributeId]! - cost.amount >= definition.min,
      'INSUFFICIENT_COST', `${cost.attributeId}: 비용 ${cost.amount} 지불 후 최솟값 ${definition.min}을 유지할 수 없습니다.`, `${path}/costs/${i}`);
  }
  if (activity.successCheck) {
    // Before policies affect probability; activity costs/effects do not. Endpoints draw too.
    const requestedChance = evaluateNumber(content, activityInput, activity.successCheck.chance, `${path}/successCheck/chance`);
    const chance = Math.min(10000, Math.max(0, requestedChance));
    const sampled = sampleBelow(draft.rng, 10000);
    result.success = { requestedChance, chance, roll: sampled.value, passed: sampled.value < chance, rawDraws: sampled.rng.draws - draft.rng.draws };
    draft.rng = sampled.rng;
  }
  activity.costs.forEach((cost, i) => {
    const before = draft.attributes[cost.attributeId]!;
    const after = int32(before - cost.amount, `${path}/costs/${i}`);
    draft.attributes[cost.attributeId] = after;
    result.changes.push({ kind: 'numeric', source: `${path}/costs/${i}`, target: { kind: 'attribute', attributeId: cost.attributeId },
      before, operand: -cost.amount, requested: after, after, delta: -cost.amount, clamped: false });
  });
  applyEffects(content, draft, activity.baseEffects, `${path}/baseEffects`, result.changes);
  if (result.success) {
    const branch = result.success.passed ? 'successEffects' : 'failureEffects';
    applyEffects(content, draft, activity[branch]!, `${path}/${branch}`, result.changes);
  }
  if (!individual) {
    applyPolicies(content, draft, result.changes, 'afterActivity');
    runDayRules(content, draft, result.changes);
  }
  draft.phase = 'activity-result';
  draft.lastActivityResult = result;
  return result;
}

/** Pure transaction: every rejection returns the exact input state object. */
export function dispatchCommand(content: ContentSnapshot, state: RunState, input: unknown, expectedRevision: number): DispatchResult {
  try {
    checkContent(content);
    requireCore(state.coreVersion === CORE_VERSION && (state.mode === 'activities-only' || state.mode === 'full'),
      'INCOMPATIBLE_STATE', '실행 규칙 버전 또는 모드가 다릅니다.', '/coreVersion');
    requireCore(state.contentFingerprint === content.contentFingerprint, 'CONTENT_MISMATCH', '실행 중인 콘텐츠와 스냅샷이 다릅니다.', '/contentFingerprint');
    requireCore(Number.isSafeInteger(expectedRevision) && expectedRevision >= 0, 'INVALID_REVISION', '예상 버전은 음수가 아닌 안전한 정수여야 합니다.', '/expectedRevision');
    requireCore(expectedRevision === state.stateRevision, 'REVISION_CONFLICT', `예상 버전 ${expectedRevision}, 현재 버전 ${state.stateRevision}.`, '/expectedRevision');
    requireCore(Number.isSafeInteger(state.stateRevision) && state.stateRevision < Number.MAX_SAFE_INTEGER,
      'REVISION_OVERFLOW', '상태 버전을 증가시킬 수 없습니다.', '/stateRevision');
    calendarTime(content, state);
    const command = parseCommand(input, content.project.periodsPerMonth);
    const draft = structuredClone(state) as RunData;
    guardProgression(draft, command);
    beginRuleCommand(draft);
    let activity: ActivityResult | null = null;
    let narrative: NarrativeStep | null = null;
    let eventDecisions: EventDecision[] = [];
    const soundCues: string[] = [];
    const settleActivity=()=>{
      activity=executeActivity(content,draft,draft);
      const definition=content.activities.find(a=>a.id===activity!.activityId)!;
      if(definition.presentation)applyScene(draft,definition.presentation,soundCues);
      if(definition.expeditionId)startExpedition(content,draft,definition.expeditionId,soundCues);
    };
    switch (command.type) {
      case 'advanceSession': case 'cancelSession': case 'cancelActorSession': case 'waitTime': progressSession(content,draft,command,soundCues,settleActivity); break;
      case 'assignActivities': assignActivities(content, state, draft, command.assignments); break;
      case 'executeActorAction':
        if(startTimedAction(content,draft,command))break;
        beginActorAction(content,draft,command.actorId,command.actionId);
        applyGameplay(content,draft,{type:'executeAction',actionId:command.actionId,quantity:command.quantity},soundCues);
        break;
      case 'tacticsUnequip': case 'tacticsAction': case 'tacticsFlee': case 'tacticsEquip': case 'acceptQuest': case 'claimQuest': case 'equip': case 'unequip': case 'moveExpedition': case 'returnFromExpedition': case 'startNodeBattle': case 'battleAction':
        applySystems(content, draft, command, soundCues); break;
      case 'navigate': case 'visit': case 'buy': case 'interact': case 'sell': case 'useService': case 'setDiet': case 'executeAction': case 'selectPolicy':
        if(command.type==='executeAction' && startTimedAction(content,draft,command))break;
        applyGameplay(content, draft, command, soundCues);
        if (command.type === 'executeAction') {
          const expeditionId = content.project.gameplay?.actions?.find(a => a.id === command.actionId)?.expeditionId;
          if (expeditionId) {
            startExpedition(content, draft, expeditionId, soundCues);
            draft.systems!.expedition!.returnPhase = state.phase as 'planning' | 'ready';
          }
        }
        break;
      case 'setSchedule': {
        requireCore(!content.project.roster, 'ROSTER_ASSIGNMENTS', '다인 프로젝트는 대상별 활동 배정을 사용하세요.', '/command');
        requireCore(state.phase === 'planning' || state.phase === 'ready', 'WRONG_PHASE', '일정 편집을 기다리는 상태가 아닙니다.', '/phase');
        command.activityIds.forEach((id, i) => {
          requireCore(content.activities.some(a => a.id === id), 'UNKNOWN_ACTIVITY', `알 수 없는 활동: ${id}`, `/command/activityIds/${i}`);
          requireCore(i >= state.periodIndex % content.project.periodsPerMonth + (state.activityIntermission ? 1 : 0) || state.schedule?.[i] === id, 'COMPLETED_SLOT', '완료한 일정은 변경할 수 없습니다.', `/command/activityIds/${i}`);
        });
        draft.schedule = command.activityIds;
        draft.phase = 'ready';
        break;
      }
      case 'executeActivity': {
        requireCore(!draft.activityIntermission, 'INTERMISSION_ACTIVE', '자유 행동을 마친 뒤 다음 일정으로 진행하세요.', '/activityIntermission');
        if (content.project.periodFlow?.preparePolicies && !draft.periodPreparation) {
          const changes: ActivityResult['changes'] = [];
          requireCore(draft.phase === 'ready' && draft.schedule, 'WRONG_PHASE', '실행할 일정이 준비되지 않았습니다.', '/phase');
          runBoundaryRules(content, draft, 'periodStart', changes);
          applyPolicies(content, draft, changes, 'beforeActivity');
          applyDiet(content, draft, changes);
          draft.periodPreparation = { changes };
          break;
        }
        if(startCalendar(content,draft))break;
        activity = executeActivity(content, state, draft);
        const presentation = content.activities.find(a => a.id === activity!.activityId)!.presentation;
        if (presentation) applyScene(draft, presentation, soundCues);
        const expeditionId = content.activities.find(a => a.id === activity!.activityId)!.expeditionId;
        if (expeditionId) startExpedition(content, draft, expeditionId, soundCues);
        break;
      }
      case 'acknowledgeResult': {
        const intermission = content.project.periodFlow?.intermission;
        if (state.activityIntermission && state.phase === 'ready') {
          requireCore(intermission && draft.flags[intermission.completionFlagId], 'INTERMISSION_PENDING', '아직 자유 행동을 마치지 않았습니다.', '/activityIntermission');
          delete draft.activityIntermission;
          draft.phase = 'period-end';
        } else {
          requireCore(state.phase === 'activity-result', 'WRONG_PHASE', '확인할 활동 결과가 없습니다.', '/phase');
          if (intermission && state.mode === 'full') {
            draft.activityIntermission = true;
            draft.flags[intermission.completionFlagId] = false;
            applyEffects(content, draft, intermission.entryEffects ?? [], '/project/periodFlow/intermission/entryEffects', []);
            draft.gameplay!.screenId = intermission.screenId;
            draft.phase = 'ready';
          } else draft.phase = 'period-end';
        }
        break;
      }
      case 'completePeriodWithoutEvents':
        requireCore(state.phase === 'period-end' && state.mode === 'activities-only', 'WRONG_PHASE', '활동 전용 모드의 기간 종료 경계가 아닙니다.', '/phase');
        runBoundaryRules(content, draft, 'periodEnd');
        finishPeriod(content, draft);
        break;
      case 'resolvePeriodEnd': eventDecisions = resolvePeriodEnd(content, draft); break;
      case 'advanceNarrative': narrative = stepNarrative(content, draft, undefined, soundCues); break;
      case 'choose': narrative = stepNarrative(content, draft, command.choiceId, soundCues); break;
    }
    if (command.type === 'visit' && draft.gameplay?.locationId !== state.gameplay?.locationId) emitRules(content, draft, { type: 'locationEnter', locationId: draft.gameplay!.locationId });
    if (command.type === 'buy') emitRules(content, draft, { type: 'purchaseComplete', shopId: command.shopId, itemId: command.itemId });
    syncRelationshipStages(content, draft);
    emitRelationshipRules(content, draft);
    if (command.type === 'acknowledgeResult' && state.phase === 'activity-result') startAppointmentEvents(content, draft, 'after-activity');
    if (command.type === 'visit' && draft.gameplay?.locationId !== state.gameplay?.locationId) startAppointmentEvents(content, draft, 'location-enter');
    if(draft.roster){delete draft.roster.actorId;delete draft.roster.activityActorId;}
    if (draft.roster) for (const id of content.project.roster!.localAttributeIds) draft.roster.attributes[content.project.protagonistId]![id] = draft.attributes[id]!;
    if (draft.phase !== state.phase || command.type === 'visit' || command.type === 'choose' || command.type === 'advanceNarrative') delete draft.uiScreenId;
    draft.stateRevision++;
    return { ok: true, state: freeze(draft), transition: freeze({ command, fromRevision: state.stateRevision,
      toRevision: draft.stateRevision, fromPhase: state.phase, toPhase: draft.phase, time: calendarTime(content, draft), activity,
      narrative, eventDecisions, endingSelected: draft.endingId !== state.endingId ? draft.endingId : null, soundCues }) };
  } catch (error) {
    if (error instanceof CoreFault) return { ok: false, state, error: error.diagnostic() };
    throw error;
  }
}

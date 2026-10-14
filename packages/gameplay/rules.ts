import type { Change, ContentSnapshot, DeepReadonly, RunData } from '../core/model.js';
import { applyEffects, evaluateCondition, evaluateNumber } from '../core/evaluation.js';
import { sampleBelow } from '../core/random.js';
import { enterNarrative } from '../core/narrative.js';
import { eventCompleted } from '../core/roster.js';
import { int32, requireCore } from '../core/util.js';
import type { GameplayDefinition, GameplayState } from './model.js';

type Action = NonNullable<GameplayDefinition['actions']>[number];
type Trigger = NonNullable<GameplayDefinition['rules']>[number]['trigger'];
interface RuleContext { stack: string[]; actions: number; occurrence: string; counts: Map<string, number>; fired: Set<string>; relationships: RunData['relationships'] }
const contexts = new WeakMap<RunData, RuleContext>();
function context(draft: RunData): RuleContext {
  let value = contexts.get(draft);
  if (!value) { value = { stack: [], actions: 0, occurrence: 'command', counts: new Map(), fired: new Set(), relationships: structuredClone(draft.relationships) }; contexts.set(draft, value); }
  return value;
}
export function beginRuleCommand(draft: RunData): void { context(draft); }
export function emitRules(content: ContentSnapshot, draft: RunData, trigger: Trigger, changes: Change[] = [], occurrence?: string): void {
  const ctx = context(draft), previous = ctx.occurrence;
  ctx.occurrence = occurrence ?? previous;
  try {
    for (const rule of content.project.gameplay?.rules ?? []) {
      if (rule.trigger.type !== trigger.type) continue;
      if (Object.entries(rule.trigger).some(([key, value]) => key !== 'type' && trigger[key as keyof Trigger] !== value)) continue;
      const key = `${ctx.occurrence}:${rule.id}`;
      if (ctx.fired.has(key)) continue;
      ctx.fired.add(key);
      if (rule.condition && !evaluateCondition(content, draft, rule.condition, `/project/gameplay/rules/${rule.id}/condition`)) continue;
      executeAction(content, draft, rule.actionId, rule.quantity ?? 1, changes, true);
    }
  } finally { ctx.occurrence = previous; }
}
export function emitRelationshipRules(content: ContentSnapshot, draft: RunData, changes: Change[] = []): void {
  const ctx = context(draft), before = ctx.relationships;
  ctx.relationships = structuredClone(draft.relationships);
  for (const character of content.characters) for (const relationship of character.relationships) {
    if (before[character.id]?.[relationship.id] !== draft.relationships[character.id]?.[relationship.id]) emitRules(content, draft, { type: 'relationshipChanged', characterId: character.id, relationshipId: relationship.id }, changes);
  }
}
export function runBoundaryRules(content: ContentSnapshot, draft: RunData, edge: 'periodStart' | 'periodEnd', changes: Change[] = []): void {
  if (!content.project.gameplay?.rules?.length || !draft.gameplay || draft.gameplay.rulePeriods?.[edge] === draft.periodIndex) return;
  (draft.gameplay.rulePeriods ??= {})[edge] = draft.periodIndex;
  if (edge === 'periodStart' && draft.periodIndex % content.project.periodsPerMonth === 0) emitRules(content, draft, { type: 'monthStart' }, changes);
  emitRules(content, draft, { type: edge }, changes);
  if (edge === 'periodEnd' && draft.periodIndex % content.project.periodsPerMonth === content.project.periodsPerMonth - 1) emitRules(content, draft, { type: 'monthEnd' }, changes);
}
export function runDayRules(content: ContentSnapshot, draft: RunData, changes: Change[]): void {
  const days = content.project.gameplay?.ruleCalendar?.daysPerMonth;
  if (!days || !content.project.gameplay?.rules?.some(x => x.trigger.type === 'dayStart' || x.trigger.type === 'dayEnd')) return;
  const slot = draft.periodIndex % content.project.periodsPerMonth, month = Math.floor(draft.periodIndex / content.project.periodsPerMonth);
  for (let day = (content.project.periodsPerMonth===3?Math.floor(slot*days/3):slot * Math.floor(days / content.project.periodsPerMonth)); day < (content.project.periodsPerMonth===3?Math.floor((slot+1)*days/3):(slot === content.project.periodsPerMonth - 1 ? days : (slot + 1) * Math.floor(days / content.project.periodsPerMonth))); day++) {
    const absolute = month * days + day;
    emitRules(content, draft, { type: 'dayStart' }, changes, `dayStart:${absolute}`);
    emitRules(content, draft, { type: 'dayEnd' }, changes, `dayEnd:${absolute}`);
  }
}
export const atLocation = (ids: readonly string[] | undefined, locationId: string): boolean => !ids?.length || ids.includes(locationId);
export function policyUsage(definition: DeepReadonly<GameplayDefinition>, state: GameplayState, budgetId: string): number {
  let used = 0;
  for (const group of definition.policyGroups ?? []) {
    const option = group.options.find(x => x.id === state.policySelections?.[group.id]);
    for (const entry of option?.budgetUses ?? []) if (entry.budgetId === budgetId) used += entry.amount;
  }
  return used;
}
export function selectPolicy(content: ContentSnapshot, draft: RunData, groupId: string, optionId: string | null): void {
  const definition = content.project.gameplay!, state = draft.gameplay!;
  const group = definition.policyGroups?.find(x => x.id === groupId);
  requireCore(group, 'UNKNOWN_POLICY_GROUP', '정책 그룹을 찾을 수 없습니다.', '/command/groupId');
  requireCore(atLocation(group.locationIds, state.locationId), 'WRONG_LOCATION', '정책을 설정할 수 있는 장소로 이동해 주세요.', '/command/groupId');
  requireCore((state.policySelections?.[groupId] ?? null) !== optionId, 'POLICY_UNCHANGED', '이미 선택한 정책입니다.', '/command/optionId');
  requireCore(optionId === null || group.options.some(x => x.id === optionId), 'UNKNOWN_POLICY_OPTION', '정책 선택지를 찾을 수 없습니다.', '/command/optionId');
  state.policySelections ??= {};
  state.policySelections[groupId] = optionId;
  for (const budget of definition.policyBudgets ?? []) requireCore(policyUsage(definition, state, budget.id) <= budget.capacity, 'POLICY_BUDGET_EXCEEDED', '정책 자원 한도를 초과합니다.', '/command/optionId');
}
export function executeAction(content: ContentSnapshot, draft: RunData, actionId: string, quantity: number, changes: Change[], scheduled = false): void {
  const action = content.project.gameplay?.actions?.find(x => x.id === actionId);
  requireCore(action, 'UNKNOWN_ACTION', '행동을 찾을 수 없습니다.', '/command/actionId');
  requireCore(scheduled || atLocation(action.locationIds, draft.gameplay!.locationId), 'WRONG_LOCATION', '행동할 수 있는 장소로 이동해 주세요.', '/command/actionId');
  requireCore(quantity <= (action.maxQuantity ?? 1), 'QUANTITY_LIMIT', '설정된 수량 한도를 초과합니다.', '/command/quantity');
  requireCore(!scheduled || !action.eventId, 'ACTION_TRIGGER_EVENT', '사건 행동은 일정 트리거에서 실행할 수 없습니다.', '/command/actionId');
  requireCore(!action.expeditionId || (!scheduled && !action.eventId && quantity === 1), 'ACTION_EXPEDITION_CONTEXT', '탐험은 사건 연결 없이 직접 한 번 실행해야 합니다.', '/command/actionId');
  const ctx = context(draft), flowPath = `/project/gameplay/actions/${actionId}/flow`;
  requireCore(!ctx.stack.includes(actionId), 'FLOW_CYCLE', '행동 흐름이 순환합니다.', flowPath);
  ctx.actions++; const count = (ctx.counts.get(ctx.occurrence) ?? 0) + 1; ctx.counts.set(ctx.occurrence, count);
  requireCore(ctx.stack.length < 32 && ctx.actions <= 4096 && count <= 128, 'FLOW_LIMIT', '행동 흐름 실행 한도를 초과합니다.', flowPath);
  ctx.stack.push(actionId);
  try {
    const changeStart = changes.length;
    applyAction(content, draft, action, quantity, changes);
    for (const step of action.flow ?? []) {
      let matched = !step.condition || evaluateCondition(content, draft, step.condition, `${flowPath}/${step.id}/condition`);
      if (matched && step.chance) {
        const chance = Math.max(0, Math.min(10000, evaluateNumber(content, draft, step.chance, `${flowPath}/${step.id}/chance`)));
        const sampled = sampleBelow(draft.rng, 10000);
        draft.rng = sampled.rng;
        matched = sampled.value < chance;
      }
      const target = matched ? step.thenActionId : step.elseActionId;
      if (target) executeAction(content, draft, target, step.quantity ?? 1, changes, true);
    }
    if (action.rejectNoEffect && action.flow?.length) requireCore(changes.slice(changeStart).some(x => !x.source.includes('/costs/') && x.before !== x.after), 'ACTION_NO_EFFECT', '현재 상태에서는 적용할 효과가 없습니다.', '/command/actionId');
  } finally { ctx.stack.pop(); }
  emitRelationshipRules(content, draft, changes);
  if (action.eventId && !eventCompleted(content,draft,action.eventId)) {
    const event = content.events.find(x => x.id === action.eventId)!;
    if (draft.periodIndex >= event.window.fromPeriod && draft.periodIndex <= event.window.throughPeriod && evaluateCondition(content, draft, event.requirements, `/events/${event.id}/requirements`)) {
      const returnPhase = draft.phase, returnPresentation = structuredClone(draft.presentation);
      enterNarrative(content, draft, 'event', event.id);
      draft.activeNarrative!.returnPhase = returnPhase as 'planning' | 'ready'; draft.activeNarrative!.returnPresentation = returnPresentation;
    }
  }
}
function applyAction(content: ContentSnapshot, draft: RunData, action: DeepReadonly<Action>, quantity: number, changes: Change[]): void {
  const path = `/project/gameplay/actions/${action.id}`;
  requireCore(!action.requirements || evaluateCondition(content, draft, action.requirements, `${path}/requirements`), 'ACTION_LOCKED', '행동 조건을 만족하지 않습니다.', '/command/actionId');
  if (action.rejectNoEffect && !action.flow?.length) {
    const probe = structuredClone(draft);
    for (let i = 0; i < quantity; i++) applyEffects(content, probe, action.effects, `${path}/effects`, []);
    requireCore(JSON.stringify([probe.attributes, probe.flags, probe.relationships, probe.gameplay?.inventory]) !== JSON.stringify([draft.attributes, draft.flags, draft.relationships, draft.gameplay?.inventory]), 'ACTION_NO_EFFECT', '현재 상태에서는 적용할 효과가 없습니다.', '/command/actionId');
  }
  for (const [i, cost] of action.costs.entries()) {
    const amount = int32(cost.amount * quantity, '/command/quantity'), definition = content.attributes.find(x => x.id === cost.attributeId)!;
    const before = draft.attributes[cost.attributeId]!, after = int32(before - amount, `/attributes/${cost.attributeId}`);
    requireCore(after >= definition.min, 'INSUFFICIENT_FUNDS', '비용이 부족합니다.', `/attributes/${cost.attributeId}`);
    draft.attributes[cost.attributeId] = after;
    changes.push({ kind: 'numeric', source: `${path}/costs/${i}`, target: { kind: 'attribute', attributeId: cost.attributeId }, before, operand: -amount, requested: after, after, delta: -amount, clamped: false });
  }
  for (let i = 0; i < quantity; i++) applyEffects(content, draft, action.effects, `${path}/effects`, changes);
}
export function applyPolicies(content: ContentSnapshot, draft: RunData, changes: Change[], trigger: 'beforeActivity' | 'afterActivity'): void {
  const state = draft.gameplay;
  if (!state) return;
  for (const group of content.project.gameplay?.policyGroups ?? []) {
    const optionId = state.policySelections?.[group.id];
    if (!optionId || group.trigger.type !== trigger || state.policyAppliedPeriods?.[group.id] === draft.periodIndex || (state.policyNextPeriods?.[group.id] ?? 0) > draft.periodIndex) continue;
    const option = group.options.find(x => x.id === optionId)!;
    executeAction(content, draft, option.actionId, 1, changes, true);
    (state.policyAppliedPeriods ??= {})[group.id] = draft.periodIndex;
    (state.policyNextPeriods ??= {})[group.id] = draft.periodIndex + (group.trigger.intervalPeriods ?? 1);
  }
}

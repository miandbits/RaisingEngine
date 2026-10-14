import type { Condition, NumericExpression, Effects } from '../schema/generated/activity.js';
import type { CalendarTime, ContentSnapshot, DeepReadonly, NumericTarget, RunState, RunData, Change } from './model.js';
import { int32, requireCore } from './util.js';

export function calendarTime(content: ContentSnapshot, state: Pick<RunState, 'periodIndex'>): CalendarTime {
  const periodIndex = state.periodIndex;
  requireCore(Number.isInteger(periodIndex) && periodIndex >= 0 && periodIndex < content.project.durationMonths * content.project.periodsPerMonth,
    'INVALID_TIME', '기간 인덱스가 프로젝트 범위를 벗어났습니다.', '/periodIndex');
  const absoluteMonth = content.project.startMonth - 1 + Math.floor(periodIndex / content.project.periodsPerMonth);
  return { periodIndex, year: content.project.startYear + Math.floor(absoluteMonth / 12),
    month: absoluteMonth % 12 + 1, periodInMonth: periodIndex % content.project.periodsPerMonth };
}

export function numericSlot(content: ContentSnapshot, state: RunState, target: DeepReadonly<NumericTarget>, path: string) {
  if (target.kind === 'inventory') {
    const item = content.project.gameplay?.items.find(x => x.id === target.itemId), values = state.gameplay?.inventory;
    requireCore(item && values && Object.hasOwn(values, target.itemId), 'UNKNOWN_TARGET', '수치 대상이 없습니다.', path);
    return { definition: { min: 0, max: item.maxStack }, value: values[target.itemId]!, key: target.itemId };
  }
  const definition = target.kind === 'attribute'
    ? content.attributes.find(a => a.id === target.attributeId)
    : content.characters.find(c => c.id === target.characterId)?.relationships.find(r => r.id === target.relationshipId);
  const values = target.kind === 'attribute' ? state.attributes : state.relationships[target.characterId];
  const key = target.kind === 'attribute' ? target.attributeId : target.relationshipId;
  requireCore(definition && values && Object.hasOwn(values, key), 'UNKNOWN_TARGET', '수치 대상이 없습니다.', path);
  return { definition, value: values[key]!, key };
}

export function evaluateNumber(content: ContentSnapshot, state: RunState, expression: DeepReadonly<NumericExpression>, path = '/expression', depth = 0): number {
  requireCore(depth < 32, 'EXPRESSION_DEPTH', '수식 중첩은 32단계를 넘을 수 없습니다.', path);
  const evaluate = (e: DeepReadonly<NumericExpression>, field: string) => evaluateNumber(content, state, e, `${path}/${field}`, depth + 1);
  let value: number;
  switch (expression.op) {
    case 'literal': value = expression.value; break;
    case 'read': value = numericSlot(content, state, expression.target, `${path}/target`).value; break;
    case 'time': value = calendarTime(content, state)[expression.field]; break;
    case 'add': value = evaluate(expression.left, 'left') + evaluate(expression.right, 'right'); break;
    case 'subtract': value = evaluate(expression.left, 'left') - evaluate(expression.right, 'right'); break;
    case 'multiply': value = evaluate(expression.left, 'left') * evaluate(expression.right, 'right'); break;
    case 'clamp': value = Math.min(expression.max, Math.max(expression.min, evaluate(expression.value, 'value'))); break;
  }
  return int32(value, path);
}

export function evaluateCondition(content: ContentSnapshot, state: RunState, condition: DeepReadonly<Condition>, path = '/condition', depth = 0): boolean {
  requireCore(depth < 32, 'CONDITION_DEPTH', '조건 중첩은 32단계를 넘을 수 없습니다.', path);
  const evaluate = (c: DeepReadonly<Condition>, field: string) => evaluateCondition(content, state, c, `${path}/${field}`, depth + 1);
  switch (condition.op) {
    case 'always': return true;
    case 'all': return condition.conditions.every((c, i) => evaluate(c, `conditions/${i}`));
    case 'any': return condition.conditions.some((c, i) => evaluate(c, `conditions/${i}`));
    case 'not': return !evaluate(condition.condition, 'condition');
    case 'flag':
      requireCore(Object.hasOwn(state.flags, condition.flagId), 'UNKNOWN_FLAG', '플래그가 없습니다.', `${path}/flagId`);
      return state.flags[condition.flagId] === condition.equals;
    case 'eventCompleted': return state.completedEvents.includes(condition.eventId);
    case 'compare': {
      const left = evaluateNumber(content, state, condition.left, `${path}/left`, depth + 1);
      const right = evaluateNumber(content, state, condition.right, `${path}/right`, depth + 1);
      switch (condition.comparator) {
        case 'eq': return left === right;
        case 'ne': return left !== right;
        case 'gt': return left > right;
        case 'gte': return left >= right;
        case 'lt': return left < right;
        case 'lte': return left <= right;
      }
    }
  }
}

/** Mutates only the transaction draft, never a published RunState. */
export function applyEffects(content: ContentSnapshot, draft: RunData, effects: DeepReadonly<Effects>, source: string, changes: Change[]): void {
  effects.forEach((effect, i) => {
    const path = `${source}/${i}`;
    if (effect.op === 'setFlag') {
      requireCore(Object.hasOwn(draft.flags, effect.flagId), 'UNKNOWN_FLAG', '플래그가 없습니다.', `${path}/flagId`);
      changes.push({ kind: 'flag', source: path, flagId: effect.flagId, before: draft.flags[effect.flagId]!, after: effect.value });
      draft.flags[effect.flagId] = effect.value;
      return;
    }
    const { definition, value: before, key } = numericSlot(content, draft, effect.target, `${path}/target`);
    const operand = evaluateNumber(content, draft, effect.value, `${path}/value`, 1);
    const requested = int32(effect.op === 'add' ? before + operand : operand, path);
    if (effect.target.kind === 'inventory') {
      requireCore(requested >= 0 && requested <= definition.max, 'INVENTORY_RANGE', '물품 수량이 보유 가능 범위를 벗어났습니다.', path);
      const itemId = effect.target.itemId;
      const reserved = Object.values(draft.systems?.equipment ?? {}).filter(id => content.project.systems?.equipment.find(x => x.id === id)?.itemId === itemId).length;
      requireCore(requested >= reserved, 'EQUIPPED_ITEM', '장착 중인 물품은 소비할 수 없습니다.', path);
    }
    const after = Math.min(definition.max, Math.max(definition.min, requested));
    const values = effect.target.kind === 'attribute' ? draft.attributes : effect.target.kind === 'inventory' ? draft.gameplay!.inventory : draft.relationships[effect.target.characterId]!;
    values[key] = after;
    changes.push({ kind: 'numeric', source: path, target: { ...effect.target }, before, operand, requested,
      after, delta: after - before, clamped: requested < definition.min ? 'min' : requested > definition.max ? 'max' : false });
  });
}

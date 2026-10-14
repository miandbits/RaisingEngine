import { isObject, type JsonObject, type ProjectDocument } from './model.js';
import { objectItems, type Index, type SemanticContext } from './context.js';

export function checkGameplay(ctx: SemanticContext, doc: ProjectDocument, gameplay: JsonObject): void {
  const p = '/gameplay';
  const locations: Index = new Map(), items: Index = new Map(), shops: Index = new Map(), interactions: Index = new Map();
  ctx.addArray(locations, doc, `${p}/locations`, gameplay.locations, '장소');
  ctx.addArray(items, doc, `${p}/items`, gameplay.items, '물품');
  ctx.addArray(shops, doc, `${p}/shops`, gameplay.shops, '상점');
  ctx.addArray(interactions, doc, `${p}/interactions`, gameplay.interactions, '행동');
  ctx.resolve(locations, gameplay.startLocationId as string, doc, `${p}/startLocationId`, '시작 장소');
  const visual = (doc.data as JsonObject).visual;
  const screens = isObject(visual) ? objectItems(visual.screens).map(x => x.id) : [];
  objectItems(gameplay.locations).forEach((location, i) => {
    const at = `${p}/locations/${i}`;
    (location.connections as string[]).forEach((id, j) => ctx.resolve(locations, id, doc, `${at}/connections/${j}`, '연결 장소'));
    if (location.screenId && (!screens.includes(location.screenId) || ['title', 'story', 'ending'].includes(location.screenId as string)))
      ctx.report(doc, `${at}/screenId`, 'SCREEN_REFERENCE', '장소 화면은 정의된 일반 화면이어야 합니다. 시작·대본·엔딩 화면은 사용할 수 없습니다.', [location.screenId as string]);
  });
  objectItems(gameplay.items).forEach((item, i) => {
    if (Number(item.initialQuantity ?? 0) > Number(item.maxStack)) ctx.report(doc, `${p}/items/${i}/initialQuantity`, 'INVENTORY_RANGE', '초기 보유량은 최대 보유량 이하여야 합니다.', [item.id as string]);
  });
  function currency(id: string, pointer: string): void {
    const entry = ctx.resolve(ctx.attributes, id, doc, pointer, '비용 자원');
    if (entry && entry.data.kind === 'stat') ctx.report(doc, pointer, 'COST_KIND', '비용은 resource 또는 condition 능력치를 사용해야 합니다.', [id]);
  }
  objectItems(gameplay.shops).forEach((shop, i) => {
    const at = `${p}/shops/${i}`;
    ctx.resolve(locations, shop.locationId as string, doc, `${at}/locationId`, '상점 장소');
    currency(shop.currencyAttributeId as string, `${at}/currencyAttributeId`);
    const seen = new Set<string>();
    for (const collection of ['offers', 'sellOffers']) { seen.clear(); objectItems(shop[collection]).forEach((offer, j) => {
      const id = offer.itemId as string, pointer = `${at}/${collection}/${j}/itemId`;
      ctx.resolve(items, id, doc, pointer, '판매 물품');
      if (seen.has(id)) ctx.report(doc, pointer, 'OFFER_DUPLICATE', '상점에 같은 물품이 중복 등록되었습니다.', [id]);
      seen.add(id);
    }); }
  });
  for (const collection of ['diets', 'services']) {
    const index: Index = new Map(); ctx.addArray(index, doc, `${p}/${collection}`, gameplay[collection], collection === 'diets' ? '식단' : '서비스');
    objectItems(gameplay[collection]).forEach((facility, i) => {
      const at = `${p}/${collection}/${i}`;
      ctx.resolve(locations, facility.locationId as string, doc, `${at}/locationId`, '시설 장소');
      const seen = new Set<string>();
      objectItems(facility.costs).forEach((cost, j) => {
        const id = cost.attributeId as string, pointer = `${at}/costs/${j}/attributeId`;
        currency(id, pointer);
        if (seen.has(id)) ctx.report(doc, pointer, 'COST_DUPLICATE', '비용 자원이 중복되었습니다.', [id]);
        seen.add(id);
        if (Number(cost.amount) * Number(facility.maxQuantity ?? 1) > 2147483647) ctx.report(doc, `${at}/costs/${j}/amount`, 'COST_OVERFLOW', '최대 수량의 비용이 정수 범위를 초과합니다.');
      });
    });
  }
  const actions: Index = new Map(), budgets: Index = new Map(), groups: Index = new Map();
  ctx.addArray(actions, doc, `${p}/actions`, gameplay.actions, '범용 행동');
  ctx.addArray(budgets, doc, `${p}/policyBudgets`, gameplay.policyBudgets, '정책 자원');
  ctx.addArray(groups, doc, `${p}/policyGroups`, gameplay.policyGroups, '정책 그룹');
  for (const [i, action] of objectItems(gameplay.actions).entries()) {
    const at = `${p}/actions/${i}`, seen = new Set<string>();
    for (const [j, id] of ((action.locationIds ?? []) as string[]).entries()) ctx.resolve(locations, id, doc, `${at}/locationIds/${j}`, '행동 장소');
    for (const [j, cost] of objectItems(action.costs).entries()) {
      const id = String(cost.attributeId), pointer = `${at}/costs/${j}`;
      currency(id, pointer + '/attributeId');
      if (seen.has(id)) ctx.report(doc, pointer + '/attributeId', 'COST_DUPLICATE', '비용 자원이 중복되었습니다.', [id]);
      seen.add(id);
      if (Number(cost.amount) * Number(action.maxQuantity ?? 1) > 2147483647) ctx.report(doc, pointer + '/amount', 'COST_OVERFLOW', '최대 수량의 비용이 정수 범위를 초과합니다.');
    }
    if (action.eventId) { const event = ctx.resolve(ctx.events, String(action.eventId), doc, at + '/eventId', '사건'); if (event && event.data.trigger !== 'interaction') ctx.report(doc, at + '/eventId', 'ACTION_EVENT_TRIGGER', '행동 사건은 interaction 트리거여야 합니다.'); }
    const steps: Index = new Map(); ctx.addArray(steps, doc, at + '/flow', action.flow, '행동 흐름 단계');
    for (const [j, step] of objectItems(action.flow).entries()) for (const field of ['thenActionId', 'elseActionId']) if (step[field]) {
      const target = ctx.resolve(actions, String(step[field]), doc, `${at}/flow/${j}/${field}`, '연결 행동');
      if (target?.data.eventId) ctx.report(doc, `${at}/flow/${j}/${field}`, 'FLOW_EVENT_ACTION', '흐름 내부에서는 사건을 직접 여는 행동을 실행할 수 없습니다. 최상위 행동에 사건을 연결하세요.');
      if (target && Number(step.quantity ?? 1) > Number(target.data.maxQuantity ?? 1)) ctx.report(doc, `${at}/flow/${j}/quantity`, 'FLOW_QUANTITY', '연결 행동의 최대 수량을 초과합니다.');
    }
  }
  const actionList = objectItems(gameplay.actions), actionMap = new Map(actionList.map(x => [String(x.id), x]));
  const edges = (action: JsonObject) => objectItems(action.flow).flatMap(step => [step.thenActionId, step.elseActionId].filter(x => typeof x === 'string') as string[]);
  const hasChance = (id: string, visited = new Set<string>()): boolean => {
    if (visited.has(id)) return false;
    visited.add(id);
    const action = actionMap.get(id);
    return !!action && (objectItems(action.flow).some(step => step.chance !== undefined) || edges(action).some(child => hasChance(child, visited)));
  };
  actionList.forEach((action, i) => {
    if (action.rejectNoEffect && hasChance(String(action.id))) ctx.report(doc, `${p}/actions/${i}/rejectNoEffect`, 'CHANCE_NO_EFFECT_RETRY', '확률 분기의 효과가 없어 행동이 거부되면 난수도 복원됩니다. 같은 상태의 재시도는 같은 결과가 되며, 자동 규칙에서는 진행이 막힐 수 있습니다. 실패 분기에도 유효한 결과를 작성하거나 효과 없음 거부를 해제하세요.', [String(action.id)], 'warning');
  });
  const marks = new Map<string, number>(), depths = new Map<string, number>();
  const depth = (id: string): number => {
    if (marks.get(id) === 1) { const i = actionList.findIndex(x => x.id === id); ctx.report(doc, `${p}/actions/${i}/flow`, 'FLOW_CYCLE', '행동 흐름이 순환합니다.', [id]); return 0; }
    if (marks.get(id) === 2) return depths.get(id) ?? 0;
    const action = actionMap.get(id); if (!action) return 0;
    marks.set(id, 1); let result = 1;
    for (const next of edges(action)) result = Math.max(result, 1 + depth(next));
    marks.set(id, 2); depths.set(id, result); return result;
  };
  actionList.forEach((action, i) => { if (depth(String(action.id)) > 32) ctx.report(doc, `${p}/actions/${i}/flow`, 'FLOW_DEPTH', '행동 흐름은 32단계를 초과할 수 없습니다.'); });
  const hasEvent = (id: string): boolean => {
    const pending = [id], seen = new Set<string>();
    while (pending.length) { const current = pending.pop()!; if (seen.has(current)) continue; seen.add(current); const action = actionMap.get(current); if (!action) continue; if (action.eventId) return true; pending.push(...edges(action)); }
    return false;
  };
  for (const [i, group] of objectItems(gameplay.policyGroups).entries()) {
    const at = `${p}/policyGroups/${i}`, options: Index = new Map();
    ctx.addArray(options, doc, at + '/options', group.options, '정책 선택지');
    for (const [j, id] of ((group.locationIds ?? []) as string[]).entries()) ctx.resolve(locations, id, doc, `${at}/locationIds/${j}`, '정책 장소');
    for (const [j, option] of objectItems(group.options).entries()) {
      const op = `${at}/options/${j}`, action = ctx.resolve(actions, String(option.actionId), doc, op + '/actionId', '정책 행동'), seen = new Set<string>();
      if (action && hasEvent(String(option.actionId))) ctx.report(doc, op + '/actionId', 'POLICY_EVENT_ACTION', '일정 트리거 정책에는 사건 연결이 없는 행동을 사용하세요.');
      for (const [k, use] of objectItems(option.budgetUses).entries()) {
        const id = String(use.budgetId), bp = `${op}/budgetUses/${k}`, budget = ctx.resolve(budgets, id, doc, bp + '/budgetId', '정책 자원');
        if (seen.has(id)) ctx.report(doc, bp + '/budgetId', 'POLICY_BUDGET_DUPLICATE', '선택지의 정책 자원이 중복되었습니다.');
        seen.add(id);
        if (budget && Number(use.amount) > Number(budget.data.capacity)) ctx.report(doc, bp + '/amount', 'POLICY_BUDGET_CAPACITY', '선택지 사용량이 정책 자원 전체 한도를 초과합니다.');
      }
    }
  }
  const rules: Index = new Map(); ctx.addArray(rules, doc, `${p}/rules`, gameplay.rules, '트리거 규칙');
  if (isObject(gameplay.ruleCalendar) && Number(gameplay.ruleCalendar.daysPerMonth) < Number((doc.data as JsonObject).periodsPerMonth ?? 3)) ctx.report(doc, `${p}/ruleCalendar/daysPerMonth`, 'RULE_CALENDAR_RANGE', '한 달의 논리 일수는 월별 기간 수 이상이어야 합니다.');
  for (const [i, rule] of objectItems(gameplay.rules).entries()) {
    const at = `${p}/rules/${i}`, trigger = rule.trigger as JsonObject, action = ctx.resolve(actions, String(rule.actionId), doc, at + '/actionId', '규칙 행동');
    if (action && hasEvent(String(rule.actionId))) ctx.report(doc, at + '/actionId', 'RULE_EVENT_ACTION', '자동 트리거에는 사건 연결이 없는 행동을 사용하세요.');
    if (action && Number(rule.quantity ?? 1) > Number(action.data.maxQuantity ?? 1)) ctx.report(doc, at + '/quantity', 'RULE_QUANTITY', '연결 행동의 최대 수량을 초과합니다.');
    const allowed: Record<string, string[]> = { locationEnter: ['locationId'], purchaseComplete: ['shopId', 'itemId'], relationshipChanged: ['characterId', 'relationshipId'] };
    for (const key of Object.keys(trigger)) if (key !== 'type' && !(allowed[String(trigger.type)] ?? []).includes(key)) ctx.report(doc, `${at}/trigger/${key}`, 'RULE_TRIGGER_FILTER', '이 트리거에서 사용할 수 없는 대상 필터입니다.');
    if (['dayStart', 'dayEnd'].includes(String(trigger.type)) && !isObject(gameplay.ruleCalendar)) ctx.report(doc, at + '/trigger/type', 'RULE_CALENDAR_REQUIRED', '일별 규칙에는 명시적인 월별 논리 일수 설정이 필요합니다.');
    if (trigger.locationId) ctx.resolve(locations, String(trigger.locationId), doc, at + '/trigger/locationId', '장소');
    if (trigger.shopId) ctx.resolve(shops, String(trigger.shopId), doc, at + '/trigger/shopId', '상점');
    if (trigger.itemId) ctx.resolve(items, String(trigger.itemId), doc, at + '/trigger/itemId', '물품');
    if (trigger.relationshipId && !trigger.characterId) ctx.report(doc, at + '/trigger/characterId', 'RULE_RELATIONSHIP_CHARACTER', '관계 수치 필터에는 인물을 선택해야 합니다.');
  }
  objectItems(gameplay.interactions).forEach((interaction, i) => {
    const at = `${p}/interactions/${i}`;
    ctx.resolve(locations, interaction.locationId as string, doc, `${at}/locationId`, '행동 장소');
    if (interaction.itemId) {
      const item = ctx.resolve(items, interaction.itemId as string, doc, `${at}/itemId`, '소비 물품');
      if (item && Number(interaction.quantity ?? 1) > Number(item.data.maxStack)) ctx.report(doc, `${at}/quantity`, 'INVENTORY_RANGE', '한 번에 필요한 물품 수량이 최대 보유량을 초과합니다.');
    }
    const seen = new Set<string>();
    objectItems(interaction.costs).forEach((cost, j) => {
      const id = cost.attributeId as string, pointer = `${at}/costs/${j}/attributeId`;
      currency(id, pointer);
      if (seen.has(id)) ctx.report(doc, pointer, 'COST_DUPLICATE', '행동 비용 자원이 중복되었습니다.', [id]);
      seen.add(id);
    });
  });
  // Structural connectivity is useful even before conditions are simulated; it is not a reachability proof.
  const seen = new Set<string>(), pending = [gameplay.startLocationId as string];
  while (pending.length) {
    const id = pending.pop()!;
    if (seen.has(id)) continue;
    seen.add(id);
    for (const next of (locations.get(id)?.[0]?.data.connections as string[] | undefined) ?? []) pending.push(next);
  }
  objectItems(gameplay.locations).forEach((location, i) => {
    if (!seen.has(location.id as string)) ctx.report(doc, `${p}/locations/${i}/id`, 'LOCATION_UNREACHABLE', '시작 장소에서 연결 경로가 없는 장소입니다.', [location.id as string], 'warning');
  });
}

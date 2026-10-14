import type { Change, ContentSnapshot, DeepReadonly, RunData, RunState } from '../core/model.js';
import { applyEffects, evaluateCondition } from '../core/evaluation.js';
import { applyScene, enterNarrative } from '../core/narrative.js';
import { CoreFault, int32, requireCore } from '../core/util.js';
import type { Availability, GameplayCommand, GameplayDefinition, GameplayState, GameplayView } from './model.js';
import { equipmentReserved } from '../systems/service.js';
import { atLocation, executeAction, selectPolicy, policyUsage } from './rules.js';

export function createGameplay(definition: DeepReadonly<GameplayDefinition>): GameplayState {
  const location = definition.locations.find(x => x.id === definition.startLocationId)!;
  return { locationId: location.id, screenId: location.screenId ?? 'schedule',
    inventory: Object.fromEntries(definition.items.map(x => [x.id, x.initialQuantity ?? 0])),
    purchased: {}, interactionPeriods: {}, visited: [location.id] };
}

/** Runs only on the core transaction draft. Any fault discards the complete draft. */
export function applyGameplay(content: ContentSnapshot, draft: RunData, command: GameplayCommand, soundCues: string[]): void {
  const definition = content.project.gameplay, state = draft.gameplay;
  requireCore(definition && state, 'GAMEPLAY_UNAVAILABLE', '이 프로젝트에는 생활 규칙이 없습니다.', '/project/gameplay');
  requireCore(draft.mode === 'full' && (draft.phase === 'planning' || draft.phase === 'ready'),
    'WRONG_PHASE', '일정 편성 중에만 장소 이동과 생활 행동을 할 수 있습니다.', '/phase');
  const p = '/project/gameplay';
  if (command.type === 'executeAction') { executeAction(content, draft, command.actionId, command.quantity, []); return; }
  if (command.type === 'selectPolicy') { selectPolicy(content, draft, command.groupId, command.optionId); return; }
  if (command.type === 'setDiet') {
    requireCore((state.dietId ?? null) !== command.dietId, 'DIET_UNCHANGED', '이미 선택한 식단입니다.', '/command/dietId');
    if (command.dietId !== null) {
      const diet = definition.diets?.find(x => x.id === command.dietId);
      requireCore(diet, 'UNKNOWN_DIET', '식단을 찾을 수 없습니다.', '/command/dietId');
      requireCore(diet.locationId === state.locationId, 'WRONG_LOCATION', '식단을 설정할 수 있는 장소로 이동해 주세요.', '/command/dietId');
    }
    state.dietId = command.dietId;
    return;
  }
  if (command.type === 'useService') {
    const service = definition.services?.find(x => x.id === command.serviceId);
    requireCore(service, 'UNKNOWN_SERVICE', '서비스를 찾을 수 없습니다.', '/command/serviceId');
    requireCore(service.locationId === state.locationId, 'WRONG_LOCATION', '서비스가 있는 장소로 이동해 주세요.', '/command/serviceId');
    requireCore(command.quantity <= (service.maxQuantity ?? 1), 'QUANTITY_LIMIT', '설정된 수량 한도를 초과합니다.', '/command/quantity');
    const path = `${p}/services/${service.id}`;
    requireCore(!service.requirements || evaluateCondition(content, draft, service.requirements, `${path}/requirements`), 'SERVICE_LOCKED', '서비스 이용 조건을 만족하지 않습니다.', '/command/serviceId');
    const probe = structuredClone(draft);
    for (let i = 0; i < command.quantity; i++) applyEffects(content, probe, service.effects, `${path}/effects`, []);
    requireCore(JSON.stringify([probe.attributes, probe.flags, probe.relationships]) !== JSON.stringify([draft.attributes, draft.flags, draft.relationships]),
      'SERVICE_NO_EFFECT', '현재 상태에서는 적용할 효과가 없습니다.', '/command/serviceId');
    chargeFacility(content, draft, service.costs, command.quantity, path, []);
    for (let i = 0; i < command.quantity; i++) applyEffects(content, draft, service.effects, `${path}/effects`, []);
    return;
  }
  if (command.type === 'sell') {
    const shop = definition.shops.find(x => x.id === command.shopId);
    requireCore(shop, 'UNKNOWN_SHOP', '상점을 찾을 수 없습니다.', '/command/shopId');
    requireCore(shop.locationId === state.locationId, 'WRONG_LOCATION', '해당 상점이 있는 장소로 이동해 주세요.', '/command/shopId');
    const offer = shop.sellOffers?.find(x => x.itemId === command.itemId);
    requireCore(offer, 'UNKNOWN_OFFER', '상점에서 매입하지 않는 물품입니다.', '/command/itemId');
    requireCore(command.quantity <= (offer.maxQuantity ?? 99), 'QUANTITY_LIMIT', '설정된 수량 한도를 초과합니다.', '/command/quantity');
    requireCore((state.inventory[command.itemId] ?? 0) - equipmentReserved(content, draft, command.itemId) >= command.quantity, 'INSUFFICIENT_ITEMS', '판매할 물품이 부족합니다.', `/gameplay/inventory/${command.itemId}`);
    const price = int32(offer.price * command.quantity, '/command/quantity');
    const currency = content.attributes.find(x => x.id === shop.currencyAttributeId)!;
    const after = int32(draft.attributes[currency.id]! + price, `/attributes/${currency.id}`);
    requireCore(after <= currency.max, 'CURRENCY_FULL', '판매 대금이 자원 한도를 초과합니다.', `/attributes/${currency.id}`);
    draft.attributes[currency.id] = after;
    state.inventory[command.itemId]! -= command.quantity;
    return;
  }
  if (command.type === 'navigate') {
    requireCore(content.project.visual?.screens.some(x => x.id === command.screenId), 'UNKNOWN_SCREEN', '화면을 찾을 수 없습니다.', '/command/screenId');
    requireCore(command.screenId !== 'title' && command.screenId !== 'story' && command.screenId !== 'ending',
      'SCREEN_RESERVED', '시작·대본·엔딩 화면은 게임 상태에 따라 표시됩니다.', '/command/screenId');
    state.screenId = command.screenId;
    return;
  }
  if (command.type === 'visit') {
    const location = definition.locations.find(x => x.id === command.locationId);
    requireCore(location, 'UNKNOWN_LOCATION', '장소를 찾을 수 없습니다.', '/command/locationId');
    const current = definition.locations.find(x => x.id === state.locationId)!;
    requireCore(current.connections.includes(location.id) || current.id === location.id, 'LOCATION_DISCONNECTED', '현재 장소에서 이동할 수 없습니다.', '/command/locationId');
    requireCore(!location.requirements || evaluateCondition(content, draft, location.requirements, `${p}/locations/${location.id}/requirements`),
      'LOCATION_LOCKED', '장소 입장 조건을 만족하지 않습니다.', '/command/locationId');
    state.locationId = location.id;
    state.screenId = location.screenId ?? 'schedule';
    if (!state.visited.includes(location.id)) state.visited.push(location.id);
    if (location.presentation) applyScene(draft, location.presentation, soundCues);
    return;
  }
  if (command.type === 'buy') {
    const shop = definition.shops.find(x => x.id === command.shopId);
    requireCore(shop, 'UNKNOWN_SHOP', '상점을 찾을 수 없습니다.', '/command/shopId');
    requireCore(shop.locationId === state.locationId, 'WRONG_LOCATION', '해당 상점이 있는 장소로 이동해 주세요.', '/command/shopId');
    const offer = shop.offers.find(x => x.itemId === command.itemId);
    requireCore(offer, 'UNKNOWN_OFFER', '상점에서 판매하지 않는 물품입니다.', '/command/itemId');
    requireCore(offer.maxQuantity === undefined || command.quantity <= offer.maxQuantity, 'QUANTITY_LIMIT', '설정된 수량 한도를 초과합니다.', '/command/quantity');
    const item = definition.items.find(x => x.id === command.itemId)!;
    const purchases = Object.hasOwn(state.purchased, shop.id) ? state.purchased[shop.id]! : undefined;
    const purchased = purchases && Object.hasOwn(purchases, item.id) ? purchases[item.id]! : 0;
    requireCore(offer.stock === null || purchased + command.quantity <= offer.stock, 'INSUFFICIENT_STOCK', '상점 재고가 부족합니다.', '/command/quantity');
    requireCore((state.inventory[item.id] ?? 0) + command.quantity <= item.maxStack, 'INVENTORY_FULL', '물품 보유 한도를 초과합니다.', '/command/quantity');
    const price = int32(offer.price * command.quantity, '/command/quantity');
    const currency = content.attributes.find(x => x.id === shop.currencyAttributeId)!;
    requireCore(draft.attributes[currency.id]! - price >= currency.min, 'INSUFFICIENT_FUNDS', '구매 비용이 부족합니다.', `/attributes/${currency.id}`);
    draft.attributes[currency.id] = int32(draft.attributes[currency.id]! - price, `/attributes/${currency.id}`);
    state.inventory[item.id] = int32((state.inventory[item.id] ?? 0) + command.quantity, `/gameplay/inventory/${item.id}`);
    if (!Object.hasOwn(state.purchased, shop.id)) state.purchased[shop.id] = {};
    state.purchased[shop.id]![item.id] = int32(purchased + command.quantity, `/gameplay/purchased/${shop.id}/${item.id}`);
    return;
  }
  const interaction = definition.interactions.find(x => x.id === command.interactionId);
  requireCore(interaction, 'UNKNOWN_INTERACTION', '행동을 찾을 수 없습니다.', '/command/interactionId');
  const path = `${p}/interactions/${interaction.id}`;
  requireCore(interaction.locationId === state.locationId, 'WRONG_LOCATION', '행동할 수 있는 장소로 이동해 주세요.', '/command/interactionId');
  requireCore(!interaction.oncePerPeriod || state.interactionPeriods[interaction.id] !== draft.periodIndex,
    'INTERACTION_COOLDOWN', '이번 기간에 이미 한 행동입니다.', '/command/interactionId');
  requireCore(!interaction.requirements || evaluateCondition(content, draft, interaction.requirements, `${path}/requirements`),
    'INTERACTION_LOCKED', '행동 조건을 만족하지 않습니다.', '/command/interactionId');
  if (interaction.itemId) requireCore((state.inventory[interaction.itemId] ?? 0) - equipmentReserved(content, draft, interaction.itemId) >= (interaction.quantity ?? 1),
    'INSUFFICIENT_ITEMS', '필요한 물품이 부족합니다.', `/gameplay/inventory/${interaction.itemId}`);
  for (const cost of interaction.costs ?? []) {
    const attribute = content.attributes.find(x => x.id === cost.attributeId)!;
    requireCore(draft.attributes[attribute.id]! - cost.amount >= attribute.min, 'INSUFFICIENT_FUNDS', '행동 비용이 부족합니다.', `/attributes/${attribute.id}`);
  }
  for (const cost of interaction.costs ?? []) draft.attributes[cost.attributeId] = int32(draft.attributes[cost.attributeId]! - cost.amount, `/attributes/${cost.attributeId}`);
  if (interaction.itemId) state.inventory[interaction.itemId]! -= interaction.quantity ?? 1;
  applyEffects(content, draft, interaction.effects, `${path}/effects`, []);
  state.interactionPeriods[interaction.id] = draft.periodIndex;
  if (interaction.eventId && !draft.completedEvents.includes(interaction.eventId)) {
    const event = content.events.find(x => x.id === interaction.eventId)!;
    if (draft.periodIndex >= event.window.fromPeriod && draft.periodIndex <= event.window.throughPeriod
      && evaluateCondition(content, draft, event.requirements, `/events/${event.id}/requirements`)) {
      const returnPhase = draft.phase;
      const returnPresentation = structuredClone(draft.presentation);
      enterNarrative(content, draft, 'event', event.id);
      draft.activeNarrative!.returnPhase = returnPhase;
      draft.activeNarrative!.returnPresentation = returnPresentation;
    }
  }
}

function chargeFacility(content: ContentSnapshot, draft: RunData, costs: readonly { readonly attributeId: string; readonly amount: number }[], quantity: number, path: string, changes: Change[]): void {
  for (const [i, cost] of costs.entries()) {
    const amount = int32(cost.amount * quantity, '/command/quantity');
    const attribute = content.attributes.find(x => x.id === cost.attributeId)!;
    const before = draft.attributes[cost.attributeId]!, after = int32(before - amount, `/attributes/${cost.attributeId}`);
    requireCore(after >= attribute.min, 'INSUFFICIENT_FUNDS', '비용이 부족합니다.', `/attributes/${cost.attributeId}`);
    draft.attributes[cost.attributeId] = after;
    changes.push({ kind: 'numeric', source: `${path}/costs/${i}`, target: { kind: 'attribute', attributeId: cost.attributeId }, before, operand: -amount, requested: after, after, delta: -amount, clamped: false });
  }
}

export function applyDiet(content: ContentSnapshot, draft: RunData, changes: Change[]): void {
  const state = draft.gameplay;
  if (!state?.dietId || state.dietAppliedPeriod === draft.periodIndex || (state.dietNextPeriod ?? 0) > draft.periodIndex) return;
  const diet = content.project.gameplay?.diets?.find(x => x.id === state.dietId);
  requireCore(diet, 'UNKNOWN_DIET', '식단을 찾을 수 없습니다.', '/gameplay/dietId');
  const path = `/project/gameplay/diets/${diet.id}`;
  chargeFacility(content, draft, diet.costs, 1, path, changes);
  applyEffects(content, draft, diet.effects, `${path}/effects`, changes);
  state.dietAppliedPeriod = draft.periodIndex;
  state.dietNextPeriod = draft.periodIndex + (diet.intervalPeriods ?? 1);
}

function availability(content: ContentSnapshot, state: RunState, command: GameplayCommand): Availability {
  try { applyGameplay(content, structuredClone(state) as RunData, command, []); return { enabled: true, reason: null }; }
  catch (error) { if (error instanceof CoreFault) return { enabled: false, reason: error.message }; throw error; }
}

/** Read-only choices use the same transaction rules as execution; UI never guesses affordability. */
export function getGameplayView(content: ContentSnapshot, state: RunState | null): GameplayView | null {
  const definition = content.project.gameplay, data = state?.gameplay;
  if (!definition || !state || !data) return null;
  const location = definition.locations.find(x => x.id === data.locationId)!;
  return {
    location: { id: location.id, name: location.name }, screenId: data.screenId,
    actions: (definition.actions ?? []).filter(x => atLocation(x.locationIds, data.locationId)).map(x => ({ id: x.id, name: x.name, costs: structuredClone(x.costs), effects: structuredClone(x.effects), maxQuantity: x.maxQuantity ?? 1, ...(x.confirmation ? { confirmation: structuredClone(x.confirmation) } : {}), ...availability(content, state, { type: 'executeAction', actionId: x.id, quantity: 1 }) })),
    policyGroups: (definition.policyGroups ?? []).filter(x => atLocation(x.locationIds, data.locationId)).map(group => ({ id: group.id, name: group.name, trigger: structuredClone(group.trigger), ...availability(content, state, { type: 'selectPolicy', groupId: group.id, optionId: null }), options: group.options.map(option => {
      const action = definition.actions!.find(x => x.id === option.actionId)!;
      return { id: option.id, name: option.name, actionId: action.id, selected: data.policySelections?.[group.id] === option.id, costs: structuredClone(action.costs), effects: structuredClone(action.effects), ...(option.budgetUses ? { budgetUses: structuredClone(option.budgetUses) } : {}), ...(action.confirmation ? { confirmation: structuredClone(action.confirmation) } : {}), ...availability(content, state, { type: 'selectPolicy', groupId: group.id, optionId: option.id }) };
    }) })),
    policyBudgets: (definition.policyBudgets ?? []).map(budget => ({ ...structuredClone(budget), used: policyUsage(definition, data as GameplayState, budget.id) })),
    locations: definition.locations.filter(x => location.connections.includes(x.id) || x.id === location.id).map(x => ({
      id: x.id, name: x.name, ...(x.screenId ? { screenId: x.screenId } : {}), ...availability(content, state, { type: 'visit', locationId: x.id }),
    })),
    shops: definition.shops.filter(x => x.locationId === data.locationId).map(shop => ({
      id: shop.id, name: shop.name, currencyAttributeId: shop.currencyAttributeId,
      offers: shop.offers.map(offer => ({ itemId: offer.itemId, name: definition.items.find(x => x.id === offer.itemId)!.name,
        price: offer.price, stock: offer.stock === null ? null : offer.stock - (Object.hasOwn(data.purchased, shop.id)
          && Object.hasOwn(data.purchased[shop.id]!, offer.itemId) ? data.purchased[shop.id]![offer.itemId]! : 0),
        owned: data.inventory[offer.itemId] ?? 0, maxQuantity: offer.maxQuantity ?? 99, ...(offer.confirmation ? { confirmation: structuredClone(offer.confirmation) } : {}), ...availability(content, state, { type: 'buy', shopId: shop.id, itemId: offer.itemId, quantity: 1 }),
      })),
      sellOffers: (shop.sellOffers ?? []).map(offer => ({ itemId: offer.itemId, name: definition.items.find(x => x.id === offer.itemId)!.name,
        price: offer.price, owned: data.inventory[offer.itemId] ?? 0, maxQuantity: offer.maxQuantity ?? 99, ...(offer.confirmation ? { confirmation: structuredClone(offer.confirmation) } : {}),
        ...availability(content, state, { type: 'sell', shopId: shop.id, itemId: offer.itemId, quantity: 1 }) })),
    })),
    diets: (definition.diets ?? []).filter(x => x.locationId === data.locationId).map(x => ({
      id: x.id, name: x.name, costs: structuredClone(x.costs), effects: structuredClone(x.effects), intervalPeriods: x.intervalPeriods ?? 1,
      selected: data.dietId === x.id, ...(x.confirmation ? { confirmation: structuredClone(x.confirmation) } : {}), ...availability(content, state, { type: 'setDiet', dietId: x.id }),
    })),
    services: (definition.services ?? []).filter(x => x.locationId === data.locationId).map(x => ({
      id: x.id, name: x.name, costs: structuredClone(x.costs), effects: structuredClone(x.effects), maxQuantity: x.maxQuantity ?? 1,
      ...(x.confirmation ? { confirmation: structuredClone(x.confirmation) } : {}), ...availability(content, state, { type: 'useService', serviceId: x.id, quantity: 1 }),
    })),
    interactions: definition.interactions.filter(x => x.locationId === data.locationId).map(x => ({
      id: x.id, name: x.name, ...(x.characterId ? { characterId: x.characterId } : {}), ...(x.itemId ? { itemId: x.itemId } : {}),
      ...availability(content, state, { type: 'interact', interactionId: x.id }),
    })),
    inventory: definition.items.filter(x => (data.inventory[x.id] ?? 0) > 0).map(x => ({ id: x.id, name: x.name, quantity: data.inventory[x.id]! })),
  };
}

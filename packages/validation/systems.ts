import { isObject, type JsonObject, type ProjectDocument } from './model.js';
import { objectItems, type Index, type SemanticContext } from './context.js';

export function checkSystems(ctx: SemanticContext, doc: ProjectDocument, data: JsonObject): void {
  const root = doc.data as JsonObject, gameplay = root.gameplay;
  if (!isObject(gameplay)) { ctx.report(doc, '/systems', 'SYSTEMS_GAMEPLAY_REQUIRED', '확장 시스템에는 장소·물품 정의가 필요합니다.'); return; }
  const indexes: Record<string, Index> = {};
  for (const key of ['equipment', 'enemies', 'expeditions', 'quests', 'relationshipStages']) { indexes[key] = new Map(); ctx.addArray(indexes[key]!, doc, `/systems/${key}`, data[key], key); }
  const items = new Map<string, JsonObject>(objectItems(gameplay.items).map(x => [x.id as string, x]));
  const locations = new Set(objectItems(gameplay.locations).map(x => x.id));
  const refItem = (id: string, pointer: string) => { if (!items.has(id)) ctx.report(doc, pointer, 'REFERENCE_MISSING', `물품 '${id}'를 찾을 수 없습니다.`, [id]); };
  const refLocation = (id: string, pointer: string) => { if (!locations.has(id)) ctx.report(doc, pointer, 'REFERENCE_MISSING', `장소 '${id}'를 찾을 수 없습니다.`, [id]); };
  if (isObject(data.hero)) for (const key of ['maxHpAttributeId', 'attackAttributeId', 'defenseAttributeId']) ctx.resolve(ctx.attributes, data.hero[key] as string, doc, `/systems/hero/${key}`, '전투 능력');
  const equipmentItems = new Set<string>();
  objectItems(data.equipment).forEach((e, i) => {
    const id = e.itemId as string; refItem(id, `/systems/equipment/${i}/itemId`);
    if (equipmentItems.has(id)) ctx.report(doc, `/systems/equipment/${i}/itemId`, 'EQUIPMENT_ITEM_DUPLICATE', '한 물품에는 하나의 장비 정의만 사용할 수 있습니다.', [id]);
    equipmentItems.add(id);
  });
  const consumables = new Set<string>();
  objectItems(data.consumables).forEach((e, i) => {
    const id = e.itemId as string; refItem(id, `/systems/consumables/${i}/itemId`);
    if (consumables.has(id)) ctx.report(doc, `/systems/consumables/${i}/itemId`, 'CONSUMABLE_DUPLICATE', '회복 물품이 중복되었습니다.', [id]); consumables.add(id);
  });
  function rewards(value: JsonObject, path: string) {
    const seen = new Set<string>();
    objectItems(value.items).forEach((item, i) => {
      const id = item.itemId as string; refItem(id, `${path}/items/${i}/itemId`);
      if (seen.has(id)) ctx.report(doc, `${path}/items/${i}/itemId`, 'REWARD_DUPLICATE', '같은 보상 물품을 중복 선언할 수 없습니다.', [id]); seen.add(id);
      if (items.has(id) && Number(item.quantity) > Number(items.get(id)!.maxStack)) ctx.report(doc, `${path}/items/${i}/quantity`, 'REWARD_CAPACITY', '한 번의 보상 수량이 최대 보유량을 초과합니다.', [id]);
    });
  }
  objectItems(data.enemies).forEach((e, i) => rewards(e.rewards as JsonObject, `/systems/enemies/${i}/rewards`));
  objectItems(data.quests).forEach((q, i) => {
    const p = `/systems/quests/${i}`; refLocation(q.locationId as string, `${p}/locationId`);
    ctx.resolve(indexes.enemies!, q.enemyId as string, doc, `${p}/enemyId`, '목표 적'); rewards(q.rewards as JsonObject, `${p}/rewards`);
  });
  objectItems(data.expeditions).forEach((e, i) => {
    const p = `/systems/expeditions/${i}`; refLocation(e.locationId as string, `${p}/locationId`);
    const nodes: Index = new Map(); ctx.addArray(nodes, doc, `${p}/nodes`, e.nodes, '탐험 지점');
    ctx.resolve(nodes, e.entryNodeId as string, doc, `${p}/entryNodeId`, '시작 지점');
    objectItems(e.nodes).forEach((n, j) => {
      if (n.enemyId) ctx.resolve(indexes.enemies!, n.enemyId as string, doc, `${p}/nodes/${j}/enemyId`, '전투 적');
      (n.connections as string[]).forEach((id, k) => ctx.resolve(nodes, id, doc, `${p}/nodes/${j}/connections/${k}`, '연결 지점'));
    });
    const seen = new Set<string>(), pending = [e.entryNodeId as string];
    while (pending.length) { const id = pending.pop()!; if (seen.has(id)) continue; seen.add(id); for (const next of (nodes.get(id)?.[0]?.data.connections as string[] | undefined) ?? []) pending.push(next); }
    if (!objectItems(e.nodes).some(n => n.exit && seen.has(n.id as string))) ctx.report(doc, `${p}/nodes`, 'EXPEDITION_NO_EXIT', '시작 지점에서 도달할 수 있는 출구가 필요합니다.');
    objectItems(e.nodes).forEach((n, j) => { if (!seen.has(n.id as string)) ctx.report(doc, `${p}/nodes/${j}/id`, 'EXPEDITION_NODE_UNREACHABLE', '시작 지점에서 연결 경로가 없습니다.', [n.id as string], 'warning'); });
  });
  objectItems(data.relationshipStages).forEach((stage, i) => {
    const p = `/systems/relationshipStages/${i}`, ch = ctx.resolve(ctx.characters, stage.characterId as string, doc, `${p}/characterId`, '인물');
    const relationship = ch ? ctx.resolve(ctx.relationships.get(ch)!, stage.relationshipId as string, doc, `${p}/relationshipId`, '관계') : undefined;
    if (relationship && (Number(stage.threshold) < Number(relationship.data.min) || Number(stage.threshold) > Number(relationship.data.max))) ctx.report(doc, `${p}/threshold`, 'STAGE_RANGE', '관계 단계 임계값은 관계 범위 안이어야 합니다.');
    if (stage.eventId) { const event = ctx.resolve(ctx.events, stage.eventId as string, doc, `${p}/eventId`, '관계 사건'); if (event && event.data.trigger !== 'interaction') ctx.report(doc, `${p}/eventId`, 'STAGE_EVENT_TRIGGER', '관계 단계 사건은 interaction 트리거여야 합니다.'); }
  });
  for (const entries of ctx.activities.values()) for (const activity of entries) {
    if (!activity.data.expeditionId) continue;
    ctx.resolve(indexes.expeditions!, activity.data.expeditionId as string, activity.doc, '/expeditionId', '탐험');
    if (activity.data.successCheck) ctx.report(activity.doc, '/successCheck', 'EXPEDITION_SUCCESS_CHECK', '탐험 활동에는 별도의 활동 성공 판정을 넣을 수 없습니다. 전투로 결과를 결정합니다.');
  }
}

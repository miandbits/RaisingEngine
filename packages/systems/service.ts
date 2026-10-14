import type { ContentSnapshot, DeepReadonly, RunData, RunState, Change } from '../core/model.js';
import { applyEffects, evaluateCondition } from '../core/evaluation.js';
import { applyScene, enterNarrative } from '../core/narrative.js';
import { sampleBelow } from '../core/random.js';
import { CoreFault, int32, requireCore } from '../core/util.js';
import type { CombatStats, SystemsCommand, SystemsDefinition, SystemsState, SystemsView } from './model.js';

export const systemsCommandTypes = ['acceptQuest', 'claimQuest', 'equip', 'unequip', 'moveExpedition', 'returnFromExpedition', 'battleAction'] as const;
export function createSystems(): SystemsState {
  return { equipment: { weapon: null, armor: null, accessory: null }, quests: {}, unlockedStages: [], pendingStageEvents: [], expedition: null, battle: null, lastBattle: null, lastExpedition: null };
}
export function equipmentReserved(content: ContentSnapshot, state: RunState, itemId: string): number {
  if (!state.systems || !content.project.systems) return 0;
  return Object.values(state.systems.equipment).filter(id => id !== null && content.project.systems!.equipment.some(e => e.id === id && e.itemId === itemId)).length;
}
export function heroStats(content: ContentSnapshot, state: RunState): CombatStats {
  const d = content.project.systems!;
  const result = { maxHp: state.attributes[d.hero.maxHpAttributeId]!, attack: state.attributes[d.hero.attackAttributeId]!, defense: state.attributes[d.hero.defenseAttributeId]! };
  for (const id of Object.values(state.systems!.equipment)) {
    const e = d.equipment.find(e => e.id === id);
    if (e) for (const key of ['maxHp', 'attack', 'defense'] as const) result[key] = int32(result[key] + e.modifiers[key], `/systems/equipment/${key}`);
  }
  result.maxHp = Math.max(1, result.maxHp); result.attack = Math.max(0, result.attack); result.defense = Math.max(0, result.defense);
  return result;
}
function reward(content: ContentSnapshot, draft: RunData, value: DeepReadonly<SystemsDefinition['quests'][number]['rewards']>, source: string, discard: boolean, recorded?: Change[]) {
  const discarded: { itemId: string; quantity: number }[] = [];
  for (const entry of value.items) {
    const item = content.project.gameplay!.items.find(i => i.id === entry.itemId)!;
    const before = draft.gameplay!.inventory[entry.itemId] ?? 0;
    const after = int32(before + entry.quantity, source);
    requireCore(discard || after <= item.maxStack, 'INVENTORY_FULL', '보상을 받을 소지품 공간이 부족합니다.', `/gameplay/inventory/${entry.itemId}`);
    draft.gameplay!.inventory[entry.itemId] = Math.min(item.maxStack, after);
    if(recorded)recorded.push({kind:'numeric',source:`${source}/items/${entry.itemId}`,target:{kind:'inventory',itemId:entry.itemId},before,operand:entry.quantity,requested:after,after:Math.min(item.maxStack,after),delta:Math.min(item.maxStack,after)-before,clamped:after>item.maxStack?'max':false});
    if (after > item.maxStack) discarded.push({ itemId: entry.itemId, quantity: after - item.maxStack });
  }
  applyEffects(content, draft, value.effects, `${source}/effects`, recorded??[]);
  return discarded;
}
function enterNode(content: ContentSnapshot, draft: RunData, nodeId: string, sounds: string[]) {
  const s = draft.systems!, trip = s.expedition!;
  const definition = content.project.systems!.expeditions.find(e => e.id === trip.expeditionId)!;
  const node = definition.nodes.find(n => n.id === nodeId)!;
  trip.nodeId = node.id;
  if (node.presentation) applyScene(draft, node.presentation, sounds);
  draft.phase = 'expedition';
  if (node.enemyId && !trip.clearedNodes.includes(node.id)) {
    const enemy = content.project.systems!.enemies.find(e => e.id === node.enemyId)!;
    s.battle = { enemyId: enemy.id, enemyHp: enemy.maxHp, turn: 0,...(content.project.systems!.battleHistoryLimit?{log:[]}: {}) }; draft.phase = 'battle';
  }
}
export function startExpedition(content: ContentSnapshot, draft: RunData, expeditionId: string, sounds: string[]): void {
  const d = content.project.systems, s = draft.systems;
  requireCore(d && s && draft.mode === 'full', 'SYSTEMS_UNAVAILABLE', '탐험 시스템이 있는 전체 실행 모드가 필요합니다.', '/project/systems');
  const expedition = d.expeditions.find(e => e.id === expeditionId);
  requireCore(expedition, 'UNKNOWN_EXPEDITION', '탐험을 찾을 수 없습니다.', '/activity/expeditionId');
  requireCore(draft.gameplay?.locationId === expedition.locationId, 'WRONG_LOCATION', '탐험 출발 장소로 이동해 주세요.', '/gameplay/locationId');
  requireCore(!s.expedition, 'EXPEDITION_ACTIVE', '이미 진행 중인 탐험이 있습니다.', '/systems/expedition');
  const stats = heroStats(content, draft);
  s.expedition = { expeditionId, nodeId: expedition.entryNodeId, clearedNodes: [], heroHp: stats.maxHp, stats, returnPresentation: structuredClone(draft.presentation) };
  s.battle = null; s.lastBattle = null; s.lastExpedition = null;
  enterNode(content, draft, expedition.entryNodeId, sounds);
}
function finishExpedition(draft: RunData, outcome: 'completed' | 'retreated' | 'defeated') {
  const s = draft.systems!, trip = s.expedition!;
  s.lastExpedition = { expeditionId: trip.expeditionId, outcome, clearedNodes: [...trip.clearedNodes] };
  draft.presentation = structuredClone(trip.returnPresentation);
  s.expedition = null; s.battle = null; draft.phase = 'activity-result';
}
export function applySystems(content: ContentSnapshot, draft: RunData, command: SystemsCommand, sounds: string[]): void {
  const d = content.project.systems, s = draft.systems;
  requireCore(d && s && draft.gameplay && draft.mode === 'full', 'SYSTEMS_UNAVAILABLE', '확장 시스템이 있는 전체 실행 모드가 필요합니다.', '/project/systems');
  if (command.type === 'equip' || command.type === 'unequip' || command.type === 'acceptQuest' || command.type === 'claimQuest') {
    requireCore(draft.phase === 'planning' || draft.phase === 'ready', 'WRONG_PHASE', '일정 편성 중에만 장비·퀘스트를 변경할 수 있습니다.', '/phase');
    if (command.type === 'equip') {
      const e = d.equipment.find(e => e.id === command.equipmentId);
      requireCore(e, 'UNKNOWN_EQUIPMENT', '장비를 찾을 수 없습니다.', '/command/equipmentId');
      requireCore(s.equipment[e.slot] !== e.id, 'ALREADY_EQUIPPED', '이미 장착한 장비입니다.', '/command/equipmentId');
      requireCore((draft.gameplay.inventory[e.itemId] ?? 0) - equipmentReserved(content, draft, e.itemId) > 0, 'INSUFFICIENT_ITEMS', '장착 가능한 물품이 없습니다.', `/gameplay/inventory/${e.itemId}`);
      s.equipment[e.slot] = e.id; heroStats(content, draft); return;
    }
    if (command.type === 'unequip') {
      requireCore(s.equipment[command.slot] !== null, 'SLOT_EMPTY', '장착한 장비가 없습니다.', '/command/slot');
      s.equipment[command.slot] = null; return;
    }
    const quest = d.quests.find(q => q.id === command.questId);
    requireCore(quest, 'UNKNOWN_QUEST', '퀘스트를 찾을 수 없습니다.', '/command/questId');
    requireCore(draft.gameplay.locationId === quest.locationId, 'WRONG_LOCATION', '퀘스트 게시판이 있는 장소로 이동해 주세요.', '/gameplay/locationId');
    const q = Object.hasOwn(s.quests, quest.id) ? s.quests[quest.id] : undefined;
    if (command.type === 'acceptQuest') {
      requireCore(!q, 'QUEST_ACCEPTED', '이미 수락한 퀘스트입니다.', '/command/questId');
      requireCore(evaluateCondition(content, draft, quest.requirements, `/systems/quests/${quest.id}/requirements`), 'QUEST_LOCKED', '퀘스트 수락 조건을 만족하지 않습니다.', '/command/questId');
      s.quests[quest.id] = { status: 'accepted', progress: 0 };
    } else {
      requireCore(q?.status === 'completed', 'QUEST_NOT_COMPLETED', '보상을 받을 완료 퀘스트가 아닙니다.', '/command/questId');
      reward(content, draft, quest.rewards, `/systems/quests/${quest.id}/rewards`, false); q.status = 'claimed';
    }
    return;
  }
  const trip = s.expedition;
  requireCore(trip, 'NO_EXPEDITION', '진행 중인 탐험이 없습니다.', '/systems/expedition');
  const expedition = d.expeditions.find(e => e.id === trip.expeditionId)!;
  const node = expedition.nodes.find(n => n.id === trip.nodeId)!;
  if (command.type === 'moveExpedition' || command.type === 'returnFromExpedition') {
    requireCore(draft.phase === 'expedition', 'WRONG_PHASE', '전투가 끝난 뒤 탐험 이동·귀환을 할 수 있습니다.', '/phase');
    if (command.type === 'returnFromExpedition') { finishExpedition(draft, node.exit ? 'completed' : 'retreated'); return; }
    requireCore(node.connections.includes(command.nodeId), 'NODE_DISCONNECTED', '현재 탐험 지점과 연결되지 않았습니다.', '/command/nodeId');
    requireCore(expedition.nodes.some(n => n.id === command.nodeId), 'UNKNOWN_NODE', '탐험 지점을 찾을 수 없습니다.', '/command/nodeId');
    enterNode(content, draft, command.nodeId, sounds); return;
  }
  requireCore(draft.phase === 'battle' && s.battle, 'WRONG_PHASE', '진행 중인 전투가 없습니다.', '/phase');
  const battle = s.battle, enemy = d.enemies.find(e => e.id === battle.enemyId)!;
  const turn = int32(battle.turn + 1, '/systems/battle/turn');
  let heroDamage = 0, enemyDamage = 0,heroHeal=0;
  const heroHpBefore=trip.heroHp,enemyHpBefore=battle.enemyHp;
  const record=()=>{if(battle.log){battle.log.push({id:`turn-${turn}`,turn,action:command.action,target:command.action==='attack'?'enemy':command.action==='flee'?'none':'self',...(command.action==='useItem'?{itemId:command.itemId}:{}),heroHpBefore,heroHpAfter:trip.heroHp,enemyHpBefore,enemyHpAfter:battle.enemyHp,heroDamage,enemyDamage,heroHeal});battle.log.splice(0,Math.max(0,battle.log.length-(d.battleHistoryLimit??128)));}};
  const result = (outcome: 'victory' | 'defeat' | 'fled', discardedItems: { itemId: string; quantity: number }[] = [],rewardChanges:Change[]=[]) => {
    s.lastBattle = { enemyId: enemy.id, outcome, turn, heroDamage, enemyDamage, discardedItems,...(battle.log?{log:structuredClone(battle.log),rewardChanges}: {}) };
  };
  if (command.action === 'flee') { record();result('fled'); finishExpedition(draft, 'retreated'); return; }
  if (command.action === 'useItem') {
    const item = d.consumables.find(i => i.itemId === command.itemId);
    requireCore(item, 'UNKNOWN_CONSUMABLE', '전투 회복 물품이 아닙니다.', '/command/itemId');
    requireCore((draft.gameplay.inventory[item.itemId] ?? 0) - equipmentReserved(content, draft, item.itemId) > 0, 'INSUFFICIENT_ITEMS', '사용 가능한 회복 물품이 없습니다.', `/gameplay/inventory/${item.itemId}`);
    requireCore(trip.heroHp < trip.stats.maxHp, 'HP_FULL', '체력이 이미 가득 찼습니다.', '/systems/expedition/heroHp');
    draft.gameplay.inventory[item.itemId]!--; trip.heroHp = Math.min(trip.stats.maxHp, int32(trip.heroHp + item.heal, '/systems/expedition/heroHp'));
    heroHeal=trip.heroHp-heroHpBefore;
  } else if (command.action === 'attack') {
    const roll = sampleBelow(draft.rng, 3); draft.rng = roll.rng;
    enemyDamage = Math.min(battle.enemyHp, Math.max(1, trip.stats.attack - enemy.defense + roll.value - 1));
    battle.enemyHp -= enemyDamage;
  }
  battle.turn = turn;
  if (battle.enemyHp === 0) {
    const rewardChanges:Change[]=[];
    const discarded = reward(content, draft, enemy.rewards, `/systems/enemies/${enemy.id}/rewards`, true,battle.log?rewardChanges:undefined);
    trip.clearedNodes.push(node.id);
    for (const quest of d.quests) {
      const q = Object.hasOwn(s.quests, quest.id) ? s.quests[quest.id] : undefined;
      if (quest.enemyId === enemy.id && q?.status === 'accepted') { q.progress = Math.min(quest.targetCount, q.progress + 1); if (q.progress >= quest.targetCount) q.status = 'completed'; }
    }
    record();result('victory', discarded,rewardChanges); s.battle = null; draft.phase = 'expedition'; return;
  }
  const roll = sampleBelow(draft.rng, 3); draft.rng = roll.rng;
  const damage = Math.max(1, enemy.attack - trip.stats.defense + roll.value - 1);
  heroDamage = Math.min(trip.heroHp, command.action === 'guard' ? Math.ceil(damage / 2) : damage);
  trip.heroHp -= heroDamage;
  record();
  if (trip.heroHp === 0) { result('defeat'); finishExpedition(draft, 'defeated'); }
}
/** Unlock once; start at most one queued relationship narrative per accepted command. */
export function syncRelationshipStages(content: ContentSnapshot, draft: RunData, finishingPeriod = false): void {
  const d = content.project.systems, s = draft.systems; if (!d || !s || draft.mode !== 'full') return;
  for (const stage of [...d.relationshipStages].sort((a, b) => a.threshold - b.threshold || (a.id < b.id ? -1 : a.id > b.id ? 1 : 0))) {
    if (!s.unlockedStages.includes(stage.id) && draft.relationships[stage.characterId]![stage.relationshipId]! >= stage.threshold) {
      s.unlockedStages.push(stage.id);
      if (stage.eventId && !s.pendingStageEvents.includes(stage.eventId) && !draft.completedEvents.includes(stage.eventId)) s.pendingStageEvents.push(stage.eventId);
    }
  }
  if (draft.phase !== 'planning' && draft.phase !== 'ready' && draft.phase !== 'period-end') return;
  for (const id of [...s.pendingStageEvents]) {
    const event = content.events.find(e => e.id === id)!;
    if (draft.completedEvents.includes(id) || draft.periodIndex > event.window.throughPeriod) { s.pendingStageEvents.splice(s.pendingStageEvents.indexOf(id), 1); continue; }
    if (draft.periodIndex < event.window.fromPeriod || !evaluateCondition(content, draft, event.requirements, `/events/${id}/requirements`)) continue;
    const phase = draft.phase, presentation = structuredClone(draft.presentation);
    s.pendingStageEvents.splice(s.pendingStageEvents.indexOf(id), 1);
    enterNarrative(content, draft, 'event', id); draft.activeNarrative!.returnPhase = phase; draft.activeNarrative!.returnPresentation = presentation;
    if (finishingPeriod) draft.activeNarrative!.finishPeriod = true;
    break;
  }
}
function availability(content: ContentSnapshot, state: RunState, command: SystemsCommand) {
  try { applySystems(content, structuredClone(state) as RunData, command, []); return { enabled: true, reason: null }; }
  catch (e) { if (e instanceof CoreFault) return { enabled: false, reason: e.message }; throw e; }
}
export function getSystemsView(content: ContentSnapshot, state: RunState | null): SystemsView | null {
  const d = content.project.systems, s = state?.systems; if (!d || !s || !state) return null;
  const trip = s.expedition, expedition = trip ? d.expeditions.find(e => e.id === trip.expeditionId)! : null;
  const node = trip && expedition ? expedition.nodes.find(n => n.id === trip.nodeId)! : null;
  const enemy = s.battle ? d.enemies.find(e => e.id === s.battle!.enemyId)! : null;
  return {
    heroStats: heroStats(content, state),
    equipment: d.equipment.map(e => ({ id: e.id, name: e.name, itemId: e.itemId, slot: e.slot, equipped: s.equipment[e.slot] === e.id,
      canUnequip: s.equipment[e.slot] === e.id && availability(content, state, { type: 'unequip', slot: e.slot }).enabled,
      owned: state.gameplay!.inventory[e.itemId] ?? 0, ...availability(content, state, { type: 'equip', equipmentId: e.id }) })),
    quests: d.quests.map(q => {
      const saved = Object.hasOwn(s.quests, q.id) ? s.quests[q.id] : undefined;
      const accept = availability(content, state, { type: 'acceptQuest', questId: q.id }), claim = availability(content, state, { type: 'claimQuest', questId: q.id });
      return { id: q.id, name: q.name, status: saved?.status ?? (accept.enabled ? 'available' : 'locked'), progress: saved?.progress ?? 0, targetCount: q.targetCount, canAccept: accept.enabled, canClaim: claim.enabled, reason: saved ? claim.reason : accept.reason };
    }),
    consumables: d.consumables.map(i => ({ itemId: i.itemId, name: content.project.gameplay!.items.find(x => x.id === i.itemId)!.name, quantity: state.gameplay!.inventory[i.itemId] ?? 0, heal: i.heal, enabled: availability(content, state, { type: 'battleAction', action: 'useItem', itemId: i.itemId }).enabled })),
    battleActions:[...(['attack','guard','flee'] as const).map(action=>({id:action,name:{attack:'공격',guard:'방어',flee:'도주'}[action],battleAction:action,target:action==='attack'?'enemy' as const:action==='guard'?'self' as const:'none' as const,...availability(content,state,{type:'battleAction',action})})),...d.consumables.map(item=>({id:`use-item--${item.itemId}`,name:content.project.gameplay!.items.find(x=>x.id===item.itemId)!.name,battleAction:'useItem' as const,itemId:item.itemId,target:'self' as const,...availability(content,state,{type:'battleAction',action:'useItem',itemId:item.itemId})}))],
    expedition: trip && expedition && node ? { expeditionId: trip.expeditionId, name: expedition.name, nodeId: node.id, nodeName: node.name, ...(node.description ? { nodeDescription: node.description } : {}), heroHp: trip.heroHp, stats: { ...trip.stats }, connections: node.connections.map(id => ({ id, name: expedition.nodes.find(n => n.id === id)!.name, enabled: state.phase === 'expedition' })), canReturn: state.phase === 'expedition' } : null,
    battle: s.battle && trip && enemy ? { enemyId:s.battle.enemyId,enemyHp:s.battle.enemyHp,turn:s.battle.turn,...(s.battle.log?{log:s.battle.log.map(x=>({...x}))}:{}), enemyName: enemy.name, enemyMaxHp: enemy.maxHp, heroHp: trip.heroHp, heroMaxHp: trip.stats.maxHp } : null,
    stages: d.relationshipStages.map(x => ({ id: x.id, name: x.name, unlocked: s.unlockedStages.includes(x.id), characterId: x.characterId, relationshipId: x.relationshipId, threshold: x.threshold })),
    lastBattle: s.lastBattle ? structuredClone(s.lastBattle) as SystemsState['lastBattle'] : null,
    lastExpedition: s.lastExpedition ? { ...s.lastExpedition, clearedNodes: [...s.lastExpedition.clearedNodes] } : null,
  };
}

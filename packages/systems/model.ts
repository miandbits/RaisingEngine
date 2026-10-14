import type { SystemsDefinition } from '../schema/generated/systems.js';
import type { PresentationState, Change } from '../core/model.js';
export type { SystemsDefinition };
export type EquipmentSlot = 'weapon' | 'armor' | 'accessory';
export interface CombatStats { maxHp: number; attack: number; defense: number }
export interface BattleLogEntry {
  id: string; turn: number; action: 'attack'|'guard'|'flee'|'useItem'; target: 'self'|'enemy'|'none'; itemId?: string;
  heroHpBefore: number; heroHpAfter: number; enemyHpBefore: number; enemyHpAfter: number;
  heroDamage: number; enemyDamage: number; heroHeal: number;
}
export interface BattleActionView {
  id: string; name: string; battleAction: 'attack'|'guard'|'flee'|'useItem'; itemId?: string;
  target: 'self'|'enemy'|'none'; enabled: boolean; reason: string|null;
}
export type SystemsCommand =
  | { type: 'acceptQuest'; questId: string }
  | { type: 'claimQuest'; questId: string }
  | { type: 'equip'; equipmentId: string }
  | { type: 'unequip'; slot: EquipmentSlot }
  | { type: 'moveExpedition'; nodeId: string }
  | { type: 'returnFromExpedition' }
  | { type: 'battleAction'; action: 'attack' | 'guard' | 'flee' }
  | { type: 'battleAction'; action: 'useItem'; itemId: string };
export interface SystemsState {
  equipment: Record<EquipmentSlot, string | null>;
  quests: Record<string, { status: 'accepted' | 'completed' | 'claimed'; progress: number }>;
  unlockedStages: string[];
  pendingStageEvents: string[];
  expedition: null | { expeditionId: string; nodeId: string; clearedNodes: string[]; heroHp: number; stats: CombatStats; returnPresentation: PresentationState };
  battle: null | { enemyId: string; enemyHp: number; turn: number; log?: BattleLogEntry[] };
  lastBattle: null | { enemyId: string; outcome: 'victory' | 'defeat' | 'fled'; turn: number; heroDamage: number; enemyDamage: number; discardedItems: { itemId: string; quantity: number }[]; log?: BattleLogEntry[]; rewardChanges?: Change[] };
  lastExpedition: null | { expeditionId: string; outcome: 'completed' | 'retreated' | 'defeated'; clearedNodes: string[] };
}
export interface SystemsView {
  heroStats: CombatStats;
  equipment: { id: string; name: string; itemId: string; slot: EquipmentSlot; equipped: boolean; canUnequip: boolean; owned: number; enabled: boolean; reason: string | null }[];
  quests: { id: string; name: string; status: 'available' | 'locked' | 'accepted' | 'completed' | 'claimed'; progress: number; targetCount: number; canAccept: boolean; canClaim: boolean; reason: string | null }[];
  consumables: { itemId: string; name: string; quantity: number; heal: number; enabled: boolean }[];
  expedition: null | { expeditionId: string; name: string; nodeId: string; nodeName: string; nodeDescription?: string; heroHp: number; stats: CombatStats; connections: { id: string; name: string; enabled: boolean }[]; canReturn: boolean };
  battle: null | { enemyId: string; enemyName: string; enemyHp: number; enemyMaxHp: number; turn: number; heroHp: number; heroMaxHp: number; log?: BattleLogEntry[] };
  battleActions: BattleActionView[];
  stages: { id: string; name: string; unlocked: boolean; characterId: string; relationshipId: string; threshold: number }[];
  lastBattle: SystemsState['lastBattle'];
  lastExpedition: SystemsState['lastExpedition'];
}

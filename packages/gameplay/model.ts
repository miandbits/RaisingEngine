import type { GameplayDefinition } from '../schema/generated/gameplay.js';
import type { DeepReadonly } from '../core/model.js';
export type { GameplayDefinition };
export type GameplayCommand =
  | { type: 'navigate'; screenId: string }
  | { type: 'visit'; locationId: string }
  | { type: 'buy'; shopId: string; itemId: string; quantity: number }
  | { type: 'sell'; shopId: string; itemId: string; quantity: number }
  | { type: 'useService'; serviceId: string; quantity: number }
  | { type: 'setDiet'; dietId: string | null }
  | { type: 'executeAction'; actionId: string; quantity: number }
  | { type: 'selectPolicy'; groupId: string; optionId: string | null }
  | { type: 'interact'; interactionId: string };
export interface GameplayState {
  locationId: string;
  screenId: string;
  inventory: Record<string, number>;
  purchased: Record<string, Record<string, number>>;
  interactionPeriods: Record<string, number>;
  visited: string[];
  dietId?: string | null;
  dietAppliedPeriod?: number;
  dietNextPeriod?: number;
  policySelections?: Record<string, string | null>;
  policyNextPeriods?: Record<string, number>;
  policyAppliedPeriods?: Record<string, number>;
  rulePeriods?: { periodStart?: number; periodEnd?: number };
}
export interface Confirmation { enabled?: boolean; title?: string; message?: string; confirmLabel?: string; cancelLabel?: string }
export interface FacilityView extends Availability {
  id: string; name: string; costs: readonly { readonly attributeId: string; readonly amount: number }[];
  effects: DeepReadonly<GameplayDefinition['interactions'][number]['effects']>; confirmation?: Confirmation;
}
export interface Availability { enabled: boolean; reason: string | null }
export interface GameplayView {
  location: { id: string; name: string };
  screenId: string;
  locations: (Availability & { id: string; name: string; screenId?: string })[];
  shops: { id: string; name: string; currencyAttributeId: string; offers: (Availability & {
    itemId: string; name: string; price: number; stock: number | null; owned: number; maxQuantity: number; confirmation?: Confirmation;
  })[]; sellOffers: (Availability & { itemId: string; name: string; price: number; owned: number; maxQuantity: number; confirmation?: Confirmation })[] }[];
  diets: (FacilityView & { intervalPeriods: number; selected: boolean })[];
  services: (FacilityView & { maxQuantity: number })[];
  actions: (FacilityView & { maxQuantity: number })[];
  policyGroups: (Availability & { id: string; name: string; trigger: { type: 'beforeActivity' | 'afterActivity'; intervalPeriods?: number }; options: (FacilityView & { actionId: string; selected: boolean; budgetUses?: readonly { readonly budgetId: string; readonly amount: number }[] })[] })[];
  policyBudgets: { id: string; name: string; capacity: number; unit?: string; used: number }[];
  interactions: (Availability & { id: string; name: string; characterId?: string; itemId?: string })[];
  inventory: { id: string; name: string; quantity: number }[];
}

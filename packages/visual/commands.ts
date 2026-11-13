import type { Element } from './model.js';
import type { SystemsCommand } from '../systems/model.js';
import type {BattleUiAction} from '../player-service/battle.js';
import type {RunState} from '../core/model.js';

export type WidgetCommand = SystemsCommand | BattleUiAction | import('./planner-entry.js').OpenPlannerCommand
  | { type: 'navigate'; screenId: string }
  | { type: 'visit'; locationId: string }
  | { type: 'buy'; shopId: string; itemId: string; quantity: number }
  | { type: 'sell'; shopId: string; itemId: string; quantity: number }
  | { type: 'useService'; serviceId: string; quantity: number }
  | { type: 'setDiet'; dietId: string | null }
  | { type: 'executeAction'; actionId: string; quantity: number }
  | { type: 'executeActorAction'; actorId: string; actionId: string; quantity: number }
  | { type: 'assignActivities'; assignments: Record<string,string> }
  | { type: 'selectPolicy'; groupId: string; optionId: string | null }
  | { type: 'interact'; interactionId: string };
export function widgetCommand(element: Element, state?:RunState|null): WidgetCommand | null {
  if(element.action==='assignActivity'&&element.actorId&&element.activityId&&state?.roster)return {type:'assignActivities',assignments:{...state.roster.assignments,[element.actorId]:element.activityId}};
  if(element.action==='executeActorAction'&&element.actorId&&element.actionId)return {type:'executeActorAction',actorId:element.actorId,actionId:element.actionId,quantity:element.quantity??1};
  if(element.action==='selectBattleAction'&&element.battleAction)return {type:'selectBattleAction',battleAction:element.battleAction,...(element.itemId?{itemId:element.itemId}:{})};
  if(element.action==='selectBattleTarget'&&element.battleTarget)return {type:'selectBattleTarget',battleTarget:element.battleTarget};
  if(element.action==='confirmBattleAction'||element.action==='cancelBattleAction')return {type:element.action};
  if ((element.action === 'acceptQuest' || element.action === 'claimQuest') && element.questId) return { type: element.action, questId: element.questId };
  if (element.action === 'equip' && element.equipmentId) return { type: 'equip', equipmentId: element.equipmentId };
  if (element.action === 'unequip' && element.slot) return { type: 'unequip', slot: element.slot };
  if (element.action === 'moveExpedition' && element.nodeId) return { type: 'moveExpedition', nodeId: element.nodeId };
  if (element.action === 'returnFromExpedition') return { type: 'returnFromExpedition' };
  if (element.action === 'battleAction' && element.battleAction) {
    if (element.battleAction === 'useItem') return element.itemId ? { type: 'battleAction', action: 'useItem', itemId: element.itemId } : null;
    return { type: 'battleAction', action: element.battleAction };
  }
  if(element.action==='openPlanner'&&element.screenId&&element.plannerElementId)return {type:'openPlanner',screenId:element.screenId,plannerElementId:element.plannerElementId,...(element.plannerGroupId?{plannerGroupId:element.plannerGroupId}:{}),...(element.plannerActivityId?{plannerActivityId:element.plannerActivityId}:{})};
  if (element.action === 'navigate' && element.screenId) return { type: 'navigate', screenId: element.screenId };
  if (element.action === 'visit' && element.locationId) return { type: 'visit', locationId: element.locationId };
  if (element.action === 'buy' && element.shopId && element.itemId) return { type: 'buy', shopId: element.shopId, itemId: element.itemId, quantity: element.quantity ?? 1 };
  if (element.action === 'sell' && element.shopId && element.itemId) return { type: 'sell', shopId: element.shopId, itemId: element.itemId, quantity: element.quantity ?? 1 };
  if (element.action === 'useService' && element.serviceId) return { type: 'useService', serviceId: element.serviceId, quantity: element.quantity ?? 1 };
  if (element.action === 'setDiet') return { type: 'setDiet', dietId: element.dietId ?? null };
  if (element.action === 'executeAction' && element.actionId) return { type: 'executeAction', actionId: element.actionId, quantity: element.quantity ?? 1 };
  if (element.action === 'selectPolicy' && element.groupId) return { type: 'selectPolicy', groupId: element.groupId, optionId: element.optionId ?? null };
  if (element.action === 'interact' && element.interactionId) return { type: 'interact', interactionId: element.interactionId };
  return null;
}


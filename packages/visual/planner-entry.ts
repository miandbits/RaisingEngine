import {presentationActivity,resolvePresentation} from './variants.js';
import {resolveScreenElements} from './components.js';
import type {ReadonlyDesign} from './model.js';
import type {RunState} from '../core/model.js';
export interface OpenPlannerCommand {type:'openPlanner';screenId:string;plannerElementId:string;plannerGroupId?:string;plannerActivityId?:string}
/** A presentation destination only. Assignment still requires the board's explicit Apply. */
export function plannerEntryError(design:ReadonlyDesign,state:RunState|null,activities:readonly {id:string}[],entry:OpenPlannerCommand,conditionPasses:(condition:unknown)=>boolean=()=>false):string|null {
  if(!state||!['planning','ready'].includes(state.phase)||state.roster)return 'The planner is unavailable in this game state.';
  if(!design.routing)return 'The planner needs explicit screen routing.';
  const screen=design.screens.find(s=>s.id===entry.screenId&&!s.modal);
  // Targets are resolved screen paths, including nested component instances.
  // Definition-local IDs are deliberately not treated as global aliases.
  const board=screen?resolveScreenElements(design,entry.screenId).find(e=>e.id===entry.plannerElementId&&e.kind==='schedule'&&e.scheduleLayout==='activity-board'):undefined;
  if(!board)return 'The target activity board is unavailable.';
  const raw=board;
  const resolved=raw?resolvePresentation(raw,presentationActivity(state,raw),conditionPasses):undefined;
  if(!resolved||resolved.hidden||(resolved.opacity??1)<=0||resolved.dataEnabled===false||!['always','planning',state.phase].includes(resolved.visibleWhen??'always')||[resolved.visibleCondition,resolved.enabledCondition].some(c=>c&&!conditionPasses(c)))return 'The target activity board is hidden or disabled.';
  const group=entry.plannerGroupId?board.scheduleGroups?.find(g=>g.id===entry.plannerGroupId):undefined;
  if(entry.plannerGroupId&&!group)return 'The planner category is unavailable.';
  if(entry.plannerActivityId&&(!activities.some(a=>a.id===entry.plannerActivityId)||(group&&!group.activityIds.includes(entry.plannerActivityId))))return 'The activity is unavailable in this planner category.';
  return null;
}

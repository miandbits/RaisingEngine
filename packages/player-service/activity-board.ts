import type {ContentSnapshot,RunState} from '../core/model.js';
import type {ScheduleDraft} from './save-metadata.js';
import {validScheduleDraft} from './save-metadata.js';
import {resolveScreenElements} from '../visual/components.js';

export interface ScheduleDraftOwner {screenId:string;elementId:string}
export function hasActivityBoard(content:Pick<ContentSnapshot,'project'>):boolean {
  const visual=content.project.visual;
  // Use the same instantiated widgets as the renderer, including nested shared
  // sections. Unused component declarations must not enable draft persistence.
  return !content.project.roster&&!!visual?.screens.some(screen=>resolveScreenElements(visual,screen.id,undefined,{ignoreClipping:true}).some(element=>element.kind==='schedule'&&element.scheduleLayout==='activity-board'));
}
export function firstDraftSlot(state:Pick<RunState,'periodIndex'|'activityIntermission'>,count:number):number {
  return Math.min(count,state.periodIndex%count+(state.activityIntermission?1:0));
}
/** Existing save envelope; unselected slots stay blank and no canonical command is created. */
export function initialScheduleDraft(state:Pick<RunState,'periodIndex'|'schedule'>,count:number):ScheduleDraft {
  return {month:Math.floor(state.periodIndex/count),slot:state.periodIndex%count,selected:state.schedule?(1<<count)-1:0,activities:state.schedule?[...state.schedule]:Array<string>(count).fill('')};
}
export function validBoardDraft(content:ContentSnapshot,state:RunState,value:unknown):value is ScheduleDraft {
  if(!validScheduleDraft(content,state,value))return false;
  const draft=value,first=firstDraftSlot(state,content.project.periodsPerMonth);
  return draft.activities.every((id,index)=>index>=first||id===state.schedule?.[index]&&!!(draft.selected&(1<<index)));
}

import { createRun as referenceCreateRun, dispatchCommand as referenceDispatchCommand } from '../core/run.js';
import { nativeCreateRun, nativeDispatchCommand } from './index.js';
import { nativeRequest } from './index.js';
import { calendarTime, getNarrativeView } from '../core/index.js';
import { getGameplayView } from '../gameplay/service.js';
import { getSystemsView } from '../systems/service.js';
import type { ContentSnapshot, RunState } from '../core/model.js';

export interface RuntimeProjection {
  time:ReturnType<typeof calendarTime>|null;
  narrative:ReturnType<typeof getNarrativeView>;
  gameplay:ReturnType<typeof getGameplayView>;
  systems:ReturnType<typeof getSystemsView>;
}

/** Reference backend is opt-in. A missing native binary never silently changes rules. */
export function runtimeBackend(): 'native' | 'typescript' {
  const value=process.env.RAISING_RUNTIME ?? 'native';
  if(value!=='native'&&value!=='typescript')throw new Error(`Unsupported RAISING_RUNTIME: ${value}`);
  return value;
}
export const createRun:typeof referenceCreateRun=(...args)=>runtimeBackend()==='native'?nativeCreateRun(...args):referenceCreateRun(...args);
export const dispatchCommand:typeof referenceDispatchCommand=(...args)=>runtimeBackend()==='native'?nativeDispatchCommand(...args):referenceDispatchCommand(...args);
export function projectRuntime(content:ContentSnapshot,state:RunState|null):RuntimeProjection {
  if(runtimeBackend()==='typescript')return {time:state?calendarTime(content,state):null,narrative:state?getNarrativeView(content,state):null,gameplay:getGameplayView(content,state),systems:getSystemsView(content,state)};
  const result=nativeRequest({operation:'view',content,state}) as ({ok:true}&RuntimeProjection)|{ok:false;error:{code:string;message:string;path:string}};
  if(!result.ok)throw new Error(`[${result.error.code}] ${result.error.message} (${result.error.path})`);
  return {time:result.time,narrative:result.narrative,gameplay:result.gameplay,systems:result.systems??null};
}

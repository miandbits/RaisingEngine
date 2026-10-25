import type {Element,ReadonlyDesign} from './model.js';
import type {RunState} from '../core/model.js';
import {resolveScreenElements,type ResolvedElement} from './components.js';
import {presentationActivity,resolvePresentation} from './variants.js';
import {readingHistoryPrefix} from './reading-history.js';

/** Current-state authored reminders only; never a transcript or source-story lookup. */
export const resumeContextBindings:ReadonlySet<string>=new Set(['none','calendar','calendar-year','calendar-month','calendar-day','calendar-weekday','location','user-field','attribute-value','relationship-value','item-count']);
export const resumeContextLimits=Object.freeze({elements:64,utf8Bytes:64*1024});
export function resumeContextEligibility(element:Readonly<Element>):'kind'|'binding'|'selection'|null {
  if(element.kind!=='text')return 'kind';
  if(element.binding==='relationship-value'&&element.valueFormat?.includes('{delta}'))return 'binding';
  if(element.selectionBindings?.length||element.binding==='selected-field'||element.dataBinding)return 'selection';
  return resumeContextBindings.has(element.binding??'none')?null:'binding';
}
export function resumeContextElements(design:ReadonlyDesign):ResolvedElement[]{
  if(!design.resumeContext)return [];
  const {screenId,elementIds}=design.resumeContext;
  const byId=new Map(resolveScreenElements(design,screenId,undefined,{ignoreClipping:true}).map(e=>[e.id,e]));
  return [...new Set(elementIds)].slice(0,resumeContextLimits.elements).map(id=>byId.get(id)).filter((e):e is ResolvedElement=>!!e&&!resumeContextEligibility(e));
}
export function resumeContextPhaseVisible(when:string|undefined,phase:string):boolean {
  return !when||when==='always'||when===phase||when==='planning'&&['planning','ready'].includes(phase)||when==='result'&&['activity-result','period-end'].includes(phase);
}
export interface ResumeContextSnapshot {entries:{elementId:string;text:string;truncated:boolean}[];utf8Bytes:number;omittedEntries:number;truncated:boolean}
export function resolveResumeContext(design:ReadonlyDesign,context:{phase:string;state?:RunState|null;evaluate:(condition:NonNullable<Element['visibleCondition']>)=>boolean;text:(element:Element)=>string}):ResumeContextSnapshot {
  const snapshot:ResumeContextSnapshot={entries:[],utf8Bytes:0,omittedEntries:0,truncated:false};
  for(const source of resumeContextElements(design)){
    const element=resolvePresentation(source,presentationActivity(context.state,source),context.evaluate);
    if(element.hidden||(element.opacity??1)<=0||element.dataEnabled===false||!resumeContextPhaseVisible(element.visibleWhen,context.phase)||element.visibleCondition&&!context.evaluate(element.visibleCondition))continue;
    const text=context.text(element);if(!text.trim())continue;
    const idBytes=new TextEncoder().encode(element.id).length,remaining=resumeContextLimits.utf8Bytes-snapshot.utf8Bytes-idBytes;
    if(remaining<=0){snapshot.omittedEntries++;snapshot.truncated=true;continue;}
    const clipped=readingHistoryPrefix(text,remaining);
    if(!clipped.text){snapshot.omittedEntries++;snapshot.truncated=true;continue;}
    snapshot.entries.push({elementId:element.id,text:clipped.text,truncated:clipped.truncated});snapshot.utf8Bytes+=idBytes+clipped.bytes;snapshot.truncated||=clipped.truncated;
  }
  return snapshot;
}

import {isObject,type JsonObject,type ProjectDocument} from './model.js';
import {objectItems,type SemanticContext} from './context.js';
import {resolveScreenElements} from '../visual/components.js';
import {resumeContextEligibility} from '../visual/resume-context.js';
import type {ReadonlyDesign} from '../visual/model.js';
export function checkResumeContext(ctx:SemanticContext,doc:ProjectDocument,visual:JsonObject):void {
  if(!isObject(visual.resumeContext))return;
  const config=visual.resumeContext,path='/visual/resumeContext';
  if(!objectItems(visual.screens).some(s=>s.id===config.screenId)){ctx.report(doc,path+'/screenId','RESUME_CONTEXT_SCREEN','Choose an existing screen for current reminders.');return;}
  let elements;try{elements=resolveScreenElements(visual as unknown as ReadonlyDesign,String(config.screenId),undefined,{ignoreClipping:true});}catch{return;}
  const byId=new Map(elements.map(e=>[e.id,e])),seen=new Set<string>();
  for(const [i,value]of (Array.isArray(config.elementIds)?config.elementIds:[]).entries()){
    const id=String(value),pointer=path+'/elementIds/'+i,element=byId.get(id);
    if(seen.has(id))ctx.report(doc,pointer,'RESUME_CONTEXT_DUPLICATE','Select each reminder text only once.',[id]);seen.add(id);
    if(!element){ctx.report(doc,pointer,'RESUME_CONTEXT_REFERENCE','This reminder text no longer exists on the selected screen.',[id]);continue;}
    const issue=resumeContextEligibility(element);
    if(issue==='kind')ctx.report(doc,pointer,'RESUME_CONTEXT_KIND','Current reminders can include text elements only.',[id]);
    if(issue==='binding')ctx.report(doc,pointer,'RESUME_CONTEXT_BINDING','Choose static text or a supported current-state binding for this reminder.',[id]);
    if(issue==='selection')ctx.report(doc,pointer,'RESUME_CONTEXT_SELECTION','Current reminders cannot use selected-row or data bindings.',[id]);
  }
}

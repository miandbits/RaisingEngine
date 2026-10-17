import type { Element } from './model.js';

export type UiContext = NonNullable<Element['uiContext']>;
export const contextFields = ['actorId','activityId','characterId','itemId','actionId','locationId','shopId','attributeId'] as const;

/** Bind only explicitly opted-in references. Authored data and explicit owners stay intact. */
export function applyElementContext<T extends Element>(source:T, inherited:UiContext={}):T {
  const element=structuredClone(source), context={...inherited,...element.uiContext};
  if(!Object.keys(context).length&&!element.contextBindings?.length)return element;
  element.uiContext=context;
  const fields=new Set(element.contextBindings??[]);
  const bind=(value:unknown):unknown=>{
    if(Array.isArray(value))return value.map(bind);
    if(!value||typeof value!=='object')return value;
    const result={...value} as Record<string,unknown>;
    if(result.owner&&typeof result.owner==='object'&&(result.owner as {kind?:string}).kind==='actor'&&context.actorId)
      result.owner={kind:'character',characterId:context.actorId};
    for(const key of Object.keys(result)){
      if(key==='owner')continue;
      if(fields.has(key as typeof contextFields[number]))result[key]=context[key as keyof UiContext]??'';
      else result[key]=bind(result[key]);
    }
    return result;
  };
  for(const key of ['dataBinding','visibleCondition','enabledCondition','variants'] as const)
    if(element[key])Object.assign(element,{[key]:bind(element[key])});
  for(const key of fields)Object.assign(element,{[key]:context[key]??''});
  if(['executeActorAction','assignActivity'].includes(element.action??'')&&!element.actorId&&context.actorId)element.actorId=context.actorId;
  if([...fields].some(key=>!context[key])){
    // Missing input must never dispatch a command against a stale authored fallback.
    Object.assign(element,{dataEnabled:false,dataReason:'부품 대상 연결이 필요합니다.',_boundEnabled:false});
  }
  return element;
}

export function usesUiContext(value:unknown):boolean {
  if(!value||typeof value!=='object')return false;
  const object=value as Record<string,unknown>;
  return !!object.uiContext||!!object.contextBindings||object.binding==='context-name'||object.action==='assignActivity'||
    !!object.owner&&typeof object.owner==='object'&&(object.owner as {kind?:string}).kind==='actor'||Object.values(object).some(usesUiContext);
}

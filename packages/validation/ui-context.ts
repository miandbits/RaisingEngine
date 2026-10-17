import type {JsonObject,ProjectDocument} from './model.js';
import {objectItems,type SemanticContext} from './context.js';
import {checkOwner} from './roster.js';

export function checkUiContext(ctx:SemanticContext,doc:ProjectDocument,element:JsonObject,path:string,resolved=false):void {
  const root=doc.data as JsonObject,gameplay=root.gameplay as JsonObject|undefined;
  const context=(element.uiContext??{}) as JsonObject;
  const refs:Record<string,Set<string>>={
    actorId:new Set(objectItems((root.roster as JsonObject|undefined)?.members).map(x=>String(x.characterId))),
    activityId:new Set(ctx.activities.keys()),characterId:new Set(ctx.characters.keys()),attributeId:new Set(ctx.attributes.keys())
  };
  for(const [field,key] of Object.entries({itemId:'items',actionId:'actions',locationId:'locations',shopId:'shops'}))refs[field]=new Set(objectItems(gameplay?.[key]).map(x=>String(x.id)));
  for(const [field,id] of Object.entries(context))if(!refs[field]?.has(String(id)))ctx.report(doc,path+'/uiContext/'+field,'UI_CONTEXT_REFERENCE',`부품에 연결한 대상 '${id}'가 없습니다.`);
  if(element.binding==='context-name'&&!element.contextField)ctx.report(doc,path+'/contextField','UI_CONTEXT_FIELD','이름을 표시할 부품 대상을 선택하세요.');
  if(!resolved)return;
  const needed=new Set((element.contextBindings??[]) as string[]);
  for(const [i,binding] of objectItems(element.selectionBindings).entries())
    if(needed.has(String(binding.property))||binding.property==='actorId'&&context.actorId)
      ctx.report(doc,path+'/selectionBindings/'+i,'UI_CONTEXT_CONFLICT','같은 대상에 부품 연결과 목록 선택 연결을 함께 사용할 수 없습니다. 한 연결을 선택하세요.');
  if(element.binding==='context-name')needed.add(String(element.contextField));
  const scan=(value:unknown,pointer:string):void=>{
    if(!value||typeof value!=='object')return;
    if(!Array.isArray(value)){
      const v=value as JsonObject;
      if(v.owner&&(v.owner as JsonObject).kind==='actor')needed.add('actorId');
      checkOwner(ctx,root,doc,v,pointer);
      if(v.characterId&&v.relationshipId){const character=ctx.characters.get(String(v.characterId))?.[0];if(character&&!objectItems(character.data.relationships).some(r=>r.id===v.relationshipId))ctx.report(doc,pointer+'/relationshipId','UI_CONTEXT_RELATIONSHIP','연결한 대상에게 이 관계 수치가 없습니다.');}
    }
    for(const [key,child] of Object.entries(value))scan(child,pointer+'/'+key);
  };
  for(const key of ['dataBinding','visibleCondition','enabledCondition','variants'])scan(element[key],path+'/'+key);
  if(['executeActorAction','assignActivity'].includes(String(element.action))&&!element.actorId)needed.add('actorId');
  for(const field of needed)if(!context[field])ctx.report(doc,path+'/uiContext/'+field,'UI_CONTEXT_REQUIRED',`'${element.id}'에 ${field} 대상 연결이 필요합니다.`);
}

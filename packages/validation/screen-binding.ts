import type { JsonObject, ProjectDocument } from './model.js';
import { objectItems, type SemanticContext } from './context.js';

export function checkScreenBinding(ctx: SemanticContext, doc: ProjectDocument, element: JsonObject, path: string, allowActor=false): void {
  if(element.binding!=='data-value'&&!element.dataBinding)return;
  const target=element.dataBinding as JsonObject|undefined;
  if(!target){ctx.report(doc,path+'/dataBinding','UI_DATA_BINDING_REQUIRED','표시할 데이터를 선택하세요.');return;}
  if(element.kind!=='text'&&element.kind!=='meter')ctx.report(doc,path+'/kind','UI_DATA_BINDING_KIND','수치 데이터 연결은 텍스트 또는 게이지 요소에 설정하세요.');
  const root=doc.data as JsonObject, roster=root.roster as JsonObject|undefined, owner=target.owner as JsonObject|undefined;
  if(owner){
    if(owner.kind==='actor'&&!allowActor)ctx.report(doc,path+'/dataBinding/owner','UI_DATA_OWNER','부품 배치본에 실행 인물을 연결하거나 특정 인물을 선택하세요.');
    if(owner.kind!=='actor'&&!objectItems(roster?.members).some(m=>m.characterId===owner.characterId))ctx.report(doc,path+'/dataBinding/owner','UI_DATA_OWNER','플레이 가능한 인물 소유자를 선택하세요.');
    if(target.kind==='attribute'&&!((roster?.localAttributeIds??[]) as string[]).includes(String(target.attributeId)))ctx.report(doc,path+'/dataBinding','UI_DATA_SHARED','공유 능력은 공용 데이터로 연결하세요.');
    if(target.kind!=='attribute'&&roster?.stateModel!=='character-v1')ctx.report(doc,path+'/dataBinding','UI_DATA_SHARED','개인 소지품·관계에는 개인 데이터 모델이 필요합니다.');
  }
  if(target.kind==='attribute')ctx.resolve(ctx.attributes,String(target.attributeId),doc,path+'/dataBinding/attributeId','능력·자원');
  else if(target.kind==='relationship'){
    const c=ctx.resolve(ctx.characters,String(target.characterId),doc,path+'/dataBinding/characterId','인물');
    if(c&&!objectItems(c.data.relationships).some(r=>r.id===target.relationshipId))ctx.report(doc,path+'/dataBinding/relationshipId','UI_BINDING_REFERENCE','관계 수치가 없습니다.');
  }else if(!objectItems((root.gameplay as JsonObject|undefined)?.items).some(i=>i.id===target.itemId))ctx.report(doc,path+'/dataBinding/itemId','UI_BINDING_REFERENCE','소지품이 없습니다.');
}

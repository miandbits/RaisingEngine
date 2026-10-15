import type { ProjectDocument, JsonObject } from './model.js';
import { objectItems, type SemanticContext } from './context.js';
import { sourceFields } from '../visual/data-ui.js';

export function componentLeaves(visual:JsonObject,id:string,stack:string[]=[],budget={remaining:10000}):JsonObject[]{
  if(stack.includes(id)||stack.length>=8||--budget.remaining<0)return [];
  const definition=objectItems(visual.components).find(x=>x.id===id);if(!definition)return [];
  return objectItems(definition.elements).flatMap(e=>{if(--budget.remaining<0)return [];return e.kind==='component'?componentLeaves(visual,String(e.componentId),[...stack,id],budget).map(child=>({...child,id:`${e.id}__${child.id}`})):[e];});
}
export function checkUiComposition(ctx:SemanticContext,doc:ProjectDocument,visual:JsonObject):void{
  const components=objectItems(visual.components),screens=objectItems(visual.screens);
  const report=(path:string,code:string,message:string)=>ctx.report(doc,path,code,message);
  let remaining=10000;
  const inspect=(id:string,stack:string[],path:string):void=>{
    if(--remaining<0){if(remaining===-1)report(path,"UI_EXPANSION_LIMIT","부품 탐색 한도를 초과했습니다.");return;}
    if(stack.includes(id)||stack.length>=8){report(path,'UI_COMPONENT_CYCLE','부품 순환 또는 최대 중첩 깊이 8 초과입니다.');return;}
    const c=components.find(c=>c.id===id);if(!c){report(path,'UI_COMPONENT_REFERENCE',`부품 '${id}'가 없습니다.`);return;}
    for(const [i,e] of objectItems(c.elements).entries())if(e.kind==='component')inspect(String(e.componentId),[...stack,id],`/visual/components/${components.indexOf(c)}/elements/${i}/componentId`);
  };
  components.forEach((c,i)=>inspect(String(c.id),[],`/visual/components/${i}`));
  const providers=new Map<string,Record<string,string>>();
  const entries=[...screens.flatMap((s,i)=>objectItems(s.elements).map((e,j)=>({e,path:`/visual/screens/${i}/elements/${j}`}))),...components.flatMap((c,i)=>objectItems(c.elements).map((e,j)=>({e,path:`/visual/components/${i}/elements/${j}`})))];
  for(const {e,path} of entries)if(e.kind==='collection'){
    const source=(e.source??{}) as JsonObject;const fields:Record<string,string>={...(sourceFields[String(source.type)]??{}),id:'text'};
    if(source.type==='table')for(const col of objectItems(objectItems(visual.dataTables).find(t=>t.id===source.tableId)?.columns))fields[String(col.id)]=String(col.type);
    if(e.selectionKey){const key=String(e.selectionKey),prior=providers.get(key);if(prior&&JSON.stringify(Object.entries(prior).sort())!==JSON.stringify(Object.entries(fields).sort()))report(path+'/selectionKey','UI_SELECTION_CONFLICT','같은 선택 키를 제공하는 목록의 필드 구조가 다릅니다.');else providers.set(key,fields);}
    const sort=e.sort as JsonObject|undefined;if(sort&&!fields[String(sort.field)])report(path+'/sort/field','UI_DATA_FIELD','정렬 필드가 원본에 없습니다.');
  }
  for(const {e,path} of entries){
    if((e.flowLayout||e.overflow)&&e.kind!=='group')report(path,'UI_GROUP_REQUIRED','자동 배치와 스크롤 범위는 그룹에 설정하세요.');
    if(e.action==='openModal'||(e.action==='selectRow'&&e.screenId)){
      if(!screens.some(s=>s.id===e.screenId&&s.modal===true))report(path+'/screenId','UI_MODAL_REFERENCE','모달 화면을 선택하세요.');
    }
    if(e.action==='navigate'&&screens.some(s=>s.id===e.screenId&&s.modal===true))report(path+'/screenId','UI_MODAL_NAVIGATION','모달은 모달 열기 행동으로 여세요.');
    if(e.binding==='selected-field'||objectItems(e.selectionBindings).length){
      const fields=providers.get(String(e.selectionKey));
      if(!fields)report(path+'/selectionKey','UI_SELECTION_REFERENCE','선택 대상 키를 제공하는 목록이 없습니다.');
      if(e.binding==='selected-field'&&!fields?.[String(e.selectionField)])report(path+'/selectionField','UI_DATA_FIELD','선택한 대상에서 표시할 필드가 없습니다.');
      for(const [i,b] of objectItems(e.selectionBindings).entries()){const type=fields?.[String(b.field)];if(!type)report(`${path}/selectionBindings/${i}/field`,'UI_DATA_FIELD','선택한 대상에서 전달할 필드가 없습니다.');else if(type!==(b.property==='quantity'?'number':'text'))report(`${path}/selectionBindings/${i}/field`,'UI_BINDING_TYPE','행동 매개변수와 선택 필드의 자료형이 다릅니다.');}
    }
  }
}

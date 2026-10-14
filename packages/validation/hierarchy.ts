import type {JsonObject,ProjectDocument} from './model.js';
import type {SemanticContext} from './context.js';

/** Parents are local to one screen or reusable component; no cross-scope lookup. */
export function checkHierarchy(ctx:SemanticContext,doc:ProjectDocument,rows:JsonObject[],path:string):void {
 const byId=new Map(rows.map((row,index)=>[String(row.id),{row,index}]));
 for(const [i,row]of rows.entries()){
  const p=`${path}/${i}`;
  if(row.clipChildren!==undefined&&row.kind!=='group')ctx.report(doc,p+'/clipChildren','UI_GROUP_REQUIRED','자식 자르기는 그룹에만 설정할 수 있습니다.');
  if(row.parentId===undefined)continue;
  const parent=byId.get(String(row.parentId));
  if(!parent){ctx.report(doc,p+'/parentId','UI_PARENT_REFERENCE','같은 화면 또는 부품 안에서 부모 그룹을 찾을 수 없습니다.');continue;}
  if(parent.row.kind!=='group'){ctx.report(doc,p+'/parentId','UI_PARENT_KIND','부모 요소는 그룹이어야 합니다.');continue;}
  const visited=new Set<string>([String(row.id)]);let current:JsonObject|undefined=row;
  while(current?.parentId){const id:string=String(current.parentId);if(visited.has(id)){ctx.report(doc,p+'/parentId','UI_PARENT_CYCLE','자기 자신이나 자손을 부모로 지정할 수 없습니다.');break;}visited.add(id);if(visited.size>33){ctx.report(doc,p+'/parentId','UI_PARENT_DEPTH','그룹 계층은 최대 32단계까지 지원합니다.');break;}current=byId.get(id)?.row;}
 }
}

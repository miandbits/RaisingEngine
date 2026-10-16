import type { ContentSnapshot, RunState, DeepReadonly } from '../core/model.js';
import type { GameplayView } from '../gameplay/model.js';
import type { Element, ReadonlyDesign } from './model.js';
import { resolveElementHierarchy, expandComponents, type ResolvedElement } from './components.js';
import type { SystemsView } from '../systems/model.js';
import type { BattleSelection } from '../player-service/battle.js';
import {galleryBranchRows,galleryBranchFields} from './gallery-branches.js';
import { galleryRows, gallerySourceFields, type GalleryUiContext } from './gallery-data.js';
import { battleSourceFields, battleCollectionRows } from './battle-data.js';

export type DataValue = string | number | boolean;
export interface DataRow { id: string; values: Record<string, DataValue>; enabled: boolean; reason: string }
export type BoundElement = ResolvedElement & { dataEnabled?: boolean; dataReason?: string };
export interface DataContext { content: ContentSnapshot; state: RunState | null; gameplay: GameplayView | null; systems?: SystemsView | null; gallery?: GalleryUiContext; battleSelection?: BattleSelection | null }
export const sourceFields:Record<string,Record<string,'text'|'number'|'boolean'>>={
  ...battleSourceFields, galleryEntries:gallerySourceFields, gallerySelection:gallerySourceFields,galleryBranches:galleryBranchFields,
  shopOffers:{id:'text',name:'text',shopId:'text',itemId:'text',price:'number',stock:'number',owned:'number',maxQuantity:'number',imageAssetId:'text',description:'text',enabled:'boolean',reason:'text',action:'text',quantity:'number'},
  policyOptions:{id:'text',name:'text',groupName:'text',groupId:'text',optionId:'text',actionId:'text',selected:'boolean',enabled:'boolean',reason:'text',action:'text',costsText:'text'},
  relationships:{id:'text',name:'text',characterId:'text',relationshipId:'text',relationshipName:'text',value:'number',min:'number',max:'number',enabled:'boolean',reason:'text'},
};
function matches(row:DataRow,filter:Element['rowFilter']):boolean {
  if(!filter)return true;const left=row.values[filter.field],right=filter.value;
  switch(filter.operator){case'eq':return left===right;case'ne':return left!==right;case'gt':return typeof left==='number'&&typeof right==='number'&&left>right;case'gte':return typeof left==='number'&&typeof right==='number'&&left>=right;case'lt':return typeof left==='number'&&typeof right==='number'&&left<right;case'lte':return typeof left==='number'&&typeof right==='number'&&left<=right;}
}
/** Definition order and entity IDs survive filtering, sorting in an editor, and save/load. */
export function collectionRows(design:ReadonlyDesign,element:DeepReadonly<Element>,context:DataContext):DataRow[]{
  const rows:DataRow[]=[],source=element.source;if(!source)return rows;
  const add=(id:string,values:Record<string,DataValue>,enabled=true,reason='')=>rows.push({id,values:{...values,id},enabled,reason});
  if(source.type==='galleryBranches'){rows.push(...galleryBranchRows(context.content,context.gallery));
  }else if(source.type==='galleryEntries'||source.type==='gallerySelection'){rows.push(...galleryRows(context.content.project.gallery,context.gallery,source.type==='gallerySelection'));
  }else if(Object.hasOwn(battleSourceFields,source.type)){
    rows.push(...battleCollectionRows(source.type,context));
  }else if(source.type==='table'){
    for(const row of design.dataTables?.find(x=>x.id===source.tableId)?.rows??[])add(row.id,{...row.values});
  }else if(source.type==='shopOffers'){
    for(const shop of context.gameplay?.shops??[])if(!source.shopId||source.shopId===shop.id)for(const offer of shop.offers){const item=context.content.project.gameplay?.items.find(x=>x.id===offer.itemId);add(`${shop.id}--${offer.itemId}`,{name:offer.name,shopId:shop.id,itemId:offer.itemId,price:offer.price,stock:offer.stock??-1,owned:offer.owned,maxQuantity:offer.maxQuantity,imageAssetId:item?.imageAssetId??'',description:item?.description??'',enabled:offer.enabled,reason:offer.reason??'',action:'buy',quantity:1},offer.enabled,offer.reason??'');}
  }else if(source.type==='policyOptions'){
    for(const group of context.gameplay?.policyGroups??[])if(!source.groupId||source.groupId===group.id)for(const option of group.options)add(`${group.id}--${option.id}`,{name:option.name,groupName:group.name,groupId:group.id,optionId:option.id,actionId:option.actionId,selected:option.selected,enabled:option.enabled,reason:option.reason??'',action:'selectPolicy',costsText:option.costs.map(x=>`${context.content.attributes.find(a=>a.id===x.attributeId)?.label??x.attributeId}: ${x.amount}`).join(', ')},option.enabled,option.reason??'');
  }else if(source.type==='relationships'&&context.state){
    for(const character of context.content.characters)if(!source.characterId||source.characterId===character.id)for(const relationship of character.relationships)add(`${character.id}--${relationship.id}`,{name:character.name,characterId:character.id,relationshipId:relationship.id,relationshipName:relationship.label,value:context.state.relationships[character.id]?.[relationship.id]??relationship.initial,min:relationship.min,max:relationship.max,enabled:true,reason:''});
  }
  const filtered=rows.filter(row=>matches(row,element.rowFilter));
  if(element.sort){const {field,direction}=element.sort;filtered.sort((a,b)=>{const x=a.values[field],y=b.values[field];if(x===y)return 0;if(x===undefined)return 1;if(y===undefined)return -1;return (x<y?-1:1)*(direction==='desc'?-1:1);});}
  return filtered.slice(0,1000);
}
/** Returns card-local primitives. Container clipping/scrolling remains the renderer's job. */
export function resolveCollectionRow(design:ReadonlyDesign,collection:DeepReadonly<Element>,row:DataRow,cardWidth:number):BoundElement[]{
  const component=design.components?.find(x=>x.id===collection.componentId);if(!component)return [];
  const children=expandComponents(design,resolveElementHierarchy(component.elements as readonly Element[],component,{width:Math.max(16,cardWidth),height:collection.rowHeight??component.height})).map(child=>{
    const element=structuredClone(child) as BoundElement;let enabled=row.enabled;
    for(const binding of collection.bindings??[]){if(binding.elementId!==child.id)continue;const value=row.values[binding.field];if(value===undefined)continue;
      if(binding.property==='visible'){if(value===false)element.hidden=true;continue;}
      if(binding.property==='enabled'){enabled=enabled&&value===true;continue;}
      const rendered=binding.format?.replaceAll('{value}',String(value));
      (element as unknown as Record<string,unknown>)[binding.property]=binding.property==='text'||binding.property==='label'?rendered??String(value):value;
      if(binding.property==='label')element.text=rendered??String(value);
    }
    if(collection.hidden)element.hidden=true;
    if(collection.locked)element.locked=true;
    if(element.kind==='button'||element.kind==='group'){element.dataEnabled=enabled;element.dataReason=row.reason;}
    return element;
  });
  for(const e of children)for(const id of e._groupIds??[]){const parent=children.find(x=>x.id===id);if(parent?.hidden)e.hidden=true;if(parent?.dataEnabled===false){e.dataEnabled=false;e.dataReason=parent.dataReason??'';}}
  return children.map(e=>({...e,id:`${collection.id}__${row.id}__${e.id}`}));
}

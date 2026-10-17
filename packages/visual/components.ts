import type { Element, ReadonlyDesign, ScreenId } from './model.js';
import { applyElementContext } from './context.js';

export interface ViewportSize {width:number;height:number}

/** Expand the logical canvas to the viewport without stretching its contents. */
export function logicalViewport(design:ReadonlyDesign, width:number, height:number):ViewportSize {
  const reference={width:design.width,height:design.height};
  if(design.viewportMode!=='responsive'||!Number.isFinite(width)||!Number.isFinite(height)||width<=0||height<=0)return reference;
  const aspect=width/height;
  return aspect>=design.width/design.height?{width:Math.round(design.height*aspect),height:design.height}:{width:design.width,height:Math.round(design.width/aspect)};
}

/** Coordinates remain relative to the authored parent size; no source data is mutated. */
export function anchoredElement(source:Readonly<Element>, reference:ViewportSize, actual:ViewportSize):Element {
  const element=structuredClone(source) as Element;
  const dx=actual.width-reference.width,dy=actual.height-reference.height;
  if(element.anchorX==='center')element.x+=dx/2;
  else if(element.anchorX==='right')element.x+=dx;
  else if(element.anchorX==='stretch')element.width=Math.max(16,element.width+dx);
  if(element.anchorY==='center')element.y+=dy/2;
  else if(element.anchorY==='bottom')element.y+=dy;
  else if(element.anchorY==='stretch')element.height=Math.max(16,element.height+dy);
  return element;
}

/** Layout is applied in sibling order; authored coordinates remain untouched. */
export function flowChildren(sources:readonly Readonly<Element>[],parent:Readonly<Element>):Element[]{
  const rows=sources.map(x=>structuredClone(x) as Element),flow=parent.flowLayout;if(!flow)return rows;
  const pad=flow.padding??0,gap=flow.gap??8,cols=flow.direction==='grid'?(flow.columns??2):1;
  let cursor=pad,rowY=pad,rowHeight=0;
  rows.forEach((e,i)=>{if(flow.direction==='row'){e.x=cursor;e.y=pad;cursor+=e.width+gap;}else if(flow.direction==='column'){e.x=pad;e.y=cursor;e.width=Math.max(16,parent.width-pad*2);cursor+=e.height+gap;}else {if(i%cols===0&&i){rowY+=rowHeight+gap;rowHeight=0;}e.width=Math.max(16,(parent.width-pad*2-gap*(cols-1))/cols);e.x=pad+(i%cols)*(e.width+gap);e.y=rowY;rowHeight=Math.max(rowHeight,e.height);}});return rows;
}
export interface ClipRect {x:number;y:number;width:number;height:number}
export type ResolvedElement=Element&{_groupIds?:string[];_clipRect?:ClipRect;dataEnabled?:boolean;dataReason?:string};
export function intersectClip(a:ClipRect|undefined,b:ClipRect):ClipRect {
  if(!a)return {...b};const x=Math.max(a.x,b.x),y=Math.max(a.y,b.y);
  return {x,y,width:Math.max(0,Math.min(a.x+a.width,b.x+b.width)-x),height:Math.max(0,Math.min(a.y+a.height,b.y+b.height)-y)};
}
/** Flatten local group scopes in stable preorder. Invalid drafts omit disconnected/cyclic subtrees. */
export function resolveElementHierarchy(sources:readonly Readonly<Element>[],reference:ViewportSize,actual:ViewportSize,options:{offset?:{x:number;y:number};inherited?:ResolvedElement;idPrefix?:string}={}):ResolvedElement[]{
  const result:ResolvedElement[]=[],byId=new Map(sources.map(x=>[x.id,x])),children=new Map<string,Readonly<Element>[]>();
  for(const source of sources)if(source.parentId&&byId.get(source.parentId)?.kind==='group'){const siblings=children.get(source.parentId)??[];siblings.push(source);children.set(source.parentId,siblings);}
  const visiting=new Set<string>();
  const visit=(source:Readonly<Element>,parent:ResolvedElement|undefined,parentReference:ViewportSize,parentActual:ViewportSize,offset:{x:number;y:number},depth:number)=>{
    if(visiting.has(source.id)||depth>32)return;visiting.add(source.id);
    const element=applyElementContext(anchoredElement(source,parentReference,parentActual),parent?.uiContext) as ResolvedElement;
    element.id=(options.idPrefix??'')+source.id;element.x+=offset.x;element.y+=offset.y;delete element.parentId;
    if(parent){
      element._groupIds=[...(parent._groupIds??[]),...(parent.kind==='group'?[parent.id]:[])];
      if(parent.dataEnabled===false){element.dataEnabled=false;element.dataReason=element.dataReason||parent.dataReason||'';}
      element.opacity=(parent.opacity??1)*(element.opacity??1);if(parent.hidden)element.hidden=true;if(parent.locked)element.locked=true;
      for(const field of ['visibleCondition','enabledCondition'] as const)if(parent[field])element[field]=element[field]?{op:'all',conditions:[structuredClone(parent[field]),structuredClone(element[field])]}:structuredClone(parent[field]);
      const parentWhen=parent.visibleWhen??'always',childWhen=element.visibleWhen??'always';
      if(parentWhen!=='always'){if(childWhen!=='always'&&childWhen!==parentWhen)element.hidden=true;else element.visibleWhen=parentWhen;}
      let clip=parent._clipRect;
      if(parent.kind==='group'&&(parent.clipChildren||parent.overflow==='hidden'||parent.overflow==='scroll'))clip=intersectClip(clip,{x:parent.x,y:parent.y,width:parent.width,height:parent.height});
      if(clip){element._clipRect={...clip};const visible=intersectClip(clip,element);if((visible.width<=0||visible.height<=0)&&!['group','component'].includes(element.kind))element.hidden=true;}
    }
    result.push(element);
    if(source.kind==='group')for(const child of flowChildren(children.get(source.id)??[],element))visit(child,element,element.flowLayout?element:source,element,{x:element.x,y:element.y},depth+1);
    visiting.delete(source.id);
  };
  for(const source of sources)if(!source.parentId)visit(source,options.inherited,reference,actual,options.offset??{x:0,y:0},0);
  return result;
}
/** Expand nested instances with path-local cycle detection and a bounded output. */
export function expandComponents(design:ReadonlyDesign,elements:ResolvedElement[],stack:readonly string[]=[],budget={remaining:10000}):ResolvedElement[]{
  const result:ResolvedElement[]=[];
  for(const instance of elements){if(--budget.remaining<0)throw new Error('UI expansion exceeds 10000 elements');
    if(instance.kind!=='component'){result.push(instance);continue;}
    const id=instance.componentId??'';if(stack.includes(id)||stack.length>=8)throw new Error('UI component cycle or depth exceeds 8');
    const component=design.components?.find(c=>c.id===id);if(!component)continue;
    const children=component.elements.map(child=>({...child,...instance.overrides?.find(o=>o.elementId===child.id)?.values})as Element);
    result.push(...expandComponents(design,resolveElementHierarchy(children,component,instance,{offset:{x:instance.x,y:instance.y},inherited:instance,idPrefix:instance.id+'__'}),[...stack,id],budget));
  }return result;
}
export function resolveScreenElements(design:ReadonlyDesign,screenId:ScreenId,size:ViewportSize={width:design.width,height:design.height}):ResolvedElement[]{
  const screen=design.screens.find(s=>s.id===screenId);if(!screen)return [];
  return expandComponents(design,resolveElementHierarchy(screen.elements as readonly Element[],design,size));
}

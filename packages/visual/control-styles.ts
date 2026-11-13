import type {Element,ReadonlyDesign} from './model.js';
export const controlStyleFields=['fill','color','borderColor','hoverFill','pressedFill','disabledColor'] as const;
export type ControlColors=Partial<Pick<Element,typeof controlStyleFields[number]>>;
export interface ControlStyleReference {styleId:string;pointer:string}
/** Generated controls opt in explicitly; a reading root is only a reading-mode fallback. */
export function resolveControlStyle(design:ReadonlyDesign,controlStyleId:string|undefined,readableActive=false):ControlColors {
  const id=controlStyleId??(readableActive?design.readableLayout?.controlStyleId:undefined);
  const style=id===undefined?undefined:design.styles?.find(style=>style.id===id),colors:ControlColors={};
  // Do not use resolveElementStyle: unrelated default styles, typography and
  // decorative surfaces must never leak into generated-control colors.
  for(const field of controlStyleFields)if(style?.[field]!==undefined)colors[field]=style[field];
  return colors;
}
/** Only declared engine slots are references; extension props remain opaque. */
export function controlStyleReferences(visual:unknown,pointer='/visual'):ControlStyleReference[] {
  const found:ControlStyleReference[]=[];
  const object=(value:unknown):value is Record<string,unknown>=>!!value&&typeof value==='object'&&!Array.isArray(value);
  const inspect=(value:unknown,at:string):void=>{if(object(value)&&typeof value.controlStyleId==='string')found.push({styleId:value.controlStyleId,pointer:at+'/controlStyleId'});};
  const list=(value:unknown,at:string,visit:(value:unknown,at:string)=>void):void=>{if(Array.isArray(value))value.forEach((child,index)=>visit(child,at+'/'+index));};
  const element=(value:unknown,at:string):void=>{
    inspect(value,at);if(!object(value))return;
    for(const field of ['variants','overrides'])list(value[field],at+'/'+field,(entry,path)=>{if(object(entry))inspect(entry.values,path+'/values');});
  };
  if(!object(visual))return found;
  inspect(visual.readableLayout,pointer+'/readableLayout');
  for(const field of ['screens','components'])list(visual[field],pointer+'/'+field,(entry,path)=>{if(object(entry))list(entry.elements,path+'/elements',element);});
  return found;
}

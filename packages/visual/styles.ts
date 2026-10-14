import type {Element,ReadonlyDesign} from './model.js';
export const styleFields=['fontAssetId','fontSize','color','fill','radius','align','padding','borderWidth','borderColor','hoverFill','pressedFill','disabledColor'] as const;
/** Local values always win. Removing a local value restores the live shared style. */
export function resolveElementStyle(design:ReadonlyDesign,element:ReadonlyDesign['screens'][number]['elements'][number]):Element {
 const shared=design.styles?.find(s=>s.id===element.styleId);
 const result=structuredClone(element) as Element;
 for(const field of styleFields){const value=element[field]??shared?.[field]??design.defaultStyle?.[field];if(value!==undefined)Object.assign(result,{[field]:value});}
 return result;
}

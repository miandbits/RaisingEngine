import type { Element, ReadonlyDesign, ScreenId, Surface } from './model.js';
import { resolveScreenElements, type ResolvedElement, type ViewportSize } from './components.js';

export const readableKinds = new Set<Element['kind']>([
  'text','image','dialogue','choices','button','stats','meter','schedule','result','input','saveSlots','loadSlots','playerSettings',
]);
export interface ReadableConfig {maxShortEdge:number;fontSize:number;minTargetSize:number;padding:number;gap:number;maxWidth:number;background:string;controlStyleId?:string;surface?:Surface}
export const readableDefaults:Readonly<ReadableConfig> = Object.freeze({maxShortEdge:600,fontSize:18,minTargetSize:44,padding:16,gap:12,maxWidth:760,background:'#172c29'});
export interface ReadablePreferences {readingLayout:'auto'|'on';textScale:1|1.25|1.5|2}
export interface ResolvedReadableItem {element:ResolvedElement;fontSize:number;imageHeight:number;schedulePresentation?:'compact';planningGuidance?:'first-use';planReuse?:'last-committed'}
export interface ReadableLayout {
  config:ReadableConfig;textScale:ReadablePreferences['textScale'];
  items:ResolvedReadableItem[];actions:ResolvedReadableItem[];actionLayout?:'grouped';primaryAction?:string;
}

/** Preferences are local presentation choices, never part of a campaign save. */
export function normalizeReadablePreferences(value:unknown):ReadablePreferences {
  const saved=value&&typeof value==='object'?value as Record<string,unknown>:{};
  return {readingLayout:saved.readingLayout==='on'?'on':'auto',
    textScale:[1,1.25,1.5,2].includes(Number(saved.textScale))&&typeof saved.textScale==='number'?saved.textScale as ReadablePreferences['textScale']:1};
}

/** Same IDs, conditions and actions; only the presentation order and sizing change. */
export function readableLayoutForScreen(design:ReadonlyDesign,screenId:ScreenId,viewport:ViewportSize,preferences?:unknown):ReadableLayout|null {
  const screen=design.screens.find(s=>s.id===screenId),authored=screen?.readableLayout;
  if(!design.readableLayout||!authored||!Number.isFinite(viewport.width)||!Number.isFinite(viewport.height)||viewport.width<=0||viewport.height<=0)return null;
  const config={...readableDefaults,...design.readableLayout},prefs=normalizeReadablePreferences(preferences);
  if(authored.activation!=='always'&&prefs.readingLayout==='auto'&&Math.min(viewport.width,viewport.height)>config.maxShortEdge)return null;
  const elements=new Map(resolveScreenElements(design,screenId,undefined,{ignoreClipping:true}).map(e=>[e.id,e]));
  const seen=new Set<string>();
  const resolve=(id:string,fontSize?:number,imageHeight?:number,schedulePresentation?:'compact',planningGuidance?:'first-use',planReuse?:'last-committed'):ResolvedReadableItem|null=>{
    const element=elements.get(id);
    if(!element||seen.has(id)||!readableKinds.has(element.kind))return null;
    seen.add(id);
    return {element,fontSize:Math.max(config.fontSize,fontSize??config.fontSize)*prefs.textScale,imageHeight:imageHeight??160,...(element.kind==='schedule'&&schedulePresentation==='compact'?{schedulePresentation,...(planningGuidance==='first-use'?{planningGuidance}:{}),...(planReuse==='last-committed'?{planReuse}:{})}: {})};
  };
  const items=authored.items.map(item=>resolve(item.elementId,item.fontSize,item.imageHeight,item.schedulePresentation,item.planningGuidance,item.planReuse)).filter((item):item is ResolvedReadableItem=>!!item);
  const actions=(authored.actions??[]).map(id=>elements.get(id)?.kind==='button'?resolve(id):null).filter((item):item is ResolvedReadableItem=>!!item);
  return {config,textScale:prefs.textScale,items,actions,...(authored.actionLayout==='grouped'?{actionLayout:authored.actionLayout,...(authored.primaryAction&&actions.some(item=>item.element.id===authored.primaryAction)?{primaryAction:authored.primaryAction}:{})}:{})};
}

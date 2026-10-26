import type {Surface} from './model.js';
export interface SurfaceReference {surface:Surface;pointer:string}
/** Inspect every authored root, including hidden elements, unused styles and variants. */
export function surfaceReferences(visual:unknown,pointer='/visual'):SurfaceReference[] {
  const found:SurfaceReference[]=[];
  const object=(value:unknown):value is Record<string,unknown>=>!!value&&typeof value==='object'&&!Array.isArray(value);
  const inspect=(value:unknown,at:string):void=>{if(object(value)&&object(value.surface))found.push({surface:value.surface as Surface,pointer:at+'/surface'});};
  const list=(value:unknown,at:string,visit:(value:unknown,at:string)=>void):void=>{if(Array.isArray(value))value.forEach((child,index)=>visit(child,at+'/'+index));};
  const element=(value:unknown,at:string):void=>{
    inspect(value,at);if(!object(value))return;
    for(const field of ['variants','overrides'])list(value[field],at+'/'+field,(entry,path)=>{if(object(entry))inspect(entry.values,path+'/values');});
  };
  if(!object(visual))return found;
  inspect(visual.defaultStyle,pointer+'/defaultStyle');inspect(visual.readableLayout,pointer+'/readableLayout');
  list(visual.styles,pointer+'/styles',inspect);
  for(const field of ['screens','components'])list(visual[field],pointer+'/'+field,(entry,path)=>{if(object(entry))list(entry.elements,path+'/elements',element);});
  return found;
}
/** Source-pixel cuts must leave a positive center region, even for hidden/state-specific surfaces. */
export function surfaceInsetsProblem(surface:Surface,dimensions:{width:number;height:number}):string|undefined {
  if(surface.mode!=='nine-slice')return;
  const insets=surface.insets;
  if(!insets||['top','right','bottom','left'].some(side=>!Number.isInteger(insets[side as keyof typeof insets])||insets[side as keyof typeof insets]<0))return 'Surface insets must be nonnegative source-pixel integers.';
  if(insets.left+insets.right>=dimensions.width||insets.top+insets.bottom>=dimensions.height)return `Surface insets must leave a positive center within the ${dimensions.width} × ${dimensions.height}px PNG (left + right < width; top + bottom < height).`;
}

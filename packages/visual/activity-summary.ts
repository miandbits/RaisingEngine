/** Current values only; never predicts a queued activity's execution state. */
export function activityPreview(
  activity: { readonly previewAttributes?: readonly { readonly attributeId:string; readonly label?:string; readonly valueFormat?:string }[] },
  attributes: readonly { readonly id:string; readonly label:string; readonly value:number }[],
): string {
  const details = (activity.previewAttributes ?? []).flatMap(entry => {
    const attribute = attributes.find(value => value.id === entry.attributeId);
    return attribute ? [`${entry.label ?? attribute.label} ${(entry.valueFormat ?? '{value}').split('{value}').join(String(attribute.value))}`] : [];
  });
  return details.length ? `현재 ${details.join(' · ')}` : '';
}


import type {ContentSnapshot} from '../core/model.js';
export interface ActivitySummaryChip {kind:'cost'|'effect';attributeId:string;label:string;value:number;operation:'add'|'set';text:string}
export interface ActivityCardSummary {chips:ActivitySummaryChip[];remaining:number;computed:boolean;other:boolean;conditional:boolean}
/** Literal authored changes only. Never evaluates a formula, combines ordered effects,
 * predicts a final/clamped value, or exposes an internal flag. Costs stay distinct. */
export function activityCardSummary(activity:Partial<Pick<ContentSnapshot['activities'][number],'costs'|'baseEffects'|'successCheck'>>,attributes:readonly {id:string;label:string}[],priority:readonly string[]=[],limit=4):ActivityCardSummary {
 const chips:ActivitySummaryChip[]=[];let computed=false,other=false;
 const add=(kind:'cost'|'effect',id:string,value:number,operation:'add'|'set')=>{const label=attributes.find(a=>a.id===id)?.label??id;chips.push({kind,attributeId:id,label,value,operation,text:label+' '+(operation==='set'?'= ':value>0?'+':'')+value});};
 for(const cost of activity.costs??[])add('cost',cost.attributeId,-cost.amount,'add');
 for(const effect of activity.baseEffects??[]){
  if((effect.op==='add'||effect.op==='set')&&effect.target.kind==='attribute'&&!effect.target.owner){
   if(effect.value.op==='literal')add('effect',effect.target.attributeId,effect.value.value,effect.op);else computed=true;
  }else other=true;
 }
 const order=[...priority,...attributes.map(a=>a.id).filter(id=>!priority.includes(id))];
 const rank=(id:string)=>{const i=order.indexOf(id);return i<0?order.length:i;};
 chips.sort((a,b)=>rank(a.attributeId)-rank(b.attributeId));
 const count=Math.max(0,Math.floor(limit));return {chips:chips.slice(0,count),remaining:Math.max(0,chips.length-count),computed,other,conditional:!!activity.successCheck};
}
/** Move only after Apply, skipping completed/assigned periods. A full draft stays put. */
export function nextUnfilledDraftSlot(slot:number,selected:number,first:number,count:number):number {
 for(let n=1;n<=count;n++){const candidate=(slot+n)%count;if(candidate>=first&&!(selected&(1<<candidate)))return candidate;}return slot;
}

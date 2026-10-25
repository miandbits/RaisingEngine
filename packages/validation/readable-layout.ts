import {isObject, type ProjectDocument, type JsonObject} from './model.js';
import { objectItems, type SemanticContext } from './context.js';
import { resolveScreenElements } from '../visual/components.js';
import { readableKinds } from '../visual/readable-layout.js';
import type { ReadonlyDesign, Element } from '../visual/model.js';

export function checkReadableLayout(ctx:SemanticContext,doc:ProjectDocument,visual:JsonObject):void {
  for(const [index,screen] of objectItems(visual.screens).entries()) {
    const pointer=`/visual/screens/${index}/readableLayout`,layout=screen.readableLayout as JsonObject|undefined;
    if(!layout) {
      if(visual.readableLayout)ctx.report(doc,`/visual/screens/${index}`,'READABLE_SCREEN_MISSING','This screen has no reading layout and will retain its scaled canvas on phones.',[], 'warning');
      continue;
    }
    if(!visual.readableLayout)ctx.report(doc,pointer,'READABLE_CONFIG_REQUIRED','Enable reading layouts in the visual design settings first.');
    let elements:Element[];
    try {elements=resolveScreenElements(visual as unknown as ReadonlyDesign,String(screen.id),undefined,{ignoreClipping:true});}
    catch {continue;} // The ordinary composition validator reports malformed expansion.
    const byId=new Map(elements.map(e=>[e.id,e])),seen=new Set<string>();
    const reference=(id:string,path:string,action=false)=>{
      if(seen.has(id))ctx.report(doc,path,'READABLE_DUPLICATE',`Reading element '${id}' appears more than once.`,[id]);
      seen.add(id);
      const element=byId.get(id);
      if(!element)ctx.report(doc,path,'READABLE_REFERENCE',`Reading element '${id}' does not exist on this screen.`,[id]);
      else if(!readableKinds.has(element.kind))ctx.report(doc,path,'READABLE_KIND',`'${element.kind}' does not support reading layout. Choose a supported element.`,[id]);
      else if(action&&element.kind!=='button')ctx.report(doc,path,'READABLE_ACTION_KIND','Bottom actions must reference buttons.',[id]);
      return element;
    };
    for(const [i,item] of objectItems(layout.items).entries()) {
      const element=reference(String(item.elementId),`${pointer}/items/${i}/elementId`);
      if(item.schedulePresentation!==undefined&&element&&element.kind!=='schedule')ctx.report(doc,`${pointer}/items/${i}/schedulePresentation`,'READABLE_SCHEDULE_KIND','Compact planning applies only to schedule elements.');
      if(item.schedulePresentation==='compact'&&isObject(doc.data)&&doc.data.roster)ctx.report(doc,`${pointer}/items/${i}/schedulePresentation`,'READABLE_COMPACT_ROSTER','Compact planning currently supports single-character schedules. Use the standard presentation for roster schedules.');
      if(item.imageHeight!==undefined&&element&&element.kind!=='image')ctx.report(doc,`${pointer}/items/${i}/imageHeight`,'READABLE_IMAGE_HEIGHT','Image height applies only to image elements.');
    }
    for(const [i,id] of ((layout.actions??[]) as string[]).entries())reference(id,`${pointer}/actions/${i}`,true);
    if(layout.primaryAction!==undefined) {
      const id=String(layout.primaryAction);
      if(layout.actionLayout!=='grouped')ctx.report(doc,`${pointer}/primaryAction`,'READABLE_PRIMARY_GROUP','A primary action requires grouped bottom actions.');
      if(!((layout.actions??[]) as string[]).includes(id))ctx.report(doc,`${pointer}/primaryAction`,'READABLE_PRIMARY_REFERENCE','The primary action must be a button listed in this screen’s bottom actions.',[id]);
    }
    const interactive=new Set(['button','input','schedule','choices','saveSlots','loadSlots','playerSettings','collection','shop','inventory','interactions','locations','actions','policies','services','diets','sell','equipment','quests','expedition','battle','relationship-stages','plugin']);
    for(const element of elements)if(!element.hidden&&interactive.has(element.kind)&&!seen.has(element.id))ctx.report(doc,pointer,'READABLE_CONTROL_OMITTED',`Interactive element '${element.id}' is absent from this screen's reading layout.`,[element.id],'warning');
  }
}

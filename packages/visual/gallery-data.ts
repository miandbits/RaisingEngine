import type { DataRow } from './data-ui.js';
import type { ContentSnapshot } from '../core/model.js';

/** Read-only UI projection. It never evaluates conditions or grants unlocks. */
export interface GalleryUiContext {
  entries?: Record<string,{records:readonly {kind:string;sourceId:string;frames?:readonly {nodeId?:string;choiceId?:string;text?:string}[];replayFrames?:readonly unknown[]}[]}>;
  selectedId?:string; kind?:string; status?:string; category?:string;
  recordIndex?:number; musicPlaying?:boolean;
}
export const gallerySourceFields:Record<string,'text'|'number'|'boolean'>={
  id:'text',entryId:'text',name:'text',kind:'text',category:'text',hint:'text',imageAssetId:'text',
  unlocked:'boolean',locked:'boolean',statusText:'text',recordCount:'number',recordLabel:'text',
  isCG:'boolean',isMusic:'boolean',isNarrative:'boolean',canReplay:'boolean',hasImage:'boolean',
  enabled:'boolean',action:'text',musicLabel:'text',reason:'text',
};
export function galleryRows(config:ContentSnapshot['project']['gallery'],ui:GalleryUiContext={},selection=false):DataRow[]{
  const rows:DataRow[]=[];
  for(const e of config?.entries??[]){
    const records=(ui.entries?.[e.id]?.records??[]).filter(r=>r.kind===e.kind&&r.sourceId===e.sourceId);
    const unlocked=records.length>0;
    if(!unlocked&&e.lockedDisplay==='hidden')continue;
    if(selection){if(e.id!==ui.selectedId||!unlocked)continue;}
    else if((ui.kind&&ui.kind!=='all'&&e.kind!==ui.kind)||(ui.category&&e.category!==ui.category)||(ui.status==='unlocked'&&!unlocked)||(ui.status==='locked'&&unlocked))continue;
    const index=Math.max(0,Math.min(Number.isSafeInteger(ui.recordIndex)?ui.recordIndex!:0,Math.max(0,records.length-1)));
    const imageAssetId=unlocked?(e.kind==='cg'?e.sourceId:e.thumbnailAssetId??''):'';
    rows.push({id:e.id,enabled:true,reason:'',values:{id:e.id,entryId:e.id,name:unlocked?e.title:'???',kind:e.kind,category:e.category,hint:unlocked?'':e.hint,imageAssetId,
      unlocked,locked:!unlocked,statusText:unlocked?'해금됨':'미해금',recordCount:records.length,recordLabel:unlocked?`${index+1} / ${records.length}`:'',
      isCG:unlocked&&e.kind==='cg',isMusic:unlocked&&e.kind==='music',isNarrative:unlocked&&(e.kind==='event'||e.kind==='ending'),
      canReplay:unlocked&&(records[index]?.replayFrames?.length??0)>0,hasImage:imageAssetId!=='',enabled:unlocked,action:'galleryView',musicLabel:ui.musicPlaying?'일시정지':'음악 재생',reason:unlocked?'':e.hint}});
  }
  return rows;
}

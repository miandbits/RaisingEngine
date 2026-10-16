import type {ContentSnapshot} from '../core/model.js';
import type {DataRow} from './data-ui.js';
import type {GalleryUiContext} from './gallery-data.js';

export const galleryBranchFields:Record<string,'text'|'number'|'boolean'>={
  id:'text',entryId:'text',branchId:'text',name:'text',prompt:'text',statusText:'text',
  unlocked:'boolean',locked:'boolean',canReplay:'boolean',recordCount:'number',
  recordIndex:'number',selected:'boolean',action:'text',enabled:'boolean',reason:'text',
};

/** Choices reachable in the authored narrative, never simulated or unlocked by this projection. */
export function galleryBranchRows(content:ContentSnapshot,ui:GalleryUiContext={}):DataRow[]{
  const entry=content.project.gallery?.entries.find(e=>e.id===ui.selectedId);
  if(!entry||!['event','ending'].includes(entry.kind))return [];
  const narrative=(entry.kind==='event'?content.events:content.endings).find(n=>n.id===entry.sourceId);
  if(!narrative)return [];
  const records=(ui.entries?.[entry.id]?.records??[]).filter(r=>r.kind===entry.kind&&r.sourceId===entry.sourceId);
  if(!records.length)return [];
  const reachable=new Set<string>(),queue=[narrative.entryNodeId],nodes=new Map(narrative.nodes.map(n=>[n.id,n]));
  for(let i=0;i<queue.length;i++){
    const id=queue[i]!;if(reachable.has(id))continue;reachable.add(id);const n=nodes.get(id);if(!n)continue;
    if(n.type==='choice')queue.push(...n.choices.map(c=>c.next));
    else if(n.type==='branch')queue.push(n.then,n.else);
    else if('next' in n)queue.push(n.next);
  }
  const rows:DataRow[]=[];
  for(const node of narrative.nodes){
    if(node.type!=='choice'||!reachable.has(node.id))continue;
    const visited=records.flatMap(r=>r.frames??[]).find(f=>f.nodeId===node.id);
    for(const choice of node.choices){
      const matching=records.map((r,i)=>({r,i})).filter(({r})=>r.frames?.some(f=>f.nodeId===node.id&&f.choiceId===choice.id));
      const unlocked=matching.length>0,playable=matching.find(({r})=>(r.replayFrames?.length??0)>0);
      const branchId=`n${node.id.length}-${node.id}-${choice.id}`;
      rows.push({id:branchId,enabled:true,reason:'',values:{id:branchId,entryId:entry.id,branchId,
        name:unlocked?choice.text:'미경험 선택지',prompt:visited?.text??'아직 보지 못한 선택 지점',
        statusText:unlocked?`경험한 분기 · 기록 ${matching.length}개`:'잠금 · 본편에서 이 선택을 경험하세요',
        unlocked,locked:!unlocked,canReplay:!!playable,recordCount:matching.length,recordIndex:playable?.i??-1,
        selected:matching.some(({i})=>i===(ui.recordIndex??0)),action:'galleryBranch',enabled:!!playable,
        reason:unlocked?'원본 연출 기록이 필요합니다':'아직 경험하지 않은 선택 분기입니다'}});
    }
  }
  return rows;
}

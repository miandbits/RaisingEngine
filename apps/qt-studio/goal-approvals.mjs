import {createHash,randomUUID} from 'node:crypto';
import {readFile,writeFile,rename,unlink,realpath,stat,mkdir} from 'node:fs/promises';
import path from 'node:path';
import {canonicalJson} from '../../dist/packages/project-service/snapshot.js';
import {captureGoalBaseline,compareGoalBaseline} from '../../dist/packages/analysis-service/goal-history.js';
import {reviewGoalItems} from './goal-partial.mjs';
import {listGoalBaselines,readGoalBaseline,saveGoalBaseline,runtimeIdentity} from './goal-baselines.mjs';

const hash=value=>createHash('sha256').update(canonicalJson(value)).digest('hex');
async function directory(root){
  const base=await realpath(root);let dir=base;
  for(const part of ['playtests','baselines']){dir=path.join(dir,part);await mkdir(dir,{recursive:true});const rel=path.relative(base,await realpath(dir));if(rel.startsWith('..')||path.isAbsolute(rel))throw Error('기준 승인 경로가 프로젝트 밖입니다.');}
  return dir;
}
async function readLedger(root){
  const dir=await directory(root),file=path.join(dir,'.approvals');
  try{
    const rel=path.relative(dir,await realpath(file));if(rel.startsWith('..')||path.isAbsolute(rel))throw Error('승인 기록 경로 오류');
    if((await stat(file)).size>2*1024*1024)throw Error('승인 기록 크기 제한');
    const data=JSON.parse(await readFile(file,'utf8'));
    if(data.version!==1||!data.active||typeof data.active!=='object'||Array.isArray(data.active)||!Array.isArray(data.history)||Object.values(data.active).some(id=>typeof id!=='string'||!/^[0-9a-f-]{36}$/.test(id)))throw Error('승인 기록 형식 오류');
    if(data.partials!==undefined&&(!data.partials||typeof data.partials!=='object'||Array.isArray(data.partials)||Object.values(data.partials).some(items=>!Array.isArray(items)||items.some(i=>!i||typeof i.id!=='string'||!Array.isArray(i.observations)))))throw Error('부분 승인 기록 형식 오류');
    return data;
  }catch(e){if(e.code==='ENOENT')return {version:1,active:{},history:[]};throw Error('기준 승인 기록을 읽을 수 없습니다: '+e.message);}
}
export async function goalBaselineSelection(root,baselines){
  baselines??=await listGoalBaselines(root);
  const ledger=await readLedger(root),active={...ledger.active};
  for(const b of baselines)if(!b.error&&!Object.hasOwn(active,b.routeId))active[b.routeId]=b.id;
  const partials=ledger.partials??{};
  return {active,partials,history:ledger.history,revision:hash({active,history:ledger.history,...(Object.keys(partials).length?{partials}:{})}),baselines};
}
async function writeLedger(dir,ledger){
  const text=JSON.stringify(ledger);if(Buffer.byteLength(text)>2*1024*1024)throw Error('승인 이력이 2 MiB 제한을 넘습니다.');
  const temp=path.join(dir,'.approval-'+randomUUID()+'.tmp');
  try{await writeFile(temp,text,{flag:'wx'});await rename(temp,path.join(dir,'.approvals'));}finally{await unlink(temp).catch(()=>{});}
}
// Serialize selection changes, retain immutable snapshots, and compare the exact reviewed selection.
export async function changeGoalBaseline(root,{expectedRevision,route,baseline,content,reportId,name,note='',restoreId,restoreReview,itemIds,comparisonRuntime,validate}){
  if(typeof note!=='string'||note.length>2000)throw Error('승인 사유는 2000자 이내로 입력하세요.');
  const dir=await directory(root),lock=path.join(dir,'.review-lock');
  await writeFile(lock,'',{flag:'wx'}).catch(()=>{throw Error('다른 창에서 기준을 변경 중입니다. 결과를 새로 불러오세요.');});
  try{
    const current=await goalBaselineSelection(root);
    if(current.revision!==expectedRevision||current.active[route.id]!==baseline.id)throw Error('검사 이후 활성 기준이 변경되었습니다. 다시 저장해 검사하세요.');
    const previousSelection={baselineId:baseline.id,accepted:current.partials[route.id]??[]};
    let next,accepted=[],kind='approve';
    if(restoreReview){
      const entry=current.history.find(h=>h.id===restoreReview.id&&h.routeId===route.id);
      if(!entry||!['before','after'].includes(restoreReview.side))throw Error('복원할 승인 이력이 없습니다.');
      const snapshot=restoreReview.side==='before'?(entry.previousSelection??{baselineId:entry.previousId,accepted:[]}):(entry.selection??{baselineId:entry.baselineId,accepted:[]});
      next=await readGoalBaseline(root,snapshot.baselineId);accepted=snapshot.accepted;kind='restore';
    }else if(restoreId){next=await readGoalBaseline(root,restoreId);kind='restore';}
    else if(itemIds!==undefined){
      if(!Array.isArray(itemIds)||!itemIds.length||new Set(itemIds).size!==itemIds.length)throw Error('승인할 변경 항목을 중복 없이 선택하세요.');
      const pair=await compareGoalBaseline(content,baseline,comparisonRuntime??await runtimeIdentity(),route);
      if(!pair.right.reached||pair.right.failure||pair.right.unexecuted)throw Error('목표에 도달하지 못한 경로는 부분 승인할 수 없습니다.');
      const items=reviewGoalItems(pair,previousSelection.accepted);
      const chosen=itemIds.map(id=>items.find(i=>i.id===id));
      if(chosen.some(i=>!i||!i.approvable||i.approved))throw Error('선택 항목이 변경되었거나 부분 승인할 수 없습니다. 다시 검사하세요.');
      const merged=new Map(previousSelection.accepted.map(i=>[i.context+':'+i.kind+':'+i.path,i]));for(const i of chosen)merged.set(i.context+':'+i.kind+':'+i.path,i);
      next=baseline;accepted=[...merged.values()];kind='partial';
    }else next=await captureGoalBaseline(content,route,name||'승인 · '+new Date().toISOString(),await runtimeIdentity());
    if(next.trace.route.id!==route.id||next.trace.route.gameId!==baseline.trace.route.gameId)throw Error('다른 프로젝트 또는 경로의 기준입니다.');
    if(kind==='approve'&&(!next.trace.reached||next.trace.failure||next.trace.unexecuted))throw Error('목표에 도달하지 못한 경로는 승인할 수 없습니다. 경로를 수정해 다시 검사하세요.');
    const selection={baselineId:next.id,accepted};
    if(hash(selection)===hash(previousSelection))throw Error('이미 활성화된 기준입니다.');
    await validate();
    if((await goalBaselineSelection(root)).revision!==expectedRevision)throw Error('검사 이후 활성 기준이 변경되었습니다. 다시 검사하세요.');
    // Pin legacy selections before creating a new file: a crash/orphan can never become the active baseline.
    await writeLedger(dir,{version:1,active:current.active,partials:current.partials,history:current.history});
    if(kind==='approve')await saveGoalBaseline(root,next);
    const entry={id:randomUUID(),routeId:route.id,kind,previousId:baseline.id,baselineId:next.id,previousSelection,selection,createdAt:new Date().toISOString(),note,reportId};
    const partials={...current.partials};if(accepted.length)partials[route.id]=accepted;else delete partials[route.id];
    await writeLedger(dir,{version:1,active:{...current.active,[route.id]:next.id},partials,history:[...current.history,entry]});
    return entry;
  }finally{await unlink(lock);}
}
export function goalReviewHistory(selection,routeId){
  const current={baselineId:selection.active[routeId],accepted:selection.partials[routeId]??[]},seen=new Set([hash(current)]),out=[];
  for(const entry of [...selection.history].reverse().filter(h=>h.routeId===routeId))for(const side of ['after','before']){
    const snapshot=side==='before'?(entry.previousSelection??{baselineId:entry.previousId,accepted:[]}):(entry.selection??{baselineId:entry.baselineId,accepted:[]});
    const key=hash(snapshot);if(seen.has(key))continue;seen.add(key);
    out.push({id:entry.id,side,createdAt:entry.createdAt,note:entry.note,acceptedCount:snapshot.accepted.length,baselineId:snapshot.baselineId});
  }
  return out;
}

import {compareReviewedBaseline} from './goal-reviewed.mjs';
import {reviewGoalItems,applyGoalPartial} from './goal-partial.mjs';
import {goalBaselineSelection,goalReviewHistory} from './goal-approvals.mjs';
import {createHash,randomUUID} from 'node:crypto';
import {mkdir,realpath,readdir,readFile,writeFile,rename,unlink,stat} from 'node:fs/promises';
import path from 'node:path';
import {listGoalRoutes} from './goal-routes.mjs';
import {listGoalBaselines,readGoalBaseline,runtimeIdentity} from './goal-baselines.mjs';
import {compareGoalBaseline,historicalStep} from '../../dist/packages/analysis-service/goal-history.js';
import {routeTarget} from '../../dist/packages/analysis-service/goal-routes.js';
import {canonicalJson} from '../../dist/packages/project-service/snapshot.js';

export const routeDigest=route=>createHash('sha256').update(canonicalJson(route)).digest('hex');
const validId=id=>typeof id==='string'&&/^[0-9a-f-]{36}$/.test(id);
// Full, exact observations drive partial approval. Long campaigns can exceed
// the former 8 MiB summary limit; keep the same finite budget as each baseline.
export const MAX_GOAL_IMPACT_BYTES=64*1024*1024;
const same=(a,b)=>canonicalJson(a??null)===canonicalJson(b??null);
function inside(root,file){const p=path.relative(root,file);if(p.startsWith('..')||path.isAbsolute(p))throw Error('프로젝트 밖의 검사 결과는 열 수 없습니다.');}
async function folder(root){const base=await realpath(root),dir=path.join(base,'.studio');await mkdir(dir,{recursive:true});inside(base,await realpath(dir));return dir;}
async function bounded(file){if((await stat(file)).size>MAX_GOAL_IMPACT_BYTES)throw Error('저장 비교 결과가 64 MiB 제한을 넘었습니다.');const bytes=await readFile(file);if(bytes.length>MAX_GOAL_IMPACT_BYTES)throw Error('저장 비교 결과가 64 MiB 제한을 넘었습니다.');return JSON.parse(bytes.toString('utf8'));}
export async function readGoalImpact(root,id){
  if(!validId(id))throw Error('잘못된 저장 비교 결과 ID입니다.');
  const dir=await folder(root),file=path.join(dir,'goal-impact-'+id+'.json');inside(dir,await realpath(file));const report=await bounded(file);
  if(report.version!==1||report.id!==id||!Array.isArray(report.rows))throw Error('지원하지 않거나 손상된 저장 비교 결과입니다.');return {...report,archived:true};
}
export async function archiveGoalImpacts(root,report){
  const dir=await folder(root),file=path.join(dir,'goal-impact-'+report.id+'.json');
  const text=JSON.stringify(report);if(Buffer.byteLength(text)>MAX_GOAL_IMPACT_BYTES)throw Error('저장 비교 결과가 64 MiB를 넘었습니다. 경로를 나누어 검사하세요.');
  const temp=path.join(dir,'.impact-'+randomUUID()+'.tmp');
  try{await writeFile(temp,text,{flag:'wx'});await rename(temp,file);}finally{await unlink(temp).catch(()=>{});}
  return {...report,file,archived:true};
}
export async function latestGoalImpact(root,content){
  const dir=await folder(root),files=(await readdir(dir)).filter(f=>/^goal-impact-[0-9a-f-]{36}\.json$/.test(f));
  const entries=await Promise.all(files.map(async f=>({file:f,time:(await stat(path.join(dir,f))).mtimeMs})));entries.sort((a,b)=>b.time-a.time||b.file.localeCompare(a.file));
  if(!entries.length)return null;
  const report=await readGoalImpact(root,entries[0].file.slice(12,-5)),routes=await listGoalRoutes(root);
  const digest=routeDigest(routes.map(e=>e.route??{file:path.basename(e.file),error:e.error}));
  return {...report,stale:!!report.stale||report.rows.some(r=>['changed','unchanged','failed'].includes(r.status)&&!Array.isArray(r.changeItems))||report.fingerprint!==content.contentFingerprint||report.routesDigest!==digest||report.runtime!==await runtimeIdentity()||report.selectionRevision!==(await goalBaselineSelection(root)).revision};
}
function summarize(pair){
  const definitions=Array.isArray(pair.content.attributes)?pair.content.attributes:(pair.content.attributes?.attributes??[]);
  const labels=new Map(definitions.map(a=>[a.id,a.label]));
  const flags={goalFailed:!pair.right.reached,executionFailed:pair.right.steps.some(s=>!s.ok),stateChanged:pair.firstStateDifference!==null,costChanged:false,effectsChanged:false,conditionsChanged:false,runtimeChanged:pair.runtime!==pair.baseline.runtime,routeChanged:pair.firstCommandDifference!==null||pair.originDiff.length>0||!same(routeTarget(pair.left.route),routeTarget(pair.right.route))};
  const values=new Map(),rules=new Map();let costStep=null,effectStep=null,conditionStep=null;
  for(let i=0;i<Math.max(pair.left.steps.length,pair.right.steps.length);i++){
    const a=Math.min(i,pair.left.steps.length-1),b=Math.min(i,pair.right.steps.length-1),d=historicalStep(pair,a,b);
    // Do not fabricate corresponding current states past the command that failed.
    if(i<pair.left.steps.length&&i<pair.right.steps.length)for(const diff of d.differences){
      if(/\/attributes\/|\/inventory\/|\/relationships\/|\/actionPoints(?:\/|$)/.test(diff.path)&&!values.has(diff.path))values.set(diff.path,{...diff,label:labels.get(diff.path.split('/').at(-1))??diff.path,step:i});
    }
    for(const change of d.history.relatedChanges){rules.set(change.path,change);if(change.category==='비용'){flags.costChanged=true;costStep??=i;}if(change.category==='효과'){flags.effectsChanged=true;effectStep??=i;}if(change.category==='조건'){flags.conditionsChanged=true;conditionStep??=i;}}
    const left=d.left,right=d.right;
    if(i<pair.left.steps.length&&i<pair.right.steps.length){
      if(!same(left.conditions,right.conditions)||!same(left.targetAfter,right.targetAfter)){flags.conditionsChanged=true;conditionStep??=i;}
      for(const [kind,flag] of [['cost','costChanged'],['effect','effectsChanged'],['condition','conditionsChanged']]){
        const select=detail=>detail.checks.filter(c=>c.kind===kind).map(c=>kind==='cost'?{path:c.path,amount:c.amount,charged:typeof c.before==='number'&&typeof c.after==='number'?c.before-c.after:null,status:c.status}:c);
        if(!same(select(left),select(right))){flags[flag]=true;if(kind==='cost')costStep??=i;else if(kind==='effect')effectStep??=i;else conditionStep??=i;}
      }
    }
  }
  const points=[pair.firstStateDifference,pair.firstCheckDifference,pair.firstCommandDifference,costStep,effectStep,conditionStep].filter(v=>v!==null);
  const firstDifference=points.length?Math.min(...points):null;
  const changed=Object.entries(flags).some(([key,v])=>!['goalFailed','executionFailed'].includes(key)&&v);
  return {status:flags.goalFailed||flags.executionFailed?'failed':changed?'changed':'unchanged',flags,firstDifference,firstStateDifference:pair.firstStateDifference,firstCheckDifference:pair.firstCheckDifference,firstCommandDifference:pair.firstCommandDifference,
    beforeReached:pair.left.reached,afterReached:pair.right.reached,failure:pair.right.failure,unexecuted:pair.right.unexecuted,
    valueChanges:[...values.values()].slice(0,40),valueChangeCount:values.size,relatedRuleChanges:[...rules.values()].slice(0,40),ruleChangeCount:rules.size};
}
export async function checkGoalImpacts(root,content,revision,{timeoutMs=60000,runtimePolicy='strict',capturedRoutes}={}){
  if(!['strict','verify-replay'].includes(runtimePolicy))throw Error('지원하지 않는 런타임 검사 정책입니다.');
  const started=Date.now(),routes=capturedRoutes??await listGoalRoutes(root),baselines=await listGoalBaselines(root),runtime=await runtimeIdentity(),selection=await goalBaselineSelection(root,baselines);
  const report={version:1,runtimePolicy,id:randomUUID(),createdAt:new Date().toISOString(),fingerprint:content.contentFingerprint,testedRevision:revision,runtime,selectionRevision:selection.revision,routesDigest:routeDigest(routes.map(e=>e.route??{file:path.basename(e.file),error:e.error})),rows:[],baselineErrors:baselines.filter(b=>b.error),stale:false};
  for(const entry of routes){
    if(!entry.route){report.rows.push({name:path.basename(entry.file),status:'error',error:entry.error});continue;}
    const route=entry.route,row={routeId:route.id,name:route.name,routeDigest:routeDigest(route)},baselineInfo=baselines.find(b=>b.id===selection.active[route.id]);
    row.reviewHistory=goalReviewHistory(selection,route.id);row.baselineHistory=baselines.filter(b=>!b.error&&b.routeId===route.id);row.approvalHistory=selection.history.filter(h=>h.routeId===route.id);
    if(selection.active[route.id]&&(!baselineInfo||baselineInfo.error)){report.rows.push({...row,status:'error',error:'활성 기준 기록이 없거나 손상되었습니다.'});continue;}
    if(!baselineInfo){report.rows.push({...row,status:'missing-baseline',message:'기준 기록이 없습니다. 수정 전에 기준을 저장하세요.'});continue;}
    row.baselineId=baselineInfo.id;row.baselineName=baselineInfo.name;row.baselineCreatedAt=baselineInfo.createdAt;
    if(Date.now()-started>=timeoutMs){report.rows.push({...row,status:'incomplete',error:'저장 비교 시간 제한에 도달했습니다. 다음 저장에서 다시 검사하세요.'});continue;}
    try{
      const baseline=await readGoalBaseline(root,baselineInfo.id);row.baselineChecksum=baseline.checksum;
      const compared=await compareReviewedBaseline(content,baseline,runtime,route,Math.max(1,timeoutMs-(Date.now()-started)),runtimePolicy);
      row.runtimeVerification=compared.runtimeVerification;row.comparisonRuntime=compared.comparisonRuntime;
      if(!compared.pair){report.rows.push({...row,status:'error',error:'현재 런타임이 원본 기준의 실행 결과를 재현하지 못했습니다.'});continue;}
      const pair=compared.pair;report.rows.push({...row,...applyGoalPartial(summarize(pair),reviewGoalItems(pair,selection.partials[route.id]))});
    }catch(e){report.rows.push({...row,status:/시간|60초/.test(e.message)?'incomplete':'error',error:e.message});}
  }
  report.counts=Object.fromEntries(['failed','changed','unchanged','missing-baseline','error','incomplete'].map(s=>[s,report.rows.filter(r=>r.status===s).length]));
  report.complete=!report.baselineErrors.length&&!report.counts.error&&!report.counts.incomplete&&!report.counts['missing-baseline'];
  report.ok=report.complete&&!report.counts.failed&&!report.counts.changed;
  return report;
}
export async function publishGoalImpacts(editor,content,revision,report){
  editor.current(revision);await editor.assertDiskUnchanged();
  if(report.fingerprint!==content.contentFingerprint)throw Error('저장 비교 콘텐츠가 일치하지 않습니다.');
  if(report.selectionRevision!==(await goalBaselineSelection(editor.root)).revision)throw Error('검사 도중 활성 기준이 변경되었습니다. 다시 검사하세요.');
  return archiveGoalImpacts(editor.root,report);
}
export async function resolveGoalImpact(root,content,id,index){
  const report=await readGoalImpact(root,id);
  if(report.error||report.archiveError||report.archived===false)throw Error('비교 보고서를 저장하지 못했습니다. 다시 저장해 검사하세요.');
  if(report.stale)throw Error('검사 도중 기준 기록이 변경되었습니다. 다시 검사하세요.');
  if(report.fingerprint!==content.contentFingerprint)throw Error('검사 이후 콘텐츠가 변경되었습니다. 다시 저장해 검사한 뒤 그래프를 여세요.');
  if(!Number.isInteger(index)||index<0||index>=report.rows.length)throw Error('잘못된 저장 비교 결과 행입니다.');
  if(report.runtime!==await runtimeIdentity())throw Error('검사 이후 런타임이 변경되었습니다. 다시 저장해 검사하세요.');
  if(report.selectionRevision!==(await goalBaselineSelection(root)).revision)throw Error('검사 이후 활성 기준이 변경되었습니다. 다시 저장해 검사하세요.');
  const row=report.rows[index];if(!['failed','changed','unchanged'].includes(row.status)||!row.baselineId)throw Error('비교 가능한 기준 결과가 없습니다.');
  const route=(await listGoalRoutes(root)).find(e=>e.route?.id===row.routeId)?.route;
  if(!route||routeDigest(route)!==row.routeDigest)throw Error('검사 이후 등록 경로가 변경되었습니다. 다시 저장해 검사하세요.');
  const baseline=await readGoalBaseline(root,row.baselineId);if(baseline.checksum!==row.baselineChecksum)throw Error('기준 기록이 검사 당시와 다릅니다. 다시 검사하세요.');
  return {row,route,baseline,report};
}

// Baseline files are immutable through the editor. Capture their file identities
// without parsing potentially large traces on the save request's critical path.
export async function goalBaselineInputStamp(root){
  const base=await realpath(root),dir=path.join(base,'playtests','baselines');
  let names;try{inside(base,await realpath(dir));names=await readdir(dir);}catch(error){if(error.code==='ENOENT')return routeDigest([]);throw error;}
  const rows=[];
  for(const name of names.filter(n=>n==='.approvals'||/^[0-9a-f-]{36}\.json$/.test(n)).sort()){
    const file=path.join(dir,name);inside(dir,await realpath(file));const value=await stat(file,{bigint:true});
    rows.push([name,String(value.size),String(value.mtimeNs),String(value.ctimeNs),String(value.ino)]);
  }
  return routeDigest(rows);
}

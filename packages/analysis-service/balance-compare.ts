import type {AnalysisJob} from './model.js';
import type {BalanceTermination} from './balance.js';
type Observation={mean:number;min:number;max:number;count:number;completeRuns:number;terminations:Partial<Record<BalanceTermination,number>>};
import {canonicalJson} from '../project-service/snapshot.js';

/** Descriptive report comparison. It neither re-runs commands nor edits a snapshot. */
export function compareBalanceJobs(before:AnalysisJob,after:AnalysisJob){
  const left=before.result.balance?.periodSeries,right=after.result.balance?.periodSeries;
  const identity=(job:AnalysisJob)=>({id:job.id,createdAt:job.createdAt,status:job.status,sourceVersion:job.sourceVersion,fingerprint:job.fingerprint,snapshotHash:job.snapshotHash??null,
    options:structuredClone(job.options),completed:job.result.completed,requested:job.result.requested,stopReason:job.result.stopReason,terminations:job.result.balance?.terminations?structuredClone(job.result.balance.terminations):null});
  const result={version:1 as const,status:'matched-setup' as 'matched-setup'|'changed-setup'|'incomparable'|'not-recorded',before:identity(before),after:identity(after),
    contentChanged:before.fingerprint!==after.fingerprint,snapshotChanged:before.snapshotHash!==after.snapshotHash,reasons:[] as string[],setupChanges:[] as string[],
    assumption:'Matching attribute IDs and kinds denote the same authored quantity; semantic renaming cannot be inferred.',
    statisticalScope:'Descriptive means of observed runs, not a paired effect, causal estimate, optimality or enjoyment measure.',
    attributes:[] as {id:string;label:string;beforeLabel:string;comparable:boolean;reason:string|null;points:{periodIndex:number;before:Observation|null;after:Observation|null;meanDifference:number|null}[]}[]};
  if(!left||!right){result.status='not-recorded';result.reasons.push('PERIOD_SERIES_NOT_RECORDED');return result;}
  if(before.status==='running'||after.status==='running')result.reasons.push('RUNNING_REPORT');
  if(left.version!==right.version||left.method!==right.method||left.runnerVersion!==right.runnerVersion||left.engineVersion!==right.engineVersion)result.reasons.push('MEASUREMENT_VERSION_CHANGED');
  if(canonicalJson(left.axis)!==canonicalJson(right.axis)||left.horizon!==right.horizon||left.stride!==right.stride)result.reasons.push('PERIOD_AXIS_CHANGED');
  if(before.options.target||after.options.target)result.reasons.push('TARGET_SEARCH_NOT_A_RUN_COHORT');
  if(result.reasons.length){result.status='incomparable';return result;}
  const setup=(job:AnalysisJob)=>{const {priorityProfile,workers,...options}=job.options;const {id,name,...policy}=priorityProfile??{};return {...options,workers:workers??1,priorityProfile:priorityProfile?policy:null};};
  const a=setup(before),b=setup(after);
  for(const key of new Set([...Object.keys(a),...Object.keys(b)]))if(canonicalJson(a[key as keyof typeof a]??null)!==canonicalJson(b[key as keyof typeof b]??null))result.setupChanges.push(key);
  if(result.setupChanges.length)result.status='changed-setup';
  if(before.result.completed!==after.result.completed)result.reasons.push('OBSERVED_RUN_COUNTS_DIFFER');
  if(before.result.completed<before.result.requested||after.result.completed<after.result.requested||before.result.stopReason!=='completed'||after.result.stopReason!=='completed')result.reasons.push('INCOMPLETE_RUN_COHORT');
  if(left.omittedAttributeIds.length||right.omittedAttributeIds.length)result.reasons.push('ATTRIBUTE_CAPTURE_LIMIT');
  const ids=[...new Set([...left.attributeIds,...right.attributeIds])].sort();
  for(const id of ids){
    const old=before.result.balance!.attributes[id],current=after.result.balance!.attributes[id];
    const reason=!left.attributeIds.includes(id)||!right.attributeIds.includes(id)?'ATTRIBUTE_NOT_RECORDED':!old||!current||old.kind!==current.kind||old.declaredMin!==current.declaredMin||old.declaredMax!==current.declaredMax?'ATTRIBUTE_DEFINITION_CHANGED':null;
    const periods=[...new Set([...left.points.map(p=>p.periodIndex),...right.points.map(p=>p.periodIndex)])].sort((a,b)=>a-b);
    const points=periods.map(periodIndex=>{const l=left.points.find(p=>p.periodIndex===periodIndex),r=right.points.find(p=>p.periodIndex===periodIndex),lv=l?.attributes[id],rv=r?.attributes[id];
      const before=lv&&lv.count?{mean:lv.sum/lv.count,min:lv.min,max:lv.max,count:lv.count,completeRuns:l!.completeRuns,terminations:structuredClone(l!.terminations)}:null;
      const after=rv&&rv.count?{mean:rv.sum/rv.count,min:rv.min,max:rv.max,count:rv.count,completeRuns:r!.completeRuns,terminations:structuredClone(r!.terminations)}:null;
      return {periodIndex,before,after,meanDifference:!reason&&before&&after?after.mean-before.mean:null};});
    result.attributes.push({id,label:current?.label??old?.label??id,beforeLabel:old?.label??id,comparable:!reason,reason,points});
  }
  if(!result.attributes.some(a=>a.comparable)){result.status='incomparable';result.reasons.push('NO_COMPARABLE_ATTRIBUTES');}
  return result;
}

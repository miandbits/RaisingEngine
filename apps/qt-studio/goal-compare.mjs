import {compareReviewedBaseline} from './goal-reviewed.mjs';
import {goalReviewGraph} from './goal-review-graph.mjs';
import {goalBaselineSelection} from './goal-approvals.mjs';
import {resolveGoalImpact,routeDigest} from './goal-impact.mjs';
import {randomUUID} from 'node:crypto';
import {compareGoalRoutes,comparisonSummary,compareGoalStep} from '../../dist/packages/analysis-service/goal-compare.js';
import {captureGoalBaseline,compareGoalBaseline,historicalSummary,historicalStep} from '../../dist/packages/analysis-service/goal-history.js';
import {readGoalBaseline,saveGoalBaseline,runtimeIdentity} from './goal-baselines.mjs';
import {listGoalRoutes} from './goal-routes.mjs';

export class GoalComparisons {
  current=null;
  async open(editor,searchJobs,content,params){
    const impact=params.impactReportId?await resolveGoalImpact(editor.root,content,params.impactReportId,params.impactRow):null;
    if(params.baselineId||impact){
      this.current=null;
      const baseline=impact?.baseline??await readGoalBaseline(editor.root,params.baselineId);
      const actualRuntime=await runtimeIdentity();
      const compared=await compareReviewedBaseline(content,baseline,actualRuntime,impact?.route,60000,impact?.report.runtimePolicy??'strict');
      if(!compared.pair)throw Error('현재 런타임이 원본 기준을 재현하지 못했습니다. 다시 검사하세요.');
      const pair=compared.pair;
      editor.current(params.revision);await editor.assertDiskUnchanged();
      const selection=impact?await goalBaselineSelection(editor.root):null;
      if(impact&&selection.revision!==impact.report.selectionRevision)throw Error('검사 이후 활성 기준이 변경되었습니다. 다시 검사하세요.');
      const review=impact?{...goalReviewGraph(pair,selection.partials[impact.route.id]),reportId:impact.report.id,rowIndex:params.impactRow,selectionRevision:selection.revision,routeId:impact.route.id,routeDigest:impact.row.routeDigest,guardrails:impact.row.flags,activePartialCount:(selection.partials[impact.route.id]??[]).length,history:impact.row.reviewHistory??[],baselines:impact.row.baselineHistory??[],canApprove:impact.row.status==='changed'&&impact.row.afterReached}:null;
      const id=randomUUID();this.current={id,root:editor.root,revision:params.revision,pair,historical:true,review,actualRuntime};
      return {id,root:editor.root,revision:params.revision,...historicalSummary(pair),...(review?{review:{...review,steps:undefined}}:{}),...(impact?{jumpStep:impact.row.firstDifference??0,impactReportId:params.impactReportId}:{} )};
    }
    const entries=await listGoalRoutes(editor.root),find=id=>{const route=entries.find(e=>e.route?.id===id)?.route;if(!route)throw Error(`등록 경로가 없습니다: ${id}`);return route;};
    let right;
    if(params.jobId){const job=searchJobs.get(editor.root,params.jobId);if(job.status!=='found'||!job.route)throw Error('발견된 탐색 경로가 없습니다.');if(job.fingerprint!==content.contentFingerprint||job.revision!==params.revision)throw Error('탐색 후 초안이 변경되었습니다. 다시 탐색하세요.');right=job.route;}
    else right=find(params.rightId);
    const left=params.leftId?find(params.leftId):null;
    // Release the previous cache before allocating another bounded replay.
    this.current=null;
    const pair=await compareGoalRoutes(content,left,right);
    editor.current(params.revision);await editor.assertDiskUnchanged();
    const id=randomUUID();this.current={id,root:editor.root,revision:params.revision,pair};
    return {id,root:editor.root,revision:params.revision,...comparisonSummary(pair)};
  }
  async capture(editor,content,params){
    const route=(await listGoalRoutes(editor.root)).find(e=>e.route?.id===params.routeId)?.route;
    if(!route)throw Error('기준으로 저장할 등록 경로를 선택하세요.');
    const baseline=await captureGoalBaseline(content,route,params.name,await runtimeIdentity());
    editor.current(params.revision);await editor.assertDiskUnchanged();
    return saveGoalBaseline(editor.root,baseline);
  }
  async step(editor,params){
    const cache=this.current;if(!cache||cache.id!==params.comparisonId||cache.root!==editor.root)throw Error('비교 스냅샷이 만료되었습니다. 다시 불러오세요.');
    let stale=cache.revision!==editor.revision;
    if(cache.review){stale ||= cache.review.selectionRevision!==(await goalBaselineSelection(editor.root)).revision;const route=(await listGoalRoutes(editor.root)).find(e=>e.route?.id===cache.review.routeId)?.route;stale ||= !route||routeDigest(route)!==cache.review.routeDigest||await runtimeIdentity()!==(cache.actualRuntime??cache.pair.runtime);try{await editor.assertDiskUnchanged();}catch{stale=true;}}
    return {id:cache.id,revision:cache.revision,stale,...(cache.review?{review:{rows:cache.review.steps[params.rightStep??0]??[],aligned:(params.leftStep??0)===(params.rightStep??0),canApprove:!stale&&cache.review.canApprove}}:{}),fingerprint:cache.pair.content.contentFingerprint,...(cache.historical?historicalStep:compareGoalStep)(cache.pair,params.leftStep??0,params.rightStep??0)};
  }
}

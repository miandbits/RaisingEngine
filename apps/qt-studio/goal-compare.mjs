import {resolveGoalImpact} from './goal-impact.mjs';
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
      const pair=await compareGoalBaseline(content,baseline,await runtimeIdentity(),impact?.route);
      editor.current(params.revision);await editor.assertDiskUnchanged();
      const id=randomUUID();this.current={id,root:editor.root,revision:params.revision,pair,historical:true};
      return {id,root:editor.root,revision:params.revision,...historicalSummary(pair),...(impact?{jumpStep:impact.row.firstDifference??0,impactReportId:params.impactReportId}:{} )};
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
  step(editor,params){
    const cache=this.current;if(!cache||cache.id!==params.comparisonId||cache.root!==editor.root)throw Error('비교 스냅샷이 만료되었습니다. 다시 불러오세요.');
    return {id:cache.id,revision:cache.revision,stale:cache.revision!==editor.revision,fingerprint:cache.pair.content.contentFingerprint,...(cache.historical?historicalStep:compareGoalStep)(cache.pair,params.leftStep??0,params.rightStep??0)};
  }
}

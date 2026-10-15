import {Worker} from 'node:worker_threads';
import {randomUUID} from 'node:crypto';
import {runGoalRoute,routeTarget} from '../../dist/packages/analysis-service/goal-routes.js';
import {validateOptions} from '../../dist/packages/analysis-service/model.js';
import {listGoalRoutes,saveGoalRoute} from './goal-routes.mjs';

export class GoalSearchJobs {
  jobs=new Map();
  public(job){const {worker,timer,route,...view}=job;return structuredClone({...view,commands:route?.commands.length??0,seed:route?.origin.seed,origin:route?.origin,sourceRouteId:route?.sourceRouteId,routePreview:route?.commands.slice(0,100),previewTruncated:(route?.commands.length??0)>100});}
  get(root,id){const job=this.jobs.get(id);if(!job||job.root!==root)throw Error('현재 프로젝트의 탐색 작업이 아닙니다.');return job;}
  async start(root,content,revision,params){
    if([...this.jobs.values()].some(j=>j.status==='running'))throw Error('진행 중인 목표 탐색을 완료하거나 취소하세요.');
    const routes=await listGoalRoutes(root);
    const source=params.sourceId?routes.find(e=>e.route?.id===params.sourceId):null;
    if(params.sourceId&&(!source||source.revision!==params.sourceRevision))throw Error('원본 경로가 변경되었습니다. 목록을 새로고침하세요.');
    const target=source?routeTarget(source.route):params.target;
    if(!target||!['event','ending'].includes(target.kind))throw Error('목표 사건 또는 엔딩을 선택하세요.');
    const options={runs:4,seed:42,maxCommands:1000,maxStates:4000,beamWidth:24,timeoutMs:30000,...params.options,target};
    validateOptions({...options,strategy:'explore'},content);
    if(options.timeoutMs>120000)throw Error('목표 경로 탐색 시간은 최대 120초입니다.');
    while(this.jobs.size>=16){const oldest=[...this.jobs.values()].find(j=>j.status!=='running');if(!oldest)break;this.jobs.delete(oldest.id);}
    const job={id:randomUUID(),root,revision,fingerprint:content.contentFingerprint,target,status:'running',progress:{phase:'searching',attemptedRoutes:0,exploredStates:0},
      sourceId:source?.route.id??null,sourceRevision:source?.revision??null,options};
    const guides=params.useSavedRoutes===false||source?[]:routes.filter(e=>e.route).map(e=>e.route).sort((a,b)=>Number(routeTarget(b).id===target.id&&routeTarget(b).kind===target.kind)-Number(routeTarget(a).id===target.id&&routeTarget(a).kind===target.kind));
    const worker=new Worker(new URL('./goal-search-worker.mjs',import.meta.url),{workerData:{content,options:{...options,guides,...(source?{source:source.route}:{})}},execArgv:[],env:{...process.env,NODE_OPTIONS:''}});
    job.worker=worker;this.jobs.set(job.id,job);
    const finish=(status,error)=>{if(job.status!=='running')return;job.status=status;if(error)job.error=error;clearTimeout(job.timer);void worker.terminate();};
    worker.on('message',message=>{
      if(job.status!=='running')return;
      if(message.progress)job.progress=message.progress;
      if(message.error)finish('error',message.error);
      if(message.result){const {route,...result}=message.result;job.result=result;job.route=route;finish(result.status);}
    });
    worker.on('error',e=>finish('error',e.message));
    worker.on('exit',code=>{if(job.status==='running')finish('error',`탐색 작업이 결과 없이 종료되었습니다 (${code}).`);});
    job.timer=setTimeout(()=>finish('not-found','시간 한도에 도달했습니다. 도달 불가능 판정이 아닙니다.'),options.timeoutMs+2000);job.timer.unref();
    return this.public(job);
  }
  cancel(root,id){const job=this.get(root,id);if(job.status==='running'){job.status='cancelled';clearTimeout(job.timer);void job.worker.terminate();}return this.public(job);}
  async register(editor,content,params){
    const job=this.get(editor.root,params.jobId);
    if(job.status!=='found'||!job.route)throw Error('검증된 탐색 결과가 없습니다.');
    if(job.revision!==params.revision||job.fingerprint!==content.contentFingerprint)throw Error('탐색 후 콘텐츠가 변경되었습니다. 현재 초안에서 다시 탐색하세요.');
    if(params.id===job.sourceId)throw Error('대체 경로는 원본과 다른 ID로 등록하세요.');
    const route={...job.route,id:params.id,name:params.name,checkpointEvery:params.checkpointEvery??10};
    const report=await runGoalRoute(content,route);
    if(!report.ok)throw Error(`등록 전 재검사 실패: ${report.failure?.message}`);
    editor.current(params.revision);await editor.assertDiskUnchanged();
    const entry=await saveGoalRoute(editor.root,route,null);
    return {entry,report};
  }
}

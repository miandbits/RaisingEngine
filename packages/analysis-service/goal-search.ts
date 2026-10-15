import type { ContentSnapshot } from '../core/model.js';
import { createSession, dispatchSession, type RunOrigin } from '../runtime-service/session.js';
import { commandAttempts, runAnalysis } from './runner.js';
import { validateOptions, type AnalysisOptions } from './model.js';
import { routeTarget, targetReached, runGoalRoute, validateGoalRoute, type GoalRoute, type GoalTarget } from './goal-routes.js';

export interface GoalSearchOptions { target: GoalTarget; seed?: number; runs?: number; maxCommands?: number; maxStates?: number; beamWidth?: number; timeoutMs?: number; source?: GoalRoute; guides?: GoalRoute[] }
export interface GoalSearchProgress { phase: string; attemptedRoutes: number; exploredStates: number }
/** Finite witness search. A miss is never a proof of unreachability. */
export async function searchGoalRoute(content: ContentSnapshot, input: GoalSearchOptions,
  hooks: { cancelled?: () => boolean; progress?: (value: GoalSearchProgress) => void } = {}) {
  if (!input.target || !['ending','event'].includes(input.target.kind)) throw Error('목표 엔딩 또는 사건을 선택하세요.');
  const options: AnalysisOptions = validateOptions({ runs: input.runs ?? 4, seed: input.seed ?? 42, strategy:'explore',
    maxCommands:input.maxCommands ?? 1000, maxStates:input.maxStates ?? 4000, beamWidth:input.beamWidth ?? 24, timeoutMs:input.timeoutMs ?? 30000, target:input.target }, content);
  const source = input.source ? validateGoalRoute(input.source) : undefined;
  if (source && (source.gameId !== content.project.gameId || routeTarget(source).kind !== input.target.kind || routeTarget(source).id !== input.target.id)) throw Error('원본 경로와 탐색 목표가 다릅니다.');
  const started=Date.now(), progress:GoalSearchProgress={phase:'searching',attemptedRoutes:0,exploredStates:0};
  const stopped=()=>hooks.cancelled?.() || Date.now()-started>=options.timeoutMs;
  const pulse=async()=>{hooks.progress?.({...progress});await new Promise<void>(r=>setImmediate(r));};
  const candidate=(commands:unknown[],origin:RunOrigin,sourceRouteId=source?.id):GoalRoute=>({version:1,id:'found-route',name:'자동 탐색 경로',gameId:content.project.gameId,
    recordedFingerprint:content.contentFingerprint,origin,commands,target:input.target,checkpoints:[],checkpointEvery:10,...(sourceRouteId?{sourceRouteId}:{})});
  const verify=async(route:GoalRoute)=>{
    if(stopped())return null;
    progress.phase='verifying';await pulse();
    const report=await runGoalRoute(content,route,{cancelled:()=>!!hooks.cancelled?.(),timeoutMs:Math.max(1,options.timeoutMs-(Date.now()-started))});
    return report.ok&&!stopped()?{status:'found' as const,route,report,...progress,elapsedMs:Date.now()-started}:null;
  };
  // Replay candidates from their real origin: recorded states are never transplanted.
  const tryCommands=async(commands:unknown[],template=source)=>{
    if(!template||stopped())return null;
    progress.attemptedRoutes++; const created=createSession(content,template.origin.seed,template.origin);if(!created.ok)return null;
    let session=created.session;const prefix:unknown[]=[];
    for(const command of commands.slice(0,options.maxCommands)){
      if(prefix.length%32===0){await pulse();if(stopped())return null;}
      const next=dispatchSession(content,session,command,session.state.stateRevision);if(!next.ok)return null;
      session=next.session;prefix.push(command);
      if(targetReached(session.state,input.target))return verify(candidate(prefix,template.origin,template.id));
    }
    return null;
  };
  // Known witnesses are hints only: replay their commands against this snapshot first.
  for(const inputGuide of (input.guides??[]).slice(0,32)){
    if(stopped())break;const guide=validateGoalRoute(inputGuide);if(guide.gameId!==content.project.gameId)continue;
    const found=await tryCommands(guide.commands,guide);if(found)return found;
  }
  if(source){
    progress.phase='repairing';const budget=Math.min(128,Math.max(1,Math.floor(options.maxStates!/3)));
    let found=await tryCommands(source.commands);if(found)return found;
    // Global activity substitutions repair balancing changes over several months.
    const schedules=source.commands.filter((c:any)=>c.type==='setSchedule') as {type:string;activityIds:string[]}[];
    for(const activity of content.activities){
      if(stopped()||progress.attemptedRoutes>=budget)break;
      found=await tryCommands(source.commands.map((c:any)=>c.type==='setSchedule'?{...c,activityIds:c.activityIds.map(()=>activity.id)}:c));if(found)return found;
    }
    // Preserve early prerequisites (e.g. rest before combat), change later schedules together.
    for(let from=1;from<schedules.length;from++)for(const activity of content.activities){
      if(stopped()||progress.attemptedRoutes>=budget)break;let schedule=-1;
      found=await tryCommands(source.commands.map((c:any)=>c.type==='setSchedule'&&++schedule>=from?{...c,activityIds:c.activityIds.map(()=>activity.id)}:c));if(found)return found;
    }
    const oldIds=new Set(schedules.flatMap(c=>c.activityIds??[]));
    for(const oldId of oldIds)for(const activity of content.activities){
      if(stopped()||progress.attemptedRoutes>=budget)break;
      if(activity.id===oldId)continue;
      found=await tryCommands(source.commands.map((c:any)=>c.type==='setSchedule'?{...c,activityIds:c.activityIds.map((id:string)=>id===oldId?activity.id:id)}:c));if(found)return found;
    }
    const created=createSession(content,source.origin.seed,source.origin);
    if(created.ok){let session=created.session;const states=[session];
      for(const command of source.commands.slice(0,options.maxCommands)){if(states.length%32===0){await pulse();if(stopped())break;}const next=dispatchSession(content,session,command,session.state.stateRevision);if(!next.ok)break;session=next.session;states.push(session);}
      const indices=source.commands.map((_,i)=>i).filter(i=>i<states.length).sort((a,b)=>Number((source.commands[b] as any).type==='setSchedule')-Number((source.commands[a] as any).type==='setSchedule')||b-a);
      for(const index of indices){
        if(stopped()||progress.attemptedRoutes>=budget)break;session=states[index]!;
        const current=source.commands[index] as any;
        const substitutions:unknown[][]=[[]]; // A deleted optional action may be removed.
        if(current.type==='setSchedule')for(let slot=0;slot<(current.activityIds??[]).length;slot++)for(const activity of content.activities){const ids=[...current.activityIds];ids[slot]=activity.id;substitutions.push([{...current,activityIds:ids}]);}
        substitutions.push(...commandAttempts(content,session.state));
        for(const commands of substitutions){
          if(stopped()||progress.attemptedRoutes>=budget)break;
          found=await tryCommands([...source.commands.slice(0,index),...commands,...source.commands.slice(index+1)]);if(found)return found;
        }
      }
    }
  }
  let witness:GoalRoute|undefined;progress.phase='searching';
  if(!stopped()){
    const remaining=options.timeoutMs-(Date.now()-started);
    if(remaining>=100)await runAnalysis(content,{...options,timeoutMs:remaining},{cancelled:()=>!!hooks.cancelled?.(),
      progress:r=>{progress.exploredStates=r.exploredStates;hooks.progress?.({...progress});},
      trace:t=>{if(t.outcome==='target')witness=candidate(t.commands,{seed:t.seed,mode:'full',initialOverrides:{attributes:{},flags:{},relationships:{}}});}});
    if(witness){const found=await verify(witness);if(found)return found;}
  }
  return {status:hooks.cancelled?.()?'cancelled' as const:'not-found' as const,...progress,elapsedMs:Date.now()-started,
    message:hooks.cancelled?.()?'탐색을 취소했습니다.':'설정한 시간·상태·명령 한도 안에서 검증된 경로를 찾지 못했습니다. 도달 불가능 판정이 아닙니다.'};
}

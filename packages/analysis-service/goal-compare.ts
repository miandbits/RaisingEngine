import {isDeepStrictEqual} from 'node:util';
import type {ContentSnapshot,RunState} from '../core/model.js';
import {createSession,dispatchSession} from '../runtime-service/session.js';
import {nativeRequest} from '../native-bridge/index.js';
import {canonicalJson} from '../project-service/snapshot.js';
import {stateDiff} from '../rule-debug/index.js';
import {validateGoalRoute,routeTarget,targetReached,type GoalRoute} from './goal-routes.js';

type Row=Record<string,any>;
export interface RouteStep {step:number;command:unknown;label:string;ok:boolean;period:number;phase:string;error?:unknown}
export interface RouteTrace {route:GoalRoute;states:RunState[];steps:RouteStep[];reached:boolean;failure:unknown;unexecuted:number;recordedDetails?:Row[]}
export interface RouteComparison {content:ContentSnapshot;left:RouteTrace|null;right:RouteTrace;firstCommandDifference:number|null;firstStateDifference:number|null;sharedThrough:number;originDiff:ReturnType<typeof stateDiff>}
const object=(v:unknown):v is Row=>!!v&&typeof v==='object'&&!Array.isArray(v);
function label(content:ContentSnapshot,raw:unknown):string {
  const c=raw as Row, id=c.actionId??c.activityId??c.nodeId??c.interactionId??c.choiceId??c.locationId??c.screenId??'';
  const names:Row={setSchedule:'일정 배정',executeActivity:'활동 실행',acknowledgeResult:'결과 확인',resolvePeriodEnd:'기간 마무리',advanceNarrative:'대본 진행',choose:'선택',visit:'방문',navigate:'화면 이동',buy:'구매',sell:'판매',executeAction:'행동',moveExpedition:'탐험 이동',tacticsAction:'전투 행동'};
  const activities=(c.activityIds??[]).map((id:string)=>content.activities.find(a=>a.id===id)?.label??id);
  return `${names[c.type]??c.type}${activities.length?' · '+activities.join(' / '):id?' · '+id:''}`;
}
export interface GoalReplayOptions {timeoutMs?:number;cancelled?:()=>boolean;progress?:(value:{executed:number;commands:number})=>void}
async function replay(content:ContentSnapshot,input:unknown,limit:{bytes:number;started:number}&GoalReplayOptions):Promise<RouteTrace>{
  const route=validateGoalRoute(input);if(route.gameId!==content.project.gameId)throw Error('다른 게임의 경로입니다.');
  const target=routeTarget(route),created=createSession(content,route.origin.seed,route.origin);
  if(!created.ok)throw Error(`시작 상태 오류: ${created.error.message}`);
  let session=created.session;const states:RunState[]=[session.state];
  const steps:RouteStep[]=[{step:0,command:null,label:'시작',ok:true,period:session.state.periodIndex,phase:session.state.phase}];
  let reached=targetReached(session.state,target),failure:unknown=null,executed=0;
  const count=(s:RunState)=>{limit.bytes+=Buffer.byteLength(JSON.stringify(s));if(limit.bytes>64*1024*1024)throw Error('경로 상태가 64 MiB를 넘었습니다. 더 짧은 경로로 나누어 비교하세요.');};count(session.state);
  for(const [index,command] of route.commands.entries()){
    if(index%16===0)await new Promise<void>(r=>setImmediate(r));
    if(limit.cancelled?.())throw Object.assign(Error('비교 재생을 취소했습니다.'),{code:'CANCELLED'});
    if(Date.now()-limit.started>(limit.timeoutMs??60000))throw Object.assign(Error('비교 재생 시간 제한을 넘었습니다. 더 짧은 경로로 나누어 비교하세요.'),{code:'TIMEOUT'});
    if(index%16===0)limit.progress?.({executed:index,commands:route.commands.length});
    const result=dispatchSession(content,session,command,session.state.stateRevision);
    if(result.ok){session=result.session;executed++;reached ||= targetReached(session.state,target);}
    else failure={step:index+1,...result.error};
    count(session.state);states.push(session.state);steps.push({step:index+1,command,label:label(content,command),ok:result.ok,period:session.state.periodIndex,phase:session.state.phase,...(!result.ok?{error:result.error}:{})});
    if(!result.ok)break;
  }
  limit.progress?.({executed,commands:route.commands.length});
  if(!failure&&!reached)failure={step:steps.length-1,code:'TARGET_NOT_REACHED',message:`목표 ${target.kind}:${target.id}에 도달하지 못했습니다. 실제 엔딩: ${session.state.endingId??'없음'}`};
  return {route,states,steps,reached,failure,unexecuted:route.commands.length-executed-(failure&&!(steps.at(-1)?.ok)?1:0)};
}
const comparable=(s:RunState)=>({...s,stateRevision:0,contentFingerprint:"",coreVersion:""});
export async function compareGoalRoutes(content:ContentSnapshot,left:unknown|null,right:unknown,options:GoalReplayOptions={}):Promise<RouteComparison>{
  const limit={...options,bytes:0,started:Date.now(),timeoutMs:options.timeoutMs??60000},a=left?await replay(content,left,limit):null,b=await replay(content,right,limit);
  return compareTraces(content,a,b);
}
export function compareTraces(content:ContentSnapshot,a:RouteTrace|null,b:RouteTrace):RouteComparison {
  let command:number|null=null,state:number|null=null,shared=0;
  const originDiff=a?stateDiff(a.route.origin,b.route.origin):[];
  if(a){
    for(let i=0;i<Math.max(a.route.commands.length,b.route.commands.length);i++)if(canonicalJson(a.route.commands[i]??null)!==canonicalJson(b.route.commands[i]??null)){command=i+1;break;}
    for(let i=0;i<Math.max(a.states.length,b.states.length);i++){if(!a.states[i]||!b.states[i]||!isDeepStrictEqual(comparable(a.states[i]!),comparable(b.states[i]!))){state=i;break;}}
    shared=originDiff.length?-1:Math.min(command===null?Infinity:command-1,state===null?Infinity:state-1,a.steps.length-1,b.steps.length-1);
  }
  return {content,left:a,right:b,firstCommandDifference:command,firstStateDifference:state,sharedThrough:shared,originDiff};
}
export function comparisonSummary(pair:RouteComparison){
  const summary=(trace:RouteTrace|null)=>trace?{id:trace.route.id,name:trace.route.name,target:routeTarget(trace.route),origin:trace.route.origin,recordedFingerprint:trace.route.recordedFingerprint,
    commands:trace.route.commands.length,steps:trace.steps,reached:trace.reached,failure:trace.failure,unexecuted:trace.unexecuted}:null;
  return {fingerprint:pair.content.contentFingerprint,left:summary(pair.left),right:summary(pair.right),firstCommandDifference:pair.firstCommandDifference,
    firstStateDifference:pair.firstStateDifference,sharedThrough:pair.sharedThrough,originDiff:pair.originDiff};
}
function conditionRows(content:ContentSnapshot,state:RunState,condition:any,path:string):Row[]{
  if(!object(condition))return [];
  const result=nativeRequest({operation:'evaluate',content,state,condition}) as Row;
  const row:Row={key:path,label:condition.op,value:result.ok?result.value:null,error:result.error??null};
  if(condition.op==='compare')for(const side of ['left','right']){const r=nativeRequest({operation:'evaluate',content,state,expression:condition[side]}) as Row;row[side]=r.ok?r.value:r.error;row.comparator=condition.comparator;}
  if(condition.op==='flag'){row.left=state.flags[condition.flagId];row.right=condition.equals;row.label=`플래그 ${condition.flagId}`;}
  return [row,...(condition.conditions??[]).flatMap((c:any,i:number)=>conditionRows(content,state,c,`${path}/conditions/${i}`)),...(condition.condition?conditionRows(content,state,condition.condition,path+'/condition'):[])];
}
export function routeStepDetails(pair:Pick<RouteComparison,'content'>,trace:RouteTrace|null,index:number){
  if(!trace)return null;
  if(!Number.isSafeInteger(index)||index<0||index>=trace.steps.length)throw Error('재생 단계가 실행된 경로 범위를 벗어났습니다.');
  const step=trace.steps[index]!,after=trace.states[index]!,before=trace.states[Math.max(0,index-1)]!;
  if(trace.recordedDetails)return {step,before,after,changes:stateDiff(before,after),...trace.recordedDetails[index]};
  const target=routeTarget(trace.route),definition=(target.kind==='ending'?pair.content.endings:pair.content.events).find(e=>e.id===target.id);
  const inspected=index?nativeRequest({operation:'inspect',content:pair.content,state:before,command:step.command,expectedRevision:before.stateRevision}) as Row:null;
  const conditions=definition?conditionRows(pair.content,before,definition.requirements,`/${target.kind}/${target.id}/requirements`):[{key:'/target',label:'목표 삭제',value:false}];
  return {step,before,after,changes:stateDiff(before,after),conditions,checks:inspected?.trace??[],availability:inspected?{ok:!!inspected.ok,error:inspected.error??null}:null,
    targetAfter:definition?conditionRows(pair.content,after,definition.requirements,`/${target.kind}/${target.id}/requirements`):[],goalReached:targetReached(after,target),failure:index===trace.steps.length-1?trace.failure:null};
}
export function compareGoalStep(pair:RouteComparison,leftStep:number,rightStep:number){
  const left=routeStepDetails(pair,pair.left,leftStep),right=routeStepDetails(pair,pair.right,rightStep)!;
  return {left,right,differences:left?stateDiff(comparable(left.after),comparable(right.after)):[],samePeriod:!!left&&left.after.periodIndex===right.after.periodIndex};
}

import {createHash,randomUUID} from 'node:crypto';
import type {ContentSnapshot} from '../core/model.js';
import {stateDiff} from '../rule-debug/index.js';
import {validateGoalRoute} from './goal-routes.js';
import {compareGoalRoutes,compareTraces,compareGoalStep,comparisonSummary,routeStepDetails,type RouteComparison,type RouteTrace} from './goal-compare.js';

type Row=Record<string,any>;
export interface GoalBaseline {
  version:1; id:string; name:string; createdAt:string; runtime:string;
  content:ContentSnapshot; trace:RouteTrace; checksum:string;
}
export interface HistoricalComparison extends RouteComparison {
  baseline:GoalBaseline; runtime:string; firstCheckDifference:number|null;
  sourceChanges:ReturnType<typeof stateDiff>; changedSteps:number[];
}
const hash=(value:unknown)=>createHash('sha256').update(JSON.stringify(value)).digest('hex');
const plain=(v:any):v is Row=>!!v&&typeof v==='object'&&!Array.isArray(v);
const MAX_BYTES=64*1024*1024;
export async function captureGoalBaseline(content:ContentSnapshot,route:unknown,name:string,runtime:string):Promise<GoalBaseline>{
  if(typeof name!=='string'||!name.trim()||name.length>120)throw Error('기준 기록 이름은 1~120자여야 합니다.');
  const started=Date.now(),pair=await compareGoalRoutes(content,null,route),trace=pair.right,records:Row[]=[];
  let bytes=Buffer.byteLength(JSON.stringify({content,trace}));
  for(let i=0;i<trace.steps.length;i++){
    if(i%16===0)await new Promise<void>(r=>setImmediate(r));
    if(Date.now()-started>60000)throw Error('기준 기록 생성이 60초를 넘었습니다. 경로를 나누어 저장하세요.');
    const {before,after,changes,step,...record}=routeStepDetails(pair,trace,i)!;
    bytes+=Buffer.byteLength(JSON.stringify(record));
    if(bytes>MAX_BYTES)throw Error('기준 기록이 64 MiB를 넘습니다. 더 짧은 경로로 나누세요.');
    records.push(record);
  }
  // Detach everything from the editor and comparison cache. Old observations are never re-evaluated.
  const payload=JSON.parse(JSON.stringify({version:1,id:randomUUID(),name:name.trim(),createdAt:new Date().toISOString(),runtime,content,trace:{...trace,recordedDetails:records}}));
  return {...payload,checksum:hash(payload)};
}
export function validateGoalBaseline(raw:unknown):GoalBaseline {
  if(!plain(raw)||raw.version!==1||typeof raw.id!=='string'||!/^[0-9a-f-]{36}$/.test(raw.id)||typeof raw.name!=='string'||!raw.name.trim()||raw.name.length>120||typeof raw.createdAt!=='string'||!Number.isFinite(Date.parse(raw.createdAt))||typeof raw.runtime!=='string')throw Error('지원하지 않거나 손상된 기준 기록입니다.');
  const {checksum,...payload}=raw;
  if(typeof checksum!=='string'||hash(payload)!==checksum)throw Error('기준 기록 무결성 검사 실패: 내용이 변경되었거나 손상되었습니다.');
  if(!plain(raw.content)||typeof raw.content.contentFingerprint!=='string'||!plain(raw.trace))throw Error('기준 콘텐츠 또는 실행 기록이 없습니다.');
  const t=raw.trace,route=validateGoalRoute(t.route);
  if(route.gameId!==raw.content.project?.gameId||!Array.isArray(t.steps)||!Array.isArray(t.states)||!Array.isArray(t.recordedDetails)||t.steps.length!==t.states.length||t.steps.length!==t.recordedDetails.length||t.steps.length<1||t.steps.length>route.commands.length+1)throw Error('기준 기록의 경로와 단계 수가 일치하지 않습니다.');
  for(let i=0;i<t.steps.length;i++){
    const s=t.steps[i],state=t.states[i],d=t.recordedDetails[i];
    if(!plain(s)||s.step!==i||typeof s.ok!=='boolean'||!plain(state)||typeof state.phase!=='string'||!Number.isInteger(state.periodIndex)||!plain(d)||!Array.isArray(d.checks)||!Array.isArray(d.conditions)||!Array.isArray(d.targetAfter)||hash(s.command)!==hash(i?route.commands[i-1]:null))throw Error(`기준 기록 ${i}단계가 손상되었습니다.`);
  }
  return raw as unknown as GoalBaseline;
}
// Native rule traces use stable IDs in collection paths. Preserve IDs across reorderings.
function keyed(value:any):any {
  if(Array.isArray(value))return value.length&&value.every(v=>plain(v)&&typeof v.id==='string')?Object.fromEntries(value.map(v=>[v.id,keyed(v)])):Object.fromEntries(value.map((v,i)=>[String(i),keyed(v)]));
  if(plain(value))return Object.fromEntries(Object.entries(value).filter(([k])=>k!=='contentFingerprint').map(([k,v])=>[k,keyed(v)]));
  return value;
}
const observation=(d:Row)=>({checks:d.checks,conditions:d.conditions,targetAfter:d.targetAfter,availability:d.availability,goalReached:d.goalReached});
const overlaps=(a:string,b:string)=>a===b||a.startsWith(b+'/')||b.startsWith(a+'/');
function rulePaths(d:Row|null):string[]{
  if(!d)return [];
  const traced=(d.checks??[]).map((x:Row)=>x.path).filter((x:unknown)=>typeof x==='string'&&x.startsWith('/')) as string[];
  const owners=traced.map(p=>p.match(/^\/(?:activities|events|endings|characters)\/[^/]+/)?.[0]??p.match(/^\/project\/gameplay\/(?:actions|interactions|items|shops)\/[^/]+/)?.[0]).filter((p):p is string=>!!p);
  return [...traced,...owners,
    ...(d.conditions??[]).map((x:Row)=>String(x.key).replace(/^\/ending\//,'/endings/').replace(/^\/event\//,'/events/'))];
}
function ruleCategory(path:string){return /requirements|condition|eligib/i.test(path)?'조건':/cost|price/i.test(path)?'비용':/effect|reward/i.test(path)?'효과':/chance|probability|outcome|success/i.test(path)?'성공 판정':'콘텐츠';}
export async function compareGoalBaseline(content:ContentSnapshot,input:unknown,runtime:string,currentRoute?:unknown,timeoutMs=60000):Promise<HistoricalComparison>{
  const started=Date.now(),baseline=validateGoalBaseline(input);
  if(baseline.trace.route.gameId!==content.project.gameId)throw Error('다른 게임의 기준 기록입니다.');
  const current=await compareGoalRoutes(content,null,currentRoute??baseline.trace.route,{timeoutMs}),pair=compareTraces(content,baseline.trace,current.right);
  const sourceChanges=stateDiff(keyed(baseline.content),keyed(content));
  let firstCheckDifference:number|null=null;const changedSteps:number[]=[];const records:Row[]=[];let recordBytes=0;
  for(let i=0;i<Math.max(pair.left!.steps.length,pair.right.steps.length);i++){
    if(i%16===0)await new Promise<void>(r=>setImmediate(r));
    if(Date.now()-started>timeoutMs)throw Error('기준 비교 검사 시간이 60초를 넘었습니다.');
    const a=i<pair.left!.steps.length?routeStepDetails(pair,pair.left,i):null,b=i<pair.right.steps.length?routeStepDetails(pair,pair.right,i):null;
    if(b){const {before,after,changes,step,...record}=b;recordBytes+=Buffer.byteLength(JSON.stringify(record));if(recordBytes>MAX_BYTES)throw Error('현재 검사 추적이 64 MiB를 넘습니다. 경로를 나누세요.');records.push(record);}
    if(hash(a?observation(a):null)!==hash(b?observation(b):null)){changedSteps.push(i);firstCheckDifference??=i;}
  }
  pair.right.recordedDetails=records;
  pair.sharedThrough=Math.min(pair.sharedThrough,firstCheckDifference===null?Infinity:firstCheckDifference-1);
  return {...pair,baseline,runtime,firstCheckDifference,sourceChanges,changedSteps};
}
export function historicalSummary(pair:HistoricalComparison){
  return {...comparisonSummary(pair),mode:'historical',baseline:{id:pair.baseline.id,name:pair.baseline.name,createdAt:pair.baseline.createdAt,fingerprint:pair.baseline.content.contentFingerprint,runtime:pair.baseline.runtime},
    runtime:pair.runtime,runtimeChanged:pair.runtime!==pair.baseline.runtime,firstCheckDifference:pair.firstCheckDifference,changedSteps:pair.changedSteps,sourceChangeCount:pair.sourceChanges.length};
}
export function historicalStep(pair:HistoricalComparison,leftIndex:number,rightIndex:number){
  const detail=compareGoalStep(pair,leftIndex,rightIndex),a=detail.left,b=detail.right;
  const paths=[...rulePaths(a),...rulePaths(b)];
  const relatedChanges=pair.sourceChanges.filter(c=>paths.some(p=>overlaps(c.path,p))).map(c=>({...c,category:ruleCategory(c.path)}));
  const beforeDifferences=a?stateDiff({...a.before,contentFingerprint:'',stateRevision:0,coreVersion:''},{...b.before,contentFingerprint:'',stateRevision:0,coreVersion:''}):[];
  return {...detail,history:{sameCommand:hash(a?.step.command??null)===hash(b.step.command),sameOrdinal:leftIndex===rightIndex,
    beforeDifferences,relatedChanges,sourceChanges:pair.sourceChanges.map(c=>({...c,category:ruleCategory(c.path)})),
    observationChanges:a?stateDiff(observation(a),observation(b)):[],runtimeChanged:pair.runtime!==pair.baseline.runtime,
    explanation:pair.runtime!==pair.baseline.runtime?'런타임도 변경되었습니다. 콘텐츠 변경만으로 원인을 단정할 수 없습니다.':beforeDifferences.length?'실행 전 상태도 달라졌습니다. 앞 단계에서 누적된 영향과 이 단계의 규칙 변경을 함께 확인하세요.':relatedChanges.length?'실행 전 상태는 같습니다. 아래 규칙 변경과 실제 검사 결과 차이를 확인하세요.':'관련 규칙 변경이 확인되지 않았습니다. 전체 콘텐츠 변경과 실행 상태를 확인하세요.'}};
}

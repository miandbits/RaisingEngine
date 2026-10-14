import type { ContentSnapshot, RunState, Change, NumericTarget } from '../core/model.js';
import type { ReadonlyDesign } from './model.js';

export type TimelineStage = NonNullable<NonNullable<ReadonlyDesign['timelines']>[number]['clips'][number]['stage']>;
export type TimelineLayer = TimelineStage['layers'][number];

/** Presentation-only frame selection: intervals are [start,end), without a separate clock. */
export function timelineLayerFrameIndex(layer: TimelineLayer, elapsedMs: number): number {
  if (!layer.frames.length) return -1;
  const total = layer.frames.reduce((sum, frame) => sum + frame.durationMs, 0);
  if (total <= 0) return 0;
  let time = Number.isFinite(elapsedMs) ? Math.max(0, elapsedMs) : 0;
  if (layer.loop === true) time %= total;
  else if (time >= total) return layer.frames.length - 1;
  for (let i = 0; i < layer.frames.length; i++) {
    if (time < layer.frames[i]!.durationMs) return i;
    time -= layer.frames[i]!.durationMs;
  }
  return layer.frames.length - 1;
}

export interface TimelineClip {
  id: string; clipId: string; durationMs: number; imageAssetId?: string; stage?: TimelineStage;
  text: string; transition: 'none'|'fade'; day: number|null;
}
export interface TimelinePlan {
  resultRevision: number; activityId: string; timelineId: string;
  clips: TimelineClip[]; totalDurationMs: number;
}
export interface TimelineCursor {
  resultRevision: number; activityId: string; timelineId: string;
  clipIndex: number; elapsedMs: number; paused: boolean; speed: 1|4; completed: boolean;
}
export interface PlayerPresentation { version: 1; timeline: TimelineCursor|null }

function targetLabel(content:ContentSnapshot,target:NumericTarget):string {
  if(target.kind==='attribute')return content.attributes.find(x=>x.id===target.attributeId)?.label??target.attributeId;
  if(target.kind==='inventory')return content.project.gameplay?.items.find(x=>x.id===target.itemId)?.name??target.itemId;
  const character=content.characters.find(x=>x.id===target.characterId);
  return `${character?.name??target.characterId} / ${character?.relationships.find(x=>x.id===target.relationshipId)?.label??target.relationshipId}`;
}
/** Aggregate committed changes only. This is never a simulation or a source of effects. */
export function timelineChangesText(content:ContentSnapshot,changes:readonly Change[]):string {
  const values=new Map<string,{label:string,delta?:number,flag?:boolean}>();
  for(const change of changes){
    if(change.kind==='numeric'){
      const target=change.target,key=target.kind==='attribute'?`a:${target.attributeId}`:target.kind==='inventory'?`i:${target.itemId}`:`r:${target.characterId}:${target.relationshipId}`;
      const prior=values.get(key);values.set(key,{label:targetLabel(content,target),delta:(prior?.delta??0)+change.delta});
    }else values.set(`f:${change.flagId}`,{label:content.flags.find(x=>x.id===change.flagId)?.label??change.flagId,flag:change.after});
  }
  return [...values.values()].filter(x=>x.delta===undefined||x.delta!==0).map(x=>x.delta===undefined?`${x.label}: ${x.flag?'true':'false'}`:`${x.label} ${x.delta>=0?'+':''}${x.delta}`).join(', ');
}

/** Compile authored presentation against one already committed activity result. */
export function planActivityTimeline(content:ContentSnapshot,state:RunState):TimelinePlan|null {
  const result=state.lastActivityResult;
  if(state.phase!=='activity-result'||!result)return null;
  const visual=content.project.visual,binding=visual?.activityTimelines?.find(x=>x.activityId===result.activityId);
  if(!binding)return null;
  const timelineId=result.success?(result.success.passed?binding.successTimelineId:binding.failureTimelineId)??binding.timelineId:binding.timelineId;
  const timeline=visual?.timelines?.find(x=>x.id===timelineId);if(!timeline?.clips.length)return null;
  const days:(number|null)[]=[null];
  if(binding.repeat==='logicalDays'){
    const count=content.project.gameplay?.ruleCalendar?.daysPerMonth;
    if(!Number.isInteger(count)||!count||count<3||count>366)return null;
    days.length=0;const slot=result.time.periodIndex%content.project.periodsPerMonth;
    for(let day=(content.project.periodsPerMonth===3?Math.floor(slot*count/3):slot*Math.floor(count/content.project.periodsPerMonth));day<(content.project.periodsPerMonth===3?Math.floor((slot+1)*count/3):(slot===content.project.periodsPerMonth-1?count:(slot+1)*Math.floor(count/content.project.periodsPerMonth)));day++)days.push(day+1);
  }
  const activity=content.activities.find(x=>x.id===result.activityId),summary=timelineChangesText(content,result.changes),clips:TimelineClip[]=[];
  for(const [index,day]of days.entries())for(const clip of timeline.clips){
    const tokens:Record<string,string>={activity:activity?.label??result.activityId,outcome:result.success?(result.success.passed?'성공':'실패'):'완료',day:day===null?'':String(day),changes:index===0?summary:''};
    clips.push({id:`${day===null?'once':`day-${day}`}__${clip.id}`,clipId:clip.id,durationMs:clip.durationMs,...(clip.imageAssetId?{imageAssetId:clip.imageAssetId}:{}),...(clip.stage?{stage:clip.stage}:{}),text:(clip.text??'').replace(/\{(activity|outcome|day|changes)\}/g,(_,key:string)=>tokens[key]!),transition:clip.transition??'none',day});
  }
  return {resultRevision:state.stateRevision,activityId:result.activityId,timelineId,clips,totalDurationMs:clips.reduce((sum,x)=>sum+x.durationMs,0)};
}

export function createTimelineCursor(plan:TimelinePlan):TimelineCursor {
  return {resultRevision:plan.resultRevision,activityId:plan.activityId,timelineId:plan.timelineId,clipIndex:0,elapsedMs:0,paused:false,speed:1,completed:false};
}
export function validateTimelineCursor(plan:TimelinePlan|null,value:unknown):value is TimelineCursor {
  if(!plan||!value||typeof value!=='object'||Array.isArray(value))return false;
  const c=value as TimelineCursor;
  if(Object.keys(c).sort().join(',')!=='activityId,clipIndex,completed,elapsedMs,paused,resultRevision,speed,timelineId')return false;
  if(c.resultRevision!==plan.resultRevision||c.activityId!==plan.activityId||c.timelineId!==plan.timelineId||!Number.isInteger(c.clipIndex)||!Number.isFinite(c.elapsedMs)||c.elapsedMs<0||typeof c.paused!=='boolean'||typeof c.completed!=='boolean'||![1,4].includes(c.speed))return false;
  return c.completed?c.clipIndex===plan.clips.length&&c.elapsedMs===0&&!c.paused:c.clipIndex>=0&&c.clipIndex<plan.clips.length&&c.elapsedMs<plan.clips[c.clipIndex]!.durationMs;
}
export function skipTimeline(plan:TimelinePlan,cursor:TimelineCursor):TimelineCursor {
  if(!validateTimelineCursor(plan,cursor))throw new Error('Invalid timeline cursor');
  return {...cursor,clipIndex:plan.clips.length,elapsedMs:0,paused:false,completed:true};
}
export function advanceTimeline(plan:TimelinePlan,cursor:TimelineCursor,deltaMs:number):TimelineCursor {
  if(!validateTimelineCursor(plan,cursor))throw new Error('Invalid timeline cursor');
  if(!Number.isFinite(deltaMs)||deltaMs<0)throw new Error('Timeline delta must be finite and nonnegative');
  if(cursor.paused||cursor.completed||deltaMs===0)return {...cursor};
  let elapsed=cursor.elapsedMs+deltaMs*cursor.speed,index=cursor.clipIndex;
  while(index<plan.clips.length&&elapsed>=plan.clips[index]!.durationMs){elapsed-=plan.clips[index]!.durationMs;index++;}
  return index===plan.clips.length?skipTimeline(plan,cursor):{...cursor,clipIndex:index,elapsedMs:elapsed};
}

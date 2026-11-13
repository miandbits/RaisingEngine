import type {ContentSnapshot,RunState} from '../core/model.js';
import type {BalanceTermination} from './balance.js';

/** Fixed limits bound both each trace and the aggregate, independently of run count. */
export const PERIOD_SERIES_MAX_POINTS=256;
export const PERIOD_SERIES_MAX_ATTRIBUTES=64;
export interface PeriodSeriesAxis {
  durationMonths:number;periodsPerMonth:number;monthsPerYear:number;startYear:number;startMonth:number;periodLabel:string;
}
interface SeriesContract {
  version:1;method:'last-accepted-state-in-period';engineVersion:string;runnerVersion:'sampled-analysis-v1';
  axis:PeriodSeriesAxis;maxPoints:number;maxAttributes:number;stride:number;horizon:number;
  attributeIds:string[];omittedAttributeIds:string[];
}
export interface TracePeriodPoint {periodIndex:number;commandIndex:number;phase:RunState['phase'];complete:boolean;values:Record<string,number>}
export interface TracePeriodSeries extends SeriesContract {observedPeriods:number;lastPeriodIndex:number;points:TracePeriodPoint[]}
export interface BalancePeriodPoint {
  periodIndex:number;runs:number;completeRuns:number;terminations:Partial<Record<BalanceTermination,number>>;
  attributes:Record<string,{min:number;max:number;sum:number;count:number}>;
}
export interface BalancePeriodSeries extends SeriesContract {observedRuns:number;observedPeriods:number;recordedPeriods:number;points:BalancePeriodPoint[]}
export function initialPeriodSeries(content:ContentSnapshot,state:RunState):TracePeriodSeries {
  const p=content.project,horizon=p.durationMonths*p.periodsPerMonth;
  const ids=content.attributes.map(a=>a.id).sort();
  return {version:1,method:'last-accepted-state-in-period',engineVersion:state.coreVersion,runnerVersion:'sampled-analysis-v1',
    axis:{durationMonths:p.durationMonths,periodsPerMonth:p.periodsPerMonth,monthsPerYear:p.calendar?.monthsPerYear??12,startYear:p.startYear,startMonth:p.startMonth,periodLabel:p.calendar?.periodLabel??''},
    maxPoints:PERIOD_SERIES_MAX_POINTS,maxAttributes:PERIOD_SERIES_MAX_ATTRIBUTES,stride:Math.max(1,Math.ceil(horizon/(PERIOD_SERIES_MAX_POINTS-1))),horizon,
    attributeIds:ids.slice(0,PERIOD_SERIES_MAX_ATTRIBUTES),omittedAttributeIds:ids.slice(PERIOD_SERIES_MAX_ATTRIBUTES),observedPeriods:0,lastPeriodIndex:-1,points:[]};
}
/** Called once per accepted command state, never for rejected/unchosen alternatives. */
export function observePeriodSeries(series:TracePeriodSeries,state:RunState,commandIndex:number):void {
  const period=state.periodIndex;
  if(period!==series.lastPeriodIndex){series.observedPeriods++;series.lastPeriodIndex=period;}
  const previous=series.points.at(-1);
  if(previous&&previous.periodIndex<period)previous.complete=true;
  // An unsampled termination is not moved to the nearest grid point. Overall
  // termination counts remain available; no values are carried into later periods.
  if(period<0||period>series.horizon||period%series.stride!==0&&period!==series.horizon)return;
  const point:TracePeriodPoint={periodIndex:period,commandIndex,phase:state.phase,complete:state.phase==='finished',values:Object.fromEntries(series.attributeIds.map(id=>[id,state.attributes[id]!]))};
  if(previous?.periodIndex===period)series.points[series.points.length-1]=point;
  else series.points.push(point);
}
export function mergePeriodSeries(aggregate:BalancePeriodSeries|undefined,trace:TracePeriodSeries,termination:BalanceTermination):BalancePeriodSeries {
  if(!aggregate){const {points,observedPeriods,lastPeriodIndex,...contract}=trace;aggregate={...structuredClone(contract),observedRuns:0,observedPeriods:0,recordedPeriods:0,points:[]};}
  aggregate.observedRuns++;aggregate.observedPeriods+=trace.observedPeriods;aggregate.recordedPeriods+=trace.points.length;
  for(const point of trace.points){
    let row=aggregate.points.find(p=>p.periodIndex===point.periodIndex);
    if(!row){row={periodIndex:point.periodIndex,runs:0,completeRuns:0,terminations:{},attributes:{}};aggregate.points.push(row);aggregate.points.sort((a,b)=>a.periodIndex-b.periodIndex);}
    row.runs++;row.completeRuns+=Number(point.complete);
    if(point.periodIndex===trace.lastPeriodIndex)row.terminations[termination]=(row.terminations[termination]??0)+1;
    for(const [id,value]of Object.entries(point.values)){
      let metric=Object.hasOwn(row.attributes,id)?row.attributes[id]:undefined;
      if(!metric){metric={min:value,max:value,sum:0,count:0};Object.defineProperty(row.attributes,id,{value:metric,enumerable:true,writable:true,configurable:true});}
      metric.min=Math.min(metric.min,value);metric.max=Math.max(metric.max,value);metric.sum+=value;metric.count++;
    }
  }
  return aggregate;
}

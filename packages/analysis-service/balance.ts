import type {ContentSnapshot,RunState} from '../core/model.js';
import type {PriorityProfile} from './priorities.js';

export type BalanceTermination='finished'|'blocked'|'command-limit'|'time-limit'|'cancelled'|'target'|'error'|'state-limit'|'incomplete';
export interface BalancePosition {commandIndex:number;periodIndex:number}
export interface BalanceWitness extends BalancePosition {run:number}
export interface BalanceAttribute {
  label:string;kind:string;declaredMin:number;declaredMax:number;bandMin?:number;bandMax?:number;
  min:number;max:number;atMin:boolean;atMax:boolean;belowBand:boolean;aboveBand:boolean;
  firstBelow?:BalancePosition;firstAbove?:BalancePosition;
}
export interface TraceBalance {version:1;termination?:BalanceTermination;observedStates:number;attributes:Record<string,BalanceAttribute>}
export interface AggregateBalanceAttribute {
  label:string;kind:string;declaredMin:number;declaredMax:number;bandMin?:number;bandMax?:number;
  min:number;max:number;runsAtMin:number;runsAtMax:number;runsBelowBand:number;runsAboveBand:number;
  firstBelow?:BalanceWitness;firstAbove?:BalanceWitness;
}
export interface AnalysisBalance {version:1;terminations:Partial<Record<BalanceTermination,number>>;observedRuns:number;observedStates:number;attributes:Record<string,AggregateBalanceAttribute>}
export function initialBalance(content:ContentSnapshot,state:RunState,profile?:PriorityProfile):TraceBalance {
  const bands=new Map(profile?.bands.map(band=>[band.attributeId,band])??[]);
  const result:TraceBalance={version:1,observedStates:0,attributes:Object.fromEntries(content.attributes.map(attribute=>{
    const band=bands.get(attribute.id),value=state.attributes[attribute.id]!;
    return [attribute.id,{label:attribute.label,kind:attribute.kind,declaredMin:attribute.min,declaredMax:attribute.max,
      ...(band?.min===undefined?{}:{bandMin:band.min}),...(band?.max===undefined?{}:{bandMax:band.max}),
      min:value,max:value,atMin:false,atMax:false,belowBand:false,aboveBand:false}];
  }))};
  observeBalance(result,state,0);return result;
}
/** Call only for states on the selected trace, including each command in a bundle. */
export function observeBalance(result:TraceBalance,state:RunState,commandIndex:number):void {
  result.observedStates++;
  for(const [id,row] of Object.entries(result.attributes)){
    const value=state.attributes[id]!;row.min=Math.min(row.min,value);row.max=Math.max(row.max,value);
    row.atMin||=value===row.declaredMin;row.atMax||=value===row.declaredMax;
    if(row.bandMin!==undefined&&value<row.bandMin){row.belowBand=true;row.firstBelow??={commandIndex,periodIndex:state.periodIndex};}
    if(row.bandMax!==undefined&&value>row.bandMax){row.aboveBand=true;row.firstAbove??={commandIndex,periodIndex:state.periodIndex};}
  }
}
/** Ordered per-run merge. Rates count runs, never command-density-biased samples. */
export function mergeBalance(result:AnalysisBalance|undefined,trace:TraceBalance,run:number):AnalysisBalance {
  result??={version:1,terminations:{},observedRuns:0,observedStates:0,attributes:{}};
  const termination=trace.termination??'incomplete';result.terminations[termination]=(result.terminations[termination]??0)+1;
  result.observedRuns++;result.observedStates+=trace.observedStates;
  for(const [id,row] of Object.entries(trace.attributes)){
    let aggregate=Object.hasOwn(result.attributes,id)?result.attributes[id]:undefined;
    if(!aggregate){
      aggregate={label:row.label,kind:row.kind,declaredMin:row.declaredMin,declaredMax:row.declaredMax,
        ...(row.bandMin===undefined?{}:{bandMin:row.bandMin}),...(row.bandMax===undefined?{}:{bandMax:row.bandMax}),
        min:row.min,max:row.max,runsAtMin:0,runsAtMax:0,runsBelowBand:0,runsAboveBand:0};
      Object.defineProperty(result.attributes,id,{value:aggregate,enumerable:true,writable:true,configurable:true});
    }
    aggregate.min=Math.min(aggregate.min,row.min);aggregate.max=Math.max(aggregate.max,row.max);
    aggregate.runsAtMin+=Number(row.atMin);aggregate.runsAtMax+=Number(row.atMax);aggregate.runsBelowBand+=Number(row.belowBand);aggregate.runsAboveBand+=Number(row.aboveBand);
    if(row.firstBelow&&!aggregate.firstBelow)aggregate.firstBelow={run,...row.firstBelow};
    if(row.firstAbove&&!aggregate.firstAbove)aggregate.firstAbove={run,...row.firstAbove};
  }
  return result;
}

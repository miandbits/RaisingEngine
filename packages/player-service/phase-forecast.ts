import type { Change, ContentSnapshot, CoreError, RunState } from '../core/model.js';
import { freeze } from '../core/util.js';

export interface PhaseForecastAttribute {
  attributeId:string;
  before:number;
  after:number;
  delta:number;
  /** Signed actual activity costs; usually negative. */
  costs:number;
  /** Actual activity base effects after clamping. */
  effects:number;
  /** Remaining net change from selected policies, diet, recovery and rules. */
  automatic:number;
}
export interface PhaseForecastIssue {
  code:string;
  path:string;
  periodIndex:number;
  stage:'draft'|'activity'|'boundary';
}
export interface PhaseForecastRow {
  slot:number;
  periodIndex:number;
  activityId:string;
  beforeAttributes:Readonly<Record<string,number>>;
  afterActivityAttributes:Readonly<Record<string,number>>;
  afterAttributes:Readonly<Record<string,number>>;
  activityChanges:readonly Change[];
  boundaryChanges:readonly Pick<PhaseForecastAttribute,'attributeId'|'before'|'after'|'delta'>[];
  activityComplete:boolean;
  boundaryComplete:boolean;
}
export interface PhaseForecastProjection {
  basis:'deterministic-prefix'|'activities-only';
  status:'exact'|'projected'|'partial'|'blocked';
  complete:boolean;
  fromPeriodIndex:number;
  /** Inclusive requested horizon, even when only a prefix can be projected. */
  throughPeriodIndex:number;
  lastProjectedPeriodIndex:number|null;
  assumptions:readonly string[];
  rows:readonly PhaseForecastRow[];
  attributes:readonly PhaseForecastAttribute[];
  issues:readonly PhaseForecastIssue[];
}
export type PhaseForecastReply = {ok:true;forecast:PhaseForecastProjection;baseline:PhaseForecastProjection}|{ok:false;error:CoreError};

/** A read-only call to the shared C++ core, never a second rules implementation.
 * A partial result describes only its known prefix. Only a complete baseline
 * reaches phase end, and its assumptions must remain visible alongside values.
 */
export function requestPhaseForecast(content:ContentSnapshot,state:RunState,activityIds:readonly string[],request:(input:unknown)=>unknown):PhaseForecastReply {
  return freeze(request({operation:'forecastPhase',content,state,activityIds}) as PhaseForecastReply);
}

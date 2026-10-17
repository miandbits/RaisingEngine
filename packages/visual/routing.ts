import type { ReadonlyDesign } from './model.js';
import type { RunState } from '../core/model.js';

export interface RoutingCandidate {
  source: 'explicit'|'condition'|'location'|'phase'|'default'|'legacy';
  label: string; screenId: string; screenLabel?: string; matched: boolean; selected: boolean; reason: string; path: string;
}
export interface RoutingDecision { screenId: string; candidates: RoutingCandidate[] }
/** Projection consumes runtime-evaluated predicates; it never changes game state. */
export function explainScreen(design: ReadonlyDesign, state: RunState|null, narrativeType?: string,
  conditions: Readonly<Record<string,boolean>> = {}): RoutingDecision {
  const phase = state?.phase ?? 'title';
  const legacy = phase === 'title' ? 'title' : phase === 'finished' ? 'ending' : phase === 'narrative' ? 'story'
    : phase === 'battle' || phase === 'expedition' ? phase
    : phase === 'planning' || phase === 'ready' ? state?.gameplay?.screenId ?? 'schedule' : 'schedule';
  const routing = design.routing;
  if (!routing) return {screenId:legacy,candidates:[{source:'legacy',label:'기존 자동 연결',screenId:legacy,matched:true,selected:true,reason:'기존 프로젝트의 화면 연결',path:'/visual'}]};
  const candidates:RoutingCandidate[]=[];
  const add=(source:RoutingCandidate['source'],label:string,screenId:string,matched:boolean,reason:string,path:string)=>
    candidates.push({source,label,screenId,screenLabel:design.screens.find(s=>s.id===screenId)?.label??screenId,matched,selected:false,reason,path});
  const valid=(id:string)=>design.screens.some(s=>s.id===id&&!s.modal);
  if(state?.uiScreenId) add('explicit','명시적 화면 이동',state.uiScreenId,valid(state.uiScreenId),'현재 이동 명령이 우선합니다.','/state/uiScreenId');
  const key = phase === 'narrative' ? narrativeType === 'choice' ? 'choice' : 'dialogue' : phase;
  for(const [i,rule] of (routing.conditions??[]).entries()) {
    const inPhase=!rule.phases?.length||rule.phases.some(p=>p===key);
    const predicate=conditions[JSON.stringify(rule.when)];
    add('condition',rule.label,rule.screenId,inPhase&&predicate===true&&valid(rule.screenId),
      !inPhase?'진행 단계가 다릅니다.':predicate===undefined?'조건 평가 결과가 없습니다.':predicate?'조건 충족':'조건 불충족',`/visual/routing/conditions/${i}`);
  }
  if(routing.useLocationScreens&&state?.gameplay?.screenId) {
    const id=state.gameplay.screenId;
    add('location','장소 연결',id,(phase==='planning'||phase==='ready')&&valid(id),'일정 계획·준비에서 장소 화면을 사용합니다.','/visual/routing/useLocationScreens');
  }
  const index=routing.phases.findIndex(r=>r.phase===key);
  if(index>=0)add('phase','진행 단계 연결',routing.phases[index]!.screenId,true,key,`/visual/routing/phases/${index}`);
  add('default','기본 화면',routing.defaultScreenId,true,'상위 연결이 없을 때 사용합니다.','/visual/routing/defaultScreenId');
  const winner=candidates.find(c=>c.matched)!;winner.selected=true;
  for(const c of candidates)if(c.matched&&!c.selected)c.reason+=` · 상위 연결 ‘${winner.label}’이 우선합니다.`;
  return {screenId:winner.screenId,candidates};
}
export function resolveScreen(design: ReadonlyDesign, state: RunState|null, narrativeType?:string,
  conditions:Readonly<Record<string,boolean>>={}):string {
  return explainScreen(design,state,narrativeType,conditions).screenId;
}

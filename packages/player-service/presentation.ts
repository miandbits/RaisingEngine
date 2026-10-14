import type {ContentSnapshot,RunState} from '../core/model.js';
import {planActivityTimeline,createTimelineCursor,validateTimelineCursor,advanceTimeline,skipTimeline,type TimelineCursor,type TimelinePlan} from '../visual/timeline.js';
import {validateBattleSelection,type BattleSelection} from './battle.js';

export interface PlayerPresentationEnvelope {version:1;timeline:TimelineCursor|null;battleSelection?:BattleSelection|null;/** Opaque per-plugin state (plugin-host CONTRACT §8), keyed by plugin id. */extensions?:Record<string,unknown>}
export interface TimelineView {plan:TimelinePlan;cursor:TimelineCursor}
export type TimelineAction={type:'timelineTick';deltaMs:number}|{type:'timelinePause'}|{type:'timelineSpeed'}|{type:'timelineSkip'};

/** UI-only playback. Never dispatches a simulation command or modifies a RunState. */
export class PlayerPresentation {
  cursor:TimelineCursor|null=null;
  battleSelection:BattleSelection|null=null;
  extensions:Record<string,unknown>={};
  clear(){this.cursor=null;this.battleSelection=null;this.extensions={};}
  selectedBattle(content:ContentSnapshot,state:RunState|null):BattleSelection|null {
    if(this.battleSelection&&(!state||!validateBattleSelection(content,state,this.battleSelection)))this.battleSelection=null;
    return this.battleSelection?structuredClone(this.battleSelection):null;
  }
  view(content:ContentSnapshot,state:RunState|null):TimelineView|null {
    const plan=state?planActivityTimeline(content,state):null;
    if(!plan){this.cursor=null;return null;}
    if(!this.cursor||!validateTimelineCursor(plan,this.cursor))this.cursor=createTimelineCursor(plan);
    return {plan,cursor:structuredClone(this.cursor)};
  }
  envelope(content:ContentSnapshot,state:RunState):PlayerPresentationEnvelope {
    const selection=this.selectedBattle(content,state);
    return {version:1,timeline:this.view(content,state)?.cursor??null,...(selection?{battleSelection:selection}:{}),...(Object.keys(this.extensions).length?{extensions:structuredClone(this.extensions)}:{})};
  }
  restore(content:ContentSnapshot,state:RunState,envelope:PlayerPresentationEnvelope|undefined):string|null {
    if(envelope!==undefined&&(!envelope||typeof envelope!=='object'||!Object.hasOwn(envelope,'timeline')||Object.keys(envelope).some(key=>!['timeline','version','battleSelection','extensions'].includes(key))||envelope.version!==1))return '지원하지 않는 연출 저장 구조입니다.';
    if(envelope?.extensions!==undefined&&(!envelope.extensions||typeof envelope.extensions!=='object'||Array.isArray(envelope.extensions)))return '플러그인 연출 저장 구조가 올바르지 않습니다.';
    const plan=planActivityTimeline(content,state),cursor=envelope?.timeline;
    if(envelope&&envelope.version!==1)return '지원하지 않는 연출 저장 버전입니다.';
    if(cursor&&(!plan||!validateTimelineCursor(plan,cursor)))return '연출 저장 위치가 확정된 활동 결과와 일치하지 않습니다.';
    if(envelope&&plan&&!cursor)return '활동 결과의 연출 저장 위치가 없습니다.';
    if(envelope&&!plan&&cursor!==null)return '활동 결과가 없는 연출 저장입니다.';
    if(envelope?.battleSelection!==undefined&&envelope.battleSelection!==null&&!validateBattleSelection(content,state,envelope.battleSelection))return '전투 선택 저장이 현재 전투 상태와 일치하지 않습니다.';
    this.cursor=cursor?structuredClone(cursor):null;this.battleSelection=envelope?.battleSelection?structuredClone(envelope.battleSelection):null;this.extensions=structuredClone(envelope?.extensions??{});return null;
  }
  act(content:ContentSnapshot,state:RunState,action:TimelineAction):string|null {
    const current=this.view(content,state);if(!current)return '재생 중인 활동 연출이 없습니다.';
    const {plan,cursor}=current;
    if(action.type==='timelineTick'){
      if(!Number.isInteger(action.deltaMs)||action.deltaMs<0||action.deltaMs>60000)return '연출 시간은 0~60000ms 정수여야 합니다.';
      this.cursor=advanceTimeline(plan,cursor,action.deltaMs);
    }else if(action.type==='timelinePause')this.cursor={...cursor,paused:cursor.completed?false:!cursor.paused};
    else if(action.type==='timelineSpeed')this.cursor={...cursor,speed:cursor.speed===1?4:1};
    else this.cursor=skipTimeline(plan,cursor);
    return null;
  }
}

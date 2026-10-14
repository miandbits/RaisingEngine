import type {ContentSnapshot,RunState} from '../core/model.js';
import type {SystemsCommand} from '../systems/model.js';

export interface BattleSelection {stateRevision:number;battleAction:'attack'|'guard'|'flee'|'useItem';itemId?:string;target?:'self'|'enemy'}
export type BattleUiAction={type:'selectBattleAction';battleAction:BattleSelection['battleAction'];itemId?:string}|{type:'selectBattleTarget';battleTarget:'self'|'enemy'}|{type:'confirmBattleAction'}|{type:'cancelBattleAction'};
export type BattleCommand=Extract<SystemsCommand,{type:'battleAction'}>;
export type RuntimeInspect=(input:unknown)=>unknown;
export const battleUiTypes=['selectBattleAction','selectBattleTarget','confirmBattleAction','cancelBattleAction'];
export function battleTarget(action:BattleSelection['battleAction']):'self'|'enemy'|null{return action==='attack'?'enemy':action==='flee'?null:'self';}
export function battleCommand(selection:BattleSelection):BattleCommand{return selection.battleAction==='useItem'?{type:'battleAction',action:'useItem',itemId:selection.itemId!}:{type:'battleAction',action:selection.battleAction};}
/** Structural presentation validation; authoritative command availability always comes from inspect. */
export function validateBattleSelection(content:ContentSnapshot,state:RunState,raw:unknown):raw is BattleSelection {
  if(!raw||typeof raw!=='object'||Array.isArray(raw)||state.phase!=='battle'||!state.systems?.battle)return false;
  const s=raw as BattleSelection;
  if(Object.keys(raw).some(key=>!['stateRevision','battleAction','itemId','target'].includes(key))||s.stateRevision!==state.stateRevision||!['attack','guard','flee','useItem'].includes(s.battleAction))return false;
  if(s.battleAction==='useItem'){if(typeof s.itemId!=='string'||!content.project.systems?.consumables.some(x=>x.itemId===s.itemId)||(state.gameplay?.inventory[s.itemId]??0)<1)return false;}
  else if(Object.hasOwn(raw,'itemId'))return false;
  if(Object.hasOwn(raw,'target')&&s.target!==battleTarget(s.battleAction))return false;
  return true;
}
function canonicalAvailability(content:ContentSnapshot,state:RunState,selection:BattleSelection,request:RuntimeInspect):string|null {
  const reply=request({operation:'inspect',content,state,command:battleCommand(selection),expectedRevision:state.stateRevision}) as {ok:boolean;error?:{message?:string}};
  return reply.ok?null:reply.error?.message??'현재 사용할 수 없는 전투 행동입니다.';
}
/** Selection and target clicks are UI-only; confirmation returns the one canonical core command. */
export function selectBattle(content:ContentSnapshot,state:RunState|null,current:BattleSelection|null,action:BattleUiAction,request:RuntimeInspect):{selection:BattleSelection|null;command?:BattleCommand;error:string|null}{
  const fail=(error:string)=>({selection:current,error});
  if(!state||state.phase!=='battle'||!state.systems?.battle)return fail('진행 중인 전투가 없습니다.');
  if(action.type==='cancelBattleAction')return {selection:null,error:null};
  if(action.type==='selectBattleAction'){
    const next:BattleSelection={stateRevision:state.stateRevision,battleAction:action.battleAction,...(action.itemId!==undefined?{itemId:action.itemId}:{})};
    if(!validateBattleSelection(content,state,next))return fail('올바른 전투 행동을 선택해 주세요.');
    const error=canonicalAvailability(content,state,next,request);return error?fail(error):{selection:next,error:null};
  }
  if(!current||!validateBattleSelection(content,state,current))return fail('현재 전투에서 행동을 먼저 선택해 주세요.');
  const error=canonicalAvailability(content,state,current,request);if(error)return fail(error);
  if(action.type==='selectBattleTarget'){
    if(action.battleTarget!==battleTarget(current.battleAction))return fail('선택한 행동에 사용할 수 없는 대상입니다.');
    return {selection:{...current,target:action.battleTarget},error:null};
  }
  const target=battleTarget(current.battleAction);
  if(target&&current.target!==target)return fail('행동 대상을 선택해 주세요.');
  return {selection:current,command:battleCommand(current),error:null};
}

import {plannerEntryError} from './planner-entry.js';
import type {ContentSnapshot, RunState} from '../core/model.js';
import type {FacilityView,GameplayView} from '../gameplay/model.js';
import type {Element} from './model.js';
import {collectionRows,resolveCollectionRow,type DataRow} from './data-ui.js';
import {widgetCommand} from './commands.js';
import {resolveScreenElements} from './components.js';
import type {SystemsView} from '../systems/model.js';
import {battleUiTypes,selectBattle,type BattleSelection,type BattleUiAction} from '../player-service/battle.js';

export interface ActorActionGroup {
  actorId:string; name:string; actions:readonly (FacilityView & {maxQuantity:number})[];
}
export interface DataUiProjection {
  actorActions?: ActorActionGroup[];
  contextNames?: Record<string,string>;
  collections: Record<string,Record<string,DataRow[]>>;
  conditions: Record<string,boolean>;
  availability: Record<string,Record<string,{enabled:boolean;reason:string}>>;
}
type Reply={ok:boolean;value?:unknown;error?:{message?:string}};
/** Only binding/layout runs in JS. The supplied runtime evaluates every game predicate and command. */
export function projectDataUi(content:ContentSnapshot,state:RunState|null,gameplay:GameplayView|null,request:(input:unknown)=>unknown,extra:{systems?:SystemsView|null;battleSelection?:BattleSelection|null}={}):DataUiProjection {
  const result:DataUiProjection={collections:{},conditions:{},availability:{}};
  const design=content.project.visual;if(!design)return result;
  result.contextNames={};
  const names=(key:string,rows:readonly {id:string;name?:string;label?:string}[])=>{for(const row of rows)result.contextNames![key+':'+row.id]=row.name??row.label??row.id;};
  names('actorId',content.characters);names('characterId',content.characters);names('activityId',content.activities);names('attributeId',content.attributes);
  for(const [key,rows] of Object.entries({itemId:content.project.gameplay?.items,actionId:content.project.gameplay?.actions,locationId:content.project.gameplay?.locations,shopId:content.project.gameplay?.shops}))names(key,rows??[]);
  for(const rule of design.routing?.conditions??[]) {
    const reply=request({operation:'evaluate',content,state,condition:rule.when}) as Reply;
    result.conditions[JSON.stringify(rule.when)]=reply.ok&&reply.value===true;
  }
  const checkConditions=(element:Readonly<Element>)=>{
    for(const condition of [element.visibleCondition,element.enabledCondition,...(element.variants??[]).map(rule=>rule.when)])if(condition){
      const key=JSON.stringify(condition);if(Object.hasOwn(result.conditions,key))continue;
      const reply=request({operation:'evaluate',content,state,condition}) as Reply;
      result.conditions[key]=reply.ok&&reply.value===true;
    }
  };
  for(const component of design.components??[])for(const child of component.elements)checkConditions(child as Element);
  const cache=new Map<string,{enabled:boolean;reason:string}>();
  const allowedCommand=(command:NonNullable<ReturnType<typeof widgetCommand>>):{enabled:boolean;reason:string}=>{
    if(command.type==='openPlanner'){const reason=plannerEntryError(design,state,content.activities,command,condition=>{const reply=request({operation:'evaluate',content,state,condition}) as Reply;return reply.ok&&reply.value===true;});if(reason)return {enabled:false,reason};return allowedCommand({type:'navigate',screenId:command.screenId});}
    if(command.type==='navigate'&&!state&&design.routing){const enabled=design.screens.some(s=>s.id===command.screenId&&!s.modal);return {enabled,reason:enabled?'':'이동할 화면을 찾을 수 없습니다.'};}
    const key=JSON.stringify(command);const previous=cache.get(key);if(previous)return previous;
    let allowed:{enabled:boolean;reason:string};
    if(battleUiTypes.includes(command.type)){
      const selected=selectBattle(content,state,extra.battleSelection??null,command as BattleUiAction,request);
      allowed={enabled:!selected.error,reason:selected.error??''};
    }else{
      const reply=state?request({operation:'inspect',content,state,command,expectedRevision:state.stateRevision}) as Reply:null;
      allowed={enabled:reply?.ok??false,reason:reply?.error?.message??(state?'':'게임 시작 후 사용할 수 있습니다.')};
    }
    cache.set(key,allowed);return allowed;
  };
  // The common actions view has no actor. Ask the runtime with each explicitly
  // assigned actor; never reuse shared availability for character-owned rules.
  const roster=content.project.roster;
  if(roster?.stateModel==='character-v1'&&design.screens.some(screen=>screen.elements.some(e=>e.kind==='actions'))){
    result.actorActions=roster.members.map(member=>({actorId:member.characterId,
      name:content.characters.find(c=>c.id===member.characterId)?.name??member.characterId,
      actions:(content.project.gameplay?.actions??[]).filter(action=>member.actionIds?.includes(action.id)).map(action=>({
        id:action.id,name:action.name,costs:action.costs,effects:action.effects,maxQuantity:action.maxQuantity??1,
        ...allowedCommand({type:'executeActorAction',actorId:member.characterId,actionId:action.id,quantity:1})
      }))}));
  }
  for(const screen of design.screens){
    for(const element of resolveScreenElements(design,screen.id))checkConditions(element);
    const collections:Record<string,DataRow[]>={},availability:Record<string,{enabled:boolean;reason:string}>={};
    result.collections[screen.id]=collections;result.availability[screen.id]=availability;
    for(const element of resolveScreenElements(design,screen.id)){
      const command=element.kind==='button'?widgetCommand(element,state):null;
      if(element.kind==='button'&&element.action==='assignActivity'&&!command)availability[element.id]={enabled:false,reason:state?'캐릭터별 활동 배정을 사용할 수 없습니다.':'게임 시작 후 사용할 수 있습니다.'};
      if(command&&(element.uiContext||command.type==='openPlanner'||battleUiTypes.includes(command.type)))availability[element.id]=allowedCommand(command);
    }
    for(const source of screen.elements){
      const element=source as Element;
      checkConditions(element);
      if(element.kind!=='collection')continue;
      const rows=collectionRows(design,element,{content,state,gameplay,...extra});collections[element.id]=rows;
      const width=design.components?.find(c=>c.id===element.componentId)?.width??element.width;
      for(const row of rows)for(const child of resolveCollectionRow(design,element,row,width)){
        checkConditions(child);
        const command=child.kind==='button'?widgetCommand(child,state):null;
        if(child.kind==='button'&&child.action==='assignActivity'&&!command)availability[child.id]={enabled:false,reason:state?'캐릭터별 활동 배정을 사용할 수 없습니다.':'게임 시작 후 사용할 수 있습니다.'};
        if(!command)continue;
        availability[child.id]=allowedCommand(command);
      }
    }
  }
  return result;
}

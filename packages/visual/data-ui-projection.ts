import type {ContentSnapshot, RunState} from '../core/model.js';
import type {GameplayView} from '../gameplay/model.js';
import type {Element} from './model.js';
import {collectionRows,resolveCollectionRow,type DataRow} from './data-ui.js';
import {widgetCommand} from './commands.js';
import {resolveScreenElements} from './components.js';
import type {SystemsView} from '../systems/model.js';
import {battleUiTypes,selectBattle,type BattleSelection,type BattleUiAction} from '../player-service/battle.js';

export interface DataUiProjection {
  collections: Record<string,Record<string,DataRow[]>>;
  conditions: Record<string,boolean>;
  availability: Record<string,Record<string,{enabled:boolean;reason:string}>>;
}
type Reply={ok:boolean;value?:unknown;error?:{message?:string}};
/** Only binding/layout runs in JS. The supplied runtime evaluates every game predicate and command. */
export function projectDataUi(content:ContentSnapshot,state:RunState|null,gameplay:GameplayView|null,request:(input:unknown)=>unknown,extra:{systems?:SystemsView|null;battleSelection?:BattleSelection|null}={}):DataUiProjection {
  const result:DataUiProjection={collections:{},conditions:{},availability:{}};
  const design=content.project.visual;if(!design)return result;
  const checkConditions=(element:Readonly<Element>)=>{
    for(const condition of [element.visibleCondition,element.enabledCondition])if(condition){
      const key=JSON.stringify(condition);if(Object.hasOwn(result.conditions,key))continue;
      const reply=request({operation:'evaluate',content,state,condition}) as Reply;
      result.conditions[key]=reply.ok&&reply.value===true;
    }
  };
  for(const component of design.components??[])for(const child of component.elements)checkConditions(child as Element);
  const cache=new Map<string,{enabled:boolean;reason:string}>();
  const allowedCommand=(command:NonNullable<ReturnType<typeof widgetCommand>>)=>{
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
  for(const screen of design.screens){
    for(const element of resolveScreenElements(design,screen.id))checkConditions(element);
    const collections:Record<string,DataRow[]>={},availability:Record<string,{enabled:boolean;reason:string}>={};
    result.collections[screen.id]=collections;result.availability[screen.id]=availability;
    for(const element of resolveScreenElements(design,screen.id)){
      const command=element.kind==='button'?widgetCommand(element):null;
      if(command&&battleUiTypes.includes(command.type))availability[element.id]=allowedCommand(command);
    }
    for(const source of screen.elements){
      const element=source as Element;
      checkConditions(element);
      if(element.kind!=='collection')continue;
      const rows=collectionRows(design,element,{content,state,gameplay,...extra});collections[element.id]=rows;
      const width=design.components?.find(c=>c.id===element.componentId)?.width??element.width;
      for(const row of rows)for(const child of resolveCollectionRow(design,element,row,width)){
        checkConditions(child);
        const command=child.kind==='button'?widgetCommand(child):null;if(!command)continue;
        availability[child.id]=allowedCommand(command);
      }
    }
  }
  return result;
}

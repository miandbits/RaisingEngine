import type { ContentSnapshot, RunState } from '../core/model.js';
import type { SystemsView } from '../systems/model.js';
import type { BattleSelection } from '../player-service/battle.js';
import type { DataRow,DataValue } from './data-ui.js';

export const battleSourceFields:Record<string,Record<string,'text'|'number'|'boolean'>>={
 battleActors:{id:'text',name:'text',side:'text',hp:'number',maxHp:'number',hpText:'text',battleTarget:'text',value:'number',maxValue:'number'},
 battleActions:{id:'text',name:'text',displayName:'text',battleAction:'text',itemId:'text',target:'text',selected:'boolean',enabled:'boolean',reason:'text',action:'text'},
 battleTargets:{id:'text',name:'text',displayName:'text',battleTarget:'text',selected:'boolean',enabled:'boolean',reason:'text',action:'text'},
 battleLog:{id:'text',text:'text',turn:'number',battleAction:'text',heroDamage:'number',enemyDamage:'number',healed:'number',outcome:'text'},
 battleRewards:{id:'text',name:'text',amount:'number',discarded:'number',text:'text',kind:'text'},
};
const actionNames:Record<string,string>={attack:'공격',guard:'방어',flee:'도주',useItem:'아이템 사용'};
const outcomeNames:Record<string,string>={victory:'승리',defeat:'패배',fled:'도주'};
/** Read committed battle projection. These adapters never simulate combat or award rewards. */
export function battleCollectionRows(kind:string,context:{content:ContentSnapshot,state:RunState|null,systems?:SystemsView|null,battleSelection?:BattleSelection|null}):DataRow[]{
 const rows:DataRow[]=[],{content,state,systems}=context;if(!state||!systems)return rows;
 const battle=systems.battle,selection=context.battleSelection?.stateRevision===state.stateRevision&&state.phase==='battle'?context.battleSelection:null;
 const add=(id:string,values:Record<string,DataValue>,enabled=true,reason='')=>rows.push({id,values:{id,...values},enabled,reason});
 if(kind==='battleActors'&&battle){
  add('self',{name:'주인공',side:'ally',hp:battle.heroHp,maxHp:battle.heroMaxHp,hpText:`${battle.heroHp} / ${battle.heroMaxHp}`,battleTarget:'self',value:battle.heroHp,maxValue:battle.heroMaxHp});
  add('enemy',{name:battle.enemyName,side:'enemy',hp:battle.enemyHp,maxHp:battle.enemyMaxHp,hpText:`${battle.enemyHp} / ${battle.enemyMaxHp}`,battleTarget:'enemy',value:battle.enemyHp,maxValue:battle.enemyMaxHp});
 }else if(kind==='battleActions'&&battle){
  for(const action of systems.battleActions??[]){const selected=!!selection&&selection.battleAction===action.battleAction&&(selection.itemId??'')===(action.itemId??'');add(action.id,{name:action.name,displayName:selected?`✓ ${action.name}`:action.name,battleAction:action.battleAction,itemId:action.itemId??'',target:action.target,selected,enabled:action.enabled,reason:action.reason??'',action:'selectBattleAction'},action.enabled,action.reason??'');}
 }else if(kind==='battleTargets'&&battle&&selection){
  const action=systems.battleActions?.find(x=>x.battleAction===selection.battleAction&&(x.itemId??'')===(selection.itemId??''));
  if(action&&action.target!=='none'){const name=action.target==='self'?'주인공':battle.enemyName,selected=selection.target===action.target;add(action.target,{name,displayName:selected?`✓ ${name}`:name,battleTarget:action.target,selected,enabled:action.enabled,reason:action.reason??'',action:'selectBattleTarget'},action.enabled,action.reason??'');}
 }else if(kind==='battleLog'){
  const entries=battle?.log??systems.lastBattle?.log??[];
  for(const entry of entries){const outcome=!battle&&systems.lastBattle?.turn===entry.turn?systems.lastBattle.outcome:'';const text=`${entry.turn}턴 · ${actionNames[entry.action]??entry.action} · 받은 피해 ${entry.heroDamage} · 준 피해 ${entry.enemyDamage}${entry.heroHeal?` · 회복 ${entry.heroHeal}`:''}${outcome?` · ${outcomeNames[outcome]}`:''}`;add(entry.id,{text,turn:entry.turn,battleAction:entry.action,heroDamage:entry.heroDamage,enemyDamage:entry.enemyDamage,healed:entry.heroHeal,outcome});}
 }else if(kind==='battleRewards'&&!battle&&systems.lastBattle?.outcome==='victory'){
  const discarded=new Map(systems.lastBattle.discardedItems.map(x=>[x.itemId,x.quantity]));
  for(const [i,change]of (systems.lastBattle.rewardChanges??[]).entries()){
   let name:string,kind:string,amount:number,lost=0,text:string;
   if(change.kind==='flag'){name=content.flags.find(x=>x.id===change.flagId)?.label??change.flagId;kind='flag';amount=change.after?1:0;text=`${name}: ${change.after?'true':'false'}`;}
   else if(change.kind==='state'){const group=content.project.stateGroups?.find(g=>g.id===change.stateGroupId);name=group?.name??change.stateGroupId;kind='state';amount=0;text=`${name}: ${group?.values.find(v=>v.id===change.after)?.name??change.after}`;}
   else{const target=change.target;amount=change.delta;
    if(target.kind==='attribute'){kind='attribute';name=content.attributes.find(x=>x.id===target.attributeId)?.label??target.attributeId;}
    else if(target.kind==='inventory'){kind='item';name=content.project.gameplay?.items.find(x=>x.id===target.itemId)?.name??target.itemId;lost=discarded.get(target.itemId)??0;discarded.delete(target.itemId);}
    else{kind='relationship';const c=content.characters.find(x=>x.id===target.characterId);name=`${c?.name??target.characterId} / ${c?.relationships.find(x=>x.id===target.relationshipId)?.label??target.relationshipId}`;}
    text=`${name} ${amount>=0?'+':''}${amount}${lost?` (버림 ${lost})`:''}`;
   }add(`reward-${i+1}`,{name,kind,amount,discarded:lost,text});
  }
  for(const [itemId,quantity]of discarded){const name=content.project.gameplay?.items.find(x=>x.id===itemId)?.name??itemId;add(`discarded--${itemId}`,{name,kind:'item',amount:0,discarded:quantity,text:`${name} +0 (버림 ${quantity})`});}
 }
 return rows;
}

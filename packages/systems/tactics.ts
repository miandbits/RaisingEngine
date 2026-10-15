import type {ContentSnapshot,RunData} from '../core/model.js';
import {sampleBelow} from '../core/random.js';
import {requireCore} from '../core/util.js';
import {reward,finishExpedition,equipmentReserved} from './service.js';
import type {Fighter,TacticsSkill,TacticsCommand,TacticsStats,TacticsState} from './tactics-model.js';

const config=(c:ContentSnapshot)=>c.project.systems!.tactics!;
const state=(s:RunData):TacticsState=>s.systems!.tactics??= {growth:{},equipment:{},active:null,last:null};
function record(s:RunData,text:string){const a=state(s).active!;a.log.push(`${a.turn}: ${text}`);if(a.log.length>200)a.log.shift();}
function fighter(c:ContentSnapshot,s:RunData,id:string,key:string):Fighter{
 const d=config(c),u=d.units.find(x=>x.id===id)!,t=state(s),level=t.growth[id]?.level??1,stats={...u.stats};
 for(const [key,attribute] of Object.entries(u.statAttributes??{})){const scoped=u.characterId?s.roster?.attributes[u.characterId]:undefined;stats[key as keyof TacticsStats]=Math.max(key==='maxHp'?1:0,(scoped??s.attributes)[attribute]!);}
 if(u.control==='player')for(const k of Object.keys(stats) as (keyof TacticsStats)[])stats[k]+=(level-1)*d.growth.perLevel[k];
 for(const eId of Object.values(t.equipment[id]??{})){const e=c.project.systems!.equipment.find(x=>x.id===eId);if(e)for(const k of ['maxHp','attack','defense'] as const)stats[k]+=e.modifiers[k];}
 stats.maxHp=Math.max(1,stats.maxHp);return {id:key,unitId:id,faction:u.faction,control:u.control,row:u.row,column:u.column,hp:stats.maxHp,stats,statuses:[],cooldowns:{},casting:null};
}
export function tacticsTargets(c:ContentSnapshot,s:RunData,actor:Fighter,skill:TacticsSkill):Fighter[]{
 const alive=state(s).active!.units.filter(u=>u.hp>0),enemy=skill.target==='enemy'||skill.target==='frontEnemy'||skill.target==='allEnemies';
 let list=alive.filter(u=>skill.target==='self'?u.id===actor.id:enemy?u.faction!==actor.faction:u.faction===actor.faction);
 if(skill.target==='frontEnemy'&&list.length){const row=Math.min(...list.map(u=>u.row));list=list.filter(u=>u.row===row);}
 return list.sort((a,b)=>a.row-b.row||a.column-b.column||(a.id<b.id?-1:a.id>b.id?1:0));
}
function apply(c:ContentSnapshot,s:RunData,actor:Fighter,skill:TacticsSkill,targetId:string){
 const d=config(c),targets=tacticsTargets(c,s,actor,skill),chosen=skill.target.startsWith('all')?targets:targets.filter(x=>x.id===targetId);
 const modified=(u:Fighter,k:'attack'|'defense')=>Math.max(0,u.stats[k]+u.statuses.reduce((n,x)=>n+d.statuses.find(z=>z.id===x.id)![k],0));
 if(!chosen.length)record(s,`${actor.id} ${skill.id} target-lost`);
 for(const target of chosen){const roll=sampleBelow(s.rng,100);s.rng=roll.rng;if(roll.value>=skill.accuracy){record(s,`${actor.id} ${skill.id} ${target.id} miss`);continue;}
  const amount=skill.kind==='damage'?Math.max(1,skill.power+modified(actor,'attack')-modified(target,'defense')):skill.kind==='heal'?skill.power:0;
  target.hp=Math.max(0,Math.min(target.stats.maxHp,target.hp+(skill.kind==='heal'?amount:-amount)));
  if(skill.statusId&&target.hp>0){target.statuses=target.statuses.filter(x=>x.id!==skill.statusId);target.statuses.push({id:skill.statusId,remaining:d.statuses.find(x=>x.id===skill.statusId)!.turns});}
  record(s,`${actor.id} ${skill.id} ${target.id} ${amount}`);
 }
 actor.cooldowns[skill.id]=skill.cooldown+1;
}
function wave(c:ContentSnapshot,s:RunData){const a=state(s).active!,e=config(c).encounters.find(x=>x.id===a.encounterId)!;a.units=a.units.filter(u=>u.control==='player');
 for(const id of e.waves[a.wave]!.unitIds)a.units.push(fighter(c,s,id,`wave-${a.wave}-${id}`));
 a.order=[...a.units].sort((a,b)=>b.stats.speed-a.stats.speed||a.row-b.row||a.column-b.column||(a.id<b.id?-1:a.id>b.id?1:0)).map(x=>x.id);a.cursor=0;record(s,`wave ${a.wave+1}`);
}
function finish(c:ContentSnapshot,s:RunData,outcome:'victory'|'defeat'|'fled'){
 const t=state(s),a=t.active!,e=config(c).encounters.find(x=>x.id===a.encounterId)!;
 t.last={encounterId:a.encounterId,outcome,turn:a.turn,log:[...a.log],experience:outcome==='victory'?e.experience:0};t.active=null;
 if(outcome==='victory'){
  for(const id of e.party){const g=t.growth[id]??={experience:0,level:1};g.experience+=e.experience;g.level=Math.min(config(c).growth.maxLevel,1+Math.floor(g.experience/config(c).growth.experiencePerLevel));}
  reward(c,s,e.rewards,`/systems/tactics/${e.id}/rewards`,true);
  const trip=s.systems!.expedition!;if(!trip.clearedNodes.includes(trip.nodeId))trip.clearedNodes.push(trip.nodeId);s.phase='expedition';
  for(const q of c.project.systems!.quests)if(q.encounterId===e.id&&s.systems!.quests[q.id]?.status==='accepted'){const progress=s.systems!.quests[q.id]!;progress.progress=Math.min(q.targetCount,progress.progress+1);if(progress.progress===q.targetCount)progress.status='completed';}
 }else finishExpedition(s,outcome==='defeat'?'defeated':'retreated');
}
function advance(c:ContentSnapshot,s:RunData){
 for(let safety=0;safety<512;safety++){
  const a=state(s).active;if(!a)return;
  if(!a.units.some(u=>u.control==='player'&&u.hp>0)){finish(c,s,'defeat');return;}
  if(!a.units.some(u=>u.control==='ai'&&u.hp>0)){const e=config(c).encounters.find(x=>x.id===a.encounterId)!;if(a.wave+1===e.waves.length){finish(c,s,'victory');return;}a.wave++;wave(c,s);}
  const actor=a.units.find(x=>x.id===a.order[a.cursor])!;a.turn++;
  if(actor.hp<=0){a.cursor=(a.cursor+1)%a.order.length;continue;}
  let stunned=false;for(const entry of actor.statuses){const d=config(c).statuses.find(x=>x.id===entry.id)!;actor.hp=Math.max(0,Math.min(actor.stats.maxHp,actor.hp-d.damage+d.heal));stunned||=d.stun;entry.remaining--;}
  actor.statuses=actor.statuses.filter(x=>x.remaining>0);for(const id of Object.keys(actor.cooldowns))actor.cooldowns[id]=Math.max(0,actor.cooldowns[id]!-1);
  if(actor.hp<=0||stunned){record(s,`${actor.id} ${actor.hp<=0?'down':'stunned'}`);a.cursor=(a.cursor+1)%a.order.length;continue;}
  if(actor.casting){actor.casting.remaining--;if(actor.casting.remaining<=0){const cast=actor.casting;actor.casting=null;apply(c,s,actor,config(c).skills.find(x=>x.id===cast.skillId)!,cast.targetId);}a.cursor=(a.cursor+1)%a.order.length;continue;}
  if(actor.control==='player')return;
  const unit=config(c).units.find(x=>x.id===actor.unitId)!;const skill=unit.skillIds.map(id=>config(c).skills.find(x=>x.id===id)!).find(x=>(actor.cooldowns[x.id]??0)===0&&tacticsTargets(c,s,actor,x).length);
  if(skill){const target=tacticsTargets(c,s,actor,skill)[0]!;if(skill.castTurns)actor.casting={skillId:skill.id,targetId:target.id,remaining:skill.castTurns};else apply(c,s,actor,skill,target.id);}
  a.cursor=(a.cursor+1)%a.order.length;
 }
 requireCore(false,'TACTICS_TURN_LIMIT','자동 진행 한도를 초과했습니다. 상태 효과와 스킬을 확인하세요.','/systems/tactics');
}
export function startTactics(c:ContentSnapshot,s:RunData,id:string){const d=config(c),e=d?.encounters.find(x=>x.id===id);requireCore(e,'UNKNOWN_ENCOUNTER','전투 구성이 없습니다.','/systems/tactics');const t=state(s);t.last=null;t.active={encounterId:id,wave:0,turn:0,cursor:0,order:[],units:e.party.map(id=>fighter(c,s,id,id)),log:[]};wave(c,s);s.phase='battle';advance(c,s);}
export function applyTactics(c:ContentSnapshot,s:RunData,cmd:TacticsCommand){
 const d=config(c);requireCore(d,'TACTICS_UNAVAILABLE','다인 전투 규격이 없습니다.','/systems/tactics');const t=state(s);
 if(cmd.type==='tacticsUnequip'){requireCore(s.phase==='planning'||s.phase==='ready','WRONG_PHASE','전투 준비 중에만 장비를 변경할 수 있습니다.','/phase');requireCore(t.equipment[cmd.unitId]?.[cmd.slot],'SLOT_EMPTY','장착한 장비가 없습니다.','/command/slot');delete t.equipment[cmd.unitId]![cmd.slot];return;}
 if(cmd.type==='tacticsEquip'){
  requireCore(s.phase==='planning'||s.phase==='ready','WRONG_PHASE','전투 준비 중에만 장비를 변경할 수 있습니다.','/phase');
  const u=d.units.find(x=>x.id===cmd.unitId&&x.control==='player'),e=c.project.systems!.equipment.find(x=>x.id===cmd.equipmentId);requireCore(u&&e,'UNKNOWN_EQUIPMENT','전투원 또는 장비가 없습니다.','/command');
  requireCore((s.gameplay!.inventory[e.itemId]??0)>equipmentReserved(c,s,e.itemId),'INSUFFICIENT_ITEMS','장착 가능한 물품이 없습니다.','/command/equipmentId');(t.equipment[u.id]??={})[e.slot]=e.id;return;
 }
 const a=t.active;requireCore(a&&s.phase==='battle','WRONG_PHASE','진행 중인 다인 전투가 없습니다.','/phase');
 if(cmd.type==='tacticsFlee'){finish(c,s,'fled');return;}
 const actor=a.units.find(x=>x.id===a.order[a.cursor])!,unit=d.units.find(x=>x.id===actor.unitId)!,skill=d.skills.find(x=>x.id===cmd.skillId);
 requireCore(actor.id===cmd.unitId&&actor.control==='player'&&actor.hp>0,'TACTICS_TURN','현재 차례의 전투원을 선택하세요.','/command/unitId');
 requireCore(skill&&unit.skillIds.includes(skill.id)&&(actor.cooldowns[skill.id]??0)===0&&!actor.casting,'TACTICS_SKILL','사용할 수 없는 스킬입니다.','/command/skillId');
 requireCore(tacticsTargets(c,s,actor,skill).some(x=>x.id===cmd.targetId),'TACTICS_TARGET','진영·배치·생존 조건에 맞는 대상을 선택하세요.','/command/targetId');
 if(skill.castTurns){actor.casting={skillId:skill.id,targetId:cmd.targetId,remaining:skill.castTurns};record(s,`${actor.id} cast ${skill.id}`);}else apply(c,s,actor,skill,cmd.targetId);
 a.cursor=(a.cursor+1)%a.order.length;advance(c,s);
}

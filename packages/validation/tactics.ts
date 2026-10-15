import {isObject,type JsonObject,type ProjectDocument} from './model.js';
import {objectItems,type SemanticContext,type Index} from './context.js';
export function checkTactics(ctx:SemanticContext,doc:ProjectDocument,systems:JsonObject){
 if(!isObject(systems.tactics)){
  objectItems(systems.expeditions).forEach((e,i)=>objectItems(e.nodes).forEach((n,j)=>{if(n.encounterId)ctx.report(doc,`/systems/expeditions/${i}/nodes/${j}/encounterId`,'TACTICS_CONFIG_REQUIRED','다인 전투 정의가 필요합니다.');}));
  objectItems(systems.quests).forEach((q,i)=>{if(q.encounterId)ctx.report(doc,`/systems/quests/${i}/encounterId`,'TACTICS_CONFIG_REQUIRED','다인 전투 정의가 필요합니다.');});return;
 }const d=systems.tactics,indexes:Record<string,Index>={};
 for(const k of ['units','skills','statuses','encounters']){indexes[k]=new Map();ctx.addArray(indexes[k]!,doc,`/systems/tactics/${k}`,d[k],k);}
 const ref=(k:string,id:unknown,path:string)=>ctx.resolve(indexes[k]!,String(id),doc,path,k),report=(p:string,code:string,m:string)=>ctx.report(doc,p,code,m);
 const units=objectItems(d.units);
 units.forEach((u,i)=>{const p=`/systems/tactics/units/${i}`;if(String(u.id).length>48)report(p+'/id','TACTICS_ID_LENGTH','전투원 ID는 48자 이하여야 합니다.');if(isObject(u.statAttributes))for(const [key,id] of Object.entries(u.statAttributes))ctx.resolve(ctx.attributes,String(id),doc,p+'/statAttributes/'+key,'전투 능력');if(u.characterId)ctx.resolve(ctx.characters,String(u.characterId),doc,p+'/characterId','인물');const skills=u.skillIds as string[];if(!skills.length)report(p+'/skillIds','TACTICS_NO_SKILL','전투원에게 스킬을 하나 이상 배정하세요.');skills.forEach((id,j)=>ref('skills',id,`${p}/skillIds/${j}`));if(new Set(skills).size!==skills.length)report(p+'/skillIds','TACTICS_DUPLICATE','스킬이 중복 배정되었습니다.');});
 objectItems(d.skills).forEach((s,i)=>{const p=`/systems/tactics/skills/${i}`;if(s.statusId)ref('statuses',s.statusId,p+'/statusId');if(s.kind==='status'&&!s.statusId)report(p+'/statusId','TACTICS_STATUS_REQUIRED','상태 스킬에는 적용할 상태 효과가 필요합니다.');});
 objectItems(d.encounters).forEach((e,i)=>{const p=`/systems/tactics/encounters/${i}`,party=e.party as string[],waves=objectItems(e.waves);if(!party.length||!waves.length)report(p,'TACTICS_EMPTY','아군과 웨이브가 모두 필요합니다.');
  const factions=new Set(party.map(id=>units.find(u=>u.id===id)?.faction));if(factions.size>1)report(p+'/party','TACTICS_PARTY_FACTION','아군 파티는 같은 진영이어야 합니다.');
  const check=(ids:string[],path:string,control:string)=>{if(!ids.length)report(path,'TACTICS_EMPTY','전투원이 필요합니다.');const slots=new Set();ids.forEach((id,j)=>{ref('units',id,`${path}/${j}`);const u=units.find(x=>x.id===id);if(!u)return;if(u.control!==control)report(`${path}/${j}`,'TACTICS_CONTROL','파티는 player, 적 웨이브는 ai 전투원을 사용하세요.');if(control==='ai'&&factions.has(u.faction))report(`${path}/${j}`,'TACTICS_ENEMY_FACTION','적은 아군과 다른 진영이어야 합니다.');const slot=`${u.faction}/${u.row}/${u.column}`;if(slots.has(slot))report(`${path}/${j}`,'TACTICS_POSITION','같은 진영 전투원의 배치가 겹칩니다.');slots.add(slot);});if(new Set(ids).size!==ids.length)report(path,'TACTICS_DUPLICATE','전투원이 중복 배정되었습니다.');};
  check(party,p+'/party','player');waves.forEach((w,j)=>check(w.unitIds as string[],`${p}/waves/${j}/unitIds`,'ai'));
 });
 objectItems(systems.expeditions).forEach((e,i)=>objectItems(e.nodes).forEach((n,j)=>{if(n.encounterId)ref('encounters',n.encounterId,`/systems/expeditions/${i}/nodes/${j}/encounterId`);if(n.enemyId&&n.encounterId)report(`/systems/expeditions/${i}/nodes/${j}`,'TACTICS_AMBIGUOUS','기존 적 전투와 다인 전투 중 하나만 연결하세요.');}));
 objectItems(systems.quests).forEach((q,i)=>{if(q.encounterId)ref('encounters',q.encounterId,`/systems/quests/${i}/encounterId`);});
}

export interface TacticsStats {maxHp:number;attack:number;defense:number;speed:number}
export interface TacticsUnit {id:string;name:string;characterId?:string;faction:string;control:'player'|'ai';row:number;column:number;stats:TacticsStats;statAttributes?:Partial<Record<keyof TacticsStats,string>>;skillIds:string[]}
export interface TacticsSkill {id:string;name:string;target:'enemy'|'frontEnemy'|'ally'|'self'|'allEnemies'|'allAllies';kind:'damage'|'heal'|'status';power:number;accuracy:number;castTurns:number;cooldown:number;statusId?:string}
export interface TacticsStatus {id:string;name:string;turns:number;damage:number;heal:number;attack:number;defense:number;stun:boolean}
export interface Fighter {id:string;unitId:string;faction:string;control:'player'|'ai';row:number;column:number;hp:number;stats:TacticsStats;statuses:{id:string;remaining:number}[];cooldowns:Record<string,number>;casting:null|{skillId:string;targetId:string;remaining:number}}
export interface TacticsState {
 growth:Record<string,{experience:number;level:number}>;
 equipment:Record<string,Record<string,string>>;
 active:null|{encounterId:string;wave:number;turn:number;cursor:number;order:string[];units:Fighter[];log:string[]};
 last:null|{encounterId:string;outcome:'victory'|'defeat'|'fled';turn:number;log:string[];experience:number};
}
export type TacticsCommand={type:'tacticsUnequip';unitId:string;slot:'weapon'|'armor'|'accessory'}|{type:'tacticsAction';unitId:string;skillId:string;targetId:string}|{type:'tacticsFlee'}|{type:'tacticsEquip';unitId:string;equipmentId:string};

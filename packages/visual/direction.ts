/** Presentation-only sequence playback. It cannot execute game rules. */
export interface DirectionCursor {sequenceId:string;elapsedMs:number;speed:1|4;paused:boolean;status:'playing'|'completed'|'stopped'}
type Cue={actorId:string;x:number;y:number;width:number;height:number;toX?:number;toY?:number;opacity?:number;expressionId?:string;outfitId?:string;motionId?:string};
type Actor={id:string;assetId:string;expressions?:{id:string;assetId:string}[];outfits?:{id:string;assetId:string}[];motions?:{id:string;frames:string[];frameMs:number}[]};
export interface Direction {actors:Actor[];sequences:{id:string;name:string;stages:{name:string;durationMs:number;cues:Cue[]}[]}[]}
export function directionSequence(d:Direction,c:DirectionCursor){return d.sequences.find(s=>s.id===c.sequenceId);}
export function directionStart(d:Direction,id:string):DirectionCursor {if(!d.sequences.some(s=>s.id===id))throw new Error('Unknown presentation sequence');return {sequenceId:id,elapsedMs:0,speed:1,paused:false,status:'playing'};}
export function directionValid(d:Direction,c:unknown):c is DirectionCursor {
 if(!c||typeof c!=='object'||Array.isArray(c))return false;const v=c as DirectionCursor;
 if(Object.keys(c).sort().join(',')!=='elapsedMs,paused,sequenceId,speed,status'||![1,4].includes(v.speed)||typeof v.paused!=='boolean'||!Number.isInteger(v.elapsedMs))return false;
 const s=directionSequence(d,v);if(!s)return false;const duration=s.stages.reduce((n,s)=>n+s.durationMs,0);
 return v.elapsedMs>=0&&v.elapsedMs<=duration&&['playing','completed','stopped'].includes(v.status)&&(v.status==='completed'?v.elapsedMs===duration&&!v.paused:v.elapsedMs<duration)&&(v.status==='playing'||!v.paused);
}
export function directionAdvance(d:Direction,c:DirectionCursor,deltaMs:number):DirectionCursor {
 if(!Number.isInteger(deltaMs)||deltaMs<0||deltaMs>60000)throw new Error('Presentation delta must be an integer in 0..60000');
 if(c.paused||c.status!=='playing')return {...c};const s=directionSequence(d,c)!;const duration=s.stages.reduce((n,s)=>n+s.durationMs,0),elapsedMs=Math.min(duration,c.elapsedMs+deltaMs*c.speed);
 return {...c,elapsedMs,status:elapsedMs===duration?'completed':'playing',paused:false};
}
export function directionFrame(d:Direction,c:DirectionCursor){
 const s=directionSequence(d,c)!;let start=0,index=0;const actors=new Map<string,any>();
 for(const stage of s.stages){const local=Math.max(0,Math.min(stage.durationMs,c.elapsedMs-start)),t=local/stage.durationMs;
  for(const cue of stage.cues){const actor=d.actors.find(a=>a.id===cue.actorId)!;const motion=actor.motions?.find(m=>m.id===cue.motionId);
   const assets=[motion?motion.frames[Math.floor(local/motion.frameMs)%motion.frames.length]:actor.assetId];
   for(const [list,id] of [[actor.outfits,cue.outfitId],[actor.expressions,cue.expressionId]] as const){const v=list?.find(x=>x.id===id);if(v)assets.push(v.assetId);}
   actors.set(cue.actorId,{actorId:cue.actorId,x:cue.x+((cue.toX??cue.x)-cue.x)*t,y:cue.y+((cue.toY??cue.y)-cue.y)*t,width:cue.width,height:cue.height,opacity:cue.opacity??1,assets});
  }
  if(c.elapsedMs<start+stage.durationMs||index===s.stages.length-1)return {name:s.name,stage:stage.name,index,actors:[...actors.values()]};start+=stage.durationMs;index++;
 }
 throw new Error('Empty presentation sequence');
}

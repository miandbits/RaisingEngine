/** Learned UI behavior is device presentation state, never a campaign save. */
const topic='compact-planning-v1';
const seen=new Set<string>(),unavailable=new Set<string>();
let owner:symbol|null=null;
export function planningGuidanceKey(gameId?:string):string {
  return 'raising-player-guidance-v1:'+JSON.stringify([gameId??null,new URL('.',document.baseURI).pathname]);
}
function topics(key:string):string[] {
  const raw=localStorage.getItem(key);if(raw===null)return [];
  const value:unknown=JSON.parse(raw);
  if(!value||typeof value!=='object'||Array.isArray(value))throw Error('Invalid guidance preferences');
  const record=value as {version?:unknown;topics?:unknown};
  if(record.version!==1||!Array.isArray(record.topics)||record.topics.length>32||record.topics.some(t=>typeof t!=='string'||t.length>80))throw Error('Invalid guidance preferences');
  return record.topics;
}
export function planningGuidanceSeen(gameId?:string):boolean {
  const key=planningGuidanceKey(gameId);if(seen.has(key))return true;
  if(!gameId||unavailable.has(key))return false;
  try{return topics(key).includes(topic);}catch{unavailable.add(key);return false;}
}
export function acknowledgePlanningGuidance(gameId?:string):void {
  const key=planningGuidanceKey(gameId);seen.add(key);
  if(seen.size>256)seen.delete(seen.values().next().value!);
  if(!gameId||unavailable.has(key))return;
  try{localStorage.setItem(key,JSON.stringify({version:1,topics:[...new Set([...topics(key),topic])].slice(-32)}));}catch{unavailable.add(key);}
}
export function claimPlanningGuidance(candidate:symbol):boolean {if(owner&&owner!==candidate)return false;owner=candidate;return true;}
export function releasePlanningGuidance(candidate:symbol):void {if(owner===candidate)owner=null;}

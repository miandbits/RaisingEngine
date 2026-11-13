import type {ContentSnapshot,RunState} from '../core/model.js';

export interface PriorityProfile {
  formatVersion:1;
  priorities:{attributeId:string;direction:'increase'|'decrease';weight:number}[];
  bands:{attributeId:string;min?:number;max?:number}[];
  id?:string;
  name?:string;
}
const object=(value:unknown):value is Record<string,unknown>=>!!value&&typeof value==='object'&&!Array.isArray(value);
const keys=(value:Record<string,unknown>,allowed:string[])=>Object.keys(value).every(key=>allowed.includes(key));
/** Authoring preferences only. They do not change content, commands or save rules. */
export function validatePriorityProfile(input:unknown,content:ContentSnapshot):PriorityProfile {
  if(!object(input)||input.formatVersion!==1||!keys(input,['formatVersion','priorities','bands','id','name']))throw Error('Unsupported player-priority profile.');
  if(!Array.isArray(input.priorities)||!Array.isArray(input.bands)||input.priorities.length>32||input.bands.length>32||input.priorities.length+input.bands.length===0)throw Error('Add at least one priority or resource band, with no more than 32 of each.');
  if((input.id===undefined)!==(input.name===undefined)||input.id!==undefined&&(typeof input.id!=='string'||!/^[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}$/.test(input.id)||typeof input.name!=='string'||input.name.trim().length<1||input.name.length>120))throw Error('A saved priority profile needs a valid ID and a name of 1–120 characters.');
  // The editor also validates profiles against a draft with unrelated broken
  // documents. Validate referenced definitions here, not the whole project.
  const attributes=new Map<string,Record<string,unknown>>(),ambiguous=new Set<string>();
  for(const row of Array.isArray(content?.attributes)?content.attributes:[])if(object(row)&&typeof row.id==='string'){
    if(attributes.has(row.id))ambiguous.add(row.id);attributes.set(row.id,row);
  }
  const definition=(id:string)=>{
    const row=attributes.get(id),integer=(value:unknown):value is number=>Number.isSafeInteger(value)&&Number(value)>=-2147483648&&Number(value)<=2147483647;
    if(!row||ambiguous.has(id)||id.length>64||!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(id)||typeof row.label!=='string'||!row.label.length||typeof row.kind!=='string'||!['stat','resource','condition'].includes(row.kind)||!integer(row.min)||!integer(row.max)||!integer(row.initial)||row.min>row.max||row.initial<row.min||row.initial>row.max)throw Error('Repair the referenced attribute definition before saving player priorities: '+id);
    return row as unknown as ContentSnapshot['attributes'][number];
 };
  const priorityIds=new Set<string>(),bandIds=new Set<string>();
  for(const row of input.priorities){
    if(!object(row)||!keys(row,['attributeId','direction','weight'])||typeof row.attributeId!=='string'||!attributes.has(row.attributeId))throw Error('A priority references an unknown attribute.');
    definition(row.attributeId);
    if(priorityIds.has(row.attributeId))throw Error('An attribute can appear only once in the priorities.');priorityIds.add(row.attributeId);
    if((row.direction!=='increase'&&row.direction!=='decrease')||!Number.isSafeInteger(row.weight)||Number(row.weight)<1||Number(row.weight)>10)throw Error('Choose increase or decrease and a whole-number priority weight from 1 to 10.');
  }
  for(const row of input.bands){
    if(!object(row)||!keys(row,['attributeId','min','max'])||typeof row.attributeId!=='string'||!attributes.has(row.attributeId))throw Error('A resource band references an unknown attribute.');
    if(bandIds.has(row.attributeId))throw Error('A resource can have only one comfort band.');bandIds.add(row.attributeId);
    const attribute=definition(row.attributeId);
    if(attribute.kind!=='resource'&&attribute.kind!=='condition')throw Error('Comfort bands apply to resource or condition attributes.');
    if(row.min===undefined&&row.max===undefined)throw Error('A resource band needs a minimum or maximum.');
    for(const limit of [row.min,row.max])if(limit!==undefined&&(!Number.isSafeInteger(limit)||Number(limit)<attribute.min||Number(limit)>attribute.max))throw Error('Resource band limits must be whole numbers within the declared attribute bounds.');
    if(row.min!==undefined&&row.max!==undefined&&Number(row.min)>Number(row.max))throw Error('A resource band minimum cannot exceed its maximum.');
  }
  return structuredClone(input) as unknown as PriorityProfile;
}
/** Immediate accepted-command results only; no lookahead or optimality claim. */
export function priorityScore(content:ContentSnapshot,before:RunState,after:RunState,profile:PriorityProfile):{deficit:number;gain:number} {
  const ranges=new Map(content.attributes.map(attribute=>[attribute.id,Math.max(1,attribute.max-attribute.min)]));
  let deficit=0,gain=0;
  for(const band of profile.bands){const value=after.attributes[band.attributeId]!;deficit+=(Math.max(0,(band.min??value)-value)+Math.max(0,value-(band.max??value)))/ranges.get(band.attributeId)!;}
  for(const priority of profile.priorities)gain+=(after.attributes[priority.attributeId]!-before.attributes[priority.attributeId]!)/ranges.get(priority.attributeId)!*priority.weight*(priority.direction==='increase'?1:-1);
  return {deficit,gain};
}

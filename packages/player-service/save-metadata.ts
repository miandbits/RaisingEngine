import type {ContentSnapshot,RunState} from '../core/model.js';

export interface ScheduleDraft {month:number;slot:number;selected:number;activities:string[]}
/** Portable slot preview metadata. Empty thumbnail means no image was captured. It is never consulted by simulation rules. */
export interface SaveRecord {version:1;date:string;place:string;screenId:string;phase:string;thumbnail:string;savedAt:string;sequence:number;automatic:boolean}
const recordKeys=['version','date','place','screenId','phase','thumbnail','savedAt','sequence','automatic'];
export function validSaveRecord(value:unknown):value is SaveRecord {
  if(!value||typeof value!=='object'||Array.isArray(value))return false;
  const r=value as Record<string,unknown>;
  if(Object.keys(r).length!==recordKeys.length||Object.keys(r).some(k=>!recordKeys.includes(k))||r.version!==1||typeof r.automatic!=='boolean'||!Number.isSafeInteger(r.sequence)||(r.sequence as number)<0)return false;
  for(const k of ['date','place','screenId','phase','thumbnail','savedAt'])if(typeof r[k]!=='string'||(r[k] as string).length>511)return false;
  return /^(frame-[0-9]+-[0-9]+\.png)?$/.test(r.thumbnail as string);
}
export function validScheduleDraft(content:ContentSnapshot,state:RunState,value:unknown):value is ScheduleDraft {
  if(!value||typeof value!=='object'||Array.isArray(value))return false;
  const d=value as ScheduleDraft,periods=content.project.periodsPerMonth;
  if(Object.keys(d).length!==4||Object.keys(d).some(k=>!['month','slot','selected','activities'].includes(k)))return false;
  if(!Number.isInteger(d.month)||d.month!==Math.floor(state.periodIndex/periods)||!Number.isInteger(d.slot)||d.slot<0||d.slot>=periods||!Number.isInteger(d.selected)||d.selected<0||d.selected>=(1<<periods)||!Array.isArray(d.activities)||d.activities.length!==periods)return false;
  return d.activities.every((id,i)=>typeof id==='string'&&((id===''&&!(d.selected&(1<<i)))||content.activities.some(a=>a.id===id)));
}

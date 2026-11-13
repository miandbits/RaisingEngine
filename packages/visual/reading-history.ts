/** Bounded, memory-only presentation receipts. Never part of canonical game state or saves. */
export const readingHistoryLimits = Object.freeze({entries:200,totalUtf8Bytes:256*1024,entryUtf8Bytes:16*1024,metadataUtf8Bytes:1024});
export type ReadingHistoryKind = 'dialogue'|'prompt'|'chosen-response';
export type ReadingHistoryReset = 'new-session'|'new-run'|'restored';
export interface ReadingHistoryReceipt {
  epoch:string;key:string;kind:ReadingHistoryKind;text:string;
  locale?:string;speaker?:string;date?:string;
}
export interface ReadingHistoryEntry {
  sequence:number;kind:ReadingHistoryKind;text:string;locale:string;speaker:string;date:string;truncated:boolean;
}
interface StoredEntry extends ReadingHistoryEntry {key:string;bytes:number}
export interface ReadingHistorySnapshot {epoch:string;reason:ReadingHistoryReset;entries:readonly ReadingHistoryEntry[];omittedEntries:number;utf8Bytes:number}
/** Iterate only to the byte budget and never split a code point or retain malformed UTF-16. */
export function readingHistoryPrefix(value:string,budget:number):{text:string;bytes:number;truncated:boolean} {
  const parts:string[]=[];let bytes=0,consumed=0;
  for(const raw of value){const point=raw.codePointAt(0)!;const scalar=point>=0xd800&&point<=0xdfff?'\ufffd':raw;const cp=scalar.codePointAt(0)!;const size=cp<=0x7f?1:cp<=0x7ff?2:cp<=0xffff?3:4;if(bytes+size>budget)break;parts.push(scalar);bytes+=size;consumed+=raw.length;}
  return {text:parts.join(''),bytes,truncated:consumed<value.length};
}
export class ReadingHistory {
  private entries:StoredEntry[]=[];private bytes=0;private omitted=0;private sequence=0;
  private epoch:string;private reason:ReadingHistoryReset='new-session';
  constructor(epoch:string){this.epoch=epoch;}
  reset(epoch:string,reason:ReadingHistoryReset='new-run'):void{this.epoch=epoch;this.reason=reason;this.entries=[];this.bytes=0;this.omitted=0;this.sequence=0;}
  snapshot():ReadingHistorySnapshot{return {epoch:this.epoch,reason:this.reason,entries:this.entries.map(({key:_,bytes:__,...entry})=>({...entry})),omittedEntries:this.omitted,utf8Bytes:this.bytes};}
  /** Hosts submit only foreground, visible, revealed text; rejected or stale UI actions submit nothing. */
  record(receipt:ReadingHistoryReceipt):boolean {
    if(receipt.epoch!==this.epoch||!receipt.key||!['dialogue','prompt','chosen-response'].includes(receipt.kind))return false;
    const metadata=readingHistoryLimits.metadataUtf8Bytes;
    // An overlong identity is rejected rather than truncated into another node's identity.
    const key=readingHistoryPrefix(receipt.key,metadata);if(key.truncated)return false;
    const locale=readingHistoryPrefix(receipt.locale??'',128),speaker=readingHistoryPrefix(receipt.speaker??'',metadata),date=readingHistoryPrefix(receipt.date??'',metadata);
    const overhead=key.bytes+locale.bytes+speaker.bytes+date.bytes;
    const text=readingHistoryPrefix(receipt.text,readingHistoryLimits.entryUtf8Bytes-overhead);
    if(!text.text.trim())return false;
    const entry:StoredEntry={sequence:0,key:key.text,kind:receipt.kind,text:text.text,locale:locale.text,speaker:speaker.text,date:date.text,truncated:text.truncated||locale.truncated||speaker.truncated||date.truncated,bytes:overhead+text.bytes};
    const last=this.entries.at(-1),same=last&&last.key===entry.key&&last.kind===entry.kind&&last.locale===entry.locale&&last.speaker===entry.speaker&&last.date===entry.date;
    if(same&&last.text===entry.text&&last.truncated===entry.truncated)return false;
    // Repainting a smaller typewriter prefix cannot remove text already presented.
    if(same&&receipt.kind!=='chosen-response'&&last.text.startsWith(entry.text)&&last.text!==entry.text)return false;
    if(same&&receipt.kind!=='chosen-response'&&entry.text.startsWith(last.text)){
      entry.sequence=last.sequence;this.bytes-=last.bytes;this.entries[this.entries.length-1]=entry;
    }else {entry.sequence=++this.sequence;this.entries.push(entry);}
    this.bytes+=entry.bytes;
    while(this.entries.length>readingHistoryLimits.entries||this.bytes>readingHistoryLimits.totalUtf8Bytes){this.bytes-=this.entries.shift()!.bytes;this.omitted=Math.min(Number.MAX_SAFE_INTEGER,this.omitted+1);}
    return true;
  }
}

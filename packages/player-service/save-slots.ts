import type {ReadonlyDesign} from '../visual/model.js';
import {resolveScreenElements} from '../visual/components.js';
import type {SaveRecord} from './save-metadata.js';

export interface SlotKey {automatic:boolean;index:number}
export interface SlotRow extends SlotKey {status:'empty'|'ready'|'invalid'|'incompatible';date?:string;periodIndex?:number;record?:SaveRecord}
export interface SaveSlotsView {checkpointPending?:boolean;rows:readonly SlotRow[];screenId:string|null;automatic:boolean;selected:number;page:number;confirmation:'save'|'load'|'import'|null;available:boolean;current?:Pick<SaveRecord,'date'|'place'>&Partial<Pick<SaveRecord,'phase'>>}
export type SaveSlotAction={type:'open';kind:'save'|'load';screenId?:string}|{type:'close'|'cancel'|'save'|'load'|'confirm'|'import'|'export'}|{type:'group';automatic:boolean}|{type:'select';index:number}|{type:'page';delta:number};
export function slotCount(design:ReadonlyDesign,automatic:boolean):number {return automatic?design.saveSystem?.autoSlots??3:design.saveSystem?.manualSlots??6;}
export function slotsEnabled(design:ReadonlyDesign|null|undefined):boolean {return !!design&&(!!design.saveSystem||[...design.screens.flatMap(s=>s.elements),...(design.components??[]).flatMap(c=>c.elements)].some(e=>e.kind==='saveSlots'||e.kind==='loadSlots'));}
/** Save-slot capability includes its authored modals, not unrelated composition features. */
export function slotScreens(design:ReadonlyDesign,kind?:'save'|'load'):string[]{return design.screens.filter(s=>s.modal&&resolveScreenElements(design,s.id).some(e=>kind?e.kind===(kind==='save'?'saveSlots':'loadSlots'):e.kind==='saveSlots'||e.kind==='loadSlots')).map(s=>s.id);}
export function slotWords(locale:string|undefined,ko:string,en:string,ja:string):string{return locale?.startsWith('en')?en:locale?.startsWith('ja')?ja:ko;}

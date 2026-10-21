import type { Element } from './model.js';
import type { RunState } from '../core/model.js';
import type { NumericBindings } from './bindings.js';

/** Numeric projection for a gauge, using the same ownership rules as text. */
export function dataBindingValue(element: Element, context: NumericBindings & {dataState?: RunState | null}): number | undefined {
  if (!element.dataBinding) return undefined;
  const text = dataBindingText({...element,binding:'data-value',valueFormat:'{value}'},context);
  if (text === null || text === '—') return undefined;
  const value = Number(text);
  return Number.isFinite(value) ? value : undefined;
}

/** Read-only field projection, shared by all screens and reusable components. */
export function dataBindingText(element: Element, context: NumericBindings & {dataState?: RunState | null}): string | null {
  if (element.binding !== 'data-value') return null;
  const target = element.dataBinding, state = context.dataState;
  if (!target) return '—';
  const owner = target.owner?.kind === 'character' ? target.owner.characterId : target.owner ? state?.activeNarrative?.actorId : undefined;
  let value: number | undefined, label = '';
  if (target.kind === 'attribute') {
    const row=context.stats.find(a=>a.id===target.attributeId);label=row?.label??'';
    value=target.owner ? owner ? state?.roster?.attributes[owner]?.[target.attributeId] : undefined : row?.value;
  } else if (target.kind === 'relationship') {
    const row=context.relationships?.find(r=>r.characterId===target.characterId&&r.relationshipId===target.relationshipId);label=row?`${row.name} · ${row.label}`:'';
    value=target.owner ? owner ? state?.roster?.relationships?.[owner]?.[target.characterId]?.[target.relationshipId] : undefined : row?.value;
  } else {
    const row=context.items?.find(i=>i.id===target.itemId);label=row?.name??'';
    value=target.owner ? owner ? state?.roster?.inventory?.[owner]?.[target.itemId] : undefined : context.gameplay?.inventory.find(i=>i.id===target.itemId)?.quantity??row?.initialQuantity;
  }
  return (element.valueFormat??'{label}: {value}').replace(/\{(value|label)\}/g,(_,key:string)=>key==='label'?label:value===undefined?'—':String(value));
}

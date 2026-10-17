import type { Element } from './model.js';
/** Values are inserted as text by renderers, never parsed as markup. */
export function userFieldBinding(element: Element, values?: Readonly<Record<string,string>>): string | null {
  if(element.binding!=='user-field')return null;
  const value=values?.[element.userFieldId??'']??'';
  return (element.valueFormat??'{value}').replace(/\{value\}/g,()=>value);
}
export interface NumericBindings {
  stats: readonly { id: string; label: string; value: number }[];
  relationships?: readonly { characterId?: string; relationshipId?: string; name: string; label: string; value: number; delta?: number }[];
  items?: readonly { id: string; name?: string; initialQuantity?: number }[];
  gameplay?: { inventory: readonly { id: string; quantity: number }[] } | null;
}
/** A deliberately small display template; no script evaluation is involved. */
export function numericBinding(element: Element, values: NumericBindings): string | null {
  let value = 0, label = '', delta = 0;
  if (element.binding === 'attribute-value') {
    const stat = values.stats.find(a => a.id === element.attributeId);
    if (!stat) return '—'; value = stat.value; label = stat.label;
  } else if (element.binding === 'relationship-value') {
    const relationship = values.relationships?.find(r => r.characterId === element.characterId && r.relationshipId === element.relationshipId);
    if (!relationship) return '—'; value = relationship.value; delta = relationship.delta ?? 0; label = `${relationship.name} · ${relationship.label}`;
  } else if (element.binding === 'item-count') {
    const item = values.items?.find(i => i.id === element.itemId);
    if (!item) return '—'; label = item.name ?? item.id;
    value = values.gameplay ? values.gameplay.inventory.find(i => i.id === item.id)?.quantity ?? 0 : item.initialQuantity ?? 0;
  } else return null;
  const tokens: Record<string,string> = {value:String(value),label,delta:delta>0?`+${delta}`:String(delta)};
  return (element.valueFormat ?? '{value}').replace(/\{(value|label|delta)\}/g, (_, key: string) => tokens[key]!);
}

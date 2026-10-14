import React from 'react';
import type { Element } from '../../../packages/visual/model.js';
type Row = Record<string, any>;
export function DataBindingFields({ element, attributes, characters, items, onChange }: {
  element: Element; attributes: readonly Row[]; characters: readonly Row[]; items: readonly Row[]; onChange: (patch: Partial<Element>) => void;
}) {
  if (!['attribute-value','relationship-value','item-count'].includes(element.binding ?? '')) return null;
  const select = (field: 'attributeId'|'characterId'|'relationshipId'|'itemId', label: string, rows: readonly Row[]) => <label>{label}
    <select aria-label={label} value={element[field] ?? ''} onChange={e => {
      const patch: Partial<Element> = { [field]: e.target.value || undefined };
      if (field === 'characterId') Object.assign(patch, { relationshipId: undefined }); onChange(patch);
    }}><option value="">선택하세요</option>{rows.map(row => <option key={row.id} value={row.id}>{row.label ?? row.name ?? row.id}</option>)}</select>
  </label>;
  return <div className="binding-fields">
    {element.binding === 'attribute-value' && select('attributeId','표시할 능력·자원',attributes)}
    {element.binding === 'relationship-value' && <>{select('characterId','관계 인물',characters)}{select('relationshipId','표시할 관계',characters.find(c=>c.id===element.characterId)?.relationships ?? [])}</>}
    {element.binding === 'item-count' && select('itemId','수량을 표시할 아이템',items)}
    <label>숫자 표시 형식<input aria-label="숫자 표시 형식" value={element.valueFormat ?? '{value}'} maxLength={256} onChange={e=>onChange({valueFormat:e.target.value})}/></label>
    <p><code>{'{value}'}</code>는 현재 수치, <code>{'{label}'}</code>은 이름입니다. 예: 소지금 {'{value}'} G</p>
  </div>;
}

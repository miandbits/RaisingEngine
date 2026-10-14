import React from 'react';
import type { Element } from '../../../packages/visual/model.js';
import type { SystemsDefinition } from '../../../packages/systems/model.js';
export function SystemsActionFields({ element, systems, onChange }: { element: Element; systems?: SystemsDefinition | undefined; onChange: (patch: Partial<Element>) => void }) {
  const pick = (label: string, value: string | undefined, rows: { id: string; name: string }[], change: (id: string) => void) => <label>{label}<select aria-label={label} value={value ?? ''} onChange={e => change(e.target.value)}><option value="">선택</option>{rows.map(row => <option key={row.id} value={row.id}>{row.name} · {row.id}</option>)}</select></label>;
  if (element.action === 'acceptQuest' || element.action === 'claimQuest') return pick('UI 퀘스트', element.questId, systems?.quests ?? [], questId => onChange({ questId }));
  if (element.action === 'equip') return pick('UI 장비', element.equipmentId, systems?.equipment ?? [], equipmentId => onChange({ equipmentId }));
  if (element.action === 'unequip') return pick('UI 장착 부위', element.slot, [{ id: 'weapon', name: '무기' }, { id: 'armor', name: '방어구' }, { id: 'accessory', name: '장신구' }], slot => onChange({ slot: slot as NonNullable<Element['slot']> }));
  if (element.action === 'moveExpedition') return pick('UI 탐험 노드', element.nodeId, [...new Map((systems?.expeditions ?? []).flatMap(trip => trip.nodes.map(node => [node.id, { id: node.id, name: `${trip.name} / ${node.name}` }] as const))).values()], nodeId => onChange({ nodeId }));
  if (element.action === 'battleAction') return <>{pick('UI 전투 행동', element.battleAction, [{ id: 'attack', name: '공격' }, { id: 'guard', name: '방어' }, { id: 'useItem', name: '회복 아이템' }, { id: 'flee', name: '도주' }], battleAction => onChange({ battleAction: battleAction as NonNullable<Element['battleAction']> }))}{element.battleAction === 'useItem' && pick('UI 회복 아이템', element.itemId, (systems?.consumables ?? []).map(item => ({ id: item.itemId, name: `회복 +${item.heal}` })), itemId => onChange({ itemId }))}</>;
  return null;
}

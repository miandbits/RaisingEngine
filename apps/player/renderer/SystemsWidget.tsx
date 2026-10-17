import React from 'react';
import type { SystemsCommand, SystemsView } from '../../../packages/systems/model.js';

export function SystemsWidget({ kind, systems, busy, editing, onCommand }: {
  kind: string; systems?: SystemsView | null | undefined; busy: boolean; editing: boolean;
  onCommand?: ((command: SystemsCommand) => void) | undefined;
}) {
  const run = (command: SystemsCommand) => { if (!busy && !editing) onCommand?.(command); };
  const labels: Record<string, string> = { equipment: '장비', quests: '퀘스트', expedition: '탐험', battle: '전투', 'relationship-stages': '관계 단계' };
  if (!systems) return <div className="journey-widget"><h3>{labels[kind]}</h3><p className="journey-empty">{editing ? '플레이 중 연결된 시스템을 표시합니다.' : '탐험·성장 시스템을 설정하세요.'}</p></div>;
  if (kind === 'equipment') return <div className="journey-widget" data-testid="equipment-widget"><h3>장비</h3><p>체력 {systems.heroStats.maxHp} · 공격 {systems.heroStats.attack} · 방어 {systems.heroStats.defense}</p>
    {systems.equipment.map(row => <div className="journey-card" key={row.id}><strong>{row.name}</strong><small>소지 {row.owned} · {({ weapon: '무기', armor: '방어구', accessory: '장신구' })[row.slot]}</small><button disabled={busy || !(row.equipped ? row.canUnequip : row.enabled)} onClick={() => run(row.equipped ? { type: 'unequip', slot: row.slot } : { type: 'equip', equipmentId: row.id })}>{row.equipped ? '장착 해제' : '장착'} · {row.name}</button>{row.reason && !(row.equipped && row.canUnequip) && <small>{row.reason}</small>}</div>)}
    {!systems.equipment.length && <p>등록된 장비가 없습니다.</p>}</div>;
  if (kind === 'quests') return <div className="journey-widget" data-testid="quests-widget"><h3>퀘스트</h3>{systems.quests.map(row => <div className="journey-card" key={row.id}><strong>{row.name}</strong><small>{({ available: '수락 가능', locked: '잠김', accepted: '진행 중', completed: '목표 달성', claimed: '보상 수령 완료' })[row.status]} · {row.progress}/{row.targetCount}</small>
    {['available', 'locked'].includes(row.status) && <button disabled={busy || !row.canAccept} onClick={() => run({ type: 'acceptQuest', questId: row.id })}>수락 · {row.name}</button>}
    {['accepted', 'completed'].includes(row.status) && <button disabled={busy || !row.canClaim} onClick={() => run({ type: 'claimQuest', questId: row.id })}>보상 받기 · {row.name}</button>}{row.reason && <small>{row.reason}</small>}</div>)}{!systems.quests.length && <p>등록된 퀘스트가 없습니다.</p>}</div>;
  if (kind === 'expedition') {
    const trip = systems.expedition;
    return <div className="journey-widget" data-testid="expedition-widget"><h3>{trip?.name ?? '탐험'}</h3>{trip ? <><p>{trip.nodeName} · 체력 {trip.heroHp}/{trip.stats.maxHp}</p>{trip.nodeDescription && <p className="expedition-description" style={{ whiteSpace: 'pre-wrap', overflowWrap: 'anywhere' }}>{trip.nodeDescription}</p>}{trip.connections.map(node => <button key={node.id} disabled={busy || !node.enabled} onClick={() => run({ type: 'moveExpedition', nodeId: node.id })}>이동 · {node.name}</button>)}{trip.manualBattle && <button disabled={busy || !trip.manualBattle.enabled} title={trip.manualBattle.reason ?? undefined} onClick={() => run({ type: 'startNodeBattle' })}>{trip.manualBattle.name}와 싸운다</button>}<button disabled={busy || !trip.canReturn} onClick={() => run({ type: 'returnFromExpedition' })}>탐험에서 귀환</button></> : <p>일정에 탐험 활동을 배치해 출발하세요.</p>}</div>;
  }
  if (kind === 'battle') {
    const battle = systems.battle;
    return <div className="journey-widget" data-testid="battle-widget"><h3>{battle ? `${battle.enemyName} · ${battle.turn}턴` : '전투'}</h3>{battle ? <><p>내 체력 {battle.heroHp}/{battle.heroMaxHp}</p><progress aria-label="내 체력" value={battle.heroHp} max={battle.heroMaxHp}/><p>적 체력 {battle.enemyHp}/{battle.enemyMaxHp}</p><progress aria-label="적 체력" value={battle.enemyHp} max={battle.enemyMaxHp}/>
      <button disabled={busy} onClick={() => run({ type: 'battleAction', action: 'attack' })}>공격</button><button disabled={busy} onClick={() => run({ type: 'battleAction', action: 'guard' })}>방어</button>
      {systems.consumables.map(item => <button key={item.itemId} disabled={busy || !item.enabled} onClick={() => run({ type: 'battleAction', action: 'useItem', itemId: item.itemId })}>{item.name} 사용 · {item.quantity}개 · 체력 +{item.heal}</button>)}
      <button disabled={busy} onClick={() => run({ type: 'battleAction', action: 'flee' })}>도주</button></> : <p>진행 중인 전투가 없습니다.</p>}
      {systems.lastBattle && <p>지난 결과: {({ victory: '승리', defeat: '패배', fled: '도주' })[systems.lastBattle.outcome]} · 받은 피해 {systems.lastBattle.heroDamage} · 준 피해 {systems.lastBattle.enemyDamage}</p>}
    </div>;
  }
  return <div className="journey-widget" data-testid="stages-widget"><h3>관계 단계</h3>{systems.stages.map(stage => <p key={stage.id}>{stage.name} · {stage.unlocked ? '해금' : `필요 수치 ${stage.threshold}`}</p>)}</div>;
}

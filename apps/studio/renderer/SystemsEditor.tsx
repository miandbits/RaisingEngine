import React, { useState } from 'react';
import type { DraftDocument, StudioView } from '../../../packages/studio-service/editor.js';
import type { Json } from '../../../packages/validation/model.js';
import type { SystemsDefinition } from '../../../packages/systems/model.js';
import { SchemaSection } from './SchemaForm.js';
import { GameplayList } from './GameplayList.js';
import { createVisual, type VisualDesign } from '../../../packages/visual/model.js';
import { ensureSystemsScreens } from '../../../packages/visual/systems-screens.js';
import './gameplay-editor.css';

type Category = 'hero' | 'equipment' | 'consumables' | 'enemies' | 'expeditions' | 'quests' | 'relationshipStages';
export interface SystemsFocus { category: Category; id: string; filter: string }
const names: Record<Category, string> = { hero: '전투 능력', equipment: '장비', consumables: '회복 아이템', enemies: '적', expeditions: '탐험', quests: '퀘스트', relationshipStages: '관계 단계' };
const object = (value: Json | undefined): Record<string, Json> => value && typeof value === 'object' && !Array.isArray(value) ? value : {};
const asJson = (value: unknown) => value as Json;
const keyOf = (row: Json) => String(object(row).id ?? object(row).itemId ?? '');

export function SystemsEditor({ view, focus, onFocus, onChange, onPreview, onWorkshop }: {
  view: StudioView; focus: SystemsFocus; onFocus: (next: SystemsFocus) => void;
  onChange: (document: DraftDocument, value: Json) => void; onPreview: () => void; onWorkshop: () => void;
}) {
  const project = view.documents.find(document => document.kind === 'project')!;
  const root = object(project.data), gameplay = object(root.gameplay);
  const definition = root.systems as unknown as SystemsDefinition | undefined;
  const [newId, setNewId] = useState(''), [newName, setNewName] = useState(''), [error, setError] = useState('');
  const [deleting, setDeleting] = useState(false);
  const attributes = view.documents.filter(d => d.kind === 'attributes').flatMap(d => object(d.data).attributes as Json[] ?? []);
  const items = (gameplay.items as Json[] | undefined) ?? [], locations = (gameplay.locations as Json[] | undefined) ?? [];
  const characters = view.documents.filter(d => d.kind === 'character');
  const category = focus.category;
  const rows = definition && category !== 'hero' ? asJson(definition[category]) as Json[] : [];
  const selectedIndex = rows.findIndex(row => keyOf(row) === focus.id);
  const index = selectedIndex < 0 ? 0 : selectedIndex;
  const row = rows[index];
  const commit = (next: SystemsDefinition) => onChange(project, { ...root, systems: asJson(next) });
  const firstId = (list: Json[]) => keyOf(list[0] ?? {});
  const rewards = () => ({ items: [], effects: [] });
  function prepareScreens() {
    const visual = ensureSystemsScreens(structuredClone((root.visual as unknown as VisualDesign) ?? createVisual()));
    const homeId = object((gameplay.locations as Json[] ?? []).find(location => object(location).id === gameplay.startLocationId)).screenId;
    const home = visual.screens.find(screen => screen.id === homeId) ?? visual.screens.find(screen => screen.id === 'schedule');
    if (home && !home.elements.some(element => element.action === 'navigate' && element.screenId === 'adventure-hub')) {
      let id = 'adventure-link'; for (let i = 2; home.elements.some(element => element.id === id); i++) id = `adventure-link-${i}`;
      home.elements.push({ id, kind: 'button', text: '모험 준비', action: 'navigate', screenId: 'adventure-hub', x: Math.round(visual.width * 1130 / 1280), y: Math.round(visual.height * 30 / 720), width: Math.round(visual.width * 130 / 1280), height: Math.round(visual.height * 48 / 720), fill: '#355c4c', color: '#ffffff', fontSize: 18 });
    }
    onChange(project, { ...root, visual: asJson(visual) });
  }
  function create() {
    if (!definition || category === 'hero') return;
    let id = newId.trim();
    if (category === 'consumables') id = id || firstId(items.filter(item => !rows.some(row => object(row).itemId === object(item).id)));
    if (!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(id) || id.length > 64) { setError('ID는 영문 소문자로 시작하고 숫자·하이픈을 사용하세요.'); return; }
    if (rows.some(value => keyOf(value) === id)) { setError('이미 있는 ID입니다.'); return; }
    const base = { id, name: newName.trim() || id };
    const firstEnemy = definition.enemies[0]?.id ?? '';
    const defaults: Record<Exclude<Category, 'hero'>, unknown> = {
      equipment: { ...base, itemId: firstId(items), slot: 'weapon', modifiers: { maxHp: 0, attack: 1, defense: 0 } },
      consumables: { itemId: id, heal: 10 },
      enemies: { ...base, maxHp: 10, attack: 2, defense: 0, rewards: rewards() },
      expeditions: { ...base, locationId: firstId(locations), entryNodeId: 'entrance', nodes: [{ id: 'entrance', name: '입구', connections: [], exit: true }] },
      quests: { ...base, locationId: firstId(locations), requirements: { op: 'always' }, enemyId: firstEnemy, targetCount: 1, rewards: rewards() },
      relationshipStages: { ...base, characterId: String(object(characters[0]?.data).id ?? ''), relationshipId: firstId(object(characters[0]?.data).relationships as Json[] ?? []), threshold: 10 },
    };
    commit({ ...definition, [category]: [...rows, defaults[category]] } as SystemsDefinition);
    onFocus({ ...focus, id, filter: '' }); setNewId(''); setNewName(''); setError('');
  }
  if (!definition) return <section className="gameplay-intro"><p className="eyebrow">EXPEDITIONS & GROWTH</p><h1>탐험과 성장을 연결하세요</h1><p>퀘스트·장비·탐험·전투·관계 단계가 같은 일정과 저장 기록으로 이어집니다. 먼저 육성 작업실에서 장소와 아이템을 만드세요.</p>
    {!root.gameplay ? <button onClick={onWorkshop}>육성 작업실 열기</button> : <button disabled={!attributes.length} className="studio-primary" onClick={() => {
      const ids = attributes.map(value => String(object(value).id));
      commit({ hero: { maxHpAttributeId: ids.find(id => /stamina|health|hp/.test(id)) ?? ids[0]!, attackAttributeId: ids.find(id => /sword|attack|strength/.test(id)) ?? ids[0]!, defenseAttributeId: ids.find(id => /defense|craft/.test(id)) ?? ids[0]! }, equipment: [], consumables: [], enemies: [], expeditions: [], quests: [], relationshipStages: [] });
    }}>탐험·성장 시스템 시작</button>}
    {!attributes.length && <p>능력·자원을 먼저 한 개 이상 정의하세요.</p>}</section>;
  return <section className="gameplay-editor" aria-label="탐험·성장 편집기">
    <header className="gameplay-heading"><div><p className="eyebrow">EXPEDITIONS & GROWTH</p><h1>탐험·성장 작업실</h1><p>장비를 준비하고, 일정으로 떠나고, 보상으로 관계를 이어갑니다.</p></div><button onClick={prepareScreens}>플레이 화면 준비</button><button onClick={onPreview}>초안 미리보기</button></header>
    <nav className="journey-steps" aria-label="시스템 종류">{(Object.keys(names) as Category[]).map(key => <button key={key} aria-pressed={category === key} onClick={() => { onFocus({ category: key, id: '', filter: '' }); setError(''); setDeleting(false); }}>{names[key]}</button>)}</nav>
    <div className="gameplay-grid">
      <aside className="gameplay-browser">{category !== 'hero' && <>
        <label>검색<input aria-label="시스템 검색" value={focus.filter} onChange={event => onFocus({ ...focus, filter: event.target.value })}/></label>
        <GameplayList items={rows.map(value => ({ id: keyOf(value), name: String(object(value).name ?? object(items.find(item => object(item).id === object(value).itemId)).name ?? keyOf(value)) }))} selected={row ? keyOf(row) : ''} query={focus.filter} onSelect={id => { onFocus({ ...focus, id }); setDeleting(false); }}/>
        <label>{category === 'consumables' ? '회복 아이템 ID' : '새 항목 ID'}<input aria-label="새 시스템 ID" value={newId} onChange={event => setNewId(event.target.value)}/></label>
        {category !== 'consumables' && <label>이름<input aria-label="새 시스템 이름" value={newName} onChange={event => setNewName(event.target.value)}/></label>}
        <button onClick={create}>{names[category]} 추가</button>{error && <p role="alert">{error}</p>}
      </>}</aside>
      <div className="gameplay-form">{category === 'hero' ? <SchemaSection schemaRef="systems.schema.json#/properties/hero" field="hero" value={asJson(definition.hero)} document={project} documents={view.documents} onChange={value => commit({ ...definition, hero: value as unknown as SystemsDefinition['hero'] })}/> : row ? <>
        <header className="gameplay-row-heading"><h2>{String(object(row).name ?? keyOf(row))}</h2><button onClick={() => setDeleting(true)}>항목 삭제</button></header>
        {deleting && <div role="alert"><p>연결된 참조는 자동으로 지우지 않습니다. 삭제 후 검증에서 남은 참조를 확인하세요.</p><button onClick={() => { commit({ ...definition, [category]: rows.filter((_, i) => i !== index) } as SystemsDefinition); onFocus({ ...focus, id: '' }); setDeleting(false); }}>삭제 확인</button><button onClick={() => setDeleting(false)}>취소</button></div>}
        <SchemaSection key={`${category}-${index}`} schemaRef={`systems.schema.json#/properties/${category}/items`} value={row} document={{ ...project, data: row }} documents={view.documents} onChange={value => { commit({ ...definition, [category]: rows.map((old, i) => i === index ? value : old) } as SystemsDefinition); onFocus({ ...focus, id: keyOf(value) }); }}/>
        {category === 'expeditions' && <fieldset><legend>일정 활동에 연결</legend><p>아래 활동을 일정에 배치하면 이 탐험을 시작합니다. 비용과 기본 효과는 활동에서 설정합니다.</p>{view.documents.filter(d => d.kind === 'activity').map(activity => <label key={activity.file} className="gameplay-check"><input type="checkbox" aria-label={`${String(object(activity.data).name ?? object(activity.data).label)} 탐험 연결`} disabled={Boolean(object(activity.data).successCheck) && object(activity.data).expeditionId !== object(row).id} checked={object(activity.data).expeditionId === object(row).id} onChange={event => {
          const next = { ...object(activity.data) }; if (event.target.checked) { if (next.successCheck) return; next.expeditionId = object(row).id!; } else delete next.expeditionId;
          onChange(activity, next);
        }}/>{String(object(activity.data).name ?? object(activity.data).label ?? object(activity.data).id)}{Boolean(object(activity.data).successCheck) && <small>성공 판정이 있는 활동입니다. 활동 편집에서 성공·실패 설정을 직접 정리하거나 별도의 탐험 활동을 만들어 연결하세요.</small>}</label>)}</fieldset>}
      </> : <p>왼쪽에서 {names[category]} 항목을 추가하세요.</p>}</div>
      <aside className="gameplay-guide"><h3>제작 순서</h3><ol><li>능력과 장비·회복 아이템 연결</li><li>적과 승리 보상 정의</li><li>탐험 노드·이동·귀환 연결</li><li>탐험을 일정 활동에 연결</li><li>퀘스트 목표와 보상 정의</li><li>관계 단계와 사건 연결</li></ol><p>데이터 변경은 기존 편집 서비스의 버전 검사·실행 취소·자동 검증을 사용합니다.</p><button onClick={onWorkshop}>장소·아이템 편집</button></aside>
    </div>
  </section>;
}

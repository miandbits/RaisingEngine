import React, { useEffect, useMemo, useRef, useState } from 'react';
import { kindNames, searchCatalog, type CatalogItem, type CatalogQuery } from '../../../packages/analysis-service/catalog.js';
export function Explorer({ items, root, selected, onSelect, onAdd, onTags }: { items: CatalogItem[]; root: string; selected: string; onSelect: (item: CatalogItem) => void; onAdd: () => void; onTags: (files: string[], tag: string) => void }) {
  const [query, setQuery] = useState<CatalogQuery>({ kind: 'activity', sort: 'label' }); const [scroll, setScroll] = useState(0);
  const [checked, setChecked] = useState<string[]>([]); const [tag, setTag] = useState('');
  const [saved, setSaved] = useState<{ name: string; query: CatalogQuery }[]>([]); const [name, setName] = useState('');
  const viewport = useRef<HTMLDivElement>(null); const storage = `raising-search:${root}`;
  useEffect(() => { try { setSaved(JSON.parse(localStorage.getItem(storage) ?? '[]')); } catch { setSaved([]); } setChecked([]); }, [root]);
  const rows = useMemo(() => searchCatalog(items, query), [items, query]);
  useEffect(() => { setScroll(0); if (viewport.current) viewport.current.scrollTop = 0; }, [query]);
  const start = Math.max(0, Math.floor(scroll / 66) - 3), visible = rows.slice(start, start + 22);
  const update = (values: Partial<CatalogQuery>) => setQuery(q => ({ ...q, ...values }));
  const counts = useMemo(() => items.reduce<Record<string, number>>((m, i) => { m[i.kind] = (m[i.kind] ?? 0) + 1; return m; }, {}), [items]);
  return <aside className="content-explorer"><div className="section-heading"><h2>콘텐츠 탐색</h2><button onClick={onAdd}>+ 추가</button></div>
    <nav className="kind-grid" aria-label="콘텐츠 종류">{Object.entries(kindNames).map(([kind, title]) => <button key={kind} aria-pressed={query.kind === kind} onClick={() => update({ kind })}>{title} <small>{counts[kind] ?? 0}</small></button>)}</nav>
    <input aria-label="콘텐츠 검색" placeholder="이름 · ID · 설명 검색" value={query.text ?? ''} onChange={e => update({ text: e.target.value })}/>
    <div className="search-filters"><select aria-label="태그 필터" value={query.tag ?? ''} onChange={e => update({ tag: e.target.value })}><option value="">모든 태그</option>{[...new Set(items.flatMap(i => i.tags))].sort().map(t => <option key={t}>{t}</option>)}</select>
      <select aria-label="관련 인물 필터" value={query.person ?? ''} onChange={e => update({ person: e.target.value })}><option value="">모든 인물</option>{items.filter(i => i.kind === 'character').map(i => <option key={i.key} value={i.id}>{i.label}</option>)}</select>
      <input aria-label="발생 기간 필터" type="number" min="0" placeholder="기간 (0부터)" value={query.period ?? ''} onChange={e => { const q = { ...query }; if (e.target.value === '') delete q.period; else q.period = Number(e.target.value); setQuery(q); }}/>
      <select aria-label="콘텐츠 정렬" value={query.sort} onChange={e => update({ sort: e.target.value as NonNullable<CatalogQuery['sort']> })}><option value="label">이름순</option><option value="id">ID순</option><option value="period">발생 기간순</option><option value="priority">우선순위</option></select>
    </div><label className="inline-check"><input type="checkbox" checked={!!query.issuesOnly} onChange={e => update({ issuesOnly: e.target.checked })}/>진단 있는 항목만</label>
    <div className="search-save"><input aria-label="검색 이름" placeholder="검색 조건 이름" value={name} onChange={e => setName(e.target.value)}/><button disabled={!name.trim()} onClick={() => { const next = [...saved.filter(s => s.name !== name.trim()), { name: name.trim(), query }].slice(-20); setSaved(next); localStorage.setItem(storage, JSON.stringify(next)); setName(''); }}>검색 저장</button></div>
    {saved.length > 0 && <select aria-label="저장한 검색" value="" onChange={e => { const q = saved[Number(e.target.value)]; if (q) setQuery(q.query); }}><option value="">저장한 검색 선택</option>{saved.map((s, i) => <option key={s.name} value={i}>{s.name}</option>)}</select>}
    <p className="result-count">{rows.length}개 결과 · 이름 / ID / 요약</p>
    <div className="catalog-viewport" ref={viewport} onScroll={e => setScroll(e.currentTarget.scrollTop)} role="list" aria-label="콘텐츠 검색 결과"><div style={{ height: rows.length * 66, position: 'relative' }}>{visible.map((item, i) => <div role="listitem" className={`catalog-row ${selected === item.key ? 'selected' : ''}`} key={item.key} style={{ position: 'absolute', top: (start + i) * 66, height: 66, width: '100%' }}>
      <input type="checkbox" aria-label={`${item.label} 선택`} checked={checked.includes(item.file)} onChange={e => setChecked(c => e.target.checked ? [...new Set([...c, item.file])] : c.filter(f => f !== item.file))}/>
      <button onClick={() => onSelect(item)} title={`${item.description}\n${item.summary}`}><strong>{item.label}</strong><small>{item.id}{item.issues ? ` · 진단 ${item.issues}` : ''}{item.from !== null ? ` · 기간 ${item.from}~${item.through}` : ''}</small><small>{item.summary || item.tags.join(' · ') || item.file}</small></button>
    </div>)}</div></div>
    <div className="search-save"><input aria-label="일괄 태그" placeholder="선택 문서에 추가할 태그" maxLength={64} value={tag} onChange={e => setTag(e.target.value)}/><button disabled={!checked.length || !tag.trim()} onClick={() => { onTags(checked, tag.trim()); setChecked([]); setTag(''); }}>태그 지정 ({checked.length})</button></div><small>태그는 선택한 항목의 소속 문서에 지정됩니다.</small>
  </aside>;
}

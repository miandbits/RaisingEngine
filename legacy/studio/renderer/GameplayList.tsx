import React, { useEffect, useMemo, useRef, useState } from 'react';
interface Item { id: string; name: string }
/** A fixed-size viewport keeps large item/location libraries inexpensive to browse. */
export function GameplayList({ items, selected, query, onSelect }: {
  items: readonly Item[]; selected: string; query: string; onSelect: (id: string) => void;
}) {
  const rows = useMemo(() => items.filter(i => `${i.name} ${i.id}`.toLocaleLowerCase().includes(query.toLocaleLowerCase())), [items, query]);
  const [scroll, setScroll] = useState(0), viewport = useRef<HTMLDivElement>(null);
  const rowHeight = 66, visibleCount = 10, start = Math.max(0, Math.floor(scroll / rowHeight) - 2);
  useEffect(() => { if (viewport.current) viewport.current.scrollTop = 0; setScroll(0); }, [query]);
  useEffect(() => {
    const index = rows.findIndex(i => i.id === selected), node = viewport.current;
    if (!node || index < 0) return;
    const top = index * rowHeight;
    if (top < node.scrollTop || top + rowHeight > node.scrollTop + node.clientHeight) node.scrollTop = top;
  }, [selected]);
  return <><small className="gameplay-list-count">{rows.length} / {items.length}개</small>
    <div className="gameplay-list-viewport" ref={viewport} onScroll={e => setScroll(e.currentTarget.scrollTop)} aria-label="육성 콘텐츠 목록">
      <div style={{ height: rows.length * rowHeight, position: 'relative' }}>
        {rows.slice(start, start + visibleCount).map((row, index) => <button key={row.id} className="gameplay-row" aria-pressed={row.id === selected}
          style={{ position: 'absolute', top: (start + index) * rowHeight, height: rowHeight - 6, width: '100%' }}
          title={`${row.name} · ${row.id}`} onClick={() => onSelect(row.id)}><strong>{row.name}</strong><small>{row.id}</small></button>)}
      </div>
      {!rows.length && <p className="gameplay-empty">검색 결과가 없습니다.</p>}
    </div></>;
}

export function ConnectionPicker({ locations, currentId, connected, onToggle }: {
  locations: readonly Item[]; currentId: string; connected: readonly string[]; onToggle: (id: string, checked: boolean) => void;
}) {
  const [query, setQuery] = useState('');
  useEffect(() => setQuery(''), [currentId]);
  const rows = locations.filter(l => l.id !== currentId && `${l.name} ${l.id}`.toLocaleLowerCase().includes(query.toLocaleLowerCase()));
  return <fieldset><legend>오갈 수 있는 장소 · 양방향 연결</legend>
    <input aria-label="연결할 장소 검색" placeholder="장소 이름 또는 ID 검색" value={query} onChange={e => setQuery(e.target.value)}/>
    <div className="connection-options">{rows.slice(0, 100).map(location => <label className="gameplay-check" key={location.id}>
      <input type="checkbox" aria-label={`${location.name} 연결`} checked={connected.includes(location.id)} onChange={e => onToggle(location.id, e.target.checked)}/>{location.name}
    </label>)}</div>
    {rows.length > 100 && <p>검색 결과 {rows.length}개 중 100개를 표시합니다. 이름으로 좁혀 주세요.</p>}
    {!rows.length && <p>{locations.length < 2 ? '장소를 더 추가하면 서로 연결할 수 있습니다.' : '검색 결과가 없습니다.'}</p>}
  </fieldset>;
}

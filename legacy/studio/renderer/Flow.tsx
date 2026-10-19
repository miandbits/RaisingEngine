import React, { useMemo, useState } from 'react';
import { buildRelations, object, relationNeighborhood, type CatalogItem } from '../../../packages/analysis-service/catalog.js';
import type { AnalysisJob } from '../../../packages/analysis-service/model.js';
function ScriptGraph({ item, job, onJump }: { item: CatalogItem; job: AnalysisJob | null; onJump: (file: string, pointer?: string) => void }) {
  const nodes: any[] = object(item.data).nodes ?? [];
  const positions = new Map(nodes.map((n,i) => [n.id, { x: 30 + (i % 3) * 260, y: 40 + Math.floor(i / 3) * 160 }]));
  const edges = nodes.flatMap(n => n.type === 'choice' ? (n.choices ?? []).map((c:any) => ({ from:n.id,to:c.next,label:c.label??c.text??c.id })) : n.type === 'branch' ? [{from:n.id,to:n.then,label:'참'},{from:n.id,to:n.else,label:'거짓'}] : n.next ? [{from:n.id,to:n.next,label:'다음'}] : []);
  if(!nodes.length)return <p>사건 또는 엔딩을 선택하세요.</p>;
  return <><div className="graph-scroll"><svg role="img" aria-label="대본 분기 연결도" width="820" height={Math.max(230,Math.ceil(nodes.length/3)*160+30)}><defs><marker id="script-arrow" markerWidth="8" markerHeight="8" refX="6" refY="3" orient="auto"><path d="M0,0 L6,3 L0,6" fill="#527866"/></marker></defs>
    {edges.map((e:any,i:number)=>{const a=positions.get(e.from),b=positions.get(e.to);if(!a||!b)return null;return <g key={i}><path d={`M${a.x+110},${a.y+70} C${a.x+110},${a.y+120} ${b.x+110},${b.y-30} ${b.x+110},${b.y}`} stroke="#78987c" fill="none" markerEnd="url(#script-arrow)"/><text x={(a.x+b.x)/2+110} y={(a.y+b.y)/2+100} fontSize="10">{String(e.label).slice(0,14)}</text><title>{e.from} → {e.to}: {e.label}</title></g>;})}
    {nodes.map((n,i)=>{const p=positions.get(n.id)!;return <g key={n.id??i} role="button" tabIndex={0} aria-label={`대본 ${n.id}`} onClick={()=>onJump(item.file,`/nodes/${i}`)} onKeyDown={e=>{if(e.key==='Enter')onJump(item.file,`/nodes/${i}`);}}><rect {...p} width="220" height="70" rx="7" fill="#fffdf7" stroke="#8ca486"/><text x={p.x+10} y={p.y+20}>{n.id} · {n.type}</text><text x={p.x+10} y={p.y+40} fontSize="11">{String(n.text??n.prompt??'').slice(0,18)}</text><text x={p.x+10} y={p.y+58} fontSize="10">{job?`방문 ${job.result.nodes[`${item.kind}:${item.id}/${n.id}`]??0}회`:n.id===object(item.data).entryNodeId?'시작 노드':''}</text></g>;})}
  </svg></div><details><summary>분기 연결 목록</summary>{edges.map((e:any,i:number)=><p key={i}>{e.from} → {e.to} · {e.label}</p>)}</details></>;
}
export function Flow({ items, selected, onJump, onFocus, job }: { items: CatalogItem[]; selected: string; onJump: (file: string, pointer?: string) => void; onFocus: (file: string, pointer?: string) => void; job: AnalysisJob | null }) {
  const [mode, setMode] = useState('relations'); const [depth, setDepth] = useState(1); const [filter, setFilter] = useState(''); const [page, setPage] = useState(0);
  const relations = useMemo(() => buildRelations(items), [items]);
  const item = items.find(i => i.key === selected) ?? items.find(i => i.file === selected.split('#')[0]) ?? items[0]; if (!item) return null;
  const graph = relationNeighborhood(relations, item.key, depth); const byKey = new Map(items.map(i => [i.key, i]));
  const nodes = new Map<string, { x: number; y: number }>(); nodes.set(item.key, { x: 290, y: 30 });
  const keys = [...new Set(graph.edges.flatMap(e => [e.from, e.to]))].filter(k => k !== item.key);
  keys.forEach((key, i) => nodes.set(key, { x: i % 2 ? 540 : 40, y: 110 + Math.floor(i / 2) * 76 }));
  const events = items.filter(i => i.kind === 'event' && `${i.label} ${i.tags.join(' ')}`.includes(filter)).sort((a,b) => (a.from ?? 0) - (b.from ?? 0));
  const scripts = Array.isArray(object(item.data).nodes) ? object(item.data).nodes : [];
  return <section className="flow-pane"><h1>흐름 · {item.label}</h1><div className="work-tabs">{[['relations','조건·효과 관계'],['timeline','기간표'],['script','대본 그래프']].map(([id,label]) => <button key={id} aria-pressed={mode===id} onClick={()=>setMode(id!)}>{label}</button>)}</div>
    <p>선은 참조·값 읽기·값 변경 관계입니다. 실제 도달 가능성은 테스트에서 확인하세요. AND·OR·NOT 전체 조건은 해당 폼에 표시됩니다.</p>
    {mode === 'relations' && <><label>관계 깊이<select aria-label="관계 깊이" value={depth} onChange={e=>setDepth(Number(e.target.value))}><option value={1}>1단계</option><option value={2}>2단계</option><option value={3}>3단계</option></select></label>
      {graph.truncated && <p>200개 연결까지 표시합니다. 중심 항목을 바꿔 범위를 좁혀 주세요.</p>}
      <div className="graph-scroll"><svg role="img" aria-label="조건 효과 관계도" width="800" height={Math.max(240, Math.ceil(keys.length/2)*76+160)}><defs><marker id="arrow" markerWidth="8" markerHeight="8" refX="6" refY="3" orient="auto"><path d="M0,0 L6,3 L0,6" fill="#8b9b87"/></marker></defs>
        {graph.edges.map((e,i)=>{const a=nodes.get(e.from)!,b=nodes.get(e.to)!;return <path key={i} d={`M${a.x+90},${a.y+24} L${b.x+90},${b.y+24}`} stroke={e.kind==='writes'?'#b27639':e.kind==='reads'?'#608f85':'#b9bfb5'} strokeWidth="1.5" markerEnd="url(#arrow)" fill="none"><title>{e.kind} · {e.pointer} · {e.context}</title></path>;})}
        {[...nodes].map(([key,pos])=>{const n=byKey.get(key)!;return <g key={key} role="button" tabIndex={0} aria-label={`관계 ${n.label}`} onClick={()=>onFocus(n.file,n.pointer)} onKeyDown={e=>{if(e.key==='Enter')onFocus(n.file,n.pointer);}}><rect {...pos} width="180" height="48" rx="7" fill={key===item.key?'#dce9d7':'#fffef9'} stroke="#78977b"/><text x={pos.x+10} y={pos.y+20}>{n.label.slice(0,18)}</text><text x={pos.x+10} y={pos.y+37} fontSize="10">{n.id.slice(0,25)}</text></g>;})}</svg></div>
      <p>갈색: 변경 · 녹색: 읽기 · 회색: 직접 참조</p><details><summary>연결 근거 {graph.edges.length}개</summary>{graph.edges.map((e,i)=><button className="flow-evidence" key={i} onClick={()=>onJump(e.file,e.pointer)}>{byKey.get(e.from)?.label} → {byKey.get(e.to)?.label} · {e.kind}<small>{e.pointer} · {e.context}</small></button>)}</details></>}
    {mode === 'timeline' && <><input aria-label="기간표 검색" placeholder="사건 이름 또는 태그" value={filter} onChange={e=>{setFilter(e.target.value);setPage(0);}}/><p>기간은 0부터 표시합니다. 동일 기간의 사건은 우선순위와 조건에 따라 선택됩니다.</p><table><thead><tr><th>사건</th><th>기간</th><th>우선순위</th><th>관측 횟수</th></tr></thead><tbody>{events.slice(page*50,page*50+50).map(e=><tr key={e.key}><td><button onClick={()=>onJump(e.file)}>{e.label}</button></td><td>{e.from}~{e.through}</td><td>{e.priority}</td><td>{job?.result.events[e.id]??'—'}</td></tr>)}</tbody></table><button disabled={!page} onClick={()=>setPage(page-1)}>이전 50개</button><button disabled={(page+1)*50>=events.length} onClick={()=>setPage(page+1)}>다음 50개</button></>}
    {mode === 'script' && <ScriptGraph item={item} job={job} onJump={onJump}/>}
    {job && <details><summary>관측된 사건 순서 (검사 버전 {job.fingerprint.slice(-12)})</summary>{job.result.traces.slice(0,3).map(t=><p key={t.run}>seed {t.seed}: {t.eventOrder.join(' → ')||'기록 없음'}</p>)}</details>}
  </section>;
}

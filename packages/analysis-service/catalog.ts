import type { DraftDocument } from '../studio-service/editor.js';
import type { Diagnostic, Json } from '../validation/model.js';

export const kindNames: Record<string, string> = { project: '프로젝트', attributes: '능력·자원', flags: '플래그', character: '인물', activity: '활동', event: '사건', ending: '엔딩', assets: '에셋', scenario: '시나리오', location:'장소', item:'아이템', shop:'상점', interaction:'교류·선물', screen:'게임 화면', extension:'플러그인 문서', pluginLock:'플러그인 잠금' };
export const object = (v: unknown): Record<string, any> => v && typeof v === 'object' && !Array.isArray(v) ? v as Record<string, any> : {};
export interface CatalogItem { key: string; file: string; pointer: string; kind: string; id: string; label: string; description: string; tags: string[]; from: number | null; through: number | null; priority: number | null; summary: string; issues: number; data: Json }
export interface CatalogQuery { kind?: string; text?: string; tag?: string; person?: string; period?: number; issuesOnly?: boolean; sort?: 'label' | 'id' | 'period' | 'priority'; descending?: boolean }
export interface Relation { from: string; to: string; kind: 'reference' | 'reads' | 'writes'; file: string; pointer: string; context: string }
const esc = (s: string) => s.replace(/~/g, '~0').replace(/\//g, '~1');
export function buildCatalog(documents: DraftDocument[], diagnostics: Diagnostic[] = []): CatalogItem[] {
  const byFile = new Map<string, Diagnostic[]>();
  for (const d of diagnostics) byFile.set(d.file, [...(byFile.get(d.file) ?? []), d]);
  return documents.flatMap(doc => {
    const root = object(doc.data);
    const field = ['attributes', 'flags', 'assets'].includes(doc.kind) ? doc.kind : null;
    const entries: {data:Json;pointer:string;kind:string}[] = field && Array.isArray(root[field]) ? root[field].map((data: Json, i: number) => ({ data, pointer: `/${field}/${i}`,kind:doc.kind })) : [{ data: doc.data, pointer: '',kind:doc.kind }];
    if(doc.kind==='project'){
      for(const [field,kind] of Object.entries({locations:'location',items:'item',shops:'shop',interactions:'interaction'}))if(Array.isArray(root.gameplay?.[field]))root.gameplay[field].forEach((data:Json,i:number)=>entries.push({data,pointer:`/gameplay/${field}/${i}`,kind}));
      if(Array.isArray(root.visual?.screens))root.visual.screens.forEach((data:Json,i:number)=>entries.push({data,pointer:`/visual/screens/${i}`,kind:'screen'}));
    }
    return entries.map(({ data, pointer, kind }) => {
      const d = object(data); const costs = Array.isArray(d.costs) ? d.costs.map((c: any) => `${c.attributeId} −${c.amount}`).join(', ') : '';
      return { key: `${doc.file}#${pointer}`, file: doc.file, pointer, kind, id: String(d.id ?? doc.file),
        label: String(d.label ?? d.name ?? d.title ?? kindNames[kind]), description: String(d.description ?? ''),
        tags: [...new Set([...(Array.isArray(root.tags) ? root.tags : []), ...(Array.isArray(d.tags) ? d.tags : [])])].filter((x: unknown) => typeof x === 'string'),
        from: typeof d.window?.fromPeriod === 'number' ? d.window.fromPeriod : null,
        through: typeof d.window?.throughPeriod === 'number' ? d.window.throughPeriod : null,
        priority: typeof d.priority === 'number' ? d.priority : null,
        summary: doc.kind === 'attributes' ? `${d.kind} · 초기 ${d.initial} · ${d.min}~${d.max}` : costs || String(d.description ?? ''),
        issues: (byFile.get(doc.file) ?? []).filter(x => !pointer || x.pointer === pointer || x.pointer.startsWith(`${pointer}/`)).length, data };
    });
  });
}
export function searchCatalog(items: CatalogItem[], query: CatalogQuery = {}): CatalogItem[] {
  const tokens = (query.text ?? '').toLocaleLowerCase().split(/\s+/).filter(Boolean);
  const selected = items.filter(i => (!query.kind || i.kind === query.kind) && (!query.tag || i.tags.includes(query.tag))
    && (!query.issuesOnly || i.issues > 0) && (query.period === undefined || i.from !== null && i.through !== null && i.from <= query.period && i.through >= query.period)
    && (!query.person || containsReference(i.data, query.person, ['characterId', 'speakerId']))
    && tokens.every(t => `${i.id} ${i.label} ${i.description} ${i.file} ${i.tags.join(' ')}`.toLocaleLowerCase().includes(t)));
  const field = query.sort ?? 'label';
  return selected.sort((a, b) => {
    const x = field === 'period' ? a.from ?? Infinity : a[field] ?? -Infinity;
    const y = field === 'period' ? b.from ?? Infinity : b[field] ?? -Infinity;
    const order = typeof x === 'number' && typeof y === 'number' ? (x === y ? 0 : x < y ? -1 : 1) : String(x).localeCompare(String(y), 'ko');
    return (query.descending ? -order : order) || a.key.localeCompare(b.key);
  });
}
function containsReference(value: unknown, id: string, keys: string[]): boolean {
  if (!value || typeof value !== 'object') return false;
  return Object.entries(value).some(([k, v]) => keys.includes(k) && v === id || containsReference(v, id, keys));
}
export function buildRelations(items: CatalogItem[]): Relation[] {
  const lookup = new Map(items.map(i => [`${i.kind}:${i.id}`, i.key])); const result: Relation[] = [];
  const targets: Record<string, string> = { attributeId: 'attributes', currencyAttributeId:'attributes', screenId:'screen', locationId:'location', startLocationId:'location', shopId:'shop', itemId:'item', interactionId:'interaction', flagId: 'flags', eventId: 'event', endingId: 'ending', characterId: 'character', speakerId: 'character', activityId: 'activity', backgroundAssetId: 'assets', imageAssetId: 'assets', musicAssetId: 'assets', soundAssetId: 'assets', assetId: 'assets' };
  for (const item of items) {
    function visit(value: unknown, pointer: string, mode: Relation['kind'], context: string) {
      if (!value || typeof value !== 'object') return;
      const obj = object(value);
      // Conditions and expression inputs read values; only effect targets write them.
      const isEffect = ['add', 'set', 'setFlag'].includes(obj.op) && ('target' in obj || 'flagId' in obj);
      for (const [key, child] of Object.entries(value)) {
        const at = `${pointer}/${esc(key)}`;
        const nextMode = isEffect && (key === 'target' || key === 'flagId') ? 'writes' : ['requirements', 'condition', 'conditions', 'left', 'right', 'value'].includes(key) ? 'reads' : mode;
        const nextContext = ['all', 'any', 'not'].includes(obj.op) ? `${context}/${obj.op}` : context;
        const target = targets[key] && typeof child === 'string' ? lookup.get(`${targets[key]}:${child}`) : undefined;
        if (target) result.push({ from: item.key, to: target, kind: nextMode, file: item.file, pointer: at, context: nextContext });
        if(key==='connections'&&Array.isArray(child))child.forEach((id,index)=>{const linked=lookup.get(`location:${id}`);if(linked)result.push({from:item.key,to:linked,kind:'reference',file:item.file,pointer:`${at}/${index}`,context:'장소 이동 연결'});});
        visit(child, at, nextMode, nextContext);
      }
    }
    visit(item.data, item.pointer, 'reference', '조건 구조는 상세 폼 참조');
  }
  return result;
}
export function relationNeighborhood(relations: Relation[], key: string, depth = 1, limit = 200) {
  const visited = new Set([key]); let frontier = new Set([key]); const edges = new Map<string, Relation>();
  for (let n = 0; n < Math.min(3, Math.max(1, depth)); n++) {
    const next = new Set<string>();
    for (const edge of relations) if (frontier.has(edge.from) || frontier.has(edge.to)) {
      const id = `${edge.from}|${edge.to}|${edge.pointer}`;
      if (!edges.has(id) && edges.size >= limit) return { edges: [...edges.values()], truncated: true };
      edges.set(id, edge); for (const node of [edge.from, edge.to]) if (!visited.has(node)) { next.add(node); visited.add(node); }
    }
    frontier = next;
  }
  return { edges: [...edges.values()], truncated: false };
}

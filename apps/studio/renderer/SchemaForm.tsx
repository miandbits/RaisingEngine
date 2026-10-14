/// <reference types="vite/client" />
import React, { useEffect, useState } from 'react';
import type { DraftDocument } from '../../../packages/studio-service/editor.js';
import type { Json } from '../../../packages/validation/model.js';

type Schema = { $ref?: string; type?: string; const?: Json; enum?: Json[]; properties?: Record<string, Schema>;
  required?: string[]; additionalProperties?: Schema | boolean; allOf?: Schema[]; oneOf?: Schema[]; anyOf?: Schema[];
  items?: Schema; minItems?: number; maximum?: number; minimum?: number; _base?: string; [key: string]: unknown };
const modules = import.meta.glob('../../../packages/schema/*.schema.json', { eager: true, import: 'default' });
const schemas = Object.fromEntries(Object.entries(modules).map(([file, schema]) => [file.split('/').pop()!, schema])) as Record<string, Schema>;
export const labels: Record<string, string> = {
  systems: '탐험·성장 시스템', hero: '전투 능력 연결', maxHpAttributeId: '최대 체력 능력', attackAttributeId: '공격 능력', defenseAttributeId: '방어 능력',
  equipment: '장비', consumables: '회복 아이템', enemies: '적', expeditions: '탐험', quests: '퀘스트', relationshipStages: '관계 단계',
  itemId: '아이템', slot: '장착 부위', modifiers: '장비 능력 보정', maxHp: '최대 체력', attack: '공격', defense: '방어', heal: '회복량',
  rewards: '보상', items: '아이템 목록', quantity: '개수', locationId: '장소', enemyId: '적', expeditionId: '연결할 탐험',
  targetCount: '필요 승리 횟수', threshold: '해금 수치', connections: '연결 노드', exit: '귀환 가능',
  id: 'ID', label: '이름', name: '이름', title: '게임 제목', description: '설명', formatVersion: '데이터 버전',
  requirements: '실행 조건', costs: '비용', baseEffects: '기본 효과', successCheck: '성공 판정', chance: '성공 확률 (0~10000)',
  successEffects: '성공 효과', failureEffects: '실패 효과', presentation: '장면 연출', amount: '소모량',
  attributeId: '능력·자원', characterId: '인물', relationshipId: '관계', flagId: '플래그', eventId: '사건',
  target: '대상', value: '값', op: '연산', kind: '종류', min: '최솟값', max: '최댓값', initial: '초깃값',
  window: '사건 기간', fromPeriod: '시작 기간 (0부터)', throughPeriod: '마지막 기간 (포함)', priority: '우선순위',
  entryNodeId: '시작 노드', nodes: '대본 노드', type: '종류', text: '대사·선택 문구', speakerId: '화자', next: '다음 노드',
  prompt: '질문', choices: '선택지', effects: '효과', condition: '조건', then: '참일 때 노드', else: '거짓일 때 노드',
  conditions: '조건 목록', left: '왼쪽 값', right: '오른쪽 값', comparator: '비교', equals: '기대값', field: '시간 항목',
  gameId: '게임 ID', startYear: '시작 연도', startMonth: '시작 월', durationMonths: '플레이 개월 수', periodsPerMonth: '월간 기간 수',
  protagonistId: '주인공', initialState: '초기 상태', defaultEndingId: '기본 엔딩', attributes: '능력·자원 목록', flags: '플래그 목록',
  relationships: '관계 목록', portraits: '인물 이미지', backgroundAssetId: '배경 이미지', musicAssetId: '배경음', soundAssetId: '효과음',
  portraitId: '인물 이미지 ID', position: '위치', scene: '장면', assets: '에셋 목록', path: '프로젝트 내부 경로',
  seed: '난수 seed', initialOverrides: '시작 상태 덮어쓰기', commands: '명령 목록', assertions: '기대값 목록', stepLimit: '진행 한도',
};
const words: Record<string, string> = { always: '항상', all: '모두 충족', any: '하나 이상 충족', not: '반대', compare: '수치 비교', flag: '플래그 검사', eventCompleted: '사건 완료',
  literal: '고정 수치', read: '현재 수치 읽기', time: '현재 시간', add: '더하기', set: '값 설정', subtract: '빼기', multiply: '곱하기', clamp: '범위 제한', setFlag: '플래그 설정',
  dialogue: '대사', choice: '선택', branch: '조건 분기', effect: '효과 적용', scene: '장면 설정', end: '종료', attribute: '능력·자원', relationship: '인물 관계',
  stat: '능력', resource: '자원', condition: '컨디션', image: '이미지', audio: '음원', eq: '같음', ne: '다름', gt: '초과', gte: '이상', lt: '미만', lte: '이하',
  left: '왼쪽', center: '가운데', right: '오른쪽' };
const word = (value: Json | undefined) => words[String(value)] ?? String(value);
const object = (v: Json | undefined): Record<string, Json> => v && typeof v === 'object' && !Array.isArray(v) ? v : {};
const pointer = (p: string, key: string | number) => `${p}/${String(key).replace(/~/g, '~0').replace(/\//g, '~1')}`;

function resolveSchema(input: Schema, base: string): Schema {
  let schema = input; let origin = input._base ?? base;
  if (schema.$ref) {
    const [file, fragment = ''] = schema.$ref.split('#'); origin = file || origin;
    let found: unknown = schemas[origin];
    for (const part of fragment.split('/').filter(Boolean)) found = (found as Record<string, unknown>)?.[part];
    if (!found) throw new Error(`스키마 참조를 찾을 수 없습니다: ${schema.$ref}`);
    return resolveSchema(found as Schema, origin);
  }
  schema = { ...schema, _base: origin };
  if (schema.allOf) {
    const properties: Record<string, Schema> = {}; const required = new Set(schema.required);
    for (const part of schema.allOf) {
      const resolved = resolveSchema(part, origin);
      for (const [key, child] of Object.entries(resolved.properties ?? {})) properties[key] = { ...child, _base: resolved._base! };
      for (const key of resolved.required ?? []) required.add(key);
    }
    schema.properties = { ...properties, ...schema.properties }; schema.required = [...required];
  }
  return schema;
}
function matches(schema: Schema, value: Json | undefined, base: string): boolean {
  const s = resolveSchema(schema, base);
  if (s.const !== undefined) return value === s.const;
  if (s.enum) return s.enum.includes(value as Json);
  if (s.type === 'null') return value === null;
  if (s.type === 'string') return typeof value === 'string';
  if (s.type === 'boolean') return typeof value === 'boolean';
  if (s.type === 'integer' || s.type === 'number') return typeof value === 'number';
  if (s.type === 'object' && (!value || typeof value !== 'object' || Array.isArray(value))) return false;
  if (s.type === 'array') return Array.isArray(value);
  for (const [key, property] of Object.entries(s.properties ?? {})) {
    const p = resolveSchema(property, s._base!);
    if ((p.const !== undefined || p.enum) && !matches(p, object(value)[key], s._base!)) return false;
  }
  return true;
}
function variantLabel(input: Schema, base: string): string {
  const s = resolveSchema(input, base);
  for (const key of ['op', 'type', 'kind']) {
    const p = s.properties?.[key];
    if (p?.const !== undefined) return word(p.const);
    if (p?.enum) return p.enum.map(word).join(' / ');
  }
  return s.type === 'null' ? '없음' : s.type === 'string' ? 'ID 지정' : s.type ?? '형식';
}
function initial(input: Schema, base: string, depth = 0): Json {
  if (depth > 24) return null;
  const s = resolveSchema(input, base); const origin = s._base!;
  if (s.const !== undefined) return s.const;
  if (s.enum) return s.enum[0]!;
  const variants = s.oneOf ?? s.anyOf;
  if (variants) return initial(variants[0]!, origin, depth + 1);
  if (s.type === 'object' || s.properties) return Object.fromEntries((s.required ?? []).map(key => [key, initial(s.properties?.[key] ?? {}, origin, depth + 1)]));
  if (s.type === 'array') return Array.from({ length: s.minItems ?? 0 }, () => initial(s.items ?? {}, origin, depth + 1));
  if (s.type === 'integer' || s.type === 'number') return Math.min(s.maximum ?? Infinity, Math.max(s.minimum ?? -Infinity, 0));
  if (s.type === 'boolean') return false;
  if (s.type === 'null') return null;
  return '';
}
function TextField({ value, onChange, multiline, numeric, name, options = [] }: { value: Json | undefined; onChange: (value: Json) => void; multiline: boolean; numeric: boolean; name: string; options?: string[] }) {
  const [text, setText] = useState(String(value ?? ''));
  useEffect(() => setText(String(value ?? '')), [value]);
  const commit = () => { const next = numeric ? Number(text) : text; if (numeric && !Number.isFinite(next)) return; if (next !== value) onChange(next); };
  return <>{multiline ? <textarea aria-label={name} value={text} onChange={e => setText(e.target.value)} onBlur={commit} rows={3}/>
    : <input aria-label={name} type={numeric ? 'number' : 'text'} value={text} onChange={e => setText(e.target.value)} onBlur={commit} onKeyDown={e => { if (e.key === 'Enter') e.currentTarget.blur(); }} list={options.length ? `suggest-${name}` : undefined}/>}
    {options.length > 0 && <datalist id={`suggest-${name}`}>{options.map(id => <option key={id} value={id}/>)}</datalist>}</>;
}
function suggestions(key: string, documents: DraftDocument[], document: DraftDocument): string[] {
  const project = object(documents.find(d => d.kind === 'project')?.data);
  const gameplay = object(project.gameplay), systems = object(project.systems);
  const sources: Record<string, Json | undefined> = { itemId: gameplay.items, locationId: gameplay.locations, enemyId: systems.enemies, expeditionId: systems.expeditions, equipmentId: systems.equipment, questId: systems.quests };
  if (sources[key]) return (sources[key] as Json[]).map(row => String(object(row).id));
  const kinds: Record<string, string> = { characterId: 'character', speakerId: 'character', protagonistId: 'character', eventId: 'event', defaultEndingId: 'ending', activityId: 'activity' };
  if (kinds[key]) return documents.filter(d => d.kind === kinds[key]).map(d => String(object(d.data).id));
  if (['next', 'then', 'else', 'entryNodeId'].includes(key)) return ((object(document.data).nodes ?? []) as Json[]).map(node => String(object(node).id));
  if (key === 'attributeId' || key.endsWith('AttributeId') || key === 'flagId') {
    const kind = key === 'flagId' ? 'flags' : 'attributes';
    return documents.filter(d => d.kind === kind).flatMap(d => (object(d.data)[kind] as Json[] ?? []).map(v => String(object(v).id)));
  }
  if (key.endsWith('AssetId')) return documents.filter(d => d.kind === 'assets').flatMap(d => (object(d.data).assets as Json[] ?? []).filter(v => object(v).kind === (key === 'backgroundAssetId' ? 'image' : 'audio')).map(v => String(object(v).id)));
  if (key === 'relationshipId') return [...new Set(documents.filter(d => d.kind === 'character').flatMap(d => (object(d.data).relationships as Json[] ?? []).map(v => String(object(v).id))))];
  return [];
}
interface FieldProps { schema: Schema; base: string; value: Json | undefined; onChange: (value: Json) => void; path: string; field: string; documents: DraftDocument[]; document: DraftDocument; depth?: number }
function Field(props: FieldProps) {
  const { value, onChange, path, field, documents, document, depth = 0 } = props;
  const s = resolveSchema(props.schema, props.base); const base = s._base!;
  const fieldLabel = (key: string) => base === 'systems.schema.json' && key === 'nodes' ? '탐험 노드' : labels[key] ?? key;
  const [mapKey, setMapKey] = useState('');
  if (depth > 32) return <p>중첩 한도를 넘었습니다. 상위 항목을 제거하거나 변경하세요.</p>;
  const child = (schema: Schema, value: Json | undefined, key: string | number, change: (v: Json) => void) => <Field schema={schema} base={base} value={value} field={String(key)} path={pointer(path, key)} onChange={change} documents={documents} document={document} depth={depth + 1}/>;
  const variants = s.oneOf ?? s.anyOf;
  if (variants) {
    const index = Math.max(0, variants.findIndex(v => matches(v, value, base)));
    return <div className="variant"><label>형식 <select aria-label={`${path} 형식`} value={index} onChange={e => {
      const next = initial(variants[Number(e.target.value)]!, base);
      if (object(value).id !== undefined && next && typeof next === 'object' && !Array.isArray(next) && 'id' in next) next.id = object(value).id!;
      onChange(next);
    }}>{variants.map((v, i) => <option value={i} key={i}>{variantLabel(v, base)}</option>)}</select></label>
      <Field {...props} schema={variants[index]!} base={base} depth={depth + 1}/></div>;
  }
  if (s.const !== undefined) return <span className="constant">{word(s.const)}</span>;
  if (s.enum) return <select aria-label={path} value={String(value)} onChange={e => onChange(s.enum!.find(v => String(v) === e.target.value)!)}>{s.enum.map(v => <option key={String(v)} value={String(v)}>{word(v)}</option>)}</select>;
  if (s.type === 'null') return <span className="constant">없음</span>;
  if (s.type === 'boolean') return <select aria-label={path} value={String(value)} onChange={e => onChange(e.target.value === 'true')}><option value="false">아니요</option><option value="true">예</option></select>;
  if (s.type === 'array') {
    const items = Array.isArray(value) ? value : [];
    return <div className="array-items">{items.map((item, i) => <div className="array-item" key={i} id={`field-${pointer(path, i)}`}>
      <div className="item-tools"><strong>{i + 1}{object(item).id ? ` · ${String(object(item).id)}` : ''}</strong><div>
        <button aria-label={`${path}/${i} 위로`} disabled={i === 0} onClick={() => { const next = [...items]; [next[i - 1], next[i]] = [next[i]!, next[i - 1]!]; onChange(next); }}>↑</button>
        <button aria-label={`${path}/${i} 아래로`} disabled={i === items.length - 1} onClick={() => { const next = [...items]; [next[i + 1], next[i]] = [next[i]!, next[i + 1]!]; onChange(next); }}>↓</button>
        <button aria-label={`${path}/${i} 삭제`} onClick={() => onChange(items.filter((_v, at) => i !== at))}>삭제</button></div></div>
      {child(s.items ?? {}, item, i, v => onChange(items.map((old, at) => at === i ? v : old)))}</div>)}
      <button className="add-item" onClick={() => onChange([...items, initial(s.items ?? {}, base)])}>{fieldLabel(field)} 추가</button></div>;
  }
  if (s.type === 'object' || s.properties) {
    const data = object(value);
    const keys = [...new Set([...Object.keys(s.properties ?? {}), ...Object.keys(data)])].filter(key => key !== '$schema' && (key !== 'formatVersion' || !Object.hasOwn(data, key)));
    if (path === '') {
      const order = ['id', 'gameId', 'title', 'label', 'name', 'description', 'window', 'requirements', 'priority', 'entryNodeId', 'nodes'];
      keys.sort((a, b) => (order.includes(a) ? order.indexOf(a) : 100) - (order.includes(b) ? order.indexOf(b) : 100));
    }
    return <div className="object-fields">{keys.map(key => {
      const schema = s.properties?.[key] ?? (typeof s.additionalProperties === 'object' ? s.additionalProperties : { type: typeof data[key] });
      const required = s.required?.includes(key);
      const present = Object.hasOwn(data, key);
      return <div className="form-field" key={key} id={`field-${pointer(path, key)}`}>
        <div className="field-label"><strong>{fieldLabel(key)}{required && <span className="required"> *</span>}</strong>
          {(!required || !present) && <button className="toggle-field" aria-label={`${pointer(path, key)} ${present ? '제거' : '사용'}`} onClick={() => {
            const next = { ...data }; if (present) delete next[key]; else next[key] = initial(schema, base); onChange(next);
          }}>{present ? '제거' : '+ 사용'}</button>}</div>
        {present && child(schema, data[key], key, v => onChange({ ...data, [key]: v }))}
      </div>;
    })}{s.additionalProperties && <div className="map-add"><input aria-label={`${path} 새 키`} placeholder="새 ID" value={mapKey} onChange={e => setMapKey(e.target.value)}/><button disabled={!mapKey || Object.hasOwn(data, mapKey) || ['__proto__', 'constructor', 'prototype'].includes(mapKey)} onClick={() => { onChange({ ...data, [mapKey]: initial(typeof s.additionalProperties === 'object' ? s.additionalProperties : { type: 'string' }, base) }); setMapKey(''); }}>항목 추가</button></div>}</div>;
  }
  return <TextField value={value} onChange={onChange} name={path} multiline={['text', 'description', 'prompt'].includes(field)} numeric={s.type === 'integer' || s.type === 'number'} options={suggestions(field, documents, document)}/>;
}
export function SchemaSection({ schemaRef, value, document, documents, onChange, field = 'document' }: {
  schemaRef: string; value: Json; document: DraftDocument; documents: DraftDocument[]; onChange: (value: Json) => void; field?: string;
}) {
  return <Field schema={{ $ref: schemaRef }} base="project.schema.json" value={value} path="" field={field} document={document} documents={documents} onChange={onChange}/>;
}

export function SchemaForm({ document, documents, onChange, itemPointer = '' }: { document: DraftDocument; documents: DraftDocument[]; onChange: (value: Json) => void; itemPointer?: string }) {
  const base = `${document.kind}.schema.json`;
  if (itemPointer) {
    const parts = itemPointer.split('/').filter(Boolean); const field = parts[0]!; const index = Number(parts[1]);
    const root = resolveSchema(schemas[base]!, base); const array = resolveSchema(root.properties?.[field] ?? {}, base);
    const items = object(document.data)[field];
    if (Array.isArray(items) && items[index] !== undefined) return <Field schema={array.items ?? {}} base={array._base ?? base} value={items[index]} path={itemPointer} field={field} documents={documents} document={document} onChange={value => onChange({ ...object(document.data), [field]: items.map((old, i) => i === index ? value : old) })}/>;
  }
  return <Field schema={schemas[base]!} base={base} value={document.data} path="" field="document" onChange={onChange} documents={documents} document={document}/>;
}

export function references(documents: DraftDocument[], selected: DraftDocument, query = ''): { file: string; pointer: string }[] {
  const id = query || object(selected.data).id; if (typeof id !== 'string') return [];
  const fields: Record<string, string[]> = { activity: ['activityId', 'activityIds'], character: ['characterId', 'speakerId', 'protagonistId'], event: ['eventId'], ending: ['endingId', 'defaultEndingId'] };
  const result: { file: string; pointer: string }[] = [];
  const visit = (value: Json, file: string, p: string, key: string) => {
    const keys = query ? ['attributeId', 'characterId', 'relationshipId', 'flagId', 'eventId', 'activityId', 'activityIds', 'endingId', 'defaultEndingId', 'protagonistId', 'speakerId', 'next', 'then', 'else', 'entryNodeId', 'portraitId', 'backgroundAssetId', 'imageAssetId', 'musicAssetId', 'soundAssetId'] : fields[selected.kind] ?? [];
    if (value === id && keys.includes(key)) result.push({ file, pointer: p });
    if (Array.isArray(value)) value.forEach((v, i) => visit(v, file, pointer(p, i), key));
    else if (value && typeof value === 'object') for (const [k, v] of Object.entries(value)) {
      if (query && k === id && ['attributes', 'flags', 'relationships', 'portraits'].includes(key)) result.push({ file, pointer: pointer(p, k) });
      visit(v, file, pointer(p, k), k);
    }
  };
  documents.forEach(d => visit(d.data, d.file, '', ''));
  return result;
}

import { explainScreen } from '../visual/routing.js';
import { randomUUID } from 'node:crypto';
import { nativeRequest } from '../native-bridge/index.js';
import type { ContentSnapshot, RunState } from '../core/model.js';
import { decodeSave } from '../runtime-service/save.js';
import { canonicalJson } from '../project-service/snapshot.js';

type Row = Record<string, any>;
export interface DebugSubject { kind: 'action' | 'event'; id: string; actorId?: string }
export interface DebugDiff { path: string; before: unknown; after: unknown }
export interface ConditionNode {
  path: string; op: string; value: unknown; children: ConditionNode[];
  kind?: 'condition' | 'expression';
  comparator?: string; leftValue?: unknown; rightValue?: unknown;
  error?: unknown;
}
const pointer = (value: string) => value.replace(/~/g, '~0').replace(/\//g, '~1');
const object = (value: unknown): value is Row => !!value && typeof value === 'object' && !Array.isArray(value);
const clone = <T>(value: T): T => structuredClone(value);

/** Display-only comparison. Execution always comes from the native core. */
export function stateDiff(before: unknown, after: unknown, path = ''): DebugDiff[] {
  if (JSON.stringify(before) === JSON.stringify(after)) return [];
  if (object(before) && object(after)) return [...new Set([...Object.keys(before), ...Object.keys(after)])]
    .flatMap(key => stateDiff(before[key], after[key], `${path}/${pointer(key)}`));
  return [{ path: path || '/', before: before ?? null, after: after ?? null }];
}

export class RuleDebugSession {
  readonly sessionId = randomUUID();
  readonly backend = 'cpp' as const;
  private current: RunState;
  private inspectionState: RunState | undefined;
  private count = 0;
  constructor(readonly content: ContentSnapshot, readonly projectRevision: number, public seed = 42) {
    if (!Number.isInteger(seed) || seed < 0 || seed > 0xffffffff) throw new Error('Seed는 0~4294967295 정수여야 합니다.');
    const result = this.native({ operation: 'create', seed, options: { mode: 'full', initialOverrides: { attributes: {}, flags: {}, relationships: {} } } });
    if (!result.ok) throw new Error(result.error?.message ?? '디버그 상태를 만들 수 없습니다.');
    this.current = result.state as RunState;
  }
  private native(request: Row): Row { return nativeRequest({ ...request, content: this.content }) as Row; }
  private traces(value: unknown): Row[] {
    if (!Array.isArray(value)) return [];
    return value.map(raw => {
      if (!object(raw) || typeof raw.path !== 'string' || !raw.path.startsWith('/')) return raw;
      let current: any = this.content; const parts: string[] = [];
      for (const encoded of raw.path.slice(1).split('/')) {
        const segment = encoded.replace(/~1/g, '/').replace(/~0/g, '~');
        if (Array.isArray(current)) {
          const index = /^\d+$/.test(segment) ? Number(segment) : current.findIndex(row => object(row) && row.id === segment);
          if (index < 0 || index >= current.length) return raw;
          parts.push(String(index)); current = current[index];
        } else if (object(current) && Object.hasOwn(current, segment)) { parts.push(pointer(segment)); current = current[segment]; }
        else return raw;
      }
      const path = '/' + parts.join('/');
      return { ...raw, path, sourcePath: path, ...(raw.path !== path ? { originalPath: raw.path } : {}) };
    });
  }
  private projection(): unknown {
    const reply = this.native({ operation: 'view', state: this.current });
    if (!reply.ok) throw new Error(reply.error?.message ?? '디버그 상태를 표시할 수 없습니다.');
    const { ok: _ok, ...view } = reply;
    const design=this.content.project.visual;
    if(design) {
      const conditions:Record<string,boolean>={};
      for(const rule of design.routing?.conditions??[]) {
        const result=this.native({operation:'evaluate',state:this.current,condition:rule.when});
        conditions[JSON.stringify(rule.when)]=result.ok&&result.value===true;
      }
      view.routing=explainScreen(design,this.current,view.narrative?.node?.type,conditions);
    }
    return view;
  }
  /** Import a player's replay-validated save, never an arbitrary state object. */
  async loadSave(text: string): Promise<Row> {
    if (typeof text !== 'string') throw new Error('세이브 JSON 텍스트가 필요합니다.');
    const decoded = await decodeSave(this.content, text);
    if (!decoded.ok) return this.view({ ok: false, error: decoded.error });
    // Even when a host selected the TS reference backend, debugging uses C++.
    const replayed = this.native({ operation: 'replay', origin: decoded.session.origin, commands: decoded.session.commands });
    if (!replayed.ok) return this.view({ ok: false, error: replayed.error });
    if (canonicalJson(replayed.state) !== canonicalJson(decoded.session.state))
      return this.view({ ok: false, error: { code: 'SAVE_STATE_MISMATCH', message: 'C++ 재현 결과가 세이브 상태와 다릅니다.', path: '/state' } });
    this.current = replayed.state as RunState; this.seed = decoded.session.origin.seed; this.count = decoded.session.commands.length;
    return this.view({ loadedSave: true });
  }
  view(extra: Row = {}): Row {
    return clone({ ok: true, backend: this.backend, sessionId: this.sessionId, projectRevision: this.projectRevision,
      contentFingerprint: this.content.contentFingerprint, seed: this.seed, commandCount: this.count,
      stateRevision: this.current.stateRevision, state: this.current, projection: this.projection(), trace: [], diff: [], ...extra });
  }
  command(command: unknown, expectedStateRevision: number): Row {
    if (!Number.isInteger(expectedStateRevision)) throw new Error('디버그 상태 버전이 필요합니다.');
    if (this.count >= 10000) throw new Error('디버그 명령 한도입니다. 새 실행을 시작하세요.');
    const before = this.current;
    const result = this.native({ operation: 'inspect', state: before, command, expectedRevision: expectedStateRevision });
    if (result.ok) { this.current = result.state as RunState; this.count++; }
    return this.view({ ok: !!result.ok, ...(result.error ? { error: result.error } : {}), command,
      transition: result.transition ?? null, trace: this.traces(result.trace), diff: result.ok ? stateDiff(before, this.current) : [] });
  }
  private expressionTree(expression: Row, path: string): ConditionNode {
    const evaluated = this.native({ operation: 'evaluate', state: this.inspectionState??this.current, expression });
    const result: ConditionNode = { kind: 'expression', path, op: String(expression.op), value: evaluated.ok ? evaluated.value : null, children: [] };
    if (!evaluated.ok) result.error = evaluated.error;
    for (const [key, child] of Object.entries(expression)) {
      if (object(child) && typeof child.op === 'string') result.children.push(this.expressionTree(child, `${path}/${pointer(key)}`));
      else if (Array.isArray(child)) child.forEach((item, index) => { if (object(item) && typeof item.op === 'string') result.children.push(this.expressionTree(item, `${path}/${pointer(key)}/${index}`)); });
    }
    return result;
  }
  private tree(condition: Row, path: string): ConditionNode {
    const evaluated = this.native({ operation: 'evaluate', state: this.inspectionState??this.current, condition });
    const result: ConditionNode = { kind: 'condition', path, op: String(condition.op), value: evaluated.ok ? evaluated.value : null, children: [] };
    if (!evaluated.ok) result.error = evaluated.error;
    if (condition.op === 'compare') {
      result.comparator = condition.comparator;
      for (const side of ['left', 'right'] as const) {
        const value = this.native({ operation: 'evaluate', state: this.inspectionState??this.current, expression: condition[side] });
        result[side === 'left' ? 'leftValue' : 'rightValue'] = value.ok ? value.value : { error: value.error };
        result.children.push(this.expressionTree(condition[side], `${path}/${side}`));
      }
    }
    if (condition.op === 'flag') { result.leftValue = this.current.flags[condition.flagId]; result.rightValue = condition.equals; }
    if (condition.op === 'eventCompleted') { result.leftValue = evaluated.ok?evaluated.value:null; result.rightValue = true; }
    if (Array.isArray(condition.conditions)) result.children = condition.conditions.map((child: Row, index: number) => this.tree(child, `${path}/conditions/${index}`));
    if (object(condition.condition)) result.children.push(this.tree(condition.condition, `${path}/condition`));
    return result;
  }
  inspect(subject: DebugSubject): Row {
    if (!subject || !['action', 'event'].includes(subject.kind) || typeof subject.id !== 'string') throw new Error('검사할 행동 또는 사건을 선택하세요.');
    const rows = subject.kind === 'action' ? this.content.project.gameplay?.actions ?? [] : this.content.events;
    const index = rows.findIndex(row => row.id === subject.id);
    if (index < 0) throw new Error(`검사 대상이 없습니다: ${subject.id}`);
    if(subject.actorId && (this.content.project.roster?.stateModel!=='character-v1'||!this.content.project.roster.members.some(m=>m.characterId===subject.actorId)))throw new Error('검사할 육성 대상이 없습니다.');
    const inspectionState=clone(this.current) as any;
    if(subject.actorId)inspectionState.roster.actorId=subject.actorId;
    this.inspectionState=inspectionState;
    try {
    const definition = rows[index] as unknown as Row;
    const path = subject.kind === 'action' ? `/project/gameplay/actions/${index}` : `/events/${index}`;
    const result = subject.kind === 'action'
      ? this.native({ operation: 'inspect', state: this.current, command: { type: subject.actorId?'executeActorAction':'executeAction', actionId: subject.id, quantity: 1, ...(subject.actorId?{actorId:subject.actorId}:{}) }, expectedRevision: this.current.stateRevision })
      : this.native({ operation: 'inspect', state: inspectionState, subject });
    const condition = this.tree(definition.requirements ?? { op: 'always' }, `${path}/requirements`);
    const eligible = subject.kind === 'action' ? !!result.ok : !!result.eligible;
    return this.view({ inspection: { subject, label: definition.name ?? definition.label ?? subject.id, path,
      source: { documentKind: subject.kind === 'action' ? 'project' : 'event', documentId: subject.kind === 'action' ? this.content.project.gameId : subject.id,
        path: subject.kind === 'action' ? `/gameplay/actions/${index}` : '' },
      eligible, reason: result.reason ?? result.error?.message ?? (eligible ? '실행 가능' : '현재 실행할 수 없습니다.'), condition,
      checks: result.checks ?? [], eventDecisions: result.eventDecisions ?? result.transition?.eventDecisions ?? [],
      preview: { ok: !!result.ok, error: result.error ?? null, diff: subject.kind === 'action' && result.ok ? stateDiff(this.current, result.state) : [] },
      trace: this.traces(result.trace) } });
    } finally {this.inspectionState=undefined;}
  }
}

export interface ImpactNode { key: string; kind: string; id: string; label: string; path: string }
export interface ImpactEdge { from: string; to: string; path: string; relation: string }
/** Reference graph only; it does not predict whether a referenced rule will run. */
export function ruleImpact(content: ContentSnapshot, subject: DebugSubject): { subject: DebugSubject; nodes: ImpactNode[]; edges: ImpactEdge[] } {
  const nodes: ImpactNode[] = [], edges: ImpactEdge[] = [], definitions: { node: ImpactNode; value: unknown }[] = [];
  const add = (kind: string, id: string, label: string, path: string, value: unknown) => {
    const node = { key: `${kind}:${id}`, kind, id, label, path }; nodes.push(node); definitions.push({ node, value });
  };
  const c = content as unknown as Row;
  for (const [field, kind] of [['attributes', 'attribute'], ['flags', 'flag'], ['events', 'event'], ['activities', 'activity'], ['endings', 'ending']] as const)
    for (const [i, row] of (c[field] ?? []).entries()) add(kind, row.id, row.name ?? row.label ?? row.id, `/${field}/${i}`, row);
  for (const [field, kind] of [['actions', 'action'], ['rules', 'rule'], ['policyGroups', 'policy'], ['policyBudgets', 'budget'], ['locations', 'location'], ['shops', 'shop'], ['items', 'item']] as const)
    for (const [i, row] of (c.project.gameplay?.[field] ?? []).entries()) add(kind, row.id, row.name ?? row.id, `/project/gameplay/${field}/${i}`, row);
  for (const [i, screen] of (c.project.visual?.screens ?? []).entries()) add('screen', screen.id, screen.label ?? screen.id, `/project/visual/screens/${i}`, screen);
  const references: Record<string, string> = { actionId: 'action', thenActionId: 'action', elseActionId: 'action', actionIds: 'action', eventId: 'event', attributeId: 'attribute', flagId: 'flag', itemId: 'item', locationId: 'location', locationIds: 'location', shopId: 'shop', groupId: 'policy', groupIds: 'policy', budgetId: 'budget', screenId: 'screen' };
  const known = new Set(nodes.map(n => n.key));
  const walk = (from: ImpactNode, value: unknown, path: string): void => {
    if (Array.isArray(value)) { value.forEach((item, i) => walk(from, item, `${path}/${i}`)); return; }
    if (!object(value)) return;
    for (const [field, child] of Object.entries(value)) {
      const target = references[field], here = `${path}/${pointer(field)}`;
      if (target) for (const [index, id] of (Array.isArray(child) ? child : [child]).entries()) if (typeof id === 'string' && known.has(`${target}:${id}`)) {
        edges.push({ from: from.key, to: `${target}:${id}`, path: Array.isArray(child) ? `${here}/${index}` : here,
          relation: /requirements|condition/.test(here) ? 'condition' : /costs/.test(here) ? 'cost' : /effects/.test(here) ? 'effect' : /flow/.test(here) ? 'branch' : from.kind === 'rule' ? 'trigger' : from.kind === 'screen' ? 'ui' : 'reference' });
      }
      walk(from, child, here);
    }
  };
  definitions.forEach(({ node, value }) => walk(node, value, node.path));
  const key = `${subject.kind}:${subject.id}`;
  if (!known.has(key)) throw new Error(`영향 분석 대상이 없습니다: ${subject.id}`);
  const selected = edges.filter(edge => edge.from === key || edge.to === key);
  const related = new Set([key, ...selected.flatMap(e => [e.from, e.to])]);
  return { subject, nodes: nodes.filter(node => related.has(node.key)), edges: selected };
}

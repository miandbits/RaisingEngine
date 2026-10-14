import { childPointer, type ProjectDocument, type JsonObject } from './model.js';
import { SemanticContext, objectItems, idOf, type Entry, type Index } from './context.js';

interface Edge { target: string; pointer: string }
function outgoing(node: JsonObject, pointer: string): Edge[] {
  if (node.type === 'end') return [];
  if (node.type === 'branch') return [{ target: node.then as string, pointer: `${pointer}/then` }, { target: node.else as string, pointer: `${pointer}/else` }];
  if (node.type === 'choice') return objectItems(node.choices).map((choice, i) => ({ target: choice.next as string, pointer: `${pointer}/choices/${i}/next` }));
  return [{ target: node.next as string, pointer: `${pointer}/next` }];
}

export function checkNarrative(ctx: SemanticContext, doc: ProjectDocument, data: JsonObject): void {
  const nodes: Index = new Map();
  ctx.addArray(nodes, doc, '/nodes', data.nodes, '대본 노드');
  const entry = ctx.resolve(nodes, data.entryNodeId as string, doc, '/entryNodeId', '진입 노드');
  const edges = new Map<string, Edge[]>();
  for (const list of nodes.values()) for (const node of list) {
    if (node.data.type === 'choice') {
      const choices: Index = new Map();
      ctx.addArray(choices, doc, `${node.pointer}/choices`, node.data.choices, '선택지');
    }
    const next = outgoing(node.data, node.pointer);
    const valid = next.filter((edge) => ctx.resolve(nodes, edge.target, doc, edge.pointer, '대본 노드') !== undefined);
    if (list.length === 1) edges.set(node.id, valid);
  }
  // Iterative DFS checks every component, including unreachable cycles.
  const color = new Map<string, number>();
  for (const [id, list] of nodes) {
    if (list.length !== 1 || color.has(id)) continue;
    const stack: { id: string; cursor: number }[] = [{ id, cursor: 0 }];
    color.set(id, 1);
    while (stack.length) {
      const frame = stack[stack.length - 1]!;
      const edge = (edges.get(frame.id) ?? [])[frame.cursor++];
      if (!edge) { color.set(frame.id, 2); stack.pop(); continue; }
      const mark = color.get(edge.target) ?? 0;
      if (mark === 1) ctx.report(doc, edge.pointer, 'NARRATIVE_CYCLE', `대본 연결 '${frame.id}' → '${edge.target}'가 순환을 만듭니다.`, [idOf(data), frame.id, edge.target]);
      else if (mark === 0) { color.set(edge.target, 1); stack.push({ id: edge.target, cursor: 0 }); }
    }
  }
  if (entry) {
    const reachable = new Set<string>();
    const pending = [entry.id];
    while (pending.length) {
      const id = pending.pop()!;
      if (reachable.has(id)) continue;
      reachable.add(id);
      for (const edge of edges.get(id) ?? []) pending.push(edge.target);
    }
    for (const [id, list] of nodes) if (!reachable.has(id)) for (const node of list) {
      ctx.report(doc, `${node.pointer}/id`, 'NARRATIVE_UNREACHABLE', `노드 '${id}'는 진입 노드에서 도달할 수 없습니다.`, [id], 'warning');
    }
  }
  const predecessors = new Map<string, string[]>();
  const canEnd = new Set<string>();
  const queue: string[] = [];
  for (const [id, list] of nodes) {
    if (list.length !== 1) continue;
    if (list[0]!.data.type === 'end') queue.push(id);
    for (const edge of edges.get(id) ?? []) predecessors.set(edge.target, [...(predecessors.get(edge.target) ?? []), id]);
  }
  while (queue.length) {
    const id = queue.pop()!;
    if (canEnd.has(id)) continue;
    canEnd.add(id);
    queue.push(...(predecessors.get(id) ?? []));
  }
  for (const [id, list] of nodes) if (list.length === 1 && !canEnd.has(id)) {
    ctx.report(doc, `${list[0]!.pointer}/id`, 'NARRATIVE_NO_END', `노드 '${id}'에서 종료 노드로 이어지는 경로가 없습니다.`, [id]);
  }
}

export function checkScenarioChoice(ctx: SemanticContext, doc: ProjectDocument, choice: JsonObject, pointer: string): void {
  const event = ctx.resolve(ctx.events, choice.eventId as string, doc, `${pointer}/eventId`, '사건');
  if (!event) return;
  const nodes = objectItems(event.data.nodes).filter((node) => node.id === choice.nodeId);
  if (nodes.length !== 1) {
    ctx.report(doc, `${pointer}/nodeId`, nodes.length ? 'REFERENCE_AMBIGUOUS' : 'REFERENCE_MISSING', `사건 '${event.id}'의 노드 '${choice.nodeId}'를 유일하게 찾을 수 없습니다.`, [event.id, choice.nodeId as string]);
    return;
  }
  if (nodes[0]!.type !== 'choice') {
    ctx.report(doc, `${pointer}/nodeId`, 'REFERENCE_KIND', `노드 '${choice.nodeId}'는 선택 노드가 아닙니다.`, [choice.nodeId as string]);
    return;
  }
  const matches = objectItems(nodes[0]!.choices).filter((item) => item.id === choice.choiceId);
  if (matches.length !== 1) ctx.report(doc, `${pointer}/choiceId`, matches.length ? 'REFERENCE_AMBIGUOUS' : 'REFERENCE_MISSING', `선택지 '${choice.choiceId}'를 해당 노드에서 유일하게 찾을 수 없습니다.`, [choice.choiceId as string]);
}

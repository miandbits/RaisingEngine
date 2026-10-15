import { syncRelationshipStages } from '../systems/service.js';
import { runBoundaryRules } from '../gameplay/rules.js';
import type { Scene } from '../schema/generated/event.js';
import type { ContentSnapshot, DeepReadonly, EventDecision, NarrativeCursor, NarrativeNode, NarrativeStep, RunData, RunState } from './model.js';
import { applyEffects, evaluateCondition } from './evaluation.js';
import { freeze, requireCore } from './util.js';
import { eventCompleted, resolveOwner } from './roster.js';

const priorityOrder = (a: { priority: number; id: string }, b: { priority: number; id: string }) =>
  b.priority - a.priority || (a.id < b.id ? -1 : a.id > b.id ? 1 : 0);

export function inspectEvents(content: ContentSnapshot, state: RunState): EventDecision[] {
  let selected = false;
  return [...content.events].sort(priorityOrder).map(event => {
    let reason: EventDecision['reason'];
    if (eventCompleted(content,state,event.id)) reason = 'completed';
    else if (event.trigger === 'interaction') reason = 'interaction-only';
    else if (state.periodIndex < event.window.fromPeriod) reason = 'before-window';
    else if (state.periodIndex > event.window.throughPeriod) reason = 'expired';
    else if (!evaluateCondition(content, state, event.requirements, `/events/${event.id}/requirements`)) reason = 'requirements';
    else if (selected) reason = 'priority';
    else { selected = true; reason = 'selected'; }
    return { eventId: event.id, priority: event.priority, reason };
  });
}

export function currentNarrativeNode(content: ContentSnapshot, state: RunState): DeepReadonly<NarrativeNode> {
  const cursor = state.activeNarrative;
  requireCore(state.phase === 'narrative' && cursor, 'WRONG_PHASE', '진행 중인 대본이 없습니다.', '/phase');
  const narrative = (cursor.kind === 'event' ? content.events : content.endings).find(n => n.id === cursor.id);
  const node = narrative?.nodes.find(n => n.id === cursor.nodeId);
  requireCore(node, 'INVALID_CURSOR', '현재 대본 노드를 찾을 수 없습니다.', '/activeNarrative');
  return node;
}

export function getNarrativeView(content: ContentSnapshot, state: RunState) {
  if (state.phase !== 'narrative') return null;
  const node = currentNarrativeNode(content, state);
  const choices = node.type === 'choice' ? node.choices.map(choice => ({ id: choice.id, text: choice.text,
    enabled: evaluateCondition(content, state, choice.requirements, `${nodePath(state.activeNarrative!)}/choices/${choice.id}/requirements`) })) : [];
  return freeze({ cursor: { ...state.activeNarrative! }, node, choices });
}

function nodePath(cursor: DeepReadonly<NarrativeCursor>): string {
  return `/${cursor.kind === 'event' ? 'events' : 'endings'}/${cursor.id}/nodes/${cursor.nodeId}`;
}

function ensureAvailableChoice(content: ContentSnapshot, state: RunState): void {
  const view = getNarrativeView(content, state);
  if (view?.node.type === 'choice') {
    requireCore(view.choices.some(c => c.enabled), 'NO_AVAILABLE_CHOICE',
      `활성 선택지가 없습니다: ${view.choices.map(c => c.id).join(', ')}`, nodePath(view.cursor));
  }
}

export function enterNarrative(content: ContentSnapshot, draft: RunData, kind: 'event' | 'ending', id: string): void {
  const definition = (kind === 'event' ? content.events : content.endings).find(n => n.id === id);
  requireCore(definition, 'UNKNOWN_NARRATIVE', `알 수 없는 대본: ${id}`, '/activeNarrative');
  draft.phase = 'narrative';
  draft.activeNarrative = { kind, id, nodeId: definition.entryNodeId };
  if(kind==='event' && 'scope' in definition && definition.scope==='actor')draft.activeNarrative.actorId=resolveOwner(content,draft,{kind:'actor'},'/activeNarrative/actorId')!;
  ensureAvailableChoice(content, draft);
}

/** Called once after no event was selected, or after the selected event's end node. */
export function finishPeriod(content: ContentSnapshot, draft: RunData): void {
  if (draft.systems) {
    draft.phase = 'period-end'; draft.activeNarrative = null;
    syncRelationshipStages(content, draft, true);
    if (draft.activeNarrative) return;
  }
  draft.activeNarrative = null;
  draft.lastActivityResult = null;
  if (draft.periodIndex === content.project.durationMonths * content.project.periodsPerMonth - 1) {
    if (draft.mode === 'activities-only') { draft.phase = 'finished'; return; }
    const candidates = content.endings.filter(ending => ending.id !== content.project.defaultEndingId
      && evaluateCondition(content, draft, ending.requirements, `/endings/${ending.id}/requirements`));
    const endingId = candidates.sort(priorityOrder)[0]?.id ?? content.project.defaultEndingId;
    draft.endingId = endingId;
    enterNarrative(content, draft, 'ending', endingId);
  } else {
    draft.periodIndex++;
    draft.phase = draft.periodIndex % content.project.periodsPerMonth === 0 ? 'planning' : 'ready';
    if (draft.phase === 'planning') draft.schedule = null;
  }
}

export function resolvePeriodEnd(content: ContentSnapshot, draft: RunData): EventDecision[] {
  requireCore(draft.mode === 'full' && draft.phase === 'period-end', 'WRONG_PHASE', '전체 게임 모드의 기간 종료 경계가 아닙니다.', '/phase');
  runBoundaryRules(content, draft, 'periodEnd');
  const decisions = inspectEvents(content, draft);
  const selected = decisions.find(d => d.reason === 'selected');
  if (selected) enterNarrative(content, draft, 'event', selected.eventId);
  else finishPeriod(content, draft);
  return decisions;
}

export function applyScene(draft: RunData, scene: DeepReadonly<Scene>, soundCues: string[]): void {
  if (scene.backgroundAssetId !== undefined) draft.presentation.backgroundAssetId = scene.backgroundAssetId;
  if (scene.musicAssetId !== undefined) draft.presentation.musicAssetId = scene.musicAssetId;
  if (scene.portraits !== undefined) draft.presentation.portraits = scene.portraits.map(p => ({ ...p }));
  if (scene.soundAssetId !== undefined) soundCues.push(scene.soundAssetId);
}

/** Exactly one node per command. Effects and cursor movement commit together. */
export function stepNarrative(content: ContentSnapshot, draft: RunData, choiceId: string | undefined, soundCues: string[]): NarrativeStep {
  requireCore(draft.mode === 'full', 'WRONG_PHASE', '전체 게임 모드가 필요합니다.', '/mode');
  const node = currentNarrativeNode(content, draft);
  const cursor = { ...draft.activeNarrative! };
  const source = nodePath(cursor);
  const step: NarrativeStep = { cursor, nodeType: node.type, choiceId: choiceId ?? null, changes: [] };
  if (choiceId !== undefined) {
    requireCore(node.type === 'choice', 'WRONG_PHASE', '선택 입력을 기다리지 않습니다.', '/activeNarrative');
    const choice = node.choices.find(c => c.id === choiceId);
    requireCore(choice, 'UNKNOWN_CHOICE', `선택지를 찾을 수 없습니다: ${choiceId}`, '/command/choiceId');
    requireCore(evaluateCondition(content, draft, choice.requirements, `${source}/choices/${choiceId}/requirements`),
      'CHOICE_DISABLED', `선택 조건을 만족하지 않습니다: ${choiceId}`, `${source}/choices/${choiceId}/requirements`);
    applyEffects(content, draft, choice.effects, `${source}/choices/${choiceId}/effects`, step.changes);
    draft.activeNarrative!.nodeId = choice.next;
  } else {
    switch (node.type) {
      case 'choice': requireCore(false, 'CHOICE_REQUIRED', '선택지 ID를 지정해야 합니다.', '/command'); break;
      case 'dialogue': draft.activeNarrative!.nodeId = node.next; break;
      case 'scene': applyScene(draft, node.scene, soundCues); draft.activeNarrative!.nodeId = node.next; break;
      case 'branch': draft.activeNarrative!.nodeId = evaluateCondition(content, draft, node.condition, `${source}/condition`) ? node.then : node.else; break;
      case 'effect': applyEffects(content, draft, node.effects, `${source}/effects`, step.changes); draft.activeNarrative!.nodeId = node.next; break;
      case 'end':
        if (cursor.kind === 'event') {
          const completed=cursor.actorId?draft.roster!.completedEvents![cursor.actorId]!:draft.completedEvents;
          if(!completed.includes(cursor.id))completed.push(cursor.id);
          if (cursor.returnPhase) {
            draft.activeNarrative = null; draft.phase = cursor.returnPhase;
            if (cursor.returnPresentation) draft.presentation = structuredClone(cursor.returnPresentation);
            if (cursor.finishPeriod) finishPeriod(content, draft);
          }
          else finishPeriod(content, draft);
        }
        else { draft.activeNarrative = null; draft.phase = 'finished'; }
        break;
    }
  }
  ensureAvailableChoice(content, draft);
  return step;
}

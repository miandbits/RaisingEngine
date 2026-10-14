import type { ActivityResult, ContentSnapshot, RunData, RunState } from './model.js';
import { requireCore } from './util.js';

export interface RosterState {
  attributes: Record<string, Record<string, number>>;
  assignments: Record<string, string>;
}

/** Optional extension: no extra state is added to legacy runs. */
export function createRoster(content: ContentSnapshot, state: RunData): void {
  const roster = content.project.roster;
  if (!roster) return;
  state.roster = { attributes: {}, assignments: {} };
  for (const member of roster.members) {
    state.roster.attributes[member.characterId] = Object.fromEntries(roster.localAttributeIds.map(id =>
      [id, member.initialAttributes[id] ?? state.attributes[id]!]));
    state.roster.assignments[member.characterId] = member.activityId;
  }
  Object.assign(state.attributes, state.roster.attributes[content.project.protagonistId]);
}

export function assignActivities(content: ContentSnapshot, state: RunState, draft: RunData, assignments: Record<string, string>): void {
  const roster = content.project.roster;
  requireCore(roster && draft.roster, 'ROSTER_REQUIRED', '다인 육성 설정이 필요합니다.', '/project/roster');
  requireCore(state.phase === 'planning' || state.phase === 'ready', 'WRONG_PHASE', '활동 배정을 기다리는 상태가 아닙니다.', '/phase');
  const ids = roster.members.map(m => m.characterId);
  requireCore(Object.keys(assignments).length === ids.length && Object.keys(assignments).every(id => ids.includes(id)),
    'ROSTER_ASSIGNMENTS', '모든 육성 대상에게 활동을 하나씩 배정해야 합니다.', '/command/assignments');
  for (const id of ids) {
    const activity = content.activities.find(a => a.id === assignments[id]);
    requireCore(activity, 'UNKNOWN_ACTIVITY', `${id}: 활동을 찾을 수 없습니다.`, `/command/assignments/${id}`);
    requireCore(!activity.expeditionId, 'ROSTER_SESSION_UNSUPPORTED', '다인 활동에서 탐험 진입은 아직 지원하지 않습니다.', `/command/assignments/${id}`);
  }
  draft.roster.assignments = Object.fromEntries(ids.map(id => [id, assignments[id]!]));
  draft.schedule = Array(content.project.periodsPerMonth).fill(assignments[content.project.protagonistId]);
  draft.phase = 'ready';
}

/** Member order, shared RNG and shared costs are deterministic; caller owns rollback. */
export function executeRoster(content: ContentSnapshot, draft: RunData,
  execute: (content: ContentSnapshot, state: RunState, draft: RunData, individual: boolean) => ActivityResult): ActivityResult {
  const roster = content.project.roster!;
  requireCore(draft.roster && draft.phase === 'ready', 'WRONG_PHASE', '실행할 다인 활동이 준비되지 않았습니다.', '/phase');
  const state = draft.roster;
  // Unscoped rules and existing UI retain the protagonist view.
  state.attributes[content.project.protagonistId] = Object.fromEntries(roster.localAttributeIds.map(id => [id, draft.attributes[id]!]));
  const results: { characterId: string; result: ActivityResult }[] = [];
  for (const member of roster.members) {
    const id = member.characterId;
    Object.assign(draft.attributes, state.attributes[id]);
    draft.schedule = Array(content.project.periodsPerMonth).fill(state.assignments[id]);
    draft.phase = 'ready';
    const before = structuredClone(draft);
    const result = execute(content, before, draft, true);
    state.attributes[id] = Object.fromEntries(roster.localAttributeIds.map(key => [key, draft.attributes[key]!]));
    results.push({ characterId: id, result });
  }
  Object.assign(draft.attributes, state.attributes[content.project.protagonistId]);
  draft.schedule = Array(content.project.periodsPerMonth).fill(state.assignments[content.project.protagonistId]);
  const result: ActivityResult = { ...structuredClone(results[0]!.result), actorResults: results };
  draft.lastActivityResult = result;
  draft.phase = 'activity-result';
  return result;
}

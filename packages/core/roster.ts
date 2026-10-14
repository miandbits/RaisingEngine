import type { ActivityResult, ContentSnapshot, RunData, RunState, NumericTarget } from './model.js';
import { requireCore } from './util.js';

export interface RosterState {
  attributes: Record<string, Record<string, number>>;
  assignments: Record<string, string>;
  inventory?: Record<string, Record<string, number>>;
  relationships?: Record<string, Record<string, Record<string, number>>>;
  completedEvents?: Record<string, string[]>;
  /** Transaction-local contexts, removed before publication. */
  actorId?: string;
  activityActorId?: string;
}

export function resolveOwner(content: ContentSnapshot, state: RunState, owner: NumericTarget['owner'], path: string): string | undefined {
  if (!owner) return undefined;
  requireCore(content.project.roster?.stateModel === 'character-v1' && state.roster?.inventory, 'CHARACTER_STATE_REQUIRED', '캐릭터 소유 상태 설정이 필요합니다.', path);
  const id = owner.kind === 'character' ? owner.characterId : state.activeNarrative?.actorId ?? state.roster.actorId;
  requireCore(id && content.project.roster.members.some(m => m.characterId === id), 'ACTOR_REQUIRED', '유효한 실행자 또는 지정 캐릭터가 필요합니다.', path);
  return id;
}

export function eventCompleted(content: ContentSnapshot, state: RunState, eventId: string, owner?: NumericTarget['owner']): boolean {
  if (owner) {const id=resolveOwner(content,state,owner,'/eventCompleted/owner')!;return state.roster!.completedEvents![id]!.includes(eventId);}
  if (content.events.find(e=>e.id===eventId)?.scope === 'actor') {
    const id=state.activeNarrative?.actorId ?? state.roster?.actorId;
    return !!id && !!state.roster?.completedEvents?.[id]?.includes(eventId);
  }
  return state.completedEvents.includes(eventId);
}

export function beginActorAction(content: ContentSnapshot, draft: RunData, actorId: string, actionId: string): void {
  resolveOwner(content,draft,{kind:'character',characterId:actorId},'/command/actorId');
  const member=content.project.roster!.members.find(m=>m.characterId===actorId)!;
  requireCore(member.actionIds?.includes(actionId),'ACTOR_ACTION_UNBOUND','이 캐릭터에게 연결되지 않은 행동입니다.','/command/actionId');
  requireCore(!content.project.gameplay?.actions?.find(a=>a.id===actionId)?.expeditionId,'ACTOR_SESSION_UNSUPPORTED','대상 지정 행동의 탐험 진입은 별도 세션 계약이 필요합니다.','/command/actionId');
  draft.roster!.actorId=actorId;
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
  if (roster.stateModel === 'character-v1') {
    state.roster.inventory={};state.roster.relationships={};state.roster.completedEvents={};
    for (const member of roster.members) {
      const id=member.characterId;
      state.roster.inventory[id]=Object.fromEntries((content.project.gameplay?.items??[]).map(item=>[item.id,member.initialInventory?.[item.id]??item.initialQuantity??0]));
      state.roster.relationships[id]=structuredClone(state.relationships);
      for(const [other,values] of Object.entries(member.initialRelationships??{}))Object.assign(state.roster.relationships[id]![other]!,values);
      state.roster.completedEvents[id]=[];
    }
  }
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
    if(roster.stateModel==='character-v1'){state.actorId=id;state.activityActorId=id;}
    Object.assign(draft.attributes, state.attributes[id]);
    draft.schedule = Array(content.project.periodsPerMonth).fill(state.assignments[id]);
    draft.phase = 'ready';
    const before = structuredClone(draft);
    const result = execute(content, before, draft, true);
    state.attributes[id] = Object.fromEntries(roster.localAttributeIds.map(key => [key, draft.attributes[key]!]));
    results.push({ characterId: id, result });
  }
  Object.assign(draft.attributes, state.attributes[content.project.protagonistId]);
  delete state.actorId;delete state.activityActorId;
  draft.schedule = Array(content.project.periodsPerMonth).fill(state.assignments[content.project.protagonistId]);
  const result: ActivityResult = { ...structuredClone(results[0]!.result), actorResults: results };
  draft.lastActivityResult = result;
  draft.phase = 'activity-result';
  return result;
}

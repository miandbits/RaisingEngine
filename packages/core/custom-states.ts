import type { ContentSnapshot, RunData, RunState, NumericTarget } from './model.js';
import { requireCore } from './util.js';
import { resolveOwner } from './roster.js';

export function createCustomStates(content: ContentSnapshot, state: RunData): void {
  const groups = content.project.stateGroups;
  if (!groups?.length) return;
  const shared: Record<string, string> = {}, characters: Record<string, Record<string, string>> = {};
  for (const group of groups) {
    if (group.scope === 'shared') shared[group.id] = group.initialValueId;
    else for (const member of content.project.roster!.members) (characters[member.characterId] ??= {})[group.id] = group.initialValueId;
  }
  state.customStates = { shared, characters };
}

/** Resolve the same owner context used by inventory, relationships and actor events. */
export function customStateSlot(content: ContentSnapshot, state: RunState, groupId: string, valueId: string,
  owner: NumericTarget['owner'], path: string) {
  const group = content.project.stateGroups?.find(g => g.id === groupId);
  requireCore(group, 'UNKNOWN_STATE_GROUP', '상태 그룹을 찾을 수 없습니다.', path + '/stateGroupId');
  requireCore(group.values.some(v => v.id === valueId), 'UNKNOWN_STATE_VALUE', '그룹에 속한 상태 값을 선택하세요.', path + '/stateValueId');
  requireCore((group.scope === 'character') === !!owner, 'STATE_SCOPE', '공용 상태에는 대상이 없어야 하고 캐릭터 상태에는 대상이 필요합니다.', path + '/owner');
  const characterId = resolveOwner(content, state, owner, path + '/owner');
  const values = characterId ? state.customStates?.characters[characterId] : state.customStates?.shared;
  requireCore(values && Object.hasOwn(values, groupId), 'STATE_STORAGE', '저장된 상태가 없습니다.', path);
  return { values, characterId, before: values[groupId]! };
}

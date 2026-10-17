import { isObject, type JsonObject, type ProjectDocument } from './model.js';
import { SemanticContext, objectItems, type Index } from './context.js';

export function checkStateGroups(ctx: SemanticContext, root: ProjectDocument, project: JsonObject): void {
  const groups: Index = new Map();
  ctx.addArray(groups, root, '/stateGroups', project.stateGroups, '상태 그룹');
  for (const entries of groups.values()) for (const entry of entries) {
    const values: Index = new Map();
    ctx.addArray(values, root, entry.pointer + '/values', entry.data.values, '상태 값');
    ctx.resolve(values, entry.data.initialValueId as string, root, entry.pointer + '/initialValueId', '초기 상태 값');
    if (entry.data.scope === 'character' && (!isObject(project.roster) || project.roster.stateModel !== 'character-v1'))
      ctx.report(root, entry.pointer + '/scope', 'CHARACTER_STATE_REQUIRED', '캐릭터 상태는 캐릭터별 데이터 설정이 필요합니다.');
  }
}

export function checkStateReference(ctx: SemanticContext, project: JsonObject | undefined, doc: ProjectDocument, value: JsonObject, pointer: string): void {
  if (value.op !== 'stateIs' && value.op !== 'setState') return;
  const group = objectItems(project?.stateGroups).find(g => g.id === value.stateGroupId);
  if (!group) { ctx.report(doc, pointer + '/stateGroupId', 'REFERENCE_MISSING', '상태 그룹을 찾을 수 없습니다.'); return; }
  if (!objectItems(group.values).some(v => v.id === value.stateValueId))
    ctx.report(doc, pointer + '/stateValueId', 'REFERENCE_MISSING', '해당 그룹에 상태 값이 없습니다.');
  if ((group.scope === 'character') !== isObject(value.owner))
    ctx.report(doc, pointer + '/owner', 'STATE_SCOPE', '공용 상태에는 대상이 없어야 하고 캐릭터 상태에는 대상이 필요합니다.');
}

import type { SemanticContext } from './context.js';
import { isObject, type ProjectDocument, type JsonObject } from './model.js';

export function checkRoster(ctx: SemanticContext, root: ProjectDocument, project: JsonObject): void {
  const roster = project.roster;
  if (!isObject(roster)) return;
  const locals = roster.localAttributeIds as string[];
  for (const [i, id] of locals.entries()) ctx.resolve(ctx.attributes, id, root, `/roster/localAttributeIds/${i}`, '개별 능력');
  const seen = new Set<string>();
  const members = roster.members as JsonObject[];
  for (const [i, member] of members.entries()) {
    const p = `/roster/members/${i}`, id = member.characterId as string;
    ctx.resolve(ctx.characters, id, root, `${p}/characterId`, '육성 대상');
    if (seen.has(id)) ctx.report(root, `${p}/characterId`, 'ROSTER_DUPLICATE', `육성 대상 '${id}'가 중복되었습니다.`);
    seen.add(id);
    const activity = ctx.resolve(ctx.activities, member.activityId as string, root, `${p}/activityId`, '활동');
    if (activity?.data.expeditionId) ctx.report(root, `${p}/activityId`, 'ROSTER_SESSION_UNSUPPORTED', '다인 활동의 탐험 진입은 아직 지원하지 않습니다.');
    for (const [key, value] of Object.entries(member.initialAttributes as JsonObject)) {
      if (!locals.includes(key)) ctx.report(root, `${p}/initialAttributes/${key}`, 'ROSTER_LOCAL_ATTRIBUTE', '개별 능력으로 지정한 값만 초기화할 수 있습니다.');
      const attr = ctx.resolve(ctx.attributes, key, root, `${p}/initialAttributes/${key}`, '능력');
      if (attr) ctx.checkInitial(attr, value as number, root, `${p}/initialAttributes/${key}`);
    }
  }
  if (members[0]?.characterId !== project.protagonistId) ctx.report(root, '/roster/members/0/characterId', 'ROSTER_PRIMARY', '첫 육성 대상은 기존 주인공 ID와 같아야 합니다.');
  if (isObject(project.gameplay) && Array.isArray(project.gameplay.diets) && project.gameplay.diets.length > 0) ctx.report(root, '/roster', 'ROSTER_LEGACY_FACILITIES', '다인 육성에는 구형 시설 대신 범용 정책·행동을 사용하세요.');
}

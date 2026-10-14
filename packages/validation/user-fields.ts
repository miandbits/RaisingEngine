import { userTextIssue } from '../core/user-fields.js';
import { objectItems, type SemanticContext } from './context.js';
import type { JsonObject, ProjectDocument } from './model.js';

export function checkUserFields(ctx: SemanticContext, doc: ProjectDocument, project: JsonObject): void {
  const ids = new Set<string>();
  for (const [index, field] of objectItems(project.userFields).entries()) {
    const id = String(field.id), path = `/userFields/${index}`;
    if (ids.has(id)) ctx.report(doc, `${path}/id`, 'USER_FIELD_DUPLICATE', `사용자 필드 '${id}'가 중복되었습니다.`);
    ids.add(id);
    const min = Number(field.minLength), max = Number(field.maxLength);
    if (min > max) ctx.report(doc, `${path}/minLength`, 'USER_FIELD_RANGE', '최소 길이는 최대 길이 이하여야 합니다.');
    const problem = userTextIssue(field.initial, min, max);
    if (problem) ctx.report(doc, `${path}/initial`, 'USER_FIELD_INITIAL', problem);
  }
}

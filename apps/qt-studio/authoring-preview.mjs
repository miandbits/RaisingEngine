import { createRun } from '../../dist/packages/core/run.js';
import { evaluateCondition } from '../../dist/packages/core/evaluation.js';
import { resolveScreenElements } from '../../dist/packages/visual/components.js';
import { presentationActivity } from '../../dist/packages/visual/variants.js';
import { explainScreen } from '../../dist/packages/visual/routing.js';

/** Disposable inspection state, never a playable save or an editor transaction. */
export function authoringPreview(content, input) {
  const fail = message => { throw new Error(`제작 미리보기: ${message}`); };
  if (!input || typeof input !== 'object' || Array.isArray(input)) fail('검사 설정이 올바르지 않습니다.');
  const visual = structuredClone(content.project.visual);
  const screen = visual.screens.find(s => s.id === input.screenId);
  if (!screen) fail('검사할 화면을 다시 선택하세요.');
  const actor = input.actorId || undefined;
  if (actor && !content.project.roster?.members.some(m => m.characterId === actor)) fail('제작 대상 캐릭터가 없습니다.');
  if (input.activityId && !content.activities.some(a => a.id === input.activityId)) fail('활동이 없습니다.');
  const elementId = input.elementId || undefined;
  if ((actor || input.activityId) && !elementId) fail('캐릭터·활동을 바꿀 검사 대상을 먼저 선택하세요.');
  if (elementId) {
    // Expanded child IDs are scoped to their authored instance; sibling instances keep their bindings.
    const element = screen.elements.find(e => e.id === elementId || elementId.startsWith(e.id + '__'));
    if (!element) fail('검사 대상이 삭제되었습니다. 다시 선택하세요.');
    if (actor || input.activityId) element.uiContext = {...element.uiContext,
      ...(actor ? {actorId:actor} : {}), ...(input.activityId ? {activityId:input.activityId} : {})};
  }
  const created = createRun(content, 42, {mode:'full'});
  if (!created.ok) fail(created.error?.message ?? '초기 상태 생성 실패');
  const state = structuredClone(created.state);
  for (const [id,value] of Object.entries(input.attributes ?? {})) {
    const definition = content.attributes.find(a => a.id === id);
    if (!definition || !Number.isSafeInteger(value) || value < definition.min || value > definition.max) fail(`능력치 범위를 확인하세요: ${id}`);
    const local = content.project.roster?.localAttributeIds.includes(id);
    if (local && actor) {
      state.roster.attributes[actor][id] = value;
      if (content.project.roster.members[0].characterId === actor) state.attributes[id] = value;
    } else if (local) fail('개인 능력치를 검사할 캐릭터를 선택하세요.');
    else state.attributes[id] = value;
  }
  for (const [id,value] of Object.entries(input.flags ?? {})) {
    if (!Object.hasOwn(state.flags,id) || typeof value !== 'boolean') fail(`플래그를 확인하세요: ${id}`);
    state.flags[id] = value;
  }
  for (const [id,value] of Object.entries(input.states ?? {})) {
    const group = content.project.stateGroups?.find(g => g.id === id);
    if (!group?.values.some(v => v.id === value)) fail(`사용자 정의 상태를 확인하세요: ${id}`);
    if (group.scope === 'character' && !actor) fail('개인 상태를 검사할 캐릭터를 선택하세요.');
    (group.scope === 'shared' ? state.customStates.shared : state.customStates.characters[actor])[id] = value;
  }
  const conditions = Object.fromEntries((visual.routing?.conditions ?? []).map(rule =>
    [JSON.stringify(rule.when),evaluateCondition(content,state,rule.when)]));
  const routing = explainScreen(visual,state,undefined,conditions);
  const screenId = input.followRouting ? routing.screenId : screen.id;
  const rules = resolveScreenElements(visual,screenId).map(element => {
    const activityId = presentationActivity(state,element);
    let chosen = false;
    const variants = (element.variants ?? []).map((rule,index) => {
      const activityMatch = !rule.activityIds?.length || rule.activityIds.includes(activityId);
      const conditionMatch = evaluateCondition(content,state,rule.when);
      const selected = !chosen && activityMatch && conditionMatch;
      chosen ||= selected;
      return {index,label:rule.label ?? `표현 ${index+1}`,selected,activityMatch,conditionMatch,
        reason:!activityMatch?'활동 불일치':!conditionMatch?'조건 불충족':selected?'첫 번째 충족 규칙':'상위 표현 규칙 우선'};
    });
    return {elementId:element.id,label:element.label ?? element.text ?? element.id,activityId,variants,
      visible:!element.hidden && (!element.visibleWhen || element.visibleWhen === 'always' || element.visibleWhen === 'planning') &&
        (!element.visibleCondition || evaluateCondition(content,state,element.visibleCondition)),
      enabled:!element.enabledCondition || evaluateCondition(content,state,element.enabledCondition)};
  });
  return {version:1,contentFingerprint:content.contentFingerprint,state,visual,screenId,
    explanation:{screenId,forced:!input.followRouting,routing,rules,
      notice:'가상 검사 상태입니다. 실제 도달 가능성·플레이 결과 검증을 대신하지 않습니다. 본편·프로젝트·저장 파일은 변경하지 않습니다.'}};
}

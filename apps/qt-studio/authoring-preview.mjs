import { nativeRequest } from '../../dist/packages/native-bridge/index.js';
import { RuleDebugSession } from '../../dist/packages/rule-debug/index.js';
import { resolveScreenElements } from '../../dist/packages/visual/components.js';
import { presentationActivity, resolvePresentation } from '../../dist/packages/visual/variants.js';
import { explainScreen } from '../../dist/packages/visual/routing.js';

/** Disposable inspection state, never a playable save or an editor transaction. */
export function authoringPreview(content, input, savedState) {
  const fail = message => { throw new Error(`제작 미리보기: ${message}`); };
  if (!input || typeof input !== 'object' || Array.isArray(input)) fail('검사 설정이 올바르지 않습니다.');
  const visual = structuredClone(content.project.visual);
  const screen = visual.screens.find(s => s.id === input.screenId);
  if (!screen) fail('검사할 화면을 다시 선택하세요.');
  const actor = input.actorId || undefined;
  if (actor && !content.project.roster?.members.some(m => m.characterId === actor)) fail('제작 대상 캐릭터가 없습니다.');
  if (input.activityId && !content.activities.some(a => a.id === input.activityId)) fail('활동이 없습니다.');
  const elementId = input.elementId || undefined;
  if (input.activityId && !elementId && !input.canvasContext) fail('활동을 바꿀 검사 대상을 먼저 선택하세요.');
  if (input.canvasContext && (actor || input.activityId)) {
    // Supply defaults, preserving deliberately bound character/activity instances.
    for (const element of screen.elements) element.uiContext = {
      ...(actor ? {actorId:actor} : {}), ...(input.activityId ? {activityId:input.activityId} : {}), ...element.uiContext
    };
  }
  if (elementId) {
    // Expanded child IDs are scoped to their authored instance; sibling instances keep their bindings.
    const element = screen.elements.find(e => e.id === elementId || elementId.startsWith(e.id + '__'));
    if (!element) fail('검사 대상이 삭제되었습니다. 다시 선택하세요.');
    if (actor || input.activityId) element.uiContext = {...element.uiContext,
      ...(actor ? {actorId:actor} : {}), ...(input.activityId ? {activityId:input.activityId} : {})};
  }
  const created = savedState ? {ok:true,state:savedState} : nativeRequest({operation:'create',content,seed:42,options:{mode:'full'}});
  if (!created.ok) fail(created.error?.message ?? '초기 상태 생성 실패');
  const state = structuredClone(created.state);
  const baseline = structuredClone(state);
  const evaluateCondition = (_content, snapshot, condition) => {
    const result = nativeRequest({operation:'evaluate',content,state:snapshot,condition});
    if (!result.ok) fail(result.error?.message ?? '조건을 평가할 수 없습니다.');
    return result.value === true;
  };
  for (const [id,value] of Object.entries(input.attributes ?? {})) {
    const definition = content.attributes.find(a => a.id === id);
    if (!definition || !Number.isSafeInteger(value) || value < definition.min || value > definition.max) fail(`능력치 범위를 확인하세요: ${id}`);
    const local = content.project.roster?.localAttributeIds.includes(id);
    if (local && actor) {
      state.roster.attributes[actor][id] = value;
      if (content.project.protagonistId === actor) state.attributes[id] = value;
    } else if (local) fail('개인 능력치를 검사할 캐릭터를 선택하세요.');
    else state.attributes[id] = value;
  }
  for (const [id,value] of Object.entries(input.flags ?? {})) {
    if(input.flagScope==='character' && actor && content.project.roster?.stateModel==='character-v1'){
      state.roster.flags ??= {}; state.roster.flags[actor] ??= {};
    }
    const target = input.flagScope === 'character' ? state.roster?.flags?.[actor] : state.flags;
    if (!target || !content.flags.some(flag=>flag.id===id) || typeof value !== 'boolean') fail(`플래그의 범위와 캐릭터를 확인하세요: ${id}`);
    target[id] = value;
  }
  for (const [id,value] of Object.entries(input.states ?? {})) {
    const group = content.project.stateGroups?.find(g => g.id === id);
    if (!group?.values.some(v => v.id === value)) fail(`사용자 정의 상태를 확인하세요: ${id}`);
    if (group.scope === 'character' && !actor) fail('개인 상태를 검사할 캐릭터를 선택하세요.');
    (group.scope === 'shared' ? state.customStates.shared : state.customStates.characters[actor])[id] = value;
  }
  const view = nativeRequest({operation:'view',content,state});
  if (!view.ok) fail(view.error?.message ?? '상태를 표시할 수 없습니다.');
  const phase = state.phase === 'narrative' ? (view.narrative?.node?.type === 'choice' ? 'choice' : 'dialogue') : state.phase;
  const visibleInPhase = element => !element.hidden && (!element.visibleWhen || element.visibleWhen === 'always' || element.visibleWhen === phase ||
    element.visibleWhen === 'planning' && ['planning','ready'].includes(phase) || element.visibleWhen === 'result' && ['activity-result','period-end'].includes(phase));
  const conditions = Object.fromEntries((visual.routing?.conditions ?? []).map(rule =>
    [JSON.stringify(rule.when),evaluateCondition(content,state,rule.when)]));
  const routing = explainScreen(visual,state,view.narrative?.node?.type,conditions);
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
    const resolved = resolvePresentation(element,activityId,condition => evaluateCondition(content,state,condition));
    return {elementId:element.id,label:element.label ?? element.text ?? element.id,activityId,variants,
      visible:visibleInPhase(resolved) && (!resolved.visibleCondition || evaluateCondition(content,state,resolved.visibleCondition)),
      enabled:!resolved.enabledCondition || evaluateCondition(content,state,resolved.enabledCondition)};
  });
  const nameOf = id => content.characters.find(c=>c.id===id)?.name ?? id;
  const values = content.attributes.map(definition => {
    const local = content.project.roster?.localAttributeIds?.includes(definition.id);
    const read = snapshot => local && actor ? snapshot.roster?.attributes?.[actor]?.[definition.id] : snapshot.attributes[definition.id];
    return {id:definition.id,label:definition.label,scope:local ? (actor ?? content.project.protagonistId) : 'shared',scopeName:local ? nameOf(actor ?? content.project.protagonistId) : '공용',value:read(state),baseline:read(baseline)};
  });
  const facts = (content.project.stateGroups ?? []).map(group=>{
    const read=snapshot=>(group.scope==='shared'?snapshot.customStates?.shared:snapshot.customStates?.characters?.[actor])?.[group.id];
    const label=value=>group.values.find(v=>v.id===value)?.name ?? '—';
    return {label:group.name,scopeName:group.scope==='shared'?'공용':nameOf(actor)??'캐릭터 미선택',value:label(read(state)),baseline:label(read(baseline))};
  });
  for(const flag of content.flags ?? []){
    const personal=input.flagScope==='character';const read=snapshot=>personal?(actor ? snapshot.roster?.flags?.[actor]?.[flag.id]??false : undefined):snapshot.flags[flag.id];
    const label=value=>value===undefined?'—':value?'참':'거짓';facts.push({label:flag.label??flag.id,scopeName:personal?nameOf(actor)??'캐릭터 미선택':'공용',value:label(read(state)),baseline:label(read(baseline))});
  }
  const event = input.eventId ? new RuleDebugSession(content,0).inspectSnapshot(state,{kind:'event',id:input.eventId,...(actor?{actorId:actor}:{})}) : null;
  if (event) {
    const annotate = node => {
      const definition = node.path.split('/').slice(1).reduce((row,key)=>row?.[key.replaceAll('~1','/').replaceAll('~0','~')],content);
      const target = definition?.left?.target;
      const named = definition?.flagId ? content.flags.find(row=>row.id===definition.flagId)
        : definition?.eventId ? content.events.find(row=>row.id===definition.eventId)
        : target?.attributeId ? content.attributes.find(row=>row.id===target.attributeId)
        : target?.relationshipId ? content.characters.find(row=>row.id===target.characterId)?.relationships?.find(row=>row.id===target.relationshipId) : undefined;
      node.subjectLabel = named?.label ?? named?.name ?? '';
      const owner=definition?.owner??target?.owner;
      if(owner)node.subjectLabel += ` · ${owner.kind==='actor'?nameOf(actor??state.activeNarrative?.actorId??state.roster?.actorId):nameOf(owner.characterId)??'공용'}`;
      for(const child of node.children??[])annotate(child);
    };
    annotate(event.condition);
  }
  const modified = JSON.stringify(baseline) !== JSON.stringify(state) || JSON.stringify(visual) !== JSON.stringify(content.project.visual);
  return {version:1,contentFingerprint:content.contentFingerprint,state,visual,screenId,
    explanation:{screenId,forced:!input.followRouting,routing,rules,
      event,values,facts,actorId:actor,actorName:nameOf(actor)??'기본 대상',phase,calendar:state.calendar,
      source:savedState?'validated-save':'initial',modified,
      notice:`${savedState?'C++로 재현 검증한 저장 상태':'새 게임 초기 상태'}${modified?' · 가상 값 변경됨':''}. 사건 선택은 조건 검사이며 사건을 강제로 시작하지 않습니다. 프로젝트·본편·저장 파일은 변경하지 않습니다.`}};
}

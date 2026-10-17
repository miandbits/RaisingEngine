import { checkStateGroups, checkStateReference } from './custom-states.js';
import { childPointer, isObject, type Diagnostic, type DocumentKind, type Json, type JsonObject, type ProjectDocument } from './model.js';
import { SemanticContext, idOf, objectItems, type Entry, type Index } from './context.js';
import { checkNarrative, checkScenarioChoice } from './narrative.js';
import { checkGallery } from './gallery.js';
import { checkVisual } from './visual.js';
import { checkLocalization } from './localization.js';
import { checkGameplay } from './gameplay.js';
import { checkUserFields } from './user-fields.js';
import { checkSystems } from './systems.js';
import { checkProgression } from './progression.js';
import { checkRoster, checkOwner } from './roster.js';

export function validateSemantics(documents: ProjectDocument[]): Diagnostic[] {
  const ctx = new SemanticContext();
  const root = documents.find((doc) => doc.kind === 'project');
  const project = root?.data as JsonObject | undefined;
  if (root && project) checkStateGroups(ctx, root, project);
  const topIndexes: Partial<Record<DocumentKind, Index>> = { character: ctx.characters, activity: ctx.activities, event: ctx.events, ending: ctx.endings, scenario: ctx.scenarios };
  for (const doc of documents) {
    const data = doc.data as JsonObject;
    const topIndex = topIndexes[doc.kind];
    if (topIndex) {
      ctx.add(topIndex, { id: idOf(data), doc, pointer: '', data }, doc.kind);
      if (doc.file.split('/').pop() !== `${data.id}.json`) ctx.report(doc, '/id', 'FILENAME_ID_MISMATCH', `파일 이름은 '${data.id}.json'이어야 합니다.`, [idOf(data)]);
    }
    if (doc.kind === 'attributes') ctx.addArray(ctx.attributes, doc, '/attributes', data.attributes, '능력치');
    if (doc.kind === 'flags') ctx.addArray(ctx.flags, doc, '/flags', data.flags, '플래그');
    if (doc.kind === 'assets') ctx.addArray(ctx.assets, doc, '/assets', data.assets, '에셋');
  }
  for (const entries of ctx.attributes.values()) for (const entry of entries) ctx.checkRange(entry);
  for (const entries of ctx.characters.values()) for (const entry of entries) {
    const relationships: Index = new Map();
    ctx.addArray(relationships, entry.doc, '/relationships', entry.data.relationships, '관계');
    ctx.relationships.set(entry, relationships);
    for (const items of relationships.values()) for (const item of items) ctx.checkRange(item);
    if (entry.id === project?.protagonistId && relationships.size) ctx.report(entry.doc, '/relationships', 'PROTAGONIST_RELATIONSHIP', '육성 대상 자신에게 관계 수치를 선언할 수 없습니다.', [entry.id]);
    for (const [portrait, assetId] of Object.entries(entry.data.portraits as JsonObject)) {
      checkAsset(assetId as string, 'image', entry.doc, childPointer('/portraits', portrait));
    }
  }
  function checkAsset(id: string, kind: string, doc: ProjectDocument, pointer: string): void {
    const asset = ctx.resolve(ctx.assets, id, doc, pointer, '에셋');
    if (asset && asset.data.kind !== kind) ctx.report(doc, pointer, 'REFERENCE_KIND', `에셋 '${id}'는 ${kind} 종류여야 합니다.`, [id]);
  }
  function checkOverrides(data: JsonObject, doc: ProjectDocument, pointer: string): void {
    for (const [id, value] of Object.entries(data.attributes as JsonObject)) {
      const p = childPointer(`${pointer}/attributes`, id);
      const attr = ctx.resolve(ctx.attributes, id, doc, p, '능력치');
      if (attr) ctx.checkInitial(attr, value as number, doc, p);
    }
    for (const id of Object.keys(data.flags as JsonObject)) ctx.resolve(ctx.flags, id, doc, childPointer(`${pointer}/flags`, id), '플래그');
    for (const [characterId, values] of Object.entries(data.relationships as JsonObject)) {
      const p = childPointer(`${pointer}/relationships`, characterId);
      const character = ctx.resolve(ctx.characters, characterId, doc, p, '인물');
      if (!character) continue;
      for (const [id, value] of Object.entries(values as JsonObject)) {
        const next = childPointer(p, id);
        const relation = ctx.resolve(ctx.relationships.get(character)!, id, doc, next, '관계');
        if (relation) ctx.checkInitial(relation, value as number, doc, next);
      }
    }
  }
  function walk(value: Json, doc: ProjectDocument, pointer: string, expressionDepth = 0): void {
    if (Array.isArray(value)) {
      value.forEach((child, i) => walk(child, doc, childPointer(pointer, i), expressionDepth));
      return;
    }
    if (!isObject(value)) return;
    checkOwner(ctx,project,doc,value,pointer);
    checkStateReference(ctx,project,doc,value,pointer);
    if (value.kind === 'inventory' && typeof value.itemId === 'string') {
      const gameplay = project && isObject(project.gameplay) ? project.gameplay : {};
      if (!objectItems(gameplay.items).some(x => x.id === value.itemId)) ctx.report(doc, `${pointer}/itemId`, 'REFERENCE_MISSING', `물품 '${value.itemId}'를 찾을 수 없습니다.`, [value.itemId]);
    }
    const depth = typeof value.op === 'string' ? expressionDepth + 1 : expressionDepth;
    if (depth > 32) { ctx.report(doc, pointer, 'EXPRESSION_DEPTH', '조건과 표현식 중첩은 32단계를 넘을 수 없습니다.'); return; }
    if (typeof value.attributeId === 'string') ctx.resolve(ctx.attributes, value.attributeId, doc, `${pointer}/attributeId`, '능력치');
    if (typeof value.flagId === 'string') ctx.resolve(ctx.flags, value.flagId, doc, `${pointer}/flagId`, '플래그');
    if (typeof value.endingId === 'string') ctx.resolve(ctx.endings, value.endingId, doc, `${pointer}/endingId`, '엔딩');
    if (typeof value.speakerId === 'string') ctx.resolve(ctx.characters, value.speakerId, doc, `${pointer}/speakerId`, '화자');
    if (typeof value.eventId === 'string' && !('choiceId' in value)) ctx.resolve(ctx.events, value.eventId, doc, `${pointer}/eventId`, '사건');
    if (typeof value.characterId === 'string') {
      const character = ctx.resolve(ctx.characters, value.characterId, doc, `${pointer}/characterId`, '인물');
      if (character && typeof value.relationshipId === 'string') ctx.resolve(ctx.relationships.get(character)!, value.relationshipId, doc, `${pointer}/relationshipId`, '관계');
      if (character && typeof value.portraitId === 'string' && !Object.hasOwn(character.data.portraits as object, value.portraitId)) ctx.report(doc, `${pointer}/portraitId`, 'REFERENCE_MISSING', `인물 '${character.id}'에 표정 '${value.portraitId}'가 없습니다.`, [character.id, value.portraitId]);
    }
    if (typeof value.backgroundAssetId === 'string') checkAsset(value.backgroundAssetId, 'image', doc, `${pointer}/backgroundAssetId`);
    if (typeof value.imageAssetId === 'string') checkAsset(value.imageAssetId, 'image', doc, `${pointer}/imageAssetId`);
    if (typeof value.musicAssetId === 'string') checkAsset(value.musicAssetId, 'audio', doc, `${pointer}/musicAssetId`);
    if (typeof value.soundAssetId === 'string') checkAsset(value.soundAssetId, 'audio', doc, `${pointer}/soundAssetId`);
    if (Array.isArray(value.portraits)) {
      const positions = new Set<string>();
      objectItems(value.portraits).forEach((portrait, i) => {
        const position = portrait.position as string;
        if (positions.has(position)) ctx.report(doc, `${pointer}/portraits/${i}/position`, 'PORTRAIT_POSITION_DUPLICATE', `표시 위치 '${position}'가 중복되었습니다.`);
        positions.add(position);
      });
    }
    if (value.op === 'clamp' && (value.min as number) > (value.max as number)) ctx.report(doc, `${pointer}/max`, 'RANGE_ORDER', 'clamp의 max는 min 이상이어야 합니다.');
    for (const [key, child] of Object.entries(value)) walk(child, doc, childPointer(pointer, key), depth);
  }
  if (root && project) {
    checkRoster(ctx, root, project);
    checkProgression(ctx, root, project);
    checkUserFields(ctx, root, project);
    checkLocalization(ctx, root, documents);
    if (isObject(project.gallery)) checkGallery(ctx, root, project.gallery);
    if (isObject(project.visual)) checkVisual(ctx, root, project.visual);
    if (isObject(project.gameplay)) checkGameplay(ctx, root, project.gameplay);
    if (isObject(project.systems)) checkSystems(ctx, root, project.systems);
    else for (const entries of ctx.activities.values()) for (const activity of entries) {
      if (activity.data.expeditionId) ctx.report(activity.doc, '/expeditionId', 'SYSTEMS_REQUIRED', '탐험 활동에는 프로젝트 확장 시스템 정의가 필요합니다.');
    }
    ctx.resolve(ctx.characters, project.protagonistId as string, root, '/protagonistId', '육성 대상');
    const fallback = ctx.resolve(ctx.endings, project.defaultEndingId as string, root, '/defaultEndingId', '기본 엔딩');
    if (fallback) {
      if ((fallback.data.requirements as JsonObject).op !== 'always') ctx.report(fallback.doc, '/requirements', 'DEFAULT_ENDING_CONDITION', '기본 엔딩 조건은 always여야 합니다.', [fallback.id]);
      if (fallback.data.priority !== 0) ctx.report(fallback.doc, '/priority', 'DEFAULT_ENDING_PRIORITY', '기본 엔딩 우선순위는 0이어야 합니다.', [fallback.id]);
    }
    checkOverrides(project.initialState as JsonObject, root, '/initialState');
    if (isObject(project.periodFlow)) {
      if (project.roster || project.progression) ctx.report(root, '/periodFlow', 'PERIOD_FLOW_MODE', '기간 단계는 현재 단일 대상의 월 일정에서 사용할 수 있습니다.');
      const between = project.periodFlow.intermission;
      if (isObject(between)) {
        ctx.resolve(ctx.flags, between.completionFlagId as string, root, '/periodFlow/intermission/completionFlagId', '완료 플래그');
        const visual = isObject(project.visual) ? project.visual : {};
        if (!objectItems(visual.screens).some(s => s.id === between.screenId)) ctx.report(root, '/periodFlow/intermission/screenId', 'REFERENCE_MISSING', '자유 행동 화면을 찾을 수 없습니다.');
        if (!project.gameplay) ctx.report(root, '/periodFlow/intermission', 'GAMEPLAY_REQUIRED', '자유 행동에는 생활·화면 상태가 필요합니다.');
      }
    }
  }
  for (const doc of documents) {
    const data = doc.data as JsonObject;
    walk(data, doc, '');
    if (doc.kind === 'activity') {
      const seen = new Set<string>();
      objectItems(data.costs).forEach((cost, i) => {
        const id = cost.attributeId as string;
        if (seen.has(id)) ctx.report(doc, `/costs/${i}/attributeId`, 'COST_DUPLICATE', `비용 대상 '${id}'가 중복되었습니다.`, [id]);
        seen.add(id);
        const matches = ctx.attributes.get(id);
        if (matches?.length === 1 && matches[0]!.data.kind === 'stat') ctx.report(doc, `/costs/${i}/attributeId`, 'COST_KIND', '비용 대상은 resource 또는 condition이어야 합니다.', [id]);
      });
    }
    if (doc.kind === 'event') {
      const window = data.window as JsonObject;
      const from = window.fromPeriod as number, through = window.throughPeriod as number;
      if (from > through) ctx.report(doc, '/window/throughPeriod', 'EVENT_WINDOW_ORDER', '사건 종료 기간은 시작 기간 이상이어야 합니다.', [idOf(data)]);
      if (project) for (const key of ['fromPeriod', 'throughPeriod']) {
        const count = (project.durationMonths as number) * (project.periodsPerMonth as number);
        if ((window[key] as number) >= count) ctx.report(doc, `/window/${key}`, 'EVENT_WINDOW_BOUNDS', `사건 기간은 0..${count - 1} 안이어야 합니다.`, [idOf(data)]);
      }
    }
    if (doc.kind === 'event' || doc.kind === 'ending') checkNarrative(ctx, doc, data);
    if (doc.kind === 'scenario') {
      checkOverrides(data.initialOverrides as JsonObject, doc, '/initialOverrides');
      objectItems(data.commands).forEach((command, i) => {
        if (command.type === 'dispatch' && isObject(command.command)) {
          const action = command.command, pointer = `/commands/${i}/command`;
          if (action.type === 'setSchedule') (action.activityIds as string[]).forEach((id, j) => ctx.resolve(ctx.activities, id, doc, `${pointer}/activityIds/${j}`, '활동'));
          const fields: Record<string, string> = { locationId: 'locations', shopId: 'shops', itemId: 'items', interactionId: 'interactions', dietId: 'diets', serviceId: 'services', actionId: 'actions', groupId: 'policyGroups' };
          for (const [field, collection] of Object.entries(fields)) if (typeof action[field] === 'string') {
            const gameplay = project && isObject(project.gameplay) ? project.gameplay : {};
            if (!objectItems(gameplay[collection]).some(x => x.id === action[field])) ctx.report(doc, `${pointer}/${field}`, 'REFERENCE_MISSING', `생활 규칙에서 '${action[field]}' 참조를 찾을 수 없습니다.`, [action[field] as string]);
          }
          if (action.type === 'selectPolicy' && typeof action.optionId === 'string') {
            const gameplay = project && isObject(project.gameplay) ? project.gameplay : {};
            const group = objectItems(gameplay.policyGroups).find(x => x.id === action.groupId);
            if (group && !objectItems(group.options).some(x => x.id === action.optionId)) ctx.report(doc, `${pointer}/optionId`, 'REFERENCE_MISSING', `정책 그룹에서 '${action.optionId}' 선택지를 찾을 수 없습니다.`, [action.optionId]);
          }
          if (typeof action.screenId === 'string') {
            const visual = project && isObject(project.visual) ? project.visual : {};
            if (!objectItems(visual.screens).some(x => x.id === action.screenId)) ctx.report(doc, `${pointer}/screenId`, 'REFERENCE_MISSING', `화면 '${action.screenId}'를 찾을 수 없습니다.`, [action.screenId]);
          }
        }
        if (command.type === 'planMonth') (command.activityIds as string[]).forEach((id, j) => ctx.resolve(ctx.activities, id, doc, `/commands/${i}/activityIds/${j}`, '활동'));
        if (command.type === 'runPeriod') objectItems(command.choices).forEach((choice, j) => checkScenarioChoice(ctx, doc, choice, `/commands/${i}/choices/${j}`));
      });
    }
  }
  return ctx.diagnostics;
}

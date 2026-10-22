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
    const scoped=roster.stateModel==='character-v1';
    if(!scoped&&['initialInventory','initialRelationships','actionIds'].some(key=>member[key]!==undefined))ctx.report(root,p,'CHARACTER_STATE_REQUIRED','개별 소지품·관계·행동 연결에는 character-v1 설정이 필요합니다.');
    const gameplay=isObject(project.gameplay)?project.gameplay:{};
    const items=(Array.isArray(gameplay.items)?gameplay.items:[]) as JsonObject[];
    const actions=(Array.isArray(gameplay.actions)?gameplay.actions:[]) as JsonObject[];
    for(const [itemId,value] of Object.entries(isObject(member.initialInventory)?member.initialInventory:{})){
      const item=items.find(x=>x.id===itemId),ip=`${p}/initialInventory/${itemId}`;
      if(!item)ctx.report(root,ip,'REFERENCE_MISSING',`물품 '${itemId}'를 찾을 수 없습니다.`);
      else if(typeof value!=='number'||value<0||value>(item.maxStack as number))ctx.report(root,ip,'INITIAL_RANGE','초기 수량이 보유 가능 범위를 벗어났습니다.');
    }
    for(const [other,values] of Object.entries(isObject(member.initialRelationships)?member.initialRelationships:{})){
      const character=ctx.resolve(ctx.characters,other,root,`${p}/initialRelationships/${other}`,'관계 상대');
      if(character&&isObject(values))for(const [key,value] of Object.entries(values)){
        const rp=`${p}/initialRelationships/${other}/${key}`,rel=ctx.resolve(ctx.relationships.get(character)!,key,root,rp,'관계');if(rel)ctx.checkInitial(rel,value as number,root,rp);
      }
    }
    for(const [j,actionId] of (Array.isArray(member.actionIds)?member.actionIds:[]).entries()){
      const action=actions.find(x=>x.id===actionId),ap=`${p}/actionIds/${j}`;
      if(!action)ctx.report(root,ap,'REFERENCE_MISSING',`행동 '${actionId}'를 찾을 수 없습니다.`);
      else if(action.expeditionId)ctx.report(root,ap,'ACTOR_SESSION_UNSUPPORTED','대상 지정 행동의 탐험 진입은 별도 세션 계약이 필요합니다.');
    }
  }
  if (members[0]?.characterId !== project.protagonistId) ctx.report(root, '/roster/members/0/characterId', 'ROSTER_PRIMARY', '첫 육성 대상은 기존 주인공 ID와 같아야 합니다.');
  if (isObject(project.gameplay) && Array.isArray(project.gameplay.diets) && project.gameplay.diets.length > 0) ctx.report(root, '/roster', 'ROSTER_LEGACY_FACILITIES', '다인 육성에는 구형 시설 대신 범용 정책·행동을 사용하세요.');
}

export function checkOwner(ctx:SemanticContext,project:JsonObject|undefined,doc:ProjectDocument,value:JsonObject,pointer:string):void {
  const roster=project&&isObject(project.roster)?project.roster:undefined;
  if(doc.kind==='event'&&pointer===''&&value.scope==='actor'){
    if(roster?.stateModel!=='character-v1')ctx.report(doc,'/scope','CHARACTER_STATE_REQUIRED','캐릭터별 사건에는 character-v1 설정이 필요합니다.');
    if(!['interaction','after-event','period-start','after-activity','location-enter'].includes(String(value.trigger)))ctx.report(doc,'/trigger','ACTOR_EVENT_TRIGGER','캐릭터별 사건은 행동·사건 완료·약속 시점에서 연결해야 합니다.');
  }
  if(!isObject(value.owner))return;
  if(roster?.stateModel!=='character-v1'){ctx.report(doc,pointer+'/owner','CHARACTER_STATE_REQUIRED','캐릭터 소유 대상에는 character-v1 설정이 필요합니다.');return;}
  const owner=value.owner;
  if(owner.kind==='character'&&!(roster.members as JsonObject[]).some(m=>m.characterId===owner.characterId))ctx.report(doc,pointer+'/owner/characterId','ACTOR_REQUIRED','육성 대상으로 등록된 캐릭터를 지정하세요.');
  const uiElement=doc.kind==='project'&&/^\/visual\/(components|screens)\/\d+\/elements\/\d+\//.test(pointer);
  if(owner.kind==='actor'&&!(uiElement||doc.kind==='activity'||doc.kind==='event'&&isObject(doc.data)&&doc.data.scope==='actor'||doc.kind==='project'&&pointer.startsWith('/gameplay/actions/')))ctx.report(doc,pointer+'/owner','ACTOR_CONTEXT','실행자 대상은 활동·대상별 사건·행동 또는 대상이 연결된 화면 요소에서 사용하세요.');
  if(value.kind==='attribute'&&!(roster.localAttributeIds as string[]).includes(value.attributeId as string))ctx.report(doc,pointer+'/attributeId','SHARED_ATTRIBUTE','공용 능력은 캐릭터 소유 대상으로 지정할 수 없습니다.');
  if(value.op==='eventCompleted'){
    const event=ctx.resolve(ctx.events,value.eventId as string,doc,pointer+'/eventId','사건');
    if(event&&event.data.scope!=='actor')ctx.report(doc,pointer+'/owner','EVENT_SCOPE','공유 사건에는 캐릭터별 완료 소유자를 지정할 수 없습니다.');
  }
}

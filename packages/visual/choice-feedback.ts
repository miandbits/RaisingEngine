import type {ContentSnapshot,RunState,Transition,Command,NumericTarget} from '../core/model.js';

export interface ChoiceFeedback {
  cursor:NonNullable<RunState['activeNarrative']>;
  choiceId:string;
  consequenceText:string;
  changes:NonNullable<Transition['narrative']>['changes'];
  collecting?:boolean;
}
function pendingFeedback(content:ContentSnapshot,state:RunState):boolean {
  const cursor=state.activeNarrative;if(!cursor)return false;
  const node=(cursor.kind==='ending'?content.endings:content.events).find(s=>s.id===cursor.id)?.nodes.find(n=>n.id===cursor.nodeId);
  return !!node&&node.type!=='dialogue'&&node.type!=='choice'&&node.type!=='end';
}
function definition(content:ContentSnapshot,cursor:ChoiceFeedback['cursor'],choiceId:string){
  const script=(cursor.kind==='ending'?content.endings:content.events).find(s=>s.id===cursor.id);
  const node=script?.nodes.find(n=>n.id===cursor.nodeId);
  return node?.type==='choice'?node.choices.find(c=>c.id===choiceId):undefined;
}
/** Display history only. The runtime has already applied all rules. */
export function trackChoiceFeedback(previous:ChoiceFeedback|null,content:ContentSnapshot,before:RunState,command:Command,result:{state:RunState;transition:Transition}):ChoiceFeedback|null {
  const cursor=before.activeNarrative;
  if(command.type==='choose'&&cursor)return {cursor,choiceId:command.choiceId,
    consequenceText:definition(content,cursor,command.choiceId)?.consequenceText??'',changes:result.transition.narrative?.changes??[],collecting:pendingFeedback(content,result.state)};
  const next=result.state.activeNarrative,owner=previous?.cursor;
  if(!owner||!next||next.kind!==owner.kind||next.id!==owner.id||next.actorId!==owner.actorId)return null;
  if(previous.collecting&&command.type==='advanceNarrative')return {...previous,changes:[...previous.changes,...result.transition.narrative?.changes??[]],collecting:pendingFeedback(content,result.state)};
  return previous;
}
function targetName(content:ContentSnapshot,target:NumericTarget):string {
  if(target.kind==='attribute')return content.attributes.find(a=>a.id===target.attributeId)?.label??target.attributeId;
  if(target.kind==='inventory')return content.project.gameplay?.items.find(i=>i.id===target.itemId)?.name??target.itemId;
  const character=content.characters.find(c=>c.id===target.characterId);
  return `${character?.name??target.characterId} / ${character?.relationships.find(r=>r.id===target.relationshipId)?.label??target.relationshipId}`;
}
export function choiceHints(content:ContentSnapshot,state:RunState|null,locale='ko'):Record<string,string> {
  const cursor=state?.activeNarrative;if(!cursor)return {};
  const script=(cursor.kind==='ending'?content.endings:content.events).find(s=>s.id===cursor.id),node=script?.nodes.find(n=>n.id===cursor.nodeId);
  if(node?.type!=='choice')return {};
  return Object.fromEntries(node.choices.map(choice=>{
    const immediate=[...choice.effects],visited=new Set<string>();let next=choice.next;
    while(!visited.has(next)){visited.add(next);const follow=script?.nodes.find(n=>n.id===next);if(!follow||(follow.type!=='effect'&&follow.type!=='scene'))break;if(follow.type==='effect')immediate.push(...follow.effects);next=follow.next;}
    const effects=immediate.flatMap(effect=>{
      if(effect.op==='setFlag')return [];
      if(effect.op==='setState'){
        const group=content.project.stateGroups?.find(g=>g.id===effect.stateGroupId);
        return [`${group?.name??effect.stateGroupId}: ${group?.values.find(v=>v.id===effect.stateValueId)?.name??effect.stateValueId}`];
      }
      if(effect.op==='add'||effect.op==='set')return [targetName(content,effect.target)+(effect.value.op==='literal'
        ?effect.op==='set'?` = ${effect.value.value}`:` ${effect.value.value>=0?'+':'−'}${Math.abs(effect.value.value)}`
        :locale.startsWith('en')?' (calculated at execution)':locale.startsWith('ja')?' (実行時に計算)':' (현재 상태 기준 계산)')];
      return [locale.startsWith('en')?'Additional rules apply':locale.startsWith('ja')?'追加ルールを適用':'추가 규칙 적용'];
    }).join(' · ');
    const label=locale.startsWith('en')?'Effects':locale.startsWith('ja')?'効果':'효과';
    return [choice.id,[effects?`${label}: ${effects}`:'',choice.consequenceText??''].filter(Boolean).join('\n')];
  }));
}
export function choiceFeedbackText(content:ContentSnapshot,state:RunState|null,feedback:ChoiceFeedback|null,locale='ko'):string {
  if(!feedback||!state?.activeNarrative)return '';
  const cursor=state.activeNarrative,owner=feedback.cursor;
  if(cursor.kind!==owner.kind||cursor.id!==owner.id||cursor.actorId!==owner.actorId)return '';
  const node=(cursor.kind==='ending'?content.endings:content.events).find(s=>s.id===cursor.id)?.nodes.find(n=>n.id===cursor.nodeId);
  if(node?.type!=='dialogue')return '';
  const changes=feedback.changes.filter(c=>c.kind==='numeric').map(c=>c.kind==='numeric'
    ?`${targetName(content,c.target)} ${c.before} → ${c.after} (${c.delta>=0?'+':'−'}${Math.abs(c.delta)})`:'').join(' · ');
  const noChange=locale.startsWith('ja')?'能力値・関係値の変化なし':locale.startsWith('en')?'No stat or relationship change':'능력치·관계 수치 변화 없음';
  const prefix=locale.startsWith('ja')?'選択結果 · ':locale.startsWith('en')?'Choice result · ':'선택 결과 · ';
  const hint=definition(content,owner,feedback.choiceId)?.consequenceText??feedback.consequenceText;
  return prefix+(changes||noChange)+(hint?'\n'+hint:'');
}

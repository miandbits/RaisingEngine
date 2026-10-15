import { periodLabel } from '../../../packages/player-service/schedule.js';
import { assetUrl } from './assets.js';
import { widgetCommand, type WidgetCommand } from './JourneyWidgets.js';
import React, { useEffect, useRef, useState } from 'react';
import type { Confirmation } from '../../../packages/gameplay/model.js';
import type { PlayerAction, PlayerView } from '../../../packages/player-service/controller.js';
import type { ReadonlyDesign, Element, ScreenId } from '../../../packages/visual/model.js';
import { VisualSurface, isVisible, type VisualContext } from './VisualSurface.js';
import { systemsScreens } from '../../../packages/visual/systems-screens.js';
import {TimelineOverlay} from './TimelineOverlay.js';
import {ExtensionPlaybackRate} from './ExtensionElement.js';
import {resultPlaybackText} from '../../../packages/visual/timeline.js';

export function VisualPlayer({view,design,busy,plan,onPlan,act,newRun,save,load,sound,muted,preview,error,notice}: {
  view:PlayerView;design:ReadonlyDesign;busy:boolean;plan:string[];onPlan:(value:string[])=>void;
  act:(action:PlayerAction)=>void;newRun:(userValues?:Record<string,string>)=>void;save:()=>void;load:()=>void;sound:()=>void;muted:boolean;preview:boolean;error:string|null;notice:string|null;
}) {
  const defaults=()=>Object.fromEntries((view.userFields??[]).map(field=>[field.id,field.initial]));
  const [draft,setDraft]=useState<Record<string,string>>(defaults);
  const fieldsKey=JSON.stringify(view.userFields??[]);
  useEffect(()=>{setDraft(defaults());},[view.generation,fieldsKey]);
  const savedValuesKey=JSON.stringify(view.state?.userValues??null);
  useEffect(()=>{if(view.state)setDraft({...view.state.userValues});},[savedValuesKey,!!view.state]);
  type Pending={command:WidgetCommand;confirmation:Confirmation;summary:string;revision:number;generation:number};
  const [pending,setPending]=useState<Pending|null>(null);const pendingRef=useRef<Pending|null>(null);
  const cancel=()=>{pendingRef.current=null;setPending(null);};
  useEffect(()=>{cancel();},[view.generation,view.state?.stateRevision]);
  useEffect(()=>{if(!pending)return;const handler=(event:KeyboardEvent)=>{if(event.key==='Escape'){event.preventDefault();event.stopImmediatePropagation();cancel();}else if(event.key==='F5'||event.key==='F9'){event.preventDefault();event.stopImmediatePropagation();}};window.addEventListener('keydown',handler,true);return()=>window.removeEventListener('keydown',handler,true);},[pending]);
  const run=(command:WidgetCommand)=>{
    if(busy||pendingRef.current)return;
    const definition=view.gameplayDefinition;let confirmation:Confirmation|undefined,summary='',requires=false;
    const quantity='quantity'in command?command.quantity:1;
    if(command.type==='buy'||command.type==='sell'){
      const shop=definition?.shops.find(s=>s.id===command.shopId);const row=(command.type==='buy'?shop?.offers:shop?.sellOffers)?.find(o=>o.itemId===command.itemId);
      confirmation=row?.confirmation;requires=command.type==='sell'||!!confirmation||!!design.confirmPurchases;
      const currency=view.attributes.find(a=>a.id===shop?.currencyAttributeId)?.label??shop?.currencyAttributeId??'';
      summary=`${definition?.items.find(i=>i.id===command.itemId)?.name??command.itemId} × ${quantity}\n${command.type==='sell'?'받을 금액':'비용'}: ${currency} ${(row?.price??0)*quantity}`;
    } else if(command.type==='executeAction'||command.type==='selectPolicy'){
      const group=command.type==='selectPolicy'?view.gameplay?.policyGroups?.find(g=>g.id===command.groupId):undefined;
      const row=command.type==='executeAction'?view.gameplay?.actions?.find(a=>a.id===command.actionId):group?.options.find(o=>o.id===command.optionId);
      confirmation=row?.confirmation;requires=true;
      if(!row)summary=`${group?.name??'정책'} 선택을 해제합니다.`;
      else {
        const costs=row.costs.map(c=>`${view.attributes.find(a=>a.id===c.attributeId)?.label??c.attributeId} ${c.amount*quantity}`).join(' · ')||'무료';
        const effects=row.effects.map(e=>{
          if(e.op==='setFlag')return `${e.flagId} ${e.value?'켜짐':'꺼짐'}`;
          const target=e.target;
          const label=target.kind==='attribute'?view.attributes.find(a=>a.id===target.attributeId)?.label??target.attributeId:target.kind==='inventory'?definition?.items.find(i=>i.id===target.itemId)?.name??target.itemId:target.relationshipId;
          return `${label} ${e.op==='set'?'= ':''}${e.value.op==='literal'?`${e.op==='add'&&e.value.value>=0?'+':''}${e.value.value*(e.op==='add'?quantity:1)}`:'현재 상태 기준 계산'}`;
        }).join(' · ');
        summary=`${row.name}${command.type==='executeAction'?` × ${quantity}`:''}\n비용: ${costs}\n효과: ${effects}`;
        if(group)summary+=`\n${group.trigger.type==='afterActivity'?'활동 종료 후':'활동 시작 전'} · ${group.trigger.intervalPeriods??1}기간마다 적용. 선택 시에는 비용이 차감되지 않습니다.`;
      }
    } else if(command.type==='useService'||command.type==='setDiet'){
      const row=command.type==='useService'?definition?.services?.find(s=>s.id===command.serviceId):definition?.diets?.find(d=>d.id===command.dietId);
      confirmation=row?.confirmation;requires=true;
      if(!row)summary='식단을 해제합니다. 이후 기간에는 식단 비용과 효과가 적용되지 않습니다.';
      else {
        const costs=row.costs.map(c=>`${view.attributes.find(a=>a.id===c.attributeId)?.label??c.attributeId} ${c.amount*quantity}`).join(' · ')||'무료';
        const effects=row.effects.map(e=>{
          if(e.op==='setFlag')return `${e.flagId} ${e.value?'켜짐':'꺼짐'}`;
          const target=e.target;
          const label=target.kind==='attribute'?view.attributes.find(a=>a.id===target.attributeId)?.label??target.attributeId:target.kind==='inventory'?definition?.items.find(i=>i.id===target.itemId)?.name??target.itemId:target.relationshipId;
          return `${label} ${e.op==='set'?'= ':''}${e.value.op==='literal'?`${e.op==='add'&&e.value.value>=0?'+':''}${e.value.value*(e.op==='add'?quantity:1)}`:'현재 상태 기준 계산'}`;
        }).join(' · ');
        summary=`${row.name}${command.type==='useService'?` × ${quantity}`:''}\n비용: ${costs}\n효과: ${effects}`;
        if(command.type==='setDiet')summary+=`\n${'intervalPeriods'in row?row.intervalPeriods??1:1}기간마다 적용. 선택 시에는 비용이 차감되지 않습니다.`;
      }
    }
    if(requires&&confirmation?.enabled!==false&&view.state){const value={command,confirmation:confirmation??{},summary,revision:view.state.stateRevision,generation:view.generation};pendingRef.current=value;setPending(value);return;}
    act(command);
  };
  const confirm=()=>{const value=pendingRef.current;if(!value||busy)return;cancel();if(view.generation===value.generation&&view.state?.stateRevision===value.revision)act(value.command);};
  const s=view.state;const node=view.narrative?.node;
  const scene=s?.presentation??design.defaultScene;
  const portraits:Record<string,string>={};
  const portraitTransforms:NonNullable<VisualContext['portraitTransforms']>={};
  for(const p of scene?.portraits??[]) {const id=view.characters.find(c=>c.id===p.characterId)?.portraits[p.portraitId];if(id)portraits[p.position]=id;}
  for(const p of scene?.portraits??[])portraitTransforms[p.position]=p;
  const screen:ScreenId=!s?'title':s.phase==='finished'?'ending':s.phase==='narrative'?'story':s.phase==='battle'?'battle':s.phase==='expedition'?'expedition':['planning','ready'].includes(s.phase)?s.gameplay?.screenId??'schedule':'schedule';
  const result=s?.lastActivityResult;
  const noNumericChanges=result&&!result.changes.some(c=>c.kind==='numeric'&&c.delta!==0);

  const needsSystemScreen = ['battle','expedition'].includes(screen) && !design.screens.some(candidate => candidate.id === screen && candidate.elements.some(element => (element.kind === screen||screen==='battle'&&(element.kind==='collection'&&element.source?.type==='battleActions'||element.kind==='button'&&['battleAction','selectBattleAction'].includes(element.action??''))) && isVisible(element, { phase: screen })));
  const effectiveDesign = needsSystemScreen ? { ...design, screens: [...design.screens.filter(candidate => candidate.id !== screen), ...systemsScreens(design.width, design.height).filter(candidate => candidate.id === screen)] } : design;
  const attributeName=(id:string)=>view.attributes.find(a=>a.id===id)?.label??id;
  const itemName=(id:string)=>view.gameplayDefinition?.items.find(item=>item.id===id)?.name??id;
  const context:VisualContext={activeDietId:s?.gameplay?.dietId??null,systems:view.systems,items:view.gameplayDefinition?.items??[],gameplay:view.gameplay,location:view.gameplay?.location.name??'',relationships:view.characters.flatMap(c=>c.relationships.map(r=>({id:`${c.id}-${r.id}`,characterId:c.id,relationshipId:r.id,name:c.name,label:r.label,value:s?.relationships[c.id]?.[r.id]??view.initialState.relationships?.[c.id]?.[r.id]??r.initial,min:r.min,max:r.max}))),title:view.title,description:view.description,calendar:view.time?`${view.time.year}년 ${view.time.month}월 · ${periodLabel(view.time.periodInMonth,view.periodsPerMonth??3)}`:'',ending:view.endingLabel??'',speaker:node?.type==='dialogue'&&node.speakerId?view.characters.find(c=>c.id===node.speakerId)?.name??'이야기':view.storyLabel??'이야기',story:node?.type==='dialogue'?node.text:node?.type==='choice'?node.prompt:'',phase:node?.type==='choice'?'choice':node?.type==='dialogue'?'dialogue':s?.phase??'title',background:s?s.presentation.backgroundAssetId:design.defaultScene?.backgroundAssetId??null,portraits,stats:view.attributes.map(a=>({...a,value:s?.attributes[a.id]??view.initialState.attributes?.[a.id]??a.initial})),activities:view.activities,plan,period:view.time?.periodInMonth??0,choices:view.narrative?.choices??[],result:result?`${view.activities.find(a=>a.id===result.activityId)?.label??''} 완료${result.success?` · ${result.success.passed?'성공':'실패'}`:''}\n`+result.changes.filter(c=>c.kind==='numeric'&&c.delta!==0).map(c=>c.kind==='numeric'?`${c.target.kind==='attribute'?attributeName(c.target.attributeId):c.target.kind==='inventory'?itemName(c.target.itemId):'관계'} ${c.delta>0?'+':''}${c.delta}`:'').join(' · '):''};
  if(noNumericChanges)context.result+='능력 변화 없음';
  if(s?.phase==='ready'&&view.schedulePlayback==='continuous'&&design.resultPlayback&&!error)context.phase='auto-ready';
  if(view.timeline?.plan.resultPlayback)context.result=resultPlaybackText(view.timeline.plan,view.timeline.cursor)??context.result;
  context.portraitTransforms=portraitTransforms;
  if(view.time)context.calendarTime={year:view.time.year,month:view.time.month};
  context.planningKey=`${view.generation}:${Math.floor((s?.periodIndex??0)/(view.periodsPerMonth??3))}`;context.scheduleCommitted=!!s?.schedule;context.periodsPerMonth=view.periodsPerMonth??3;context.schedulePlayback=view.schedulePlayback??'manual';
  const action=(action:NonNullable<Element['action']>,element:Element)=>{
    if(pendingRef.current)return;const command=widgetCommand(element);if(command){run(command);return;}
    if(action==='start'){if(!busy)newRun(view.userFields?.length?{...(s?.userValues??draft)}:undefined);}else if(action==='save')save();else if(action==='load')load();else if(action==='sound')sound();
    else if(action==='advance'&&s){if(['planning','ready'].includes(s.phase)){if(!s.schedule||plan.some((id,i)=>id!==s.schedule?.[i]))act({type:'schedule',activityIds:plan});else act({type:'execute'});}else act({type:'continue'});}
  };
  return <ExtensionPlaybackRate.Provider value={view.timeline?.plan.resultPlayback?(view.timeline.cursor.paused?0:view.timeline.cursor.speed):1}><div className="visual-player" data-runtime-backend={view.runtimeBackend}><div className="visual-status"><span>{view.title} · {busy?'처리 중…':preview?'초안 미리보기':'플레이 중'}</span><span>{muted?'소리 꺼짐':'소리 켜짐'}</span></div>{error&&<div role="alert" className="alert">{error}</div>}{notice&&<p role="status" className="notice">{notice}</p>}
    <VisualSurface design={effectiveDesign} screenId={screen} context={{...context,userFields:view.userFields??[],userValues:s?.userValues??draft,userFieldsEditable:!s,...(view.dataUi?{dataUi:view.dataUi}:{})}} assetUrl={id=>assetUrl(id,view.generation)} busy={busy||!!pending} fitViewport onUserValue={(id,value)=>setDraft(previous=>({...previous,[id]:value}))} onAction={action} onCommitPlan={()=>act({type:'commitPlan',activityIds:plan})} onCommand={run} onChoice={choiceId=>act({type:'choose',choiceId})} onPlan={(index,id)=>{const next:string[]=[...plan];next[index]=id;onPlan(next);}}>
      {pending&&<div className="transaction-overlay"><div className="transaction-dialog" role="dialog" aria-modal="true" aria-labelledby="transaction-title"><h3 id="transaction-title">{pending.confirmation.title??'거래 확인'}</h3><p>{pending.confirmation.message??'계속하시겠습니까?'}</p><p className="transaction-summary">{pending.summary}</p><small>효과는 능력치 범위에 따라 제한됩니다.</small><div><button autoFocus disabled={busy} onClick={confirm}>{pending.confirmation.confirmLabel??'확인'}</button><button disabled={busy} onClick={cancel}>{pending.confirmation.cancelLabel??'취소'}</button></div></div></div>}
    </VisualSurface>
    <TimelineOverlay timeline={view.timeline??null} generation={view.generation} busy={busy} act={act} save={save} load={load}/>
  </div></ExtensionPlaybackRate.Provider>;
}

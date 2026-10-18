import { resolvePresentation, presentationActivity } from '../../../packages/visual/variants.js';
import React, { useEffect, useRef, useState } from 'react';
import type { Element, ScreenId, ReadonlyDesign } from '../../../packages/visual/model.js';
import './visual.css';
import { dataBindingText } from '../../../packages/visual/data-binding.js';
import { numericBinding, userFieldBinding } from '../../../packages/visual/bindings.js';
import { JourneyWidget, type WidgetCommand, type GameWidgetsView, type RelationshipRow } from './JourneyWidgets.js';
import { SystemsWidget } from './SystemsWidget.js';
import type { SystemsView } from '../../../packages/systems/model.js';
import { resolveElementStyle } from '../../../packages/visual/styles.js';
import { logicalViewport, resolveScreenElements } from '../../../packages/visual/components.js';
import { resolveCollectionRow, type BoundElement } from '../../../packages/visual/data-ui.js';
import type { DataUiProjection } from '../../../packages/visual/data-ui-projection.js';
import {ScheduleWidget,useSchedulePlanner,type ScheduleActivity} from './ScheduleWidget.js';
import {RosterSchedule} from './RosterSchedule.js';
import {ExtensionView} from './ExtensionElement.js';
import type {ExtensionElement as ExtensionSpec} from '../../../packages/web-player/extensions.js';

export interface VisualContext {
  characters?: readonly {id:string;name:string}[];
  actorResults?: readonly {characterId:string;text:string}[];
  dataState?: import('../../../packages/core/model.js').RunState | null;
  userFields?:readonly {id:string;label:string;initial:string;minLength:number;maxLength:number}[];
  userValues?:Readonly<Record<string,string>>;
  userFieldsEditable?:boolean;
  dataUi?: DataUiProjection;
  systems?: SystemsView | null;
  title:string; description:string; calendar:string; ending:string; speaker:string; story:string;
  activeDietId?:string|null; location?:string; gameplay?:GameWidgetsView|null; relationships?:readonly RelationshipRow[];
  items?:readonly {id:string;name?:string;initialQuantity?:number;imageAssetId?:string;description?:string;maxStack?:number}[];
  phase:string; background:string|null; portraits:Record<string,string>;
  portraitTransforms?:Record<string,{scale?:number;offsetX?:number;offsetY?:number}>;
  stats:{id:string;label:string;value:number;min:number;max:number}[];
  activities:readonly ScheduleActivity[]; plan:readonly string[]; period:number;
  periodsPerMonth?:number; schedulePlayback?:string;
  calendarTime?:{year:number;month:number}; planningKey?:string; scheduleCommitted?:boolean;
  choices:readonly {id:string;text:string;enabled:boolean}[]; result:string;
}
export function isVisible(element:Pick<Element,'hidden'|'visibleWhen'>, context:Pick<VisualContext, 'phase'>): boolean {
  if(element.hidden)return false;
  const when=element.visibleWhen??'always';
  return when==='always'||when===context.phase||when==='planning'&&['planning','ready'].includes(context.phase)||when==='result'&&['activity-result','period-end'].includes(context.phase)||when==='dialogue'&&context.phase==='dialogue'||when==='choice'&&context.phase==='choice'||when==='finished'&&context.phase==='finished';
}
export function imageFor(element:Element,context:VisualContext):string|undefined {
  const source=element.imageSource??'asset';
  return source==='background'?context.background??undefined:source.startsWith('portrait-')?context.portraits[source.slice(9)]:element.imageAssetId;
}
function TransitionImage({src,element}:{src:string;element:Element}) {
  const previous=useRef(src);const [old,setOld]=useState<string|null>(null);
  useEffect(()=>{if(previous.current!==src){setOld(previous.current);previous.current=src;}const timer=setTimeout(()=>setOld(null),element.durationMs??400);return()=>clearTimeout(timer);},[src,element.durationMs]);
  return <>{old&&element.motion==='fade'&&<img className="visual-old-image" src={old} alt="" aria-hidden="true" style={{objectFit:element.fit??'contain'}}/>}<img key={src} className={`visual-image motion-${element.motion??'none'}`} src={src} alt={element.label??element.id} draggable={false} style={{objectFit:element.fit??'contain',animationDuration:`${element.durationMs??400}ms`}}/></>;
}
export function VisualSurface({design,screenId,context,assetUrl,busy=false,editing=false,fitViewport=false,onAction,onPlan,onCommitPlan,onChoice,onCommand,onUserValue,children}: {
  design:ReadonlyDesign;screenId:ScreenId;context:VisualContext;assetUrl:(id:string)=>string;busy?:boolean;editing?:boolean;fitViewport?:boolean;
  onAction?:(action:NonNullable<Element['action']>,element:Element)=>void;onCommand?:(command:WidgetCommand)=>void;onPlan?:(index:number,id:string)=>void;onCommitPlan?:()=>void;onChoice?:(id:string)=>void;onUserValue?:(id:string,value:string)=>void;children?:React.ReactNode;
}) {
  const host=useRef<HTMLDivElement>(null);const [viewport,setViewport]=useState({width:design.width,height:design.height});
  useEffect(()=>{if(!host.current)return;const observer=new ResizeObserver(([entry])=>{if(entry&&entry.contentRect.width>0&&entry.contentRect.height>0){setViewport({width:entry.contentRect.width,height:entry.contentRect.height});}});observer.observe(host.current);return()=>observer.disconnect();},[]);
  const size=fitViewport?logicalViewport(design,viewport.width,viewport.height):{width:design.width,height:design.height};
  const scale=fitViewport?Math.min(viewport.width/size.width,viewport.height/size.height):viewport.width/design.width;
  const offset=fitViewport?{x:(viewport.width-size.width*scale)/2,y:(viewport.height-size.height*scale)/2}:{x:0,y:0};
  const elements=resolveScreenElements(design,screenId,size);
  useEffect(()=>{window.raisingExtensions?.commitPresentation();},[context.dataState,screenId]);
  const planning=useSchedulePlanner(context);
  const calendarBinding=(binding:Element['binding']):string|null=>{
    if(!binding?.startsWith('calendar-'))return null;
    const {year,month}=context.calendarTime??{year:1,month:1},day=context.period*((context.periodsPerMonth??3)===3?10:Math.floor(new Date(year,month,0).getDate()/(context.periodsPerMonth??3)))+1;
    if(binding==='calendar-year')return String(year);
    if(binding==='calendar-month')return `${month}월`;
    if(binding==='calendar-day')return String(day);
    if(binding==='calendar-weekday'){const date=new Date(0);date.setUTCHours(0,0,0,0);date.setUTCFullYear(year,month-1,day);return `${['일','월','화','수','목','금','토'][date.getUTCDay()]}요일`;}
    return null;
  };
  const schedulePrompt=planning.planner.mask!==((1<<(context.periodsPerMonth??3))-1)?`스케줄 결정.\n이번 달 딸의 예정은? 제 ${planning.planner.slot+1}회`:context.schedulePlayback==='continuous'?'이번 달 일정을 연속으로 진행합니다.':context.period===0?'예, 스케줄에 편성하겠습니다.\n첫 일정을 시작하시겠습니까?':`이번 달 ${context.period}번째 일정을 마쳤습니다.\n다음 일정을 진행하시겠습니까?`;
  const scheduleGroups=elements.find(e=>e.kind==='schedule'&&e.scheduleLayout==='menu')?.scheduleGroups??design.screens.flatMap(s=>s.elements).find(e=>e.kind==='schedule'&&e.scheduleLayout==='menu')?.scheduleGroups??[];
  const fontIds=[...new Set([...elements,...(design.components??[]).flatMap(c=>c.elements)].map(e=>resolveElementStyle(design,e).fontAssetId).filter((id):id is string=>!!id))];
  const fontCss=fontIds.map(id=>`@font-face{font-family:"engine-${id}";src:url(${JSON.stringify(assetUrl(id))});font-display:block;}`).join('\n');
  const conditionPasses=(condition:Element['visibleCondition'])=>!condition||context.dataUi?.conditions[JSON.stringify(condition)]===true;
  const renderElement=(raw:BoundElement):React.ReactNode=>{
      let element=resolveElementStyle(design,editing?raw:resolvePresentation(raw,presentationActivity(context.dataState,raw),conditionPasses));
      const portrait=element.kind==='image'&&element.imageSource?.startsWith('portrait-')?context.portraitTransforms?.[element.imageSource.slice(9)]:undefined;
      if(portrait){const scale=portrait.scale??1,w=element.width,h=element.height;element={...element,x:element.x+(w-w*scale)/2+w*(portrait.offsetX??0),y:element.y+h-h*scale+h*(portrait.offsetY??0),width:w*scale,height:h*scale};}
      if(!editing&&(!isVisible(element,context)||!conditionPasses(element.visibleCondition)))return null;
      const availability=context.dataUi?.availability[screenId]?.[raw.id];
      const enabled=editing||conditionPasses(element.enabledCondition)&&raw.dataEnabled!==false&&availability?.enabled!==false;
      const reason=raw.dataReason||availability?.reason||(!conditionPasses(element.enabledCondition)||raw.dataEnabled===false?'사용 조건을 충족하지 못했습니다.':'');
      const style:React.CSSProperties={left:element.x,top:element.y,width:element.width,height:element.height,background:element.fill??'transparent',color:element.color??'#304d46',fontSize:element.fontSize??22,borderRadius:element.radius??0,opacity:editing&&!isVisible(element,context)?0.3:element.opacity??1,textAlign:element.align??'left'};
      if(raw._clipRect){const clip=raw._clipRect,left=Math.max(0,clip.x-element.x),top=Math.max(0,clip.y-element.y),right=Math.max(0,element.x+element.width-clip.x-clip.width),bottom=Math.max(0,element.y+element.height-clip.y-clip.height);if(left+right>=element.width||top+bottom>=element.height)return null;style.clipPath=`inset(${top}px ${right}px ${bottom}px ${left}px)`;}
      if(element.kind==='group')style.pointerEvents='none';
      if(element.fontAssetId)style.fontFamily=`"engine-${element.fontAssetId}", sans-serif`;
      if(element.padding!==undefined)style.padding=element.padding;
      else if(element.kind==='text'&&element.uiContext)style.padding=0;
      if(element.borderWidth!==undefined){style.borderWidth=element.borderWidth;style.borderStyle='solid';style.borderColor=element.borderColor??'#57806c';}
      Object.assign(style,{'--style-hover':element.hoverFill,'--style-pressed':element.pressedFill,'--style-disabled':element.disabledColor});
      const source=imageFor(element,context);
      const contextText=element.binding==='context-name'?(context.dataUi?.contextNames?.[`${element.contextField}:${element.uiContext?.[element.contextField??'actorId']}`]??'—'):null;
      const text=contextText??userFieldBinding(element,context.userValues)??dataBindingText(element,context)??numericBinding(element,context)??calendarBinding(element.binding)??(element.binding==='schedule-prompt'?schedulePrompt:null)??(element.binding&&element.binding!=='none'&&element.binding in context?String(context[element.binding as keyof VisualContext]??''):element.text??'텍스트');
      let body:React.ReactNode=null;
      if(element.kind==='image')body=source?<TransitionImage src={assetUrl(source)} element={element}/>:editing?<div className="visual-placeholder">{element.imageSource??'이미지'} · 에셋 선택</div>:null;
      if(element.kind==='text')body=<span>{text}</span>;
      if((element.kind as string)==='plugin'){style.overflow='visible';body=<ExtensionView instance={`${screenId}/${element.id}`} extension={(element as unknown as {extension?:ExtensionSpec}).extension} width={element.width} height={element.height} assetUrl={assetUrl} editing={editing}/>;}
      if(element.kind==='input'){
        const field=context.userFields?.find(f=>f.id===element.userFieldId);
        body=<input className="visual-input" type="text" aria-label={field?.label??element.label??element.id} value={context.userValues?.[element.userFieldId??'']??field?.initial??''} disabled={busy||!enabled||!field||editing||!context.userFieldsEditable} autoComplete="off" spellCheck={false} onChange={event=>field&&onUserValue?.(field.id,event.target.value)}/>;
      }
      if(element.kind==='meter')body=<div className="visual-meter" style={{'--meter-track':element.fill??'transparent'} as React.CSSProperties}><span>{element.text??element.label??''}</span><progress aria-label={element.label??element.text??element.id} max={Math.max(1,element.maxValue??100)} value={Math.max(0,Math.min(element.value??0,element.maxValue??100))}/></div>;
      if(element.kind==='stats')body=<div className="visual-stat-list">{context.stats.map(s=><div key={s.id}><div><span>{s.label}</span><strong data-testid={`stat-${s.id}`}>{s.value}</strong></div><progress aria-label={s.label} value={s.value-s.min} max={s.max-s.min||1}/></div>)}</div>;
      if(element.kind==='schedule')body=context.dataState?.roster
        ?<RosterSchedule context={context} disabled={busy||!enabled||editing} onCommand={onCommand} onExecute={()=>onAction?.('advance',element)}/>
        :<ScheduleWidget element={element} context={context} planning={planning} groups={element.scheduleGroups??scheduleGroups} assetUrl={assetUrl} disabled={busy||!enabled||editing} onPlan={onPlan} onCommit={onCommitPlan}/>;
      if(element.kind==='dialogue')body=<p className="visual-dialogue" data-testid="dialogue">{context.story}</p>;
      if(element.kind==='result')body=context.actorResults?.length
        ?<div className="visual-actor-results">{context.actorResults.map(row=><div key={row.characterId}>{row.text}</div>)}</div>
        :<div className="visual-result">{context.result||'활동 결과가 여기에 표시됩니다.'}</div>;
      if(element.kind==='choices')body=<div className="visual-choices">{(editing&&!context.choices.length?[{id:'preview-one',text:'함께 도전한다',enabled:true},{id:'preview-two',text:'조금 더 준비한다',enabled:true}]:context.choices).map(c=><button key={c.id} disabled={busy||!enabled||!c.enabled} onClick={()=>onChoice?.(c.id)}>{c.text}{!c.enabled&&<small>조건 미충족</small>}</button>)}</div>;
      if(element.kind==='button')body=<button className="visual-action" title={reason||undefined} disabled={busy||!enabled||element.action==='advance'&&['choice','finished','title'].includes(context.phase)} onClick={()=>element.action&&onAction?.(element.action,element)}>{element.action==='advance'&&context.phase==='activity-result'?'다음으로':element.text??'버튼'}</button>;
      if(element.kind==='collection'){
        style.padding=0;
        const rows=context.dataUi?.collections[screenId]?.[element.id]??[];
        const component=design.components?.find(c=>c.id===element.componentId);
        const gap=element.gap??8,columns=element.layout==='grid'?Math.max(1,element.columns??1):1;
        const width=Math.max(16,(element.width-16-gap*(columns-1))/columns);
        const height=element.rowHeight??component?.height??100;
        body=<div className="visual-collection-grid" style={{display:'grid',gridTemplateColumns:`repeat(${columns},minmax(0,1fr))`,gap}}>
          {rows.map(row=><div key={row.id} className="visual-collection-row" data-row-id={row.id} style={{position:'relative',height,minWidth:0}}>{resolveCollectionRow(design,element,row,width).map(child=>renderElement({...child,dataEnabled:enabled&&child.dataEnabled!==false,dataReason:child.dataReason||reason}))}</div>)}
          {!rows.length&&<div className="visual-collection-empty">표시할 데이터가 없습니다.</div>}
        </div>;
      }
      if(['locations','shop','inventory','interactions','relationships','diets','services','sell','policies','actions'].includes(element.kind))body=<JourneyWidget element={element} gameplay={context.gameplay} actorActions={context.dataUi?.actorActions} relationships={context.relationships} items={context.items} stats={context.stats} activeDietId={context.activeDietId} assetUrl={assetUrl} busy={busy||!enabled} editing={editing} onCommand={onCommand}/>;
      if(['equipment','quests','expedition','battle','relationship-stages'].includes(element.kind))body=<SystemsWidget kind={element.kind} systems={context.systems} busy={busy||!enabled} editing={editing} onCommand={onCommand}/>;
      return <div className={`visual-element visual-${element.kind} ${raw.styleId||design.defaultStyle?'visual-styled':''}`} style={style} key={element.id} data-element-id={element.id}>{body}</div>;
  };
  return <div className={`visual-viewport ${fitViewport?'visual-fit':''}`} ref={host} style={fitViewport?{}:{aspectRatio:`${design.width}/${design.height}`}}><div className="visual-stage" data-screen={screenId} style={{width:size.width,height:size.height,left:offset.x,top:offset.y,transform:`scale(${scale})`}}>
    <style>{fontCss}</style>
    {elements.map(renderElement)}{children}
  </div></div>;
}


import {SurfacePaint,hasSurface} from './SurfacePaint.js';
import {resolveControlStyle} from '../../../packages/visual/control-styles.js';
import {controlColorProps} from './ControlColors.js';
import {PhaseForecast} from './PhaseForecast.js';
import {progressionButtonLabel} from './progression-label.js';
import {useResumeContext} from './ResumeContext.js';
import {PresentedHistoryText,useReadingHistory,historyTextVisible} from './ReadingHistory.js';
import type {ReadingHistoryReceipt} from '../../../packages/visual/reading-history.js';
import {ReadingDialog} from './ReadingDialog.js';
import {useReadingPosition} from './reading-position.js';
import {slotWords} from '../../../packages/player-service/save-slots.js';
import {readableLayoutForScreen,type ResolvedReadableItem} from '../../../packages/visual/readable-layout.js';
import {usePlayerPreferences,PlayerSettingsControls} from './PlayerSettings.js';
import {SaveSlotsWidget} from './SaveSlotsWidget.js';
import type {SaveSlotsView,SaveSlotAction} from '../../../packages/player-service/save-slots.js';
import {calendarGrid,calendarLabels,type CalendarProject} from '../../../packages/core/calendar.js';
import { resolvePresentation, presentationActivity } from '../../../packages/visual/variants.js';
import React, { useEffect, useRef, useState } from 'react';
import type { Element, ScreenId, ReadonlyDesign } from '../../../packages/visual/model.js';
import './visual.css';
import { dataBindingText, dataBindingValue } from '../../../packages/visual/data-binding.js';
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
  forecastPhase?:import('./ScheduleWidget.js').ScheduleContext['forecastPhase'];
  historyReceipt?:Omit<ReadingHistoryReceipt,'text'|'speaker'>;
  gameId?:string;generation?:number;lastCommittedPlan?:import('../../../packages/player-service/plan-reuse.js').LastCommittedPlan;
  readingKey?:string;
  saveSlots?:SaveSlotsView;
  onSlotAction?:(action:SaveSlotAction)=>void;
  rosterCharacterIds?: readonly string[];
  'choice-result'?:string;
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
  speakerPortrait?:string|null;
  portraitTransforms?:Record<string,{scale?:number;offsetX?:number;offsetY?:number}>;
  stats:{id:string;label:string;value:number;min:number;max:number}[];
  activities:readonly ScheduleActivity[]; plan:readonly string[]; period:number;
  periodsPerMonth?:number; schedulePlayback?:string;
  calendarProject?:CalendarProject;locale?:string;
  calendarTime?:{year:number;month:number}; planningKey?:string; scheduleCommitted?:boolean;
  choices:readonly {id:string;text:string;enabled:boolean;hint?:string}[]; result:string;
}
// Hierarchy resolution already multiplies ancestor opacity. Scroll containers
// must paint their own decoration with that alpha rather than composite the
// whole DOM subtree, preserving the native and original flat-render semantics.
function paintAlpha(colour:string,opacity:number):string {
  const match=/^#([\da-f]{6})([\da-f]{2})?$/i.exec(colour);
  if(!match)return colour;
  const rgb=match[1]!,alpha=(match[2]?parseInt(match[2],16)/255:1)*Math.max(0,Math.min(1,opacity));
  return `rgba(${parseInt(rgb.slice(0,2),16)},${parseInt(rgb.slice(2,4),16)},${parseInt(rgb.slice(4,6),16)},${alpha})`;
}
export function isVisible(element:Pick<Element,'hidden'|'visibleWhen'>, context:Pick<VisualContext, 'phase'>): boolean {
  if(element.hidden)return false;
  const when=element.visibleWhen??'always';
  return when==='always'||when===context.phase||when==='planning'&&['planning','ready'].includes(context.phase)||when==='result'&&['activity-result','period-end'].includes(context.phase)||when==='dialogue'&&context.phase==='dialogue'||when==='choice'&&context.phase==='choice'||when==='finished'&&context.phase==='finished';
}
export function imageFor(element:Element,context:VisualContext):string|undefined {
  const source=element.imageSource??'asset';
  if(source==='speaker-portrait')return context.speakerPortrait??undefined;
  return source==='background'?context.background??undefined:source.startsWith('portrait-')?context.portraits[source.slice(9)]:element.imageAssetId;
}
function TransitionImage({src,element}:{src:string;element:Element}) {
  const previous=useRef(src);const [old,setOld]=useState<string|null>(null);
  useEffect(()=>{if(previous.current!==src){setOld(previous.current);previous.current=src;}const timer=setTimeout(()=>setOld(null),element.durationMs??400);return()=>clearTimeout(timer);},[src,element.durationMs]);
  return <>{old&&element.motion==='fade'&&<img className="visual-old-image" src={old} alt="" aria-hidden="true" style={{objectFit:element.fit??'contain'}}/>}<img key={src} className={`visual-image motion-${element.motion??'none'}`} src={src} alt={element.label??element.id} draggable={false} style={{objectFit:element.fit??'contain',animationDuration:`${element.durationMs??400}ms`}}/></>;
}
function readableInk(background:string):string {
  const rgb=parseInt(background.slice(1),16);
  return .2126*((rgb>>16)&255)+.7152*((rgb>>8)&255)+.0722*(rgb&255)>145?'#263c36':'#f5efdf';
}
export function VisualSurface({design,screenId,context,assetUrl,busy=false,editing=false,fitViewport=false,onAction,onPlan,onPlanReplace,onCommitPlan,onChoice,onCommand,onUserValue,children}: {
  design:ReadonlyDesign;screenId:ScreenId;context:VisualContext;assetUrl:(id:string)=>string;busy?:boolean;editing?:boolean;fitViewport?:boolean;
  onAction?:(action:NonNullable<Element['action']>,element:Element)=>void;onCommand?:(command:WidgetCommand)=>void;onPlan?:(index:number,id:string)=>void;onPlanReplace?:(ids:string[])=>void;onCommitPlan?:()=>void;onChoice?:(id:string,text?:string)=>void;onUserValue?:(id:string,value:string)=>void;children?:React.ReactNode;
}) {
  const preferences=usePlayerPreferences(),history=useReadingHistory(),resume=useResumeContext();
  const [menuOpen,setMenuOpen]=useState(false);
  useEffect(()=>{const close=()=>setMenuOpen(false);window.addEventListener('raising-open-reading-history',close);window.addEventListener('raising-open-resume-context',close);return()=>{window.removeEventListener('raising-open-reading-history',close);window.removeEventListener('raising-open-resume-context',close);};},[]);
  const readingScroll=useRef<HTMLDivElement>(null),readingActions=useRef<HTMLDivElement>(null);
  const gesture=useRef({x:0,y:0,dragged:false});
  const host=useRef<HTMLDivElement>(null);const [viewport,setViewport]=useState({width:design.width,height:design.height,safeWidth:design.width,safeHeight:design.height});
  useEffect(()=>{if(!host.current)return;const observer=new ResizeObserver(([entry])=>{if(entry&&entry.contentRect.width>0&&entry.contentRect.height>0){const shell=host.current?.closest<HTMLElement>('.visual-shell'),style=shell?getComputedStyle(shell):null;
      setViewport({width:entry.contentRect.width,height:entry.contentRect.height,safeWidth:shell&&style?shell.clientWidth-parseFloat(style.paddingLeft)-parseFloat(style.paddingRight):entry.contentRect.width,safeHeight:shell&&style?shell.clientHeight-parseFloat(style.paddingTop)-parseFloat(style.paddingBottom):entry.contentRect.height});}});observer.observe(host.current);return()=>observer.disconnect();},[]);
  const size=fitViewport?logicalViewport(design,viewport.width,viewport.height):{width:design.width,height:design.height};
  const scale=fitViewport?Math.min(viewport.width/size.width,viewport.height/size.height):viewport.width/design.width;
  const offset=fitViewport?{x:(viewport.width-size.width*scale)/2,y:(viewport.height-size.height*scale)/2}:{x:0,y:0};
  const defaultReadable=fitViewport&&!editing?readableLayoutForScreen(design,screenId,{width:viewport.safeWidth,height:viewport.safeHeight},preferences?.value):null;
  // Retain the opening layout while a modal is present. Switching a desktop
  // schedule into the reading tree would unmount the dialog that just opened.
  const overlayReadable=useRef(!!defaultReadable),overlayActive=menuOpen||preferences?.readingOverlayActive;
  if(!overlayActive)overlayReadable.current=!!defaultReadable;
  const readable=overlayActive?(overlayReadable.current?readableLayoutForScreen(design,screenId,{width:viewport.safeWidth,height:viewport.safeHeight},{...preferences?.value,readingLayout:'on'}):null):defaultReadable;
  // Native scroll groups retain children outside the initial clip. Resolve the
  // same tree for DOM scrolling; preserve the existing flat path on other screens.
  const flatElements=resolveScreenElements(design,screenId,size);
  const scrollGroups=!readable&&flatElements.some(element=>element.kind==='group'&&element.overflow==='scroll');
  const elements=readable?[...readable.items,...readable.actions].map(item=>item.element):scrollGroups?resolveScreenElements(design,screenId,size,{ignoreClipping:true}):flatElements;
  const positionKey=JSON.stringify([screenId,context.readingKey??[context.dataState?.periodIndex,context.dataState?.activeNarrative,context.story,context.phase],screenId===context.saveSlots?.screenId?context.saveSlots?.confirmation??null:null]);
  const readingPosition=useReadingPosition(readingScroll,positionKey,JSON.stringify([viewport.width,viewport.height,readable?.textScale]),!!readable);
  useEffect(()=>{setMenuOpen(false);readingActions.current?.scrollTo(0,0);},[positionKey]);
  useEffect(()=>{window.raisingExtensions?.commitPresentation(context.dataState);},[context.dataState,screenId]);
  const planning=useSchedulePlanner(context);
  const calendarBinding=(binding:Element['binding']):string|null=>{
    if(!binding?.startsWith('calendar-'))return null;
    if(context.calendarProject?.calendar){const p=context.calendarProject,c=p.calendar!,date={...(context.calendarTime??{year:p.startYear,month:p.startMonth}),periodInMonth:context.period,periodIndex:context.dataState?.periodIndex??0},grid=calendarGrid(p,date),labels=calendarLabels(c,context.locale);
      if(binding==='calendar-year')return String(date.year);if(binding==='calendar-month')return `${labels.month} ${date.month}`;if(binding==='calendar-day')return String(grid.day);if(binding==='calendar-weekday')return labels.weekdays[(grid.offset+grid.day-1)%7]!;
    }
    const {year,month}=context.calendarTime??{year:1,month:1},day=context.period*((context.periodsPerMonth??3)===3?10:Math.floor(new Date(year,month,0).getDate()/(context.periodsPerMonth??3)))+1;
    if(binding==='calendar-year')return String(year);
    if(binding==='calendar-month')return `${month}월`;
    if(binding==='calendar-day')return String(day);
    if(binding==='calendar-weekday'){const date=new Date(0);date.setUTCHours(0,0,0,0);date.setUTCFullYear(year,month-1,day);return `${['일','월','화','수','목','금','토'][date.getUTCDay()]}요일`;}
    return null;
  };
  const schedulePrompt=context.calendarProject?.calendar?`${calendarLabels(context.calendarProject.calendar,context.locale).month} · ${calendarLabels(context.calendarProject.calendar,context.locale).period} ${planning.planner.slot+1}`:planning.planner.mask!==((1<<(context.periodsPerMonth??3))-1)?`스케줄 결정.\n이번 달 딸의 예정은? 제 ${planning.planner.slot+1}회`:context.schedulePlayback==='continuous'?'이번 달 일정을 연속으로 진행합니다.':context.period===0?'예, 스케줄에 편성하겠습니다.\n첫 일정을 시작하시겠습니까?':`이번 달 ${context.period}번째 일정을 마쳤습니다.\n다음 일정을 진행하시겠습니까?`;
  const scheduleGroups=elements.find(e=>e.kind==='schedule'&&e.scheduleLayout==='menu')?.scheduleGroups??design.screens.flatMap(s=>s.elements).find(e=>e.kind==='schedule'&&e.scheduleLayout==='menu')?.scheduleGroups??[];
  const fontIds=[...new Set([...elements,...(design.components??[]).flatMap(c=>c.elements)].map(e=>resolveElementStyle(design,e).fontAssetId).filter((id):id is string=>!!id))];
  const fontCss=fontIds.map(id=>`@font-face{font-family:"engine-${id}";src:url(${JSON.stringify(assetUrl(id))});font-display:block;}`).join('\n');
  const conditionPasses=(condition:Element['visibleCondition'])=>!condition||context.dataUi?.conditions[JSON.stringify(condition)]===true;
  const buttonLabel=(element:Element)=>progressionButtonLabel(element,context);
  const progressionUnavailable=(element:Element)=>element.action==='advance'&&['choice','finished','title'].includes(context.phase)||element.action==='commitPlan'&&!['planning','ready'].includes(context.phase);
  const planningButton=readable?[...elements].sort((a,b)=>Number(b.id===readable.primaryAction)-Number(a.id===readable.primaryAction)).map(e=>resolveElementStyle(design,resolvePresentation(e,presentationActivity(context.dataState,e),conditionPasses))).find(e=>e.kind==='button'&&['advance','commitPlan'].includes(e.action??'')&&!progressionUnavailable(e)&&(e.opacity??1)>0&&isVisible(e,context)&&conditionPasses(e.visibleCondition)&&conditionPasses(e.enabledCondition)&&e.dataEnabled!==false&&context.dataUi?.availability[screenId]?.[e.id]?.enabled!==false):undefined;
  const planningControl=planningButton?{label:buttonLabel(planningButton),effect:planningButton.action==='commitPlan'?'confirm-and-execute' as const:context.dataState?.schedule&&context.plan.every((id,i)=>context.dataState!.schedule![i]===id)?(context.dataState.phase==='ready'&&context.dataState.activityIntermission?'acknowledge' as const:'execute' as const):'confirm' as const}:undefined;
  const renderElement=(raw:BoundElement,readingItem?:ResolvedReadableItem,groupChildren?:React.ReactNode,parent?:BoundElement):React.ReactNode=>{
      let element=resolveElementStyle(design,editing?raw:resolvePresentation(raw,presentationActivity(context.dataState,raw),conditionPasses));
      if(readingItem&&readable)element={...element,fontSize:readingItem.fontSize,width:Math.max(44,Math.min(viewport.width,readable.config.maxWidth)-2*readable.config.padding),height:readingItem.imageHeight};
      const portrait=!readingItem&&element.kind==='image'&&element.imageSource?.startsWith('portrait-')?context.portraitTransforms?.[element.imageSource.slice(9)]:undefined;
      if(portrait){const scale=portrait.scale??1,w=element.width,h=element.height;element={...element,x:element.x+(w-w*scale)/2+w*(portrait.offsetX??0),y:element.y+h-h*scale+h*(portrait.offsetY??0),width:w*scale,height:h*scale};}
      if(!editing&&(!isVisible(element,context)||!conditionPasses(element.visibleCondition)))return null;
      const availability=context.dataUi?.availability[screenId]?.[raw.id];
      const enabled=editing||conditionPasses(element.enabledCondition)&&raw.dataEnabled!==false&&availability?.enabled!==false;
      const reason=raw.dataReason||availability?.reason||(!conditionPasses(element.enabledCondition)||raw.dataEnabled===false?'사용 조건을 충족하지 못했습니다.':'');
      const style:React.CSSProperties={left:element.x,top:element.y,width:element.width,height:element.height,background:element.fill??'transparent',color:element.color??(readingItem&&readable?readableInk(element.fill??readable.config.background):'#304d46'),fontSize:element.fontSize??22,borderRadius:element.radius??0,opacity:editing&&!isVisible(element,context)?0.3:element.opacity??1,textAlign:element.align??'left'};
      if(readingItem&&readable&&element.color===undefined)Object.assign(style,{'--readable-control-ink':'#263c36','--readable-slot-ink':readableInk(element.fill??'#f5efdf')});
      if(!readingItem&&raw._clipRect){const clip=raw._clipRect,left=Math.max(0,clip.x-element.x),top=Math.max(0,clip.y-element.y),right=Math.max(0,element.x+element.width-clip.x-clip.width),bottom=Math.max(0,element.y+element.height-clip.y-clip.height);if(left+right>=element.width||top+bottom>=element.height)return null;style.clipPath=`inset(${top}px ${right}px ${bottom}px ${left}px)`;}
      if(parent){
        // Resolved coordinates are absolute; DOM children use the padding box.
        const border=resolveElementStyle(design,parent).borderWidth??0;
        style.left=element.x-parent.x-border;style.top=element.y-parent.y-border;
        style.pointerEvents='auto';
      }
      if(element.kind==='group'){
        style.pointerEvents=scrollGroups&&element.overflow==='scroll'?'auto':'none';
        if(scrollGroups){style.overflow=element.clipChildren||element.overflow==='hidden'?'hidden':'visible';if(element.overflow==='scroll'){style.overflowX='hidden';style.overflowY='auto';}style.padding=0;}
      }
      if(element.fontAssetId)style.fontFamily=`"engine-${element.fontAssetId}", sans-serif`;
      if(element.padding!==undefined&&!(scrollGroups&&element.kind==='group'))style.padding=element.padding;
      else if(element.kind==='text'&&element.uiContext)style.padding=0;
      if(element.borderWidth!==undefined){style.borderWidth=element.borderWidth;style.borderStyle='solid';style.borderColor=element.borderColor??'#57806c';}
      const groupPaintOpacity=scrollGroups&&element.kind==='group'?Number(style.opacity):1;
      if(scrollGroups&&element.kind==='group'){
        style.opacity=1;
        style.background=paintAlpha(element.fill??'transparent',groupPaintOpacity);
        style.backgroundClip='padding-box';
        if(element.borderWidth!==undefined)style.borderColor=paintAlpha(element.borderColor??'#57806c',groupPaintOpacity);
      }
      if(element.kind==='text'&&!readingItem){
        // Compact authored labels should not scroll just because the default
        // paragraph leading exceeds their box. Keep normal leading in taller
        // boxes, and retain scrolling when the text itself really overflows.
        const size=element.fontSize??22,padding=element.padding??(element.uiContext?0:4);
        const available=element.height-2*(padding+(element.borderWidth??0));
        style.lineHeight=`${Math.min(size*1.5,Math.max(size,available))}px`;
      }
      Object.assign(style,{'--style-hover':element.hoverFill,'--style-pressed':element.pressedFill,'--style-disabled':element.disabledColor});
      if(element.kind==='choices'&&element.choiceLayout==='cards'){
        Object.assign(style,{'--choice-fill':element.fill??'#f5efdf','--choice-color':element.color??(readingItem?readableInk(element.fill??'#f5efdf'):'#263c36'),'--choice-border':element.borderColor??'#c7b99a','--choice-border-width':`${element.borderWidth??1}px`,'--choice-radius':`${element.radius??2}px`});
        style.background='transparent';style.borderWidth=0;
      }
      const source=imageFor(element,context);
      const contextText=element.binding==='narrative-actor'?(context.characters?.find(c=>c.id===context.dataState?.activeNarrative?.actorId)?.name??''):element.binding==='context-name'?(context.dataUi?.contextNames?.[`${element.contextField}:${element.uiContext?.[element.contextField??'actorId']}`]??'—'):null;
      const text=contextText??userFieldBinding(element,context.userValues)??dataBindingText(element,context)??numericBinding(element,context)??calendarBinding(element.binding)??(element.binding==='schedule-prompt'?schedulePrompt:null)??(element.binding&&element.binding!=='none'&&element.binding in context?String(context[element.binding as keyof VisualContext]??''):element.text??'텍스트');
      if(readingItem){
        if((['text','button'].includes(element.kind)&&!text.trim())||(element.kind==='dialogue'&&!context.story.trim())||(element.kind==='result'&&!context.result.trim()&&!context.actorResults?.length)||(element.kind==='choices'&&!context.choices.length)||(element.kind==='image'&&!source))return null;
        Object.assign(style,{position:'relative',left:undefined,top:undefined,width:undefined,height:element.kind==='image'?readingItem.imageHeight:undefined,minWidth:0,lineHeight:1.5,overflow:element.kind==='image'?'hidden':'visible'});
        if(element.kind==='text')style.padding=element.padding??0;
      }
      const controlOwner=['schedule','saveSlots','loadSlots'].includes(element.kind)||(!!readingItem&&element.kind==='playerSettings');
      const controlProps:ReturnType<typeof controlColorProps>|{style?:React.CSSProperties}=controlOwner?controlColorProps(resolveControlStyle(design,element.controlStyleId,!!readingItem)):{};
      Object.assign(style,controlProps.style);
      const surface=element.surface,painted=hasSurface(surface),surfaceSource=painted?assetUrl(surface.imageAssetId):'';
      const surfacePaint=painted?<SurfacePaint surface={element.surface} src={surfaceSource} identity={context.gameId??''} opacity={groupPaintOpacity}/>:null;
      let body:React.ReactNode=null;
      if(element.kind==='image')body=source?<TransitionImage src={assetUrl(source)} element={element}/>:editing?<div className="visual-placeholder">{element.imageSource??'이미지'} · 에셋 선택</div>:null;
      if(element.kind==='text')body=element.binding==='story'&&!editing?<PresentedHistoryText text={text} receipt={context.historyReceipt}/>:<span {...(element.binding==='speaker'?{'data-history-speaker':true}:{})}>{text}</span>;
      if((element.kind as string)==='plugin'){style.overflow='visible';body=<ExtensionView instance={`${screenId}/${element.id}`} extension={(element as unknown as {extension?:ExtensionSpec}).extension} width={element.width} height={element.height} assetUrl={assetUrl} editing={editing}/>;}
      if(element.kind==='playerSettings'&&preferences)body=<fieldset className="visual-settings-fields" disabled={busy||!enabled||editing}><PlayerSettingsControls {...preferences} locale={context.locale}/></fieldset>;
      if(element.kind==='saveSlots'||element.kind==='loadSlots')body=<SaveSlotsWidget element={element} {...(context.saveSlots?{view:context.saveSlots}:{})} {...(context.locale?{locale:context.locale}:{})} busy={busy||!enabled||editing} {...(context.onSlotAction?{onAction:context.onSlotAction}:{})}/>;
      if(element.kind==='input'){
        const field=context.userFields?.find(f=>f.id===element.userFieldId);
        body=<input className="visual-input" type="text" aria-label={field?.label??element.label??element.id} value={context.userValues?.[element.userFieldId??'']??field?.initial??''} disabled={busy||!enabled||!field||editing||!context.userFieldsEditable} autoComplete="off" spellCheck={false} onChange={event=>field&&onUserValue?.(field.id,event.target.value)}/>;
      }
      if(element.kind==='meter'&&element.meterThickness!==undefined)style.background='transparent';
      const meterValue=element.dataBinding?(dataBindingValue(element,context)??0):(element.value??0);
      if(element.kind==='meter')body=<div className="visual-meter" style={{'--meter-track':element.fill??'transparent'} as React.CSSProperties}><span>{element.text??element.label??''}</span><progress aria-label={element.label??element.text??element.id} style={element.meterThickness===undefined?undefined:{height:Math.min(element.height,element.meterThickness)}} max={Math.max(1,element.maxValue??100)} value={Math.max(0,Math.min(meterValue,element.maxValue??100))}/></div>;
      if(element.kind==='stats')body=<div className="visual-stat-list">{context.stats.map(s=><div key={s.id}><div><span>{s.label}</span><strong data-testid={`stat-${s.id}`}>{s.value}</strong></div><progress aria-label={s.label} value={s.value-s.min} max={s.max-s.min||1}/></div>)}</div>;
      if(element.kind==='schedule')body=context.dataState?.roster
        ?<>{element.showPhaseForecast&&<PhaseForecast id={element.id} context={context} disabled={busy||!enabled||editing}/>}<RosterSchedule context={context} disabled={busy||!enabled||editing} onCommand={onCommand} onExecute={()=>onAction?.('advance',element)} showActivityDescription={element.showActivityDescription??false} showActivityRules={element.showActivityRules??false}/></>
        :<ScheduleWidget ownerKey={screenId} readable={!!readingItem} compact={readingItem?.schedulePresentation==='compact'} guidance={readingItem?.planningGuidance==='first-use'} planReuse={readingItem?.planReuse==='last-committed'} planningControl={planningControl} element={element} context={context} planning={planning} groups={element.scheduleGroups??scheduleGroups} assetUrl={assetUrl} disabled={busy||!enabled||editing} onPlan={onPlan} onPlanReplace={onPlanReplace} onCommit={onCommitPlan}/>;
      if(element.kind==='dialogue')body=<p className="visual-dialogue" data-testid="dialogue"><PresentedHistoryText text={context.story} receipt={!editing?context.historyReceipt:undefined}/></p>;
      if(element.kind==='result')body=context.actorResults?.length
        ?<div className="visual-actor-results">{context.actorResults.map(row=><div key={row.characterId}>{row.text}</div>)}</div>
        :<div className="visual-result">{context.result||slotWords(context.locale,'활동 결과가 여기에 표시됩니다.','Activity results will appear here.','活動結果がここに表示されます。')}</div>;
      if(element.kind==='choices')body=<div className={`visual-choices${element.choiceLayout==='cards'?' choice-cards':''}`} style={{gap:element.gap??12}}>{(editing&&!context.choices.length?[{id:'preview-one',text:'함께 도전한다',enabled:true,hint:''},{id:'preview-two',text:'조금 더 준비한다',enabled:true,hint:''}]:context.choices).map((c,index)=><button key={c.id} style={readingItem?{minHeight:readable!.config.minTargetSize}:element.choiceLayout==='cards'?{minHeight:element.rowHeight??80}:undefined} disabled={busy||!enabled||!c.enabled} onClick={event=>{const label=event.currentTarget.querySelector('[data-history-choice-label]');onChoice?.(c.id,label&&historyTextVisible(label)?label.textContent??undefined:undefined);}}>{element.choiceLayout==='cards'&&<span className="choice-number" aria-hidden="true">{String(index+1).padStart(2,'0')}</span>}<span className="choice-copy"><span data-history-choice-label>{c.text}</span>{element.showChoiceEffects&&c.hint&&<small className="choice-hint">{c.hint}</small>}{!c.enabled&&<small>{slotWords(context.locale,'조건 미충족','Requirements not met','条件を満たしていません')}</small>}</span></button>)}</div>;
      if(element.kind==='button')body=<button className={`visual-action${painted?' engine-surface-host':''}`} title={reason||undefined} disabled={busy||!enabled||progressionUnavailable(element)} onClick={event=>{if(element.action){if(element.action==='resumeContext')event.currentTarget.focus({preventScroll:true});setMenuOpen(false);onAction?.(element.action,element);}}}>{surfacePaint}{buttonLabel(element)}</button>;
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
      if(['equipment','quests','expedition','battle','relationship-stages'].includes(element.kind)){
        const trip=context.systems?.expedition;
        // Reuse the authored action widget and the normal command/confirmation path.
        // An empty node list must not fall through to showing every global action.
        const nodeActions=element.kind==='expedition'&&trip?.actionIds?.length
          ?<JourneyWidget key={trip.nodeId} element={{...element,id:`${element.id}__node-${trip.nodeId}`,kind:'actions',actionIds:trip.actionIds}} gameplay={context.gameplay} items={context.items} stats={context.stats} assetUrl={assetUrl} busy={busy||!enabled} editing={editing} onCommand={onCommand}/>
          :null;
        body=<SystemsWidget kind={element.kind} systems={context.systems} busy={busy||!enabled} editing={editing} onCommand={onCommand}>{nodeActions}</SystemsWidget>;
      }
      return <div className={`visual-element visual-${element.kind} ${raw.styleId||design.defaultStyle?'visual-styled':''}${painted&&element.kind!=='button'?' engine-surface-host':''}`} {...controlProps} {...(controlOwner?{'data-control-owner':true}:{})} style={style} key={element.id} data-element-id={element.id} {...(scrollGroups&&element.kind==='group'&&element.overflow==='scroll'?{tabIndex:0,role:'region','aria-label':element.label??slotWords(context.locale,'스크롤 콘텐츠','Scrollable content','スクロールコンテンツ')}:{} )} {...(painted?{'data-engine-surface':'image'}:{})}>{element.kind!=='button'&&surfacePaint}{body}{groupChildren}</div>;
  };
  const renderGroupTree=(parent?:BoundElement):React.ReactNode=>elements.filter(element=>(element._groupIds?.at(-1)??undefined)===parent?.id).map(element=>renderElement(element,undefined,element.kind==='group'?renderGroupTree(element):undefined,parent));
  if(readable){
    const {config}=readable,vars={'--readable-target':`${config.minTargetSize}px`,'--readable-gap':`${config.gap}px`,'--readable-padding':`${config.padding}px`,'--readable-width':`${config.maxWidth}px`,fontSize:config.fontSize*readable.textScale,background:config.background,'--readable-background':config.background,'--readable-ink':readableInk(config.background)} as React.CSSProperties;
    const readingColors=controlColorProps(resolveControlStyle(design,undefined,true),true);Object.assign(vars,readingColors.style);
    const grouped=readable.actionLayout==='grouped',words=(ko:string,en:string,ja:string)=>slotWords(context.locale,ko,en,ja);
    const primary=grouped?readable.actions.find(item=>item.element.id===readable.primaryAction&&isVisible(item.element,context)&&conditionPasses(item.element.visibleCondition)&&conditionPasses(item.element.enabledCondition)&&context.dataUi?.availability[screenId]?.[item.element.id]?.enabled!==false&&!progressionUnavailable(item.element)):undefined;
    const secondary=readable.actions.filter(item=>item!==primary);
    return <div className={`visual-viewport visual-fit visual-readable${grouped?' readable-grouped':''}${hasSurface(config.surface)?' engine-surface-host':''}`} {...readingColors} data-screen={screenId} data-readable-layout="true" {...(hasSurface(config.surface)?{'data-engine-surface':'image'}:{})} ref={host} style={vars}
      onPointerDownCapture={event=>{gesture.current={x:event.clientX,y:event.clientY,dragged:false};}}
      onPointerMoveCapture={event=>{if(Math.hypot(event.clientX-gesture.current.x,event.clientY-gesture.current.y)>8)gesture.current.dragged=true;}}
      onPointerCancelCapture={()=>{gesture.current.dragged=true;}}
      onClickCapture={event=>{if(event.detail>0&&gesture.current.dragged){event.preventDefault();event.stopPropagation();gesture.current.dragged=false;}}}>
      {hasSurface(config.surface)&&<SurfacePaint surface={config.surface} src={assetUrl(config.surface.imageAssetId)} identity={context.gameId??''}/>}<style>{fontCss}</style><div className="readable-scroll" ref={readingScroll} {...readingPosition} tabIndex={0} role="region" aria-label={design.screens.find(s=>s.id===screenId)?.label??screenId}><div className="readable-items">{readable.items.map(item=>renderElement(item.element,item))}</div></div>
      {grouped?<div className="readable-actions readable-grouped-actions" ref={readingActions}>
        {primary&&renderElement(primary.element,primary)}
        <button className="reading-menu-launcher" data-reading-id="engine-reading-menu" aria-haspopup="dialog" onClick={()=>setMenuOpen(true)}>{words('메뉴','Menu','メニュー')}</button>
      </div>:readable.actions.length>0&&<div className="readable-actions" ref={readingActions}>{readable.actions.map(item=>renderElement(item.element,item))}</div>}
      {grouped&&menuOpen&&<ReadingDialog id="engine-reading-menu" title={words('게임 메뉴','Game menu','ゲームメニュー')} onClose={()=>setMenuOpen(false)}>
        <div className="reading-dialog-body reading-menu-items">{secondary.map(item=>renderElement(item.element,item))}
          {resume?.enabled&&<button data-reading-id="engine-resume-context" onClick={resume.open}>{words('현재 상황','Current reminders','現在のメモ')}</button>}
          {history?.enabled&&<button data-reading-id="engine-reading-history" onClick={history.open}>{words('최근 읽은 내용','Recent reading','最近の文章')}</button>}
          {preferences&&<button data-reading-id="engine-reading-options" onClick={()=>window.dispatchEvent(new Event('raising-open-reading-settings'))}>{words('소리·읽기 설정','Sound & reading settings','音声と読み方の設定')}</button>}
        </div><div className="reading-dialog-actions"><button data-reading-id="engine-reading-menu-close" onClick={()=>setMenuOpen(false)}>{words('닫기','Close','閉じる')}</button></div>
      </ReadingDialog>}
      {children}
    </div>;
  }
  return <div className={`visual-viewport ${fitViewport?'visual-fit':''}`} ref={host} style={fitViewport?{}:{aspectRatio:`${design.width}/${design.height}`}}><div className="visual-stage" data-screen={screenId} style={{width:size.width,height:size.height,left:offset.x,top:offset.y,transform:`scale(${scale})`,'--canvas-control-min':`${Math.ceil(44/Math.max(scale,0.01))}px`} as React.CSSProperties}>
    <style>{fontCss}</style>
    {scrollGroups?renderGroupTree():elements.map(element=>renderElement(element))}{children}
  </div></div>;
}


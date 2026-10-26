import {controlStyleReferences} from '../visual/control-styles.js';
import {surfaceReferences} from '../visual/surfaces.js';
import {slotScreens} from '../player-service/save-slots.js';
import {resolveScreenElements} from '../visual/components.js';
import type { ContentSnapshot } from '../core/model.js';
import { usesPresentationVariants } from '../visual/variants.js';
import { usesUiContext } from '../visual/context.js';

export function hasPortraitTransforms(value: unknown): boolean {
  if (Array.isArray(value)) return value.some(hasPortraitTransforms);
  if (!value || typeof value !== 'object') return false;
  const object = value as Record<string, unknown>;
  if (Array.isArray(object.portraits) && object.portraits.some(item => item && typeof item === 'object' &&
    ['scale', 'offsetX', 'offsetY'].some(key => Object.hasOwn(item, key)))) return true;
  return Object.values(object).some(hasPortraitTransforms);
}

export function requiredPlayerFeatures(content: ContentSnapshot, extensionFeatures: readonly string[] = []): string[] {
  const visual=content.project.visual,requiredFeatures:string[]=[];
  const scheduleElements=visual?[...visual.screens.flatMap(s=>s.elements),...(visual.components??[]).flatMap(c=>c.elements)]:[];
  if(scheduleElements.some(e=>e.scheduleLayout==='activity-board'))requiredFeatures.push('activity-board-v1');
  if(surfaceReferences(visual).length)requiredFeatures.push('ui-surfaces-v1');
  if(controlStyleReferences(visual).length)requiredFeatures.push('ui-control-colors-v1');
  if(visual?.screens.some(screen=>screen.readableLayout?.activation!==undefined))requiredFeatures.push('readable-screen-activation-v1');
  if(content.project.calendar)requiredFeatures.push('calendar-v1');
  if(visual?.readableLayout||visual?.screens.some(s=>s.readableLayout))requiredFeatures.push('readable-layout-v1');
  const hasForecast=(value:unknown):boolean=>!!value&&typeof value==='object'&&(Array.isArray(value)?value.some(hasForecast):(value as Record<string,unknown>).showPhaseForecast===true||Object.values(value).some(hasForecast));
  if(hasForecast(visual))requiredFeatures.push('phase-forecast-v1');
  if(visual?.resumeContext)requiredFeatures.push('resume-context-v1');
  if(visual?.readingHistory)requiredFeatures.push('reading-history-v1');
  if(visual?.screens.some(s=>s.readableLayout?.actionLayout==='grouped'))requiredFeatures.push('readable-actions-v1');
  if(visual?.screens.some(s=>s.readableLayout?.items.some(item=>item.schedulePresentation==='compact')))requiredFeatures.push('compact-planning-v1');
  if(visual?.screens.some(s=>s.readableLayout?.items.some(item=>item.planReuse==='last-committed')))requiredFeatures.push('plan-reuse-v1');
  if(visual?.screens.some(s=>s.readableLayout?.items.some(item=>item.planningGuidance==='first-use')))requiredFeatures.push('planning-guidance-v1');
  if(visual&&[...visual.screens.flatMap(s=>s.elements),...(visual.components??[]).flatMap(c=>c.elements)].some(e=>e.binding==='narrative-actor'))requiredFeatures.push('narrative-actor-binding-v1');
  if(visual&&[...visual.screens.flatMap(s=>s.elements),...(visual.components??[]).flatMap(c=>c.elements)].some(e=>e.showActivityRules))requiredFeatures.push('activity-rules-summary-v1');
  if(scheduleElements.some(e=>e.scheduleLayout!=='activity-board'&&e.activityIcons?.length))requiredFeatures.push("schedule-board-icons-v1");
  if(visual&&[...visual.screens.flatMap(s=>s.elements),...(visual.components??[]).flatMap(c=>c.elements)].some(e=>["timelinePause","timelineSpeed","timelineSkip"].includes(e.action??"")||(e.binding==="activity-progress"&&e.valueFormat)))requiredFeatures.push("authored-playback-controls-v1");
  if(visual&&[...visual.screens.flatMap(s=>s.elements),...(visual.components??[]).flatMap(c=>c.elements)].some(e=>e.kind==='meter'&&(e.dataBinding||e.meterThickness!==undefined)))requiredFeatures.push('data-meters-v1');
  if(visual&&[...visual.screens.flatMap(s=>s.elements),...(visual.components??[]).flatMap(c=>c.elements)].some(e=>e.action==='dialogueAuto'))requiredFeatures.push('dialogue-controls-v1');
  if(visual&&[...visual.screens.flatMap(s=>s.elements),...(visual.components??[]).flatMap(c=>c.elements)].some(e=>e.kind==='playerSettings'||e.binding==='reading-status'))requiredFeatures.push('player-settings-v1');
  if(visual&&[...visual.screens.flatMap(s=>s.elements),...(visual.components??[]).flatMap(c=>c.elements)].some(e=>e.binding==='save-summary'||e.binding==='player-notice'))requiredFeatures.push('save-panel-v1');
  if(visual&&(visual.saveSystem||[...visual.screens.flatMap(s=>s.elements),...(visual.components??[]).flatMap(c=>c.elements)].some(e=>e.kind==='saveSlots'||e.kind==='loadSlots')))requiredFeatures.push('save-slots-v1');
  if(visual&&[...visual.screens.flatMap(s=>s.elements),...(visual.components??[]).flatMap(c=>c.elements)].some(e=>e.choiceLayout==='cards'))requiredFeatures.push('narrative-cards-v1');
  if(visual&&[...visual.screens.flatMap(s=>s.elements),...(visual.components??[]).flatMap(c=>c.elements)].some(e=>e.showChoiceEffects||e.binding==='choice-result'))requiredFeatures.push('choice-feedback-v1');
  if(visual&&[...visual.screens.flatMap(s=>s.elements),...(visual.components??[]).flatMap(c=>c.elements)].some(e=>e.scheduleLayout==='board'||e.resultLayout==='changes'||e.binding==='schedule-summary'||e.binding==='activity-progress'))requiredFeatures.push('schedule-board-v1');
  if(visual&&[...visual.screens.flatMap(s=>s.elements),...(visual.components??[]).flatMap(c=>c.elements)].some(e=>e.action==='commitPlan'))requiredFeatures.push('commit-plan-action-v1');
  if(usesUiContext(visual))requiredFeatures.push('component-context-v1');
  if(visual?.routing?.conditions?.length)requiredFeatures.push('conditional-routing-v1');
  if(content.project.stateGroups?.length)requiredFeatures.push('custom-states-v1');
  if(usesPresentationVariants(visual))requiredFeatures.push('presentation-variants-v1');
  const hasFreeBinding=(value:unknown):boolean=>!!value&&typeof value==='object'&&('dataBinding' in value||Object.values(value).some(hasFreeBinding));
  if(visual?.routing||hasFreeBinding(visual))requiredFeatures.push('free-screens-v1');
  const hasRelationshipChange=(value:unknown):boolean=>{
    if(!value||typeof value!=='object')return false;
    const element=value as Record<string,unknown>;
    return element.binding==='relationship-value'&&typeof element.valueFormat==='string'&&element.valueFormat.includes('{delta}')||Object.values(element).some(hasRelationshipChange);
  };
  if(hasRelationshipChange(visual))requiredFeatures.push('relationship-change-v1');
  const hasGalleryUi=(value:unknown):boolean=>{
    if(!value||typeof value!=='object')return false;
    const obj=value as Record<string,unknown>;
    return (typeof obj.action==='string'&&obj.action.startsWith('gallery')) ||
      (typeof obj.binding==='string'&&obj.binding.startsWith('gallery-')) ||
      obj.type==='galleryEntries'||obj.type==='gallerySelection'||Object.values(obj).some(hasGalleryUi);
  };
  const hasReplayPlayback=(value:unknown):boolean=>{if(!value||typeof value!=='object')return false;const obj=value as Record<string,unknown>;return ['recollectionAuto','recollectionSpeed','recollectionSkip'].includes(String(obj.action))||['replay-mode','replay-speed'].includes(String(obj.binding))||Object.values(obj).some(hasReplayPlayback);};
  if(hasReplayPlayback(visual))requiredFeatures.push('recollection-playback-v1');
  const hasReplayUi=(value:unknown):boolean=>{if(!value||typeof value!=='object')return false;const obj=value as Record<string,unknown>;return (typeof obj.action==='string'&&obj.action.startsWith('recollection'))||(typeof obj.binding==='string'&&obj.binding.startsWith('replay-'))||Object.values(obj).some(hasReplayUi);};
  if(content.project.gallery?.replayControlsScreenId||hasReplayUi(visual))requiredFeatures.push('authored-recollection-v1');
  const hasBranches=(v:unknown):boolean=>!!v&&typeof v==='object'&&(Array.isArray(v)?v.some(hasBranches):Object.entries(v).some(([k,x])=>((k==='type'||k==='action')&&(x==='galleryBranches'||x==='galleryBranch'))||hasBranches(x)));
  if(content.project.gallery?.branchScreenId||hasBranches(visual))requiredFeatures.push('gallery-branches-v1');
  if(content.project.gallery?.screenId||hasGalleryUi(visual))requiredFeatures.push('authored-gallery-v1');
  if(content.project.gallery){requiredFeatures.push('narrative-gallery-v1');if(content.project.gallery.entries.some(e=>e.kind==='cg'||e.kind==='music'||e.unlockCondition||e.lockedDisplay))requiredFeatures.push('media-gallery-v2');}
  if(visual?.resultPlayback)requiredFeatures.push('inline-result-playback-v1');
  if(content.project.roster)requiredFeatures.push('roster-activities-v1');
  if((content.project.visual as any)?.direction)requiredFeatures.push('direction-sequences-v1');
  if(content.project.systems?.expeditions.some(e=>e.nodes.some(n=>n.requirements)))requiredFeatures.push('expedition-conditions-v1');
  if(content.project.systems?.tactics||content.project.systems?.expeditions.some(e=>e.nodes.some(n=>n.encounterId))||content.project.systems?.quests.some(q=>q.encounterId))requiredFeatures.push('party-tactics-v1');
  if(content.project.progression)requiredFeatures.push('progression-sessions-v1');
  if(content.project.progression?.model==='unified-v2')requiredFeatures.push('unified-time-v2');
  const ui=content.project.visual;
  // save-slots-v1 includes its record modals. Keep the broader composition gate
  // for ordinary modals, scrolling, sorting, selection, tooltips and nested components.
  if(ui){const recordModals=new Set(slotScreens(ui));
    if([...ui.screens.flatMap(s=>s.elements),...(ui.components??[]).flatMap(c=>c.elements)].some(e=>e.kind==='group'&&e.overflow))requiredFeatures.push('ui-scroll-groups-v1');
    if(ui.screens.some(s=>s.modal&&!recordModals.has(s.id)||resolveScreenElements(ui,s.id).some(e=>recordModals.has(s.id)&&(!['panel','text','image','stats','meter','group','component','button','saveSlots','loadSlots'].includes(e.kind)||!!e.action&&!['openModal','closeModal','save','load','sound'].includes(e.action))||e.action==='openModal'&&!recordModals.has(e.screenId??'')||e.action==='closeModal'&&!recordModals.has(s.id)))||[...ui.screens.flatMap(s=>s.elements),...(ui.components??[]).flatMap(c=>c.elements)].some(e=>e.flowLayout||(e.overflow&&e.kind!=='group')||e.sort||e.selectionKey||e.tooltip||e.action==='selectRow')||(ui.components??[]).some(c=>c.elements.some(e=>e.kind==='component')))requiredFeatures.push('ui-composition-v1');
  }
  if(content.project.roster?.stateModel==='character-v1')requiredFeatures.push('character-scoped-rules-v1');
  const personalStory=(v:unknown):boolean=>!!v&&typeof v==='object'&&(Array.isArray(v)?v.some(personalStory):((v as any).trigger==='after-event'||(['flag','setFlag'].includes((v as any).op)&&!!(v as any).owner)||Object.values(v).some(personalStory)));
  if(personalStory(content))requiredFeatures.push('actor-story-links-v1');
  if(content.events.some(e=>['period-start','after-activity','location-enter'].includes(e.trigger??'')))requiredFeatures.push('story-appointments-v1');
  if(content.project.periodsPerMonth!==3||content.project.schedulePlayback==='continuous')requiredFeatures.push('monthly-schedule-v1');
  if(content.project.gameplay?.actions?.some(action=>action.flow?.some(step=>step.chance)))requiredFeatures.push('probabilistic-actions-v1');
  const hasImmediateConfirmation=(value:unknown):boolean=>{if(!value||typeof value!=='object')return false;const obj=value as Record<string,unknown>;return (obj.confirmation as {enabled?:boolean}|undefined)?.enabled===false||Object.values(obj).some(hasImmediateConfirmation);};
  if(hasImmediateConfirmation(content.project.gameplay))requiredFeatures.push('optional-confirmation-v1');
  if(hasPortraitTransforms(content))requiredFeatures.push('scene-portrait-transform-v1');
  if(content.activities.some(activity=>activity.previewAttributes?.length))requiredFeatures.push('activity-summary-v1');
  if(content.project.userFields?.length || (visual && [...visual.screens.flatMap(x=>x.elements),...(visual.components??[]).flatMap(x=>x.elements)].some(x=>x.kind==='input'||x.binding==='user-field')))requiredFeatures.push('user-fields-v1');
  if(visual&&[...visual.screens.flatMap(x=>x.elements),...(visual.components??[]).flatMap(x=>x.elements)].some(x=>x.kind==='group'||x.parentId||x.clipChildren!==undefined))requiredFeatures.push('ui-hierarchy-v1');
  if(visual&&(visual.screens.some(screen=>screen.elements.some(e=>String(e.source?.type).startsWith('battle')))||[...visual.screens.flatMap(x=>x.elements),...(visual.components??[]).flatMap(x=>x.elements)].some(x=>x.kind==='meter'||['selectBattleAction','selectBattleTarget','confirmBattleAction','cancelBattleAction'].includes(x.action??''))))requiredFeatures.push('battle-hud-v1');
  if(visual?.timelines?.length||visual?.activityTimelines?.length)requiredFeatures.push('activity-timeline-v1');
  if(visual&&(visual.dataTables?.length||[...visual.screens.flatMap(x=>x.elements),...(visual.components??[]).flatMap(x=>x.elements)].some(x=>x.kind==='collection'||x.visibleCondition||x.enabledCondition)))requiredFeatures.push('data-ui-v1');
  const gameplay=content.project.gameplay;
  const hasInventoryTarget=(value:unknown):boolean=>value!==null&&typeof value==='object'&&((value as {kind?:string}).kind==='inventory'||Object.values(value).some(hasInventoryTarget));
  if(gameplay?.rules?.length||gameplay?.actions?.some(action=>action.flow?.length)||hasInventoryTarget(content))requiredFeatures.push('action-flows-v1');
  if(gameplay&&(gameplay.actions?.length||gameplay.policyGroups?.length||gameplay.policyBudgets?.length))requiredFeatures.push('generic-rules-v1');
  if(gameplay&&(gameplay.diets?.length||gameplay.services?.length||gameplay.shops.some(shop=>shop.sellOffers?.length||shop.offers.some(offer=>offer.maxQuantity!==undefined||offer.confirmation))))requiredFeatures.push('facilities-v1');
  if(visual){
    const elements=[...visual.screens.flatMap(screen=>screen.elements),...(visual.components??[]).flatMap(component=>component.elements)];
    if(elements.some(element=>['actions','policies'].includes(element.kind)||['executeAction','selectPolicy'].includes(element.action??''))&&!requiredFeatures.includes('generic-rules-v1'))requiredFeatures.push('generic-rules-v1');
    if(visual.components?.length||visual.screens.some(screen=>screen.elements.some(element=>element.kind==='component')))requiredFeatures.push('components-v1');
    if(elements.some(element=>element.anchorX!==undefined||element.anchorY!==undefined))requiredFeatures.push('anchors-v1');
    if(visual.viewportMode==='responsive')requiredFeatures.push('responsive-v1');
    if(elements.some(element=>['diets','services','sell'].includes(element.kind)||['sell','setDiet','useService'].includes(element.action??''))&&!requiredFeatures.includes('facilities-v1'))requiredFeatures.push('facilities-v1');
  }
  const pluginElementCount=visual?visual.screens.reduce((n,screen)=>n+screen.elements.filter(element=>element.kind==='plugin').length,0):0;
  if(pluginElementCount)requiredFeatures.push('plugin-elements-v1');
  // Plugin documents may need more of the player (plugin-mesh-v1 for cutout meshes): reported by the plugin's service at prepare time.
  for(const feature of extensionFeatures)if(!requiredFeatures.includes(feature))requiredFeatures.push(feature);
  return [...new Set(requiredFeatures)];
}

export function missingPlayerFeatures(required: readonly string[], supported: readonly string[]): string[] {
  return required.filter(feature => !supported.includes(feature) && !(feature === 'ui-scroll-groups-v1' && supported.includes('ui-composition-v1')) && !(feature === 'expedition-conditions-v1' && supported.includes('party-tactics-v1')) && !(feature === 'commit-plan-action-v1' && supported.includes('schedule-board-v1')));
}

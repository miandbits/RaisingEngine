import {usesPresentationVariants} from '../visual/variants.js';
import {usesUiContext} from '../visual/context.js';
import { createHash, randomUUID } from 'node:crypto';
import { cp, lstat, mkdir, readFile, readdir, realpath, rename, rm, writeFile } from 'node:fs/promises';
import path from 'node:path';
import type { ContentSnapshot } from '../core/model.js';
import type { ExportTarget, GameBundle } from './model.js';
import type { PreparedExtensions } from '../plugin-host/bundle.js';
import { PluginError } from '../plugin-host/zip.js';
import { preparePresentationRevision } from './presentation-revision.js';

export interface ExportOptions {
  content: ContentSnapshot; projectRoot: string; destination: string; target: ExportTarget;
  runtimes: { web: string; windows: string };
  /** Plugin runtime modules + documents from StudioEditor.exportExtensions (null/absent when no plugin element is used). */
  extensions?: PreparedExtensions | null | undefined;
  /** Published bundle to preserve for a strictly image-only update. No implicit save migration. */
  presentationBase?: GameBundle;
  /** Recheck the source immediately before publishing the owned staging directory. */
  beforePublish?: () => Promise<void>;
}
export interface ExportResult { directory: string; target: ExportTarget; contentFingerprint: string; assetCount: number }
const inside = (root: string, candidate: string) => {
  const relative = path.relative(root, candidate);
  return relative === '' || (!relative.startsWith(`..${path.sep}`) && relative !== '..' && !path.isAbsolute(relative));
};
async function exists(file: string) { try { await lstat(file); return true; } catch (e) { if ((e as NodeJS.ErrnoException).code === 'ENOENT') return false; throw e; } }
const hash = (bytes: Uint8Array) => createHash('sha256').update(bytes).digest('hex');
export function hasPortraitTransforms(value: unknown): boolean {
  if (Array.isArray(value)) return value.some(hasPortraitTransforms);
  if (!value || typeof value !== 'object') return false;
  const object = value as Record<string, unknown>;
  if (Array.isArray(object.portraits) && object.portraits.some(item => item && typeof item === 'object' &&
    ['scale', 'offsetX', 'offsetY'].some(key => Object.hasOwn(item, key)))) return true;
  return Object.values(object).some(hasPortraitTransforms);
}

/** Export a validated immutable snapshot. Source assets are rechecked to prevent stale content packaging. */
export async function exportGame(options: ExportOptions): Promise<ExportResult> {
  const { content, target } = options;
  const presentation = options.presentationBase
    ? preparePresentationRevision(content, options.presentationBase, options.extensions?.game) : undefined;
  const runtimeContent = presentation?.content ?? content;
  if (target !== 'web' && target !== 'windows') throw new Error('지원하지 않는 내보내기 대상입니다.');
  const projectRoot = await realpath(options.projectRoot);
  const destination = path.resolve(options.destination);
  if (inside(destination, projectRoot)) throw new Error('프로젝트 폴더 또는 그 상위 폴더에 내보낼 수 없습니다.');
  if (await exists(destination)) throw new Error('대상 폴더가 이미 있습니다. 새 폴더 이름을 선택해 주세요.');
  const runtime = await realpath(options.runtimes[target]);
  const entry = target === 'web' ? 'index.html' : 'RaisingPlayer.exe';
  if (!(await lstat(path.join(runtime, entry))).isFile()) throw new Error('내보내기용 플레이어가 준비되지 않았습니다.');
  const capabilities = JSON.parse(await readFile(path.join(runtime, 'capabilities.json'), 'utf8')) as {
    formatVersion: string; imageExtensions: string[]; audioExtensions: string[]; fontExtensions?: string[]; visualFeatures?: string[];
  };
  if (capabilities.formatVersion !== '1' || !Array.isArray(capabilities.imageExtensions) || !Array.isArray(capabilities.audioExtensions))
    throw new Error('플레이어 지원 형식 정보가 올바르지 않습니다. 내보내기용 플레이어를 다시 빌드해 주세요.');
  const visual=content.project.visual,requiredFeatures:string[]=[];
  if(visual&&[...visual.screens.flatMap(s=>s.elements),...(visual.components??[]).flatMap(c=>c.elements)].some(e=>e.action==='dialogueAuto'))requiredFeatures.push('dialogue-controls-v1');
  if(visual&&[...visual.screens.flatMap(s=>s.elements),...(visual.components??[]).flatMap(c=>c.elements)].some(e=>e.kind==='playerSettings'||e.binding==='reading-status'))requiredFeatures.push('player-settings-v1');
  if(visual&&[...visual.screens.flatMap(s=>s.elements),...(visual.components??[]).flatMap(c=>c.elements)].some(e=>e.binding==='save-summary'||e.binding==='player-notice'))requiredFeatures.push('save-panel-v1');
  if(visual&&(visual.saveSystem||[...visual.screens.flatMap(s=>s.elements),...(visual.components??[]).flatMap(c=>c.elements)].some(e=>e.kind==='saveSlots'||e.kind==='loadSlots')))requiredFeatures.push('save-slots-v1');
  if(visual&&[...visual.screens.flatMap(s=>s.elements),...(visual.components??[]).flatMap(c=>c.elements)].some(e=>e.choiceLayout==='cards'))requiredFeatures.push('narrative-cards-v1');
  if(visual&&[...visual.screens.flatMap(s=>s.elements),...(visual.components??[]).flatMap(c=>c.elements)].some(e=>e.scheduleLayout==='board'||e.resultLayout==='changes'||e.action==='commitPlan'||e.binding==='schedule-summary'||e.binding==='activity-progress'))requiredFeatures.push('schedule-board-v1');
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
  if(ui&&(ui.screens.some(s=>s.modal)||[...ui.screens.flatMap(s=>s.elements),...(ui.components??[]).flatMap(c=>c.elements)].some(e=>e.flowLayout||e.overflow||e.sort||e.selectionKey||e.tooltip||['selectRow','openModal','closeModal'].includes(e.action??''))||(ui.components??[]).some(c=>c.elements.some(e=>e.kind==='component'))))requiredFeatures.push('ui-composition-v1');
  if(content.project.roster?.stateModel==='character-v1')requiredFeatures.push('character-scoped-rules-v1');
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
  for(const feature of options.extensions?.features??[])if(!requiredFeatures.includes(feature))requiredFeatures.push(feature);
  const supportedFeatures=Array.isArray(capabilities.visualFeatures)?capabilities.visualFeatures:[];
  // Older party-capable players already implement conditional expedition exits.
  // The narrower feature must never imply support for party battles or encounters.
  const missingFeatures=requiredFeatures.filter(feature=>!supportedFeatures.includes(feature)&&
    !(feature==='expedition-conditions-v1'&&supportedFeatures.includes('party-tactics-v1')));
  if(missingFeatures.length)throw new Error(`${target} 플레이어가 지원하지 않는 UI 기능입니다: ${missingFeatures.join(', ')}. 내보내기용 플레이어를 다시 빌드해 주세요.`);
  // Every plugin used on a screen must ship its runtime; never export a game that silently drops it.
  const extensions = options.extensions ?? null;
  if (pluginElementCount && !extensions) throw new PluginError('PLUGIN_RUNTIME_MISSING', '화면에 플러그인 요소가 있지만 플러그인 런타임을 준비하지 않았습니다. 내보내기를 중단했습니다.');
  if (extensions) {
    const used = new Set(visual!.screens.flatMap(screen => screen.elements.flatMap(element => element.kind === 'plugin' && element.extension ? [element.extension.plugin] : [])));
    const missing = [...used].filter(id => !extensions.game.plugins.some(plugin => plugin.id === id));
    if (missing.length) throw new PluginError('PLUGIN_RUNTIME_MISSING', `플러그인 런타임이 없습니다: ${missing.join(', ')}. 내보내기를 중단했습니다.`);
    for (const file of extensions.files) if (!/^plugins\/[a-z][a-z0-9]*(?:-[a-z0-9]+)*\/[A-Za-z0-9][A-Za-z0-9._-]*$/.test(file.target) || /\.\./.test(file.target))
      throw new PluginError('PLUGIN_RUNTIME_MISSING', `잘못된 플러그인 런타임 경로: ${file.target}`);
    // Images a plugin made for this export (atlas pages): plain PNG assets next to the project's.
    for (const asset of extensions.assets ?? []) {
      if (!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(asset.id) || asset.target !== `assets/${asset.id}.png` || content.assets.some(a => a.id === asset.id))
        throw new PluginError('PLUGIN_SERVICE_ERROR', `잘못된 플러그인 내보내기 에셋: ${asset.id}`);
      if (!capabilities.imageExtensions.includes('.png')) throw new Error(`${target} 플레이어가 PNG를 지원하지 않습니다: ${asset.id}`);
    }
  }
  // Project images whose pixels a plugin moved into its export assets: their files are left out
  // unless any other part of the content still names them (content.assets keeps listing them, so
  // the snapshot and its fingerprint are unchanged).
  const omitted = new Set<string>();
  if (extensions?.replacedAssets?.length) {
    const named = new Set<string>();
    const walk = (value: unknown): void => {
      if (typeof value === 'string') named.add(value);
      else if (Array.isArray(value)) value.forEach(walk);
      else if (value && typeof value === 'object') Object.values(value).forEach(walk);
    };
    const { assets: _list, assetHashes: _hashes, ...rest } = content as ContentSnapshot & Record<string, unknown>;
    walk(rest);
    for (const id of extensions.replacedAssets) if (!named.has(id)) omitted.add(id);
  }
  for (const asset of content.assets) {
    const supported = asset.kind === 'font' ? capabilities.fontExtensions??[] : asset.kind === 'image' ? capabilities.imageExtensions : capabilities.audioExtensions;
    if (!supported.includes(path.extname(asset.path).toLowerCase()))
      throw new Error(`${target} 플레이어가 지원하지 않는 에셋 형식입니다: ${asset.id} (${asset.path}). 지원 형식: ${supported.join(', ')}`);
  }
  await mkdir(path.dirname(destination), { recursive: true });
  const parent = await realpath(path.dirname(destination));
  const actualDestination = path.join(parent, path.basename(destination));
  if (inside(actualDestination, projectRoot)) throw new Error('대상 폴더가 프로젝트 경로와 겹칩니다.');
  const staging = path.join(parent, `.raising-export-${randomUUID()}`);
  await mkdir(staging);
  try {
    // Copy entries into the owned staging directory: Electron's fs.cp rejects an existing root with errorOnExist.
    for (const name of await readdir(runtime)) await cp(path.join(runtime, name), path.join(staging, name), {
      recursive: true, force: false, errorOnExist: true,
      filter: async source => { if ((await lstat(source)).isSymbolicLink()) throw new Error('런타임 파일의 심볼릭 링크는 내보낼 수 없습니다.'); return true; } });
    await mkdir(path.join(staging, 'assets'), { recursive: true });
    const assets: Record<string, string> = Object.create(null) as Record<string, string>;
    for (const asset of content.assets) {
      if (!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(asset.id)) throw new Error(`유효하지 않은 에셋 ID: ${asset.id}`);
      if (omitted.has(asset.id)) continue;
      const source = await realpath(path.resolve(projectRoot, asset.path));
      if (!inside(projectRoot, source) || !(await lstat(source)).isFile()) throw new Error(`프로젝트 밖의 에셋입니다: ${asset.id}`);
      const bytes = await readFile(source);
      if (hash(bytes) !== content.assetHashes[asset.id]) throw new Error(`검증 이후 에셋이 변경되었습니다: ${asset.id}. 다시 검증해 주세요.`);
      const extension = path.extname(asset.path).toLowerCase();
      if (!/^\.[a-z0-9]+$/.test(extension)) throw new Error(`지원하지 않는 에셋 확장자: ${asset.id}`);
      const relative = `assets/${asset.id}${extension}`;
      await writeFile(path.join(staging, relative), bytes, { flag: 'wx' }); assets[asset.id] = relative;
    }
    if (extensions) for (const file of extensions.files) {
      const bytes = await readFile(file.source);
      // The store is re-read here: a module replaced after preparation must not be shipped.
      if (hash(bytes) !== file.sha256) throw new PluginError('PLUGIN_RUNTIME_MISSING', `준비 이후 플러그인 런타임 파일이 변경되었습니다: ${file.target}`);
      await mkdir(path.dirname(path.join(staging, file.target)), { recursive: true });
      await writeFile(path.join(staging, file.target), bytes, { flag: 'wx' });
    }
    if (extensions) for (const asset of extensions.assets ?? []) {
      if (hash(asset.bytes) !== asset.sha256) throw new PluginError('PLUGIN_SERVICE_ERROR', `준비 이후 플러그인 내보내기 에셋이 변경되었습니다: ${asset.id}`);
      await writeFile(path.join(staging, asset.target), asset.bytes, { flag: 'wx' });
      assets[asset.id] = asset.target;
    }
    // `extensions` is omitted entirely for plugin-free games so their bundles stay byte-identical.
    const bundle: GameBundle = extensions ? { formatVersion: '1', content: runtimeContent, assets, extensions: extensions.game } : { formatVersion: '1', content: runtimeContent, assets };
    if (presentation) bundle.presentationRevision = presentation.presentationRevision;
    await writeFile(path.join(staging, 'game.json'), JSON.stringify(bundle), { flag: 'wx' });
    await writeFile(path.join(staging, 'README.txt'), target === 'web'
      ? `${content.project.title}\n\n이 폴더 전체를 정적 웹 호스팅에 올리세요. index.html은 HTTP 또는 HTTPS에서 열어야 합니다.\n저장하기는 JSON 파일을 내려받고 불러오기는 그 파일을 선택합니다.\nC++ 코어의 WebAssembly 빌드로 실행합니다.\n`
      : `${content.project.title}\n\nRaisingPlayer.exe를 실행하세요. 같은 폴더의 game.json과 assets 폴더가 필요합니다.\nC++ / SDL3 / RmlUi 독립 플레이어입니다.\n`, { flag: 'wx' });
    await writeFile(path.join(staging, 'export.json'), JSON.stringify({ formatVersion: '1', target,
      contentFingerprint: runtimeContent.contentFingerprint, coreVersion: '0.2.0', assetCount: content.assets.length,
      ...(presentation ? { presentationRevision: presentation.presentationRevision } : {}) }, null, 2), { flag: 'wx' });
    await options.beforePublish?.();
    // A competing export or user-created directory must never be replaced.
    if (await exists(actualDestination)) throw new Error('출력 폴더가 이미 있습니다. 새 이름을 선택하세요.');
    await rename(staging, actualDestination);
    return { directory: actualDestination, target, contentFingerprint: runtimeContent.contentFingerprint, assetCount: content.assets.length };
  } catch (error) {
    // Only the uniquely named staging directory created above is removed on failure.
    await rm(staging, { recursive: true, force: true }); throw error;
  }
}

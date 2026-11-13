import {applyReleaseBranding} from './branding.js';
import {assertDistributableRuntimePath} from './distribution-files.js';
import {prepareRelease,savedReleaseMetadata,verifyPreparedRelease,packageNotices,type PreparedRelease} from './release-metadata.js';
import {controlStyleReferences} from '../visual/control-styles.js';
import {surfaceReferences,surfaceInsetsProblem} from '../visual/surfaces.js';
import {validatedPngDimensions} from '../media/png.js';
import { requiredPlayerFeatures, missingPlayerFeatures } from './compatibility.js';
export { hasPortraitTransforms } from './compatibility.js';
import { gameDirectory, playerExecutable } from './targets.js';
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
  runtimes: { web: string; windows: string; macos?: string; ios?: string };
  /** Current draft declarations, detached and hashed before exporting. Omit to read saved declarations. */
  release?: PreparedRelease;
  prepareBundle?: (directory: string) => Promise<void>;
  /** Plugin runtime modules + documents from StudioEditor.exportExtensions (null/absent when no plugin element is used). */
  extensions?: PreparedExtensions | null | undefined;
  /** Published simulation baseline for an explicitly verified presentation update. */
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
/** Export a validated immutable snapshot. Source assets are rechecked to prevent stale content packaging. */
export async function exportGame(options: ExportOptions): Promise<ExportResult> {
  const { content, target } = options;
  if (target === 'ios' && !options.prepareBundle) throw new Error('iOS exports require Xcode source preparation. Use the editor or build:ios.');
  const presentation = options.presentationBase
    ? preparePresentationRevision(content, options.presentationBase, options.extensions?.game) : undefined;
  const runtimeContent = presentation?.content ?? content;
  if (!['web', 'windows', 'macos', 'ios'].includes(target)) throw new Error('지원하지 않는 내보내기 대상입니다.');
  const projectRoot = await realpath(options.projectRoot);
  if(options.release)await verifyPreparedRelease(projectRoot,options.release);
  // Detach bytes and declarations from a caller that may retain and mutate its preparation.
  const release=await prepareRelease(projectRoot,options.release?.metadata??await savedReleaseMetadata(projectRoot));
  const destination = path.resolve(options.destination);
  if (inside(destination, projectRoot)) throw new Error('프로젝트 폴더 또는 그 상위 폴더에 내보낼 수 없습니다.');
  if (await exists(destination)) throw new Error('대상 폴더가 이미 있습니다. 새 폴더 이름을 선택해 주세요.');
  const runtimePath = options.runtimes[target];
  if (!runtimePath) throw new Error(`The ${target} runtime is not installed. Build or configure that runtime first.`);
  const runtime = await realpath(runtimePath);
  const entry = target === 'web' ? 'index.html' : target === 'ios' ? 'capabilities.json' : playerExecutable(target, '');
  if (!(await lstat(path.join(runtime, entry))).isFile()) throw new Error('내보내기용 플레이어가 준비되지 않았습니다.');
  const capabilities = JSON.parse(await readFile(path.join(runtime, 'capabilities.json'), 'utf8')) as {
    formatVersion: string; imageExtensions: string[]; audioExtensions: string[]; fontExtensions?: string[]; visualFeatures?: string[];
  };
  if (capabilities.formatVersion !== '1' || !Array.isArray(capabilities.imageExtensions) || !Array.isArray(capabilities.audioExtensions))
    throw new Error('플레이어 지원 형식 정보가 올바르지 않습니다. 내보내기용 플레이어를 다시 빌드해 주세요.');
  const requiredFeatures = requiredPlayerFeatures(content, options.extensions?.features ?? []);
  if(presentation?.presentationRevision.visual)requiredFeatures.push('presentation-visual-updates-v1');
  const visual = content.project.visual;
  for(const ref of controlStyleReferences(visual))if(!visual?.styles?.some(style=>style.id===ref.styleId))throw new Error(`Generated control style does not exist at ${ref.pointer}: ${ref.styleId}`);
  const surfaces=surfaceReferences(visual);
  for(const {surface} of surfaces)if(surface.mode!=='none'){const asset=content.assets.find(a=>a.id===surface.imageAssetId);if(!asset||asset.kind!=='image'||path.extname(asset.path).toLowerCase()!=='.png')throw new Error(`UI surface requires a whole PNG image asset: ${surface.imageAssetId}`);}
  const pluginElementCount = visual ? visual.screens.reduce((n, screen) => n + screen.elements.filter(element => element.kind === 'plugin').length, 0) : 0;
  const supportedFeatures=Array.isArray(capabilities.visualFeatures)?capabilities.visualFeatures:[];
  // Older party-capable players already implement conditional expedition exits.
  // The narrower feature must never imply support for party battles or encounters.
  const missingFeatures=missingPlayerFeatures(requiredFeatures,supportedFeatures);
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
    for (const name of (await readdir(runtime)).filter(name => target !== 'ios' || ['fonts', 'licenses', 'capabilities.json'].includes(name))) await cp(path.join(runtime, name), path.join(staging, name), {
      recursive: true, force: false, errorOnExist: true,
      filter: async source => { assertDistributableRuntimePath(path.relative(runtime,source)); if ((await lstat(source)).isSymbolicLink()) throw new Error('런타임 파일의 심볼릭 링크는 내보낼 수 없습니다.'); return true; } });
    const data = gameDirectory(target, staging);
    await mkdir(data, { recursive: true });
    await mkdir(path.join(data, 'assets'), { recursive: true });
    const assets: Record<string, string> = Object.create(null) as Record<string, string>;
    for (const asset of content.assets) {
      if (!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(asset.id)) throw new Error(`유효하지 않은 에셋 ID: ${asset.id}`);
      const assetSurfaces=surfaces.filter(ref=>ref.surface.mode!=='none'&&ref.surface.imageAssetId===asset.id);
      if(omitted.has(asset.id)&&assetSurfaces.length)throw new Error(`UI surface PNG cannot be omitted from an export: ${asset.id}`);
      if (omitted.has(asset.id)) continue;
      const source = await realpath(path.resolve(projectRoot, asset.path));
      if (!inside(projectRoot, source) || !(await lstat(source)).isFile()) throw new Error(`프로젝트 밖의 에셋입니다: ${asset.id}`);
      const bytes = await readFile(source);
      if (hash(bytes) !== content.assetHashes[asset.id]) throw new Error(`검증 이후 에셋이 변경되었습니다: ${asset.id}. 다시 검증해 주세요.`);
      if(assetSurfaces.length){const dimensions=validatedPngDimensions(bytes);for(const {surface} of assetSurfaces){const problem=surfaceInsetsProblem(surface,dimensions);if(problem)throw new Error(`UI surface ${asset.id}: ${problem}`);}}
      const extension = path.extname(asset.path).toLowerCase();
      if (!/^\.[a-z0-9]+$/.test(extension)) throw new Error(`지원하지 않는 에셋 확장자: ${asset.id}`);
      const relative = `assets/${asset.id}${extension}`;
      await writeFile(path.join(data, relative), bytes, { flag: 'wx' }); assets[asset.id] = relative;
    }
    if (extensions) for (const file of extensions.files) {
      const bytes = await readFile(file.source);
      // The store is re-read here: a module replaced after preparation must not be shipped.
      if (hash(bytes) !== file.sha256) throw new PluginError('PLUGIN_RUNTIME_MISSING', `준비 이후 플러그인 런타임 파일이 변경되었습니다: ${file.target}`);
      await mkdir(path.dirname(path.join(data, file.target)), { recursive: true });
      await writeFile(path.join(data, file.target), bytes, { flag: 'wx' });
    }
    if (extensions) for (const asset of extensions.assets ?? []) {
      if (hash(asset.bytes) !== asset.sha256) throw new PluginError('PLUGIN_SERVICE_ERROR', `준비 이후 플러그인 내보내기 에셋이 변경되었습니다: ${asset.id}`);
      await writeFile(path.join(data, asset.target), asset.bytes, { flag: 'wx' });
      assets[asset.id] = asset.target;
    }
    // `extensions` is omitted entirely for plugin-free games so their bundles stay byte-identical.
    const bundle: GameBundle = extensions ? { formatVersion: '1', content: runtimeContent, assets, extensions: extensions.game } : { formatVersion: '1', content: runtimeContent, assets };
    if (presentation) bundle.presentationRevision = presentation.presentationRevision;
    if(release.metadata.displayName)bundle.application={displayName:release.metadata.displayName};
    await writeFile(path.join(data, 'game.json'), JSON.stringify(bundle), { flag: 'wx' });
    await writeFile(path.join(staging, 'README.txt'), target === 'web'
      ? `${content.project.title}\n\n이 폴더 전체를 정적 웹 호스팅에 올리세요. index.html은 HTTP 또는 HTTPS에서 열어야 합니다.\n저장하기는 JSON 파일을 내려받고 불러오기는 그 파일을 선택합니다.\nC++ 코어의 WebAssembly 빌드로 실행합니다.\n`
      : target === 'macos' ? `${content.project.title}\n\nOpen RaisingPlayer.app. Keep the app intact; game content and fonts are in Contents/Resources. Saves are stored in your user Library. This is an unsigned development build.\n`
      : target === 'ios' ? `${content.project.title}\n\nOpen the generated Xcode project, select a simulator or your signing team and device, then Run. If the project has not been generated, run Generate Xcode project.command on a Mac with Xcode and CMake.\n`
      : `${content.project.title}\n\nRaisingPlayer.exe를 실행하세요. 같은 폴더의 game.json과 assets 폴더가 필요합니다.\nC++ / SDL3 / RmlUi 독립 플레이어입니다.\n`, { flag: 'wx' });
    await writeFile(path.join(staging, 'export.json'), JSON.stringify({ formatVersion: '1', target,
      contentFingerprint: runtimeContent.contentFingerprint, coreVersion: '0.2.0', assetCount: content.assets.length,
      ...(presentation ? { presentationRevision: presentation.presentationRevision } : {}) }, null, 2), { flag: 'wx' });
    await packageNotices(data,release);
    await applyReleaseBranding(staging,target,release);
    await options.prepareBundle?.(staging);
    await verifyPreparedRelease(projectRoot,release);
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

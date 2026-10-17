import { createHash } from 'node:crypto';
import { readFile, realpath } from 'node:fs/promises';
import path from 'node:path';
import { findModule, moduleFiles, RUNTIME_PLATFORMS } from './manifest.js';
import { resolveProjectPlugins } from './resolve.js';
import { loadService } from './service.js';
import { pluginStorePath } from './store.js';
import { PluginError } from './zip.js';

export interface GameExtensionPlugin { id: string; version: string; abi: string; entry: string; documents: { type: string; id: string; data: unknown }[] }
/** `game.json` `extensions` (CONTRACT §6). Absent from bundles that use no plugin. */
export interface GameExtensions { version: 1; plugins: GameExtensionPlugin[] }
/**
 * `features`: player capabilities (capabilities.json `visualFeatures`) the exported game needs
 * beyond `plugin-elements-v1`, reported by each used plugin's service `requiredHostFeatures`
 * (e.g. `plugin-mesh-v1` for cutout mesh attachments). exportGame refuses a player lacking one.
 */
/**
 * `assets`: images a plugin's `prepareExport` made for the export (e.g. atlas pages), written as
 * `assets/<id>.png` and added to game.json `assets` (never to the content snapshot, so the
 * contentFingerprint stays the same). `replacedAssets`: project images whose pixels now live in
 * those assets; exportGame omits their files unless something else in the content names them.
 */
export interface PreparedExtensions { game: GameExtensions; files: { source: string; target: string; sha256: string }[]; features?: string[];
  assets?: { id: string; target: string; sha256: string; bytes: Uint8Array }[]; replacedAssets?: string[]; reports?: Record<string, unknown> }
const FEATURE = /^[a-z0-9][a-z0-9.-]{0,63}$/;
const ASSET_ID = /^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/;
const PNG = [137, 80, 78, 71, 13, 10, 26, 10];
const MAX_EXPORT_ASSET_BYTES = 64 * 1024 * 1024;
/** Project assets as exportBundle sees them (the content snapshot's list and hashes). */
export interface ExportAssetSource { root: string | null | undefined; assets: readonly { id: string; kind: string; path: string }[]; hashes: Readonly<Record<string, string>> }
export interface PluginElementRef { pointer: string; element: { id?: unknown; extension?: { plugin?: unknown; type?: unknown; ref?: unknown; props?: unknown } } }
const isRecord = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === 'object' && !Array.isArray(value);

/** Screen elements of kind `plugin` with their JSON pointers inside project.json. */
export function pluginElements(visual: unknown): PluginElementRef[] {
  if (!isRecord(visual) || !Array.isArray(visual.screens)) return [];
  return visual.screens.flatMap((screen, i) => isRecord(screen) && Array.isArray(screen.elements)
    ? screen.elements.flatMap((element, j) => {
      if(!isRecord(element)||element.kind!=='plugin')return [];
      const pointer=`/visual/screens/${i}/elements/${j}`;
      const variants=(Array.isArray(element.variants)?element.variants:[]).flatMap((rule,k)=>
        isRecord(rule)&&isRecord(rule.values)&&typeof rule.values.extensionRef==='string'&&isRecord(element.extension)
          ?[{pointer:`${pointer}/variants/${k}/values`,element:{...element,extension:{...element.extension,ref:rule.values.extensionRef}} as PluginElementRef['element']}]:[]);
      return [{pointer,element:element as PluginElementRef['element']},...variants];
    })
    : []);
}
export const extensionPath = (plugin: string, type: string, id: string) => `plugins/${plugin}/${type}/${id}.json`;

/**
 * Runtime modules and documents for every plugin used by a screen element. Only the
 * runtime module of `target` is copied; authoring and service modules never leave the
 * store. Refuses (PLUGIN_RUNTIME_MISSING) instead of silently dropping animation.
 */
export async function exportBundle(options: { store?: string | null; lock: unknown; documents: readonly { kind: string; data: unknown }[]; visual: unknown; target: 'windows' | 'web'; assets?: ExportAssetSource; hostFeatures?: readonly string[] }): Promise<PreparedExtensions | null> {
  const used = [...new Set(pluginElements(options.visual).map(e => String(e.element.extension?.plugin ?? '')))].sort();
  if (!used.length) return null;
  const resolved = await resolveProjectPlugins(pluginStorePath(options.store), options.lock);
  const platform = RUNTIME_PLATFORMS[options.target];
  const game: GameExtensions = { version: 1, plugins: [] }, files: PreparedExtensions['files'] = [], features = new Set<string>();
  const assets: NonNullable<PreparedExtensions['assets']> = [], replaced = new Set<string>(), reports: Record<string, unknown> = {};
  const projectAssets = options.assets?.assets ?? [];
  // The only project files a service can read at export: snapshot assets, re-hashed (stale = refused).
  const readAsset = async (id: unknown): Promise<Uint8Array> => {
    const asset = projectAssets.find(a => a.id === id);
    const root = options.assets?.root;
    if (!asset || !root) throw new PluginError('PLUGIN_SERVICE_ERROR', `내보내기 에셋이 없습니다: ${String(id)}`);
    const base = await realpath(root), file = await realpath(path.resolve(base, asset.path));
    const relative = path.relative(base, file);
    if (relative.startsWith('..') || path.isAbsolute(relative)) throw new PluginError('PLUGIN_SERVICE_ERROR', `프로젝트 밖의 에셋입니다: ${asset.id}`);
    const bytes = await readFile(file);
    const expected = options.assets?.hashes[asset.id];
    if (expected && createHash('sha256').update(bytes).digest('hex') !== expected) throw new PluginError('PLUGIN_SERVICE_ERROR', `검증 이후 에셋이 변경되었습니다: ${asset.id}. 다시 검증해 주세요.`);
    return bytes;
  };
  for (const id of used) {
    const plugin = resolved.find(p => p.id === id);
    const why = !plugin ? '잠금 파일에 없습니다' : plugin.state === 'disabled' ? '꺼져 있습니다' : plugin.state === 'missing' ? '설치되어 있지 않습니다'
      : plugin.state !== 'active' ? '호환되지 않습니다' : null;
    if (why) throw new PluginError('PLUGIN_RUNTIME_MISSING', `화면에서 사용하는 플러그인 '${id}'가 ${why}. 내보내기를 중단했습니다.`);
    const manifest = plugin!.manifest!, module = findModule(manifest, 'runtime', platform);
    if (!module || (manifest.exportTargets && !manifest.exportTargets.includes(options.target)))
      throw new PluginError('PLUGIN_RUNTIME_MISSING', `플러그인 '${id}' ${manifest.version}에 ${options.target}용 런타임 모듈(${platform})이 없습니다. 내보내기를 중단했습니다.`);
    const names = new Set<string>();
    for (const file of moduleFiles(module)) {
      const name = path.posix.basename(file);
      if (names.has(name.toLowerCase())) throw new PluginError('PLUGIN_RUNTIME_MISSING', `런타임 모듈 파일 이름이 겹칩니다: ${file}`);
      names.add(name.toLowerCase());
      const source = path.join(plugin!.directory!, ...file.split('/'));
      let bytes: Buffer;
      try { bytes = await readFile(source); }
      catch { throw new PluginError('PLUGIN_RUNTIME_MISSING', `플러그인 런타임 파일이 없습니다: ${id}/${file}. 플러그인을 다시 설치하세요.`); }
      const sha256 = createHash('sha256').update(bytes).digest('hex');
      // Re-verify: an edited store file must not be shipped as the signed-off version.
      if (sha256 !== manifest.files[file]) throw new PluginError('PLUGIN_RUNTIME_MISSING', `설치된 런타임 파일이 변경되었습니다: ${id}/${file}. 플러그인을 다시 설치하세요.`);
      files.push({ source, target: `plugins/${id}/${name}`, sha256 });
    }
    const sorted = (list: { type: string; id: string; data: unknown }[]) => list.sort((a, b) => a.type < b.type ? -1 : a.type > b.type ? 1 : a.id < b.id ? -1 : a.id > b.id ? 1 : 0);
    let documents = sorted(options.documents.filter(d => d.kind === 'extension' && isRecord(d.data) && d.data.plugin === id)
      .map(d => d.data as { type: string; id: string; data: unknown })
      .map(d => ({ type: d.type, id: d.id, data: structuredClone(d.data) })));
    // Optional service hooks: prepareExport (rewrites the exported documents, adds export assets)
    // and what the documents need from the player (checked by exportGame).
    let service: Awaited<ReturnType<typeof loadService>>;
    try { service = await loadService({ directory: plugin!.directory!, manifest }); }
    catch (error) { throw new PluginError('PLUGIN_SERVICE_ERROR', `플러그인 '${id}' 서비스 모듈을 불러오지 못해 필요한 플레이어 기능을 확인할 수 없습니다: ${error instanceof Error ? error.message : String(error)}`); }
    if (typeof service?.prepareExport === 'function') {
      let result: unknown;
      try {
        result = await service.prepareExport({ target: options.target, documents: structuredClone(documents), assets: projectAssets.map(a => ({ id: a.id, kind: a.kind, path: a.path })), readAsset,
          ...(options.hostFeatures ? { hostFeatures: [...options.hostFeatures] } : {}) });
      }
      catch (error) { throw new PluginError('PLUGIN_SERVICE_ERROR', `플러그인 '${id}'가 내보내기를 준비하지 못했습니다: ${error instanceof Error ? error.message : String(error)}`); }
      const bad = (why: string) => new PluginError('PLUGIN_SERVICE_ERROR', `플러그인 '${id}'의 prepareExport 결과가 올바르지 않습니다: ${why}`);
      if (!isRecord(result)) throw bad('객체가 아닙니다');
      if (result.documents !== undefined) {
        const types = new Set(manifest.documentTypes.map(t => t.type)), seen = new Set<string>();
        if (!Array.isArray(result.documents)) throw bad('documents');
        for (const d of result.documents as unknown[]) {
          if (!isRecord(d) || typeof d.type !== 'string' || typeof d.id !== 'string' || !types.has(d.type) || !ASSET_ID.test(d.id) || seen.has(`${d.type}/${d.id}`)) throw bad('documents의 type/id');
          seen.add(`${d.type}/${d.id}`);
        }
        documents = sorted((result.documents as { type: string; id: string; data: unknown }[]).map(d => ({ type: d.type, id: d.id, data: structuredClone(d.data) })));
      }
      for (const a of Array.isArray(result.assets) ? result.assets as unknown[] : result.assets === undefined ? [] : [null]) {
        const asset = a as { id?: unknown; bytes?: unknown; extension?: unknown } | null;
        if (!asset || typeof asset.id !== 'string' || !ASSET_ID.test(asset.id) || asset.extension !== '.png' || !(asset.bytes instanceof Uint8Array)) throw bad('assets');
        if (projectAssets.some(p => p.id === asset.id) || assets.some(p => p.id === asset.id)) throw bad(`에셋 ID가 겹칩니다: ${asset.id}`);
        const bytes = asset.bytes;
        if (bytes.length > MAX_EXPORT_ASSET_BYTES || !PNG.every((b, i) => bytes[i] === b)) throw bad(`PNG가 아닙니다: ${asset.id}`);
        assets.push({ id: asset.id, target: `assets/${asset.id}.png`, sha256: createHash('sha256').update(bytes).digest('hex'), bytes });
      }
      for (const r of Array.isArray(result.replacedAssets) ? result.replacedAssets as unknown[] : []) {
        if (typeof r !== 'string' || !projectAssets.some(p => p.id === r)) throw bad(`replacedAssets: ${String(r)}`);
        replaced.add(r);
      }
      if (result.report !== undefined) reports[id] = structuredClone(result.report);
    }
    game.plugins.push({ id, version: manifest.version, abi: module.abi!, entry: `plugins/${id}/${path.posix.basename(module.path)}`, documents });
    if (typeof service?.requiredHostFeatures === 'function') {
      const listed: unknown = await service.requiredHostFeatures(structuredClone(documents));
      if (!Array.isArray(listed) || listed.some(f => typeof f !== 'string' || !FEATURE.test(f)))
        throw new PluginError('PLUGIN_SERVICE_ERROR', `플러그인 '${id}'의 requiredHostFeatures 결과가 올바르지 않습니다.`);
      for (const f of listed as string[]) features.add(f);
    }
  }
  return { game, files, features: [...features].sort(), ...(assets.length ? { assets } : {}), ...(replaced.size ? { replacedAssets: [...replaced].sort() } : {}), ...(Object.keys(reports).length ? { reports } : {}) };
}

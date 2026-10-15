import { access } from 'node:fs/promises';
import path from 'node:path';
import { apiCompatible, ENGINE_PLUGIN_API, findModule, type PluginManifest } from './manifest.js';
import { satisfies } from './semver.js';
import { listInstalled, pluginStorePath, type InstalledPlugin } from './store.js';
import { loadService, type PluginService } from './service.js';
import { PluginError } from './zip.js';

export interface LockEntry { id: string; version: string; enabled: boolean }
export type PluginState = 'active' | 'disabled' | 'missing' | 'incompatible';
export interface PluginIssue { code: string; severity: 'error' | 'warning'; message: string; pointer: string }
export interface ResolvedPlugin {
  id: string; version: string; enabled: boolean; state: PluginState;
  /** JSON pointer of the lock entry (`/plugins/<i>`). */
  pointer: string;
  directory: string | null; manifest: PluginManifest | null; diagnostics: PluginIssue[];
}
const isRecord = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === 'object' && !Array.isArray(value);

export function lockEntries(lock: unknown): LockEntry[] {
  if (!isRecord(lock) || !Array.isArray(lock.plugins)) return [];
  return lock.plugins.filter(isRecord).filter(e => typeof e.id === 'string' && typeof e.version === 'string')
    .map(e => ({ id: e.id as string, version: e.version as string, enabled: e.enabled !== false }));
}

/**
 * Resolve every lock entry against the store. Projects pin exact versions; any other
 * installed version of the same plugin is ignored. Nothing here touches project data:
 * unavailable plugins only produce diagnostics (CONTRACT §3).
 */
export async function resolveProjectPlugins(store: string, lock: unknown, installed?: InstalledPlugin[]): Promise<ResolvedPlugin[]> {
  const entries = isRecord(lock) && Array.isArray(lock.plugins) ? lock.plugins : [];
  if (!entries.length) return [];
  const available = installed ?? await listInstalled(store).catch(() => [] as InstalledPlugin[]);
  const result: ResolvedPlugin[] = [], seen = new Set<string>();
  entries.forEach((raw, i) => {
    if (!isRecord(raw) || typeof raw.id !== 'string' || typeof raw.version !== 'string') return;
    const pointer = `/plugins/${i}`, id = raw.id, version = raw.version, enabled = raw.enabled !== false;
    if (seen.has(id)) {
      result.push({ id, version, enabled: false, state: 'incompatible', pointer, directory: null, manifest: null,
        diagnostics: [{ code: 'PLUGIN_LOCK_DUPLICATE', severity: 'error', pointer: `${pointer}/id`, message: `플러그인 '${id}'가 잠금 파일에 중복되었습니다. 한 버전만 고정하세요.` }] });
      return;
    }
    seen.add(id);
    const found = available.find(p => p.id === id && p.version === version);
    const base = { id, version, enabled, pointer, directory: found?.directory ?? null, manifest: found?.manifest ?? null };
    if (!enabled) { result.push({ ...base, state: 'disabled', diagnostics: [{ code: 'PLUGIN_DISABLED', severity: 'warning', pointer: `${pointer}/enabled`,
      message: `플러그인 '${id}'가 꺼져 있습니다. 문서는 그대로 보존하며 화면 요소는 자리표시로 보입니다.` }] }); return; }
    if (!found) { result.push({ ...base, state: 'missing', diagnostics: [{ code: 'PLUGIN_MISSING', severity: 'warning', pointer: `${pointer}/version`,
      message: `플러그인 '${id}' ${version}이(가) 설치되어 있지 않습니다. 문서는 그대로 보존합니다. 같은 버전을 설치하세요.` }] }); return; }
    if (!apiCompatible(found.manifest)) { result.push({ ...base, state: 'incompatible', diagnostics: [{ code: 'PLUGIN_API_INCOMPATIBLE', severity: 'error', pointer,
      message: `플러그인 '${id}' ${version}은(는) 엔진 플러그인 API ${found.manifest.engineApi.min}..${found.manifest.engineApi.max}용입니다(현재 ${ENGINE_PLUGIN_API}).` }] }); return; }
    result.push({ ...base, state: 'active', diagnostics: [] });
  });
  // Dependencies resolve against the project's own pins, after every entry is known.
  for (const plugin of result) {
    if (plugin.state !== 'active') continue;
    for (const dependency of plugin.manifest!.dependencies ?? []) {
      const target = result.find(p => p.id === dependency.id);
      const problem = !target ? '잠금 파일에 없습니다' : target.state === 'disabled' ? '꺼져 있습니다' : target.state !== 'active' ? '사용할 수 없습니다'
        : !satisfies(target.version, dependency.range) ? `고정 버전 ${target.version}이 요구 범위 ${dependency.range}에 맞지 않습니다` : null;
      if (!problem) continue;
      plugin.state = 'incompatible';
      plugin.diagnostics.push({ code: 'PLUGIN_DEPENDENCY', severity: 'error', pointer: plugin.pointer,
        message: `플러그인 '${plugin.id}'가 의존하는 '${dependency.id}' ${dependency.range}이(가) ${problem}.` });
    }
  }
  // A dependency that became incompatible above makes its dependents incompatible too.
  for (let changed = true; changed;) {
    changed = false;
    for (const plugin of result) if (plugin.state === 'active' && (plugin.manifest!.dependencies ?? []).some(d => result.find(p => p.id === d.id)?.state !== 'active')) {
      plugin.state = 'incompatible'; changed = true;
      plugin.diagnostics.push({ code: 'PLUGIN_DEPENDENCY', severity: 'error', pointer: plugin.pointer, message: `플러그인 '${plugin.id}'의 의존 플러그인을 사용할 수 없습니다.` });
    }
  }
  return result;
}

export interface PluginStatusInstalled { id: string; version: string; name: string; directory: string; documentTypes: PluginManifest['documentTypes']; elementTypes: PluginManifest['elementTypes']; modules: PluginManifest['modules'] }
export interface PluginStatusProject { id: string; version: string | null; enabled: boolean; state: PluginState | 'notLocked'; directory: string | null; authoring: string | null; diagnostics: PluginIssue[] }
export interface PluginStatus { store: string; engineApi: number; installed: PluginStatusInstalled[]; project: PluginStatusProject[] }

/** Absolute path of the authoring module for this host OS, when the file is present. */
export async function authoringModule(plugin: { directory: string | null; manifest: PluginManifest | null }): Promise<string | null> {
  if (!plugin.directory || !plugin.manifest) return null;
  const prefix = process.platform === 'win32' ? 'windows-' : process.platform === 'darwin' ? 'macos-' : 'linux-';
  const module = plugin.manifest.modules.find(m => m.role === 'authoring' && m.platform.startsWith(prefix)) ?? findModule(plugin.manifest, 'authoring');
  if (!module) return null;
  const file = path.join(plugin.directory, ...module.path.split('/'));
  try { await access(file); return file; } catch { return null; }
}

/** Shared Qt/MCP status view: store contents plus this project's lock resolution. */
export async function pluginStatus(storeOption: string | null | undefined, lock: unknown, extensionPlugins: Iterable<string> = []): Promise<PluginStatus> {
  const store = pluginStorePath(storeOption);
  const installed = await listInstalled(store).catch(() => [] as InstalledPlugin[]);
  const resolved = await resolveProjectPlugins(store, lock, installed);
  const project: PluginStatusProject[] = [];
  for (const p of resolved) project.push({ id: p.id, version: p.version, enabled: p.enabled, state: p.state, directory: p.directory,
    authoring: p.state === 'active' ? await authoringModule(p) : null, diagnostics: p.diagnostics });
  for (const id of [...new Set(extensionPlugins)].sort()) if (!resolved.some(p => p.id === id)) project.push({ id, version: null, enabled: false, state: 'notLocked',
    directory: null, authoring: null, diagnostics: [{ code: 'PLUGIN_NOT_LOCKED', severity: 'warning', pointer: '', message: `플러그인 '${id}' 문서가 있지만 잠금 파일에 없습니다. 문서는 그대로 보존합니다.` }] });
  return { store, engineApi: ENGINE_PLUGIN_API, project,
    installed: installed.map(p => ({ id: p.id, version: p.version, name: p.manifest.name, directory: p.directory,
      documentTypes: p.manifest.documentTypes, elementTypes: p.manifest.elementTypes, modules: p.manifest.modules })) };
}

/**
 * Run a plugin service op for a project (Qt `pluginCall`, MCP `plugin_call`). Only plugins
 * the project has locked, enabled and resolved can be called; ops never write project files.
 */
export async function callProjectPlugin(storeOption: string | null | undefined, lock: unknown, pluginId: string, op: string, payload: unknown): Promise<unknown> {
  const { service } = await projectPluginService(storeOption, lock, pluginId);
  if (!service?.call) throw new PluginError('PLUGIN_SERVICE', `플러그인 '${pluginId}'에는 서비스 명령이 없습니다.`);
  return await service.call(op, structuredClone(payload ?? {}));
}
/** The active (locked, enabled, resolved) plugin and its loaded node service (null = none). */
export async function projectPluginService(storeOption: string | null | undefined, lock: unknown, pluginId: string): Promise<{ plugin: ResolvedPlugin; service: PluginService | null }> {
  const plugin = (await resolveProjectPlugins(pluginStorePath(storeOption), lock)).find(p => p.id === pluginId);
  if (!plugin) throw new PluginError('PLUGIN_NOT_LOCKED', `플러그인 '${pluginId}'가 이 프로젝트의 잠금 파일에 없습니다.`);
  if (plugin.state !== 'active') throw new PluginError(plugin.diagnostics[0]?.code ?? 'PLUGIN_UNAVAILABLE', plugin.diagnostics[0]?.message ?? `플러그인 '${pluginId}'를 사용할 수 없습니다.`);
  return { plugin, service: await loadService({ directory: plugin.directory!, manifest: plugin.manifest! }) };
}

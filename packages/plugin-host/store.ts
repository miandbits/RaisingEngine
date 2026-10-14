import { createHash, randomUUID } from 'node:crypto';
import { mkdir, open, readFile, rename, rm, stat, unlink, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { apiCompatible, ENGINE_PLUGIN_API, parseManifest, type PluginManifest } from './manifest.js';
import { readZip, PluginError, type ZipEntry } from './zip.js';

export const MAX_PACKAGE_BYTES = 128 * 1024 * 1024;
export interface RegistryEntry { id: string; version: string; sha256: string; source: 'local' | 'remote'; installedAt: string }
export interface Registry { version: 1; installed: RegistryEntry[] }
export interface InstalledPlugin { id: string; version: string; sha256: string; source: 'local' | 'remote'; installedAt: string; directory: string; manifest: PluginManifest }
export interface InspectedPackage { manifest: PluginManifest; sha256: string; entries: ZipEntry[]; apiCompatible: boolean }
export interface InstallResult { plugin: InstalledPlugin; alreadyInstalled: boolean }

const sha256 = (bytes: Uint8Array | string) => createHash('sha256').update(bytes).digest('hex');
/** dist/packages/plugin-host/store.js → engine root (three levels up). */
export const engineRoot = fileURLToPath(new URL('../../../', import.meta.url));
/** `$RAISING_PLUGIN_STORE` or `<engineRoot>/plugin-store`; an explicit path wins (tests, hosts). */
export function pluginStorePath(explicit?: string | null): string {
  return path.resolve(explicit || process.env.RAISING_PLUGIN_STORE || path.join(engineRoot, 'plugin-store'));
}
export const installedDirectory = (store: string, id: string, version: string) => path.join(store, 'installed', id, version);

export async function readRegistry(store: string): Promise<Registry> {
  let text: string;
  try { text = await readFile(path.join(store, 'registry.json'), 'utf8'); }
  catch (error) { if ((error as NodeJS.ErrnoException).code === 'ENOENT') return { version: 1, installed: [] }; throw error; }
  const value = JSON.parse(text) as Registry;
  if (value?.version !== 1 || !Array.isArray(value.installed)) throw new PluginError('PLUGIN_STORE', `플러그인 저장소 registry.json이 올바르지 않습니다: ${store}`);
  return value;
}
async function writeRegistry(store: string, registry: Registry): Promise<void> {
  const target = path.join(store, 'registry.json'), temporary = `${target}.${randomUUID()}.tmp`;
  await writeFile(temporary, JSON.stringify(registry, null, 2) + '\n', { flag: 'wx' });
  try { await rename(temporary, target); } finally { await unlink(temporary).catch(() => {}); }
}

/** Cross-process store lock: wx file with owner pid; a dead owner's lock is reclaimed once. */
async function withStoreLock<T>(store: string, action: () => Promise<T>): Promise<T> {
  await mkdir(store, { recursive: true });
  const lockFile = path.join(store, '.lock');
  let handle;
  for (let attempt = 0; !handle; attempt++) {
    try { handle = await open(lockFile, 'wx'); }
    catch (error) {
      if ((error as NodeJS.ErrnoException).code !== 'EEXIST' || attempt > 0) throw new PluginError('PLUGIN_STORE_BUSY', '다른 작업이 플러그인 저장소를 사용 중입니다. 잠시 후 다시 시도하세요.');
      const owner = Number((await readFile(lockFile, 'utf8').catch(() => '')).trim());
      let alive = false;
      if (Number.isInteger(owner) && owner > 0) try { process.kill(owner, 0); alive = true; } catch (e) { alive = (e as NodeJS.ErrnoException).code === 'EPERM'; }
      if (alive) throw new PluginError('PLUGIN_STORE_BUSY', '다른 작업이 플러그인 저장소를 사용 중입니다. 잠시 후 다시 시도하세요.');
      await unlink(lockFile).catch(() => {});
    }
  }
  try { await handle.writeFile(String(process.pid)); await handle.close(); return await action(); }
  finally { await unlink(lockFile).catch(() => {}); }
}

/** Parse and fully verify an archive in memory: manifest, path rules, and every file hash. */
export function inspectBytes(bytes: Buffer): InspectedPackage {
  if (bytes.length > MAX_PACKAGE_BYTES) throw new PluginError('PLUGIN_PACKAGE_LIMIT', '패키지 파일은 128 MiB 이하여야 합니다.');
  const entries = readZip(bytes);
  const manifestEntry = entries.find(e => e.name === 'plugin.json');
  if (!manifestEntry) throw new PluginError('PLUGIN_MANIFEST', '패키지 최상위에 plugin.json이 없습니다.');
  const manifest = parseManifest(manifestEntry.data);
  const present = new Map(entries.filter(e => e !== manifestEntry).map(e => [e.name, e]));
  for (const [file, hash] of Object.entries(manifest.files)) {
    const entry = present.get(file);
    if (!entry) throw new PluginError('PLUGIN_FILE_MISSING', `manifest에 있는 파일이 패키지에 없습니다: ${file}`);
    if (sha256(entry.data) !== hash) throw new PluginError('PLUGIN_FILE_HASH', `파일 해시가 manifest와 다릅니다: ${file}`);
  }
  for (const file of present.keys()) if (!Object.hasOwn(manifest.files, file)) throw new PluginError('PLUGIN_FILE_UNLISTED', `manifest files에 없는 파일이 있습니다: ${file}`);
  return { manifest, sha256: sha256(bytes), entries, apiCompatible: apiCompatible(manifest) };
}
export async function inspectPackage(archivePath: string): Promise<InspectedPackage> {
  const info = await stat(archivePath);
  if (!info.isFile()) throw new PluginError('PLUGIN_PACKAGE_FORMAT', '패키지 경로는 일반 파일이어야 합니다.');
  if (info.size > MAX_PACKAGE_BYTES) throw new PluginError('PLUGIN_PACKAGE_LIMIT', '패키지 파일은 128 MiB 이하여야 합니다.');
  return inspectBytes(await readFile(archivePath));
}

const manifestCache = new Map<string, PluginManifest>();
/** Installed versions are immutable, so a parsed manifest is cached per directory. */
export async function readInstalledManifest(directory: string): Promise<PluginManifest> {
  const cached = manifestCache.get(directory);
  if (cached) return cached;
  const manifest = parseManifest(await readFile(path.join(directory, 'plugin.json')));
  manifestCache.set(directory, manifest);
  return manifest;
}

export async function listInstalled(store: string): Promise<InstalledPlugin[]> {
  const registry = await readRegistry(store), result: InstalledPlugin[] = [];
  for (const entry of registry.installed) {
    const directory = installedDirectory(store, entry.id, entry.version);
    try {
      const manifest = await readInstalledManifest(directory);
      if (manifest.id === entry.id && manifest.version === entry.version) result.push({ ...entry, directory, manifest });
    } catch { /* A damaged installation is reported as missing by project resolution. */ }
  }
  return result.sort((a, b) => a.id < b.id ? -1 : a.id > b.id ? 1 : a.version < b.version ? -1 : a.version > b.version ? 1 : 0);
}
export async function findInstalled(store: string, id: string, version: string): Promise<InstalledPlugin | undefined> {
  return (await listInstalled(store)).find(p => p.id === id && p.version === version);
}

/**
 * Install a verified package. Remote installs must download to a temporary file and call
 * this same function (source 'remote'). Same id+version+hash is idempotent; a different
 * hash is PLUGIN_VERSION_CONFLICT because installed versions are immutable.
 */
export async function installPackage(store: string, archivePath: string, { source = 'local' }: { source?: 'local' | 'remote' } = {}): Promise<InstallResult> {
  const inspected = await inspectPackage(archivePath);
  const { manifest } = inspected;
  if (!inspected.apiCompatible) throw new PluginError('PLUGIN_API_INCOMPATIBLE',
    `이 엔진의 플러그인 API ${ENGINE_PLUGIN_API}는 '${manifest.id}' ${manifest.version}(지원 ${manifest.engineApi.min}..${manifest.engineApi.max})과 호환되지 않습니다.`);
  return withStoreLock(store, async () => {
    const registry = await readRegistry(store);
    const prior = registry.installed.find(e => e.id === manifest.id && e.version === manifest.version);
    const directory = installedDirectory(store, manifest.id, manifest.version);
    if (prior) {
      if (prior.sha256 !== inspected.sha256) throw new PluginError('PLUGIN_VERSION_CONFLICT',
        `'${manifest.id}' ${manifest.version}이(가) 다른 내용으로 이미 설치되어 있습니다. 설치된 버전은 바꿀 수 없으니 버전 번호를 올려 다시 패키징하세요.`);
      return { plugin: { ...prior, directory, manifest: await readInstalledManifest(directory) }, alreadyInstalled: true };
    }
    const staging = path.join(store, `.staging-${randomUUID()}`);
    await mkdir(staging);
    try {
      for (const entry of inspected.entries) {
        const target = path.join(staging, ...entry.name.split('/'));
        await mkdir(path.dirname(target), { recursive: true });
        await writeFile(target, entry.data, { flag: 'wx' });
      }
      await mkdir(path.dirname(directory), { recursive: true });
      // An unregistered directory is a crashed earlier install, never a published version.
      await rm(directory, { recursive: true, force: true });
      manifestCache.delete(directory);
      await rename(staging, directory);
    } catch (error) { await rm(staging, { recursive: true, force: true }); throw error; }
    const entry: RegistryEntry = { id: manifest.id, version: manifest.version, sha256: inspected.sha256, source, installedAt: new Date().toISOString() };
    registry.installed.push(entry);
    registry.installed.sort((a, b) => a.id < b.id ? -1 : a.id > b.id ? 1 : a.version < b.version ? -1 : a.version > b.version ? 1 : 0);
    await writeRegistry(store, registry);
    return { plugin: { ...entry, directory, manifest }, alreadyInstalled: false };
  });
}

export async function uninstallPlugin(store: string, id: string, version: string): Promise<boolean> {
  return withStoreLock(store, async () => {
    const registry = await readRegistry(store);
    const index = registry.installed.findIndex(e => e.id === id && e.version === version);
    if (index < 0) return false;
    registry.installed.splice(index, 1);
    // Unregister first: a crash afterwards leaves an orphan directory, never a half-deleted "installed" version.
    await writeRegistry(store, registry);
    const directory = installedDirectory(store, id, version), trash = path.join(store, `.trash-${randomUUID()}`);
    manifestCache.delete(directory);
    await rename(directory, trash).catch(() => {});
    await rm(trash, { recursive: true, force: true });
    return true;
  });
}

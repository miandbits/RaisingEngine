import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import { Ajv2020 } from 'ajv/dist/2020.js';
import type { ValidateFunction } from 'ajv';
import { findModule, SERVICE_PLATFORM, type PluginManifest } from './manifest.js';
import { PluginError } from './zip.js';

/** Input of a plugin service's `validate`. Structure (envelope + plugin JSON schema) is already checked. */
export interface ServiceValidateInput {
  documents: { file: string; type: string; id: string; data: unknown }[];
  assets: { id: string; kind: string; path: string; absolutePath: string | null }[];
  elements: { file: string; pointer: string; element: unknown }[];
}
export interface ServiceDiagnostic { file: string; pointer: string; code: string; severity: 'error' | 'warning'; message: string; relatedIds?: string[] }
/**
 * Node service module contract (`role: "service"`, `platform: "node"`):
 * `export async function createService({ pluginDir, manifest })`. `call` must never write
 * project files; document edits always go through the shared StudioEditor transaction.
 */
export interface PluginService {
  validate?(input: ServiceValidateInput): ServiceDiagnostic[] | Promise<ServiceDiagnostic[]>;
  call?(op: string, payload: unknown): unknown;
  /** Player capabilities (`visualFeatures`) an exported game with these documents needs beyond
   *  `plugin-elements-v1` (CONTRACT.md §7); exportBundle collects them, exportGame enforces them. */
  requiredHostFeatures?(documents: { type: string; id: string; data: unknown }[]): string[] | Promise<string[]>;
}
export interface LoadablePlugin { directory: string; manifest: PluginManifest }
const services = new Map<string, Promise<PluginService | null>>();

/** Load (once per installed id@version directory) the node service module, or null when the plugin has none. */
export function loadService(plugin: LoadablePlugin): Promise<PluginService | null> {
  const key = plugin.directory;
  let pending = services.get(key);
  if (!pending) {
    pending = (async () => {
      const module = findModule(plugin.manifest, 'service', SERVICE_PLATFORM);
      if (!module) return null;
      const url = pathToFileURL(path.join(plugin.directory, ...module.path.split('/'))).href;
      const loaded = await import(url) as { createService?: (options: { pluginDir: string; manifest: PluginManifest }) => Promise<PluginService> | PluginService };
      if (typeof loaded.createService !== 'function') throw new PluginError('PLUGIN_SERVICE', `플러그인 서비스 모듈에 createService가 없습니다: ${plugin.manifest.id}`);
      return await loaded.createService({ pluginDir: plugin.directory, manifest: structuredClone(plugin.manifest) }) ?? null;
    })();
    // A failed load is retried on the next request instead of being cached forever.
    pending.catch(() => services.delete(key));
    services.set(key, pending);
  }
  return pending;
}

const validators = new Map<string, Promise<Map<string, ValidateFunction>>>();
/** Per installed directory: one Ajv instance so plugin `$id`s never collide across versions. */
export function dataValidators(plugin: LoadablePlugin): Promise<Map<string, ValidateFunction>> {
  let pending = validators.get(plugin.directory);
  if (!pending) {
    pending = (async () => {
      const ajv = new Ajv2020({ strict: false, allErrors: true });
      const result = new Map<string, ValidateFunction>();
      for (const type of plugin.manifest.documentTypes) {
        const schema = JSON.parse(await readFile(path.join(plugin.directory, ...type.schema.split('/')), 'utf8')) as object;
        result.set(type.type, ajv.compile(schema));
      }
      return result;
    })();
    pending.catch(() => validators.delete(plugin.directory));
    validators.set(plugin.directory, pending);
  }
  return pending;
}

/** Read a plugin document schema (MCP `get_schema`). */
export async function readDocumentSchema(plugin: LoadablePlugin, type: string): Promise<unknown> {
  const entry = plugin.manifest.documentTypes.find(t => t.type === type);
  if (!entry) throw new PluginError('EXTENSION_TYPE_UNKNOWN', `플러그인 '${plugin.manifest.id}'에 문서 종류 '${type}'가 없습니다.`);
  return JSON.parse(await readFile(path.join(plugin.directory, ...entry.schema.split('/')), 'utf8'));
}

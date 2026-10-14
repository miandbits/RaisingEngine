import { Ajv2020 } from 'ajv/dist/2020.js';
import { isSafeRelativePath, PluginError } from './zip.js';
import { RANGE_PATTERN, VERSION_PATTERN } from './semver.js';

/** Engine plugin API implemented by this build (CONTRACT.md "plugin api 1"). */
export const ENGINE_PLUGIN_API = 1;
export const PLUGIN_ID_PATTERN = '^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$';
export type ModuleRole = 'authoring' | 'runtime' | 'service';
export interface PluginModule { role: ModuleRole; platform: string; abi?: string; path: string; files?: string[] }
export interface PluginManifest {
  manifestVersion: 1; id: string; name: string; version: string; description?: string;
  engineApi: { min: number; max: number };
  dependencies?: { id: string; range: string }[];
  documentTypes: { type: string; label: string; schema: string }[];
  elementTypes: { type: string; label: string; refType?: string }[];
  modules: PluginModule[];
  exportTargets?: ('windows' | 'web')[];
  capabilities?: string[];
  files: Record<string, string>;
}
/** Export target → runtime module platform. */
export const RUNTIME_PLATFORMS = { windows: 'windows-x64', web: 'web-wasm' } as const;
export const SERVICE_PLATFORM = 'node';

const id = { type: 'string', pattern: PLUGIN_ID_PATTERN, maxLength: 64 };
const relative = { type: 'string', minLength: 1, maxLength: 240 };
const schema = {
  type: 'object', additionalProperties: false,
  required: ['manifestVersion', 'id', 'name', 'version', 'engineApi', 'documentTypes', 'elementTypes', 'modules', 'files'],
  properties: {
    manifestVersion: { const: 1 }, id, name: { type: 'string', minLength: 1, maxLength: 100 },
    version: { type: 'string', pattern: VERSION_PATTERN }, description: { type: 'string', maxLength: 2000 },
    engineApi: { type: 'object', additionalProperties: false, required: ['min', 'max'],
      properties: { min: { type: 'integer', minimum: 1, maximum: 1000 }, max: { type: 'integer', minimum: 1, maximum: 1000 } } },
    dependencies: { type: 'array', maxItems: 32, items: { type: 'object', additionalProperties: false, required: ['id', 'range'],
      properties: { id, range: { type: 'string', pattern: RANGE_PATTERN } } } },
    documentTypes: { type: 'array', maxItems: 64, items: { type: 'object', additionalProperties: false, required: ['type', 'label', 'schema'],
      properties: { type: id, label: { type: 'string', minLength: 1, maxLength: 100 }, schema: relative } } },
    elementTypes: { type: 'array', maxItems: 64, items: { type: 'object', additionalProperties: false, required: ['type', 'label'],
      properties: { type: id, label: { type: 'string', minLength: 1, maxLength: 100 }, refType: id } } },
    modules: { type: 'array', maxItems: 32, items: { type: 'object', additionalProperties: false, required: ['role', 'platform', 'path'],
      properties: { role: { enum: ['authoring', 'runtime', 'service'] }, platform: { type: 'string', pattern: '^[a-z0-9][a-z0-9.-]*$', maxLength: 64 },
        abi: { type: 'string', pattern: '^[a-z0-9][a-z0-9.-]*$', maxLength: 64 }, path: relative,
        files: { type: 'array', maxItems: 64, items: relative } } } },
    exportTargets: { type: 'array', uniqueItems: true, items: { enum: ['windows', 'web'] } },
    capabilities: { type: 'array', maxItems: 64, uniqueItems: true, items: { type: 'string', pattern: '^[a-z0-9][a-z0-9.-]*$', maxLength: 64 } },
    files: { type: 'object', maxProperties: 1999, propertyNames: relative, additionalProperties: { type: 'string', pattern: '^[0-9a-f]{64}$' } },
  },
} as const;
let compiled: ReturnType<Ajv2020['compile']> | undefined;

/** Structure (JSON Schema) and cross-field rules of plugin.json. Throws PluginError. */
export function parseManifest(source: string | Buffer): PluginManifest {
  let value: unknown;
  try { value = JSON.parse(typeof source === 'string' ? source : new TextDecoder('utf-8', { fatal: true }).decode(source)); }
  catch { throw new PluginError('PLUGIN_MANIFEST', 'plugin.json이 올바른 UTF-8 JSON이 아닙니다.'); }
  const validate = compiled ??= new Ajv2020({ strict: true, allErrors: true }).compile(schema);
  if (!validate(value)) {
    const issue = validate.errors?.[0];
    throw new PluginError('PLUGIN_MANIFEST', `plugin.json 구조 오류 ${issue?.instancePath || '/'}: ${issue?.message ?? 'invalid'}`);
  }
  const manifest = value as PluginManifest;
  const problem = (message: string): never => { throw new PluginError('PLUGIN_MANIFEST', `plugin.json: ${message}`); };
  if (manifest.engineApi.min > manifest.engineApi.max) problem('engineApi.min은 max 이하여야 합니다.');
  const unique = (values: string[], label: string) => { if (new Set(values).size !== values.length) problem(`${label}이(가) 중복되었습니다.`); };
  unique(manifest.documentTypes.map(t => t.type), '문서 종류');
  unique(manifest.elementTypes.map(t => t.type), '화면 요소 종류');
  unique((manifest.dependencies ?? []).map(d => d.id), '의존 플러그인');
  if (manifest.dependencies?.some(d => d.id === manifest.id)) problem('자기 자신에 의존할 수 없습니다.');
  const files = Object.keys(manifest.files);
  for (const file of files) if (!isSafeRelativePath(file) || file === 'plugin.json') problem(`허용되지 않는 파일 경로: ${file}`);
  unique(files.map(f => f.toLowerCase()), '파일 경로');
  const listed = (file: string, what: string) => { if (!Object.hasOwn(manifest.files, file)) problem(`${what} '${file}'가 files 목록에 없습니다.`); };
  for (const type of manifest.documentTypes) listed(type.schema, '문서 스키마');
  for (const type of manifest.elementTypes) if (type.refType && !manifest.documentTypes.some(t => t.type === type.refType)) problem(`화면 요소 '${type.type}'의 refType '${type.refType}'가 문서 종류에 없습니다.`);
  const keys = new Set<string>();
  for (const module of manifest.modules) {
    listed(module.path, '모듈'); for (const extra of module.files ?? []) listed(extra, '모듈 파일');
    if (module.role === 'runtime' && !module.abi) problem(`런타임 모듈 '${module.path}'에 abi가 필요합니다.`);
    const key = `${module.role}:${module.platform}`;
    if (keys.has(key)) problem(`같은 역할·플랫폼 모듈이 중복되었습니다: ${key}`);
    keys.add(key);
  }
  return manifest;
}

export const apiCompatible = (manifest: PluginManifest, api = ENGINE_PLUGIN_API) => manifest.engineApi.min <= api && api <= manifest.engineApi.max;
export const findModule = (manifest: PluginManifest, role: ModuleRole, platform?: string) =>
  manifest.modules.find(m => m.role === role && (platform === undefined || m.platform === platform));
/** Every file a module needs at runtime, entry first. */
export const moduleFiles = (module: PluginModule) => [module.path, ...(module.files ?? [])];

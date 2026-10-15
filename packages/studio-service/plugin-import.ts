import { createHash } from 'node:crypto';
import { realpath } from 'node:fs/promises';
import path from 'node:path';
import { MAX_EDIT_BATCH, type PrimitiveEdit, type StudioEditor } from './editor.js';
import { decodePng } from './png.js';
import { publishImmutableAsset } from './immutable-asset.js';
import { PLUGIN_ID_PATTERN } from '../plugin-host/manifest.js';
import { projectPluginService } from '../plugin-host/resolve.js';
import { PluginError } from '../plugin-host/zip.js';
import type { Json } from '../validation/model.js';

/** Qt `pluginImport`, MCP `plugin_import`: an outside file through a plugin's `prepareImport` hook. */
export interface PluginImportInput { plugin: string; op: string; payload?: unknown; dryRun?: boolean; expectedVersion?: string }
export interface PluginImportResult {
  dryRun: boolean; changed: boolean; report: unknown;
  /** Every image the import references / the ones new to the asset manifest. */
  assetIds: string[]; addedAssetIds: string[];
  /** Plugin documents created or replaced (project-relative files). */
  files: string[];
}
const MAX_IMAGES = 1024, MAX_IMAGE_BYTES = 256 * 1024 * 1024, MAX_DOCUMENTS = 32;
const ID = /^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/;
const isRecord = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === 'object' && !Array.isArray(value);
const inside = (root: string, file: string) => { const relative = path.relative(root, file); return !relative.startsWith('..') && !path.isAbsolute(relative); };
/** Same content-hash ids as importPng: a re-imported unchanged picture is the same asset. */
export const imageAssetId = (bytes: Uint8Array) => `image-${createHash('sha256').update(bytes).digest('hex').slice(0, 24)}`;

/**
 * Runs the plugin's import hook on the draft at `revision` and applies its result as ONE batch
 * edit (asset manifest + the plugin's documents), so a single undo removes the whole import.
 * Image files are published first under content-hash names (immutable, never overwritten; an
 * undone import only leaves unreferenced files behind, like importPng). The revision and draft
 * are re-checked after every await: an edit made meanwhile refuses the import instead of being
 * overwritten. `dryRun` returns the plugin's report (preview) and changes nothing.
 */
export async function importPluginContent(editor: StudioEditor, input: PluginImportInput, revision: number): Promise<PluginImportResult> {
  editor.current(revision);
  const dryRun = input?.dryRun === true;
  if (!editor.root) throw new Error('프로젝트를 열어 주세요.');
  if (!dryRun && editor.readOnly) throw new Error('편집 가능한 프로젝트를 열어 주세요.');
  if (typeof input?.plugin !== 'string' || !new RegExp(PLUGIN_ID_PATTERN).test(input.plugin)) throw new Error('가져올 플러그인 ID를 지정하세요.');
  if (typeof input.op !== 'string' || !/^[a-zA-Z][a-zA-Z0-9]{0,63}$/.test(input.op)) throw new Error('잘못된 가져오기 명령입니다.');
  if (input.payload !== undefined && !isRecord(input.payload)) throw new Error('가져오기 인자는 객체여야 합니다.');
  const root = await realpath(editor.root), draft = editor.draftVersion();
  const current = () => {
    editor.current(revision);
    if (editor.draftVersion() !== draft) throw new Error('편집 내용이 갱신되었습니다. 다시 시도해 주세요.');
    if (input.expectedVersion !== undefined && input.expectedVersion !== editor.diskVersion()) throw new Error('프로젝트 버전이 변경되었습니다. 다시 불러오세요.');
  };
  current();
  const lock = editor.documents.find(d => d.kind === 'pluginLock')?.data ?? null;
  const { plugin, service } = await projectPluginService(editor.pluginStore, lock, input.plugin);
  if (!service?.prepareImport) throw new PluginError('PLUGIN_SERVICE', `플러그인 '${input.plugin}'에는 가져오기 기능이 없습니다.`);
  current();
  const manifestDoc = editor.documents.find(d => d.kind === 'assets');
  const manifestData = manifestDoc?.data as { assets?: { id: string; kind: string; path: string }[] } | undefined;
  const projectAssets = Array.isArray(manifestData?.assets) ? manifestData!.assets : [];
  const assets = projectAssets.filter(isRecord).map(a => {
    const absolute = path.resolve(root, String(a.path));
    return { id: String(a.id), kind: String(a.kind), path: String(a.path), absolutePath: inside(root, absolute) ? absolute : null };
  });
  const own = editor.documents.filter(d => d.kind === 'extension' && isRecord(d.data) && d.data.plugin === plugin.id && isRecord(d.data) && typeof d.data.type === 'string' && typeof d.data.id === 'string');
  const documents = own.map(d => { const e = d.data as Record<string, Json>; return { file: d.file, type: String(e.type), id: String(e.id), data: structuredClone(e.data) }; });
  let raw: unknown;
  try { raw = await service.prepareImport({ op: input.op, payload: structuredClone(input.payload ?? {}), dryRun, documents, assets }); }
  catch (error) { throw new PluginError((error as { code?: string })?.code ?? 'PLUGIN_SERVICE_ERROR', error instanceof Error ? error.message : String(error)); }
  current();

  // The hook's result is checked like any outside input: known types, plugin-owned files only,
  // images whose ids are their content hashes and that decode as bounded PNGs.
  const bad = (why: string): never => { throw new PluginError('PLUGIN_SERVICE_ERROR', `플러그인 '${plugin.id}'의 가져오기 결과가 올바르지 않습니다: ${why}`); };
  if (!isRecord(raw)) bad('객체가 아닙니다');
  const result = raw as Record<string, unknown>;
  const types = new Set(plugin.manifest!.documentTypes.map(t => t.type));
  const outDocs = result.documents ?? [], outImages = result.images ?? [];
  if (!Array.isArray(outDocs) || outDocs.length > MAX_DOCUMENTS) bad(`documents는 ${MAX_DOCUMENTS}개 이하 배열입니다`);
  if (!Array.isArray(outImages) || outImages.length > MAX_IMAGES) bad(`images는 ${MAX_IMAGES}개 이하 배열입니다`);
  const docs: { type: string; id: string; data: Json }[] = [], keys = new Set<string>();
  for (const d of outDocs as unknown[]) {
    if (!isRecord(d) || typeof d.type !== 'string' || !types.has(d.type) || typeof d.id !== 'string' || !ID.test(d.id) || d.id.length > 64 || !isRecord(d.data)) bad('문서 type/id/data');
    const doc = d as { type: string; id: string; data: Json };
    if (keys.has(`${doc.type}/${doc.id}`)) bad(`중복 문서 ${doc.type}/${doc.id}`);
    keys.add(`${doc.type}/${doc.id}`);
    docs.push(doc);
  }
  const images: { id: string; bytes: Buffer }[] = [];
  let total = 0;
  for (const item of outImages as unknown[]) {
    if (!isRecord(item) || typeof item.id !== 'string' || !(item.bytes instanceof Uint8Array)) bad('이미지 id/bytes');
    const image = item as { id: string; bytes: Uint8Array }, bytes = Buffer.from(image.bytes);
    if (imageAssetId(bytes) !== image.id) bad(`이미지 ID '${image.id}'가 내용 해시와 다릅니다`);
    total += bytes.length;
    if (total > MAX_IMAGE_BYTES) bad('이미지 전체 크기가 256 MiB를 넘습니다');
    if (!images.some(i => i.id === image.id)) images.push({ id: image.id, bytes });
  }
  const assetIds = images.map(i => i.id);
  if (dryRun) return { dryRun: true, changed: false, report: result.report ?? null, assetIds, addedAssetIds: [], files: [] };

  for (const image of images) decodePng(image.bytes);  // throws on a malformed / oversized PNG
  if (!manifestDoc || !manifestData || !Array.isArray(manifestData.assets)) throw new Error('에셋 목록이 없습니다.');
  const additions: { id: string; kind: 'image'; path: string }[] = [];
  for (const image of images) {
    const existing = projectAssets.find(a => a.id === image.id);
    if (existing) { if (existing.kind !== 'image') throw new Error(`이미 사용 중인 에셋 ID입니다: ${image.id}`); continue; }
    additions.push({ id: image.id, kind: 'image', path: `assets/${image.id}.png` });
  }
  const edits: PrimitiveEdit[] = [], files: string[] = [];
  for (const doc of docs) {
    const file = `plugins/${plugin.id}/${doc.type}/${doc.id}.json`;
    const existing = editor.documents.find(d => d.file === file);
    if (existing && existing.kind !== 'extension') bad(`플러그인 문서가 아닌 파일 ${file}`);
    if (existing && JSON.stringify((existing.data as Record<string, Json>).data) === JSON.stringify(doc.data)) continue;
    const envelope = existing ? { ...(existing.data as Record<string, Json>), data: doc.data } : { formatVersion: '0.1.0', plugin: plugin.id, type: doc.type, id: doc.id, data: doc.data };
    edits.push(existing ? { type: 'replace', file, data: envelope } : { type: 'create', file, data: envelope });
    files.push(file);
  }
  if (additions.length) edits.unshift({ type: 'replace', file: manifestDoc.file, data: { ...manifestData, assets: [...manifestData.assets, ...additions] } as Json });
  if (edits.length > MAX_EDIT_BATCH) throw new Error(`가져오기 변경이 ${MAX_EDIT_BATCH}개를 넘습니다.`);
  if (!edits.length) return { dryRun: false, changed: false, report: result.report ?? null, assetIds, addedAssetIds: [], files };
  if (additions.length) {
    const directory = await realpath(path.join(root, 'assets'));
    if (!inside(root, directory)) throw new Error('프로젝트 밖의 에셋 경로입니다.');
    for (const image of images) if (additions.some(a => a.id === image.id)) await publishImmutableAsset(directory, `${image.id}.png`, image.bytes);
  }
  // Async decoding/storage cannot overwrite a newer draft or an externally changed project.
  await editor.assertDiskUnchanged(); current();
  editor.edit({ type: 'batch', edits }, revision);
  return { dryRun: false, changed: true, report: result.report ?? null, assetIds, addedAssetIds: additions.map(a => a.id), files };
}

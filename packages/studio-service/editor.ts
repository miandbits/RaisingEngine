import { readFile, realpath } from 'node:fs/promises';
import type { StudioTemplate } from './templates.js';
import path from 'node:path';
import { documentSizeMessage, loadProject, maxDocumentBytes } from '../project-service/loader.js';
import { parseDocument } from '../project-service/json-document.js';
import { buildSnapshot } from '../project-service/snapshot.js';
import { bytesOrNull, commitProject, EXTENSION_FILE, fileHash, PLUGIN_LOCK_FILE, recoverProject, safeFile } from '../project-service/transaction.js';
import { exportBundle, type PreparedExtensions } from '../plugin-host/bundle.js';
import { pluginStatus, type PluginStatus } from '../plugin-host/resolve.js';
import type { ContentSnapshot } from '../core/model.js';
import { ProjectConflict, projectVersion } from '../project-service/version.js';
import { validateProject } from '../validation/index.js';
import { releaseUnsavedAssets } from './unsaved-assets.js';
import type { Diagnostic, DocumentKind, Json, LoadedProject, ProjectDocument } from '../validation/model.js';
import type { ExportTarget } from '../export-service/model.js';

export interface DraftDocument { file: string; kind: DocumentKind; data: Json }
export interface StudioView {
  revision: number; root: string | null; documents: DraftDocument[]; dirty: boolean;
  draftVersion: string; diskVersion: string | null;
  undo: boolean; redo: boolean; diagnostics: Diagnostic[]; readOnly: boolean;
}
export interface StudioReply { view: StudioView; error: string | null; notice: string | null }
export type PrimitiveEdit = { type: 'replace'; file: string; data: Json } | { type: 'add'; kind: 'activity' | 'event' | 'ending' | 'character'; id: string }
  | { type: 'delete'; file: string }
  /** Plugin documents only (plugins/lock.json, plugins/<plugin>/<type>/<id>.json); fails if the draft already has the file. */
  | { type: 'create'; file: string; data: Json }
  | { type: 'importScenario'; id: string; data: Json };
export const MAX_EDIT_BATCH = 100;
export type Edit = PrimitiveEdit | { type: 'undo' } | { type: 'redo' }
  | { type: 'batch'; edits: PrimitiveEdit[] };
export interface StudioBridge {
  view(): Promise<StudioReply>; edit(edit: Edit, revision: number): Promise<StudioReply>;
  open(): Promise<StudioReply>; create(template?: StudioTemplate): Promise<StudioReply>; reload(revision: number): Promise<StudioReply>;
  validate(revision: number): Promise<StudioReply>; save(revision: number): Promise<StudioReply>;
  preview(revision: number): Promise<StudioReply>;
  importImage(revision: number): Promise<StudioReply>;
  exportGame(target: ExportTarget, revision: number): Promise<StudioReply>;
}
const serialize = (data: Json) => JSON.stringify(data, null, 2) + '\n';
/*
 * Draft documents are immutable values: an edit builds a new array with new objects for the
 * documents it changes and shares the others with the previous revision (history keeps whole
 * arrays). Per-object caches below therefore never go stale, and a large plugin document that an
 * edit did not touch is never cloned, re-serialized or re-parsed again (the round trip of one rig
 * edit used to clone, stringify and re-parse every document several times).
 */
const keys = new WeakMap<DraftDocument, string>();
const texts = new WeakMap<DraftDocument, string>();
const versions = new WeakMap<DraftDocument[], string>();
const keyOf = (doc: DraftDocument) => { let key = keys.get(doc); if (key === undefined) keys.set(doc, key = JSON.stringify(doc)); return key; };
/** JSON.stringify(documents), assembled from cached per-document parts (same text). */
const sourceKey = (documents: DraftDocument[]) => '[' + documents.map(keyOf).join(',') + ']';
const locationsOf = new WeakMap<DraftDocument, ProjectDocument['locations']>();
const textOf = (doc: DraftDocument) => { let text = texts.get(doc); if (text === undefined) texts.set(doc, text = serialize(doc.data)); return text; };
export interface StudioEditorOptions { /** Plugin store directory; default $RAISING_PLUGIN_STORE or <engine>/plugin-store. */ pluginStore?: string | null | undefined }
export class StudioEditor {
  root: string | null = null;
  readonly pluginStore: string | null;
  documents: DraftDocument[] = [];
  revision = 0;
  readOnly = false;
  diagnostics: Diagnostic[] = [];
  private original = '';
  private baseline = new Map<string, string>();
  private originalDocuments = new Map<string, string>();
  private loaderDiagnostics: Diagnostic[] = [];
  private history: DraftDocument[][] = [];
  private future: DraftDocument[][] = [];
  constructor(options: StudioEditorOptions = {}) { this.pluginStore = options.pluginStore ?? null; }

  diskVersion(): string {
    if (!this.root) throw new Error('프로젝트를 열어 주세요.');
    return projectVersion(this.root, this.baseline);
  }
  fileVersions(): Record<string, string> { return Object.fromEntries(this.baseline); }

  async assertDiskUnchanged(ownsLock = false): Promise<void> {
    if (!this.root) throw new Error('프로젝트를 열어 주세요.');
    const checkLock = async () => {
      if (!ownsLock && await bytesOrNull(path.join(this.root!, '.studio/pending-save.json')) !== null)
        throw Object.assign(new Error('다른 저장 작업 또는 복구 기록이 있습니다. 완료 후 다시 시도하세요.'), { code: 'PROJECT_BUSY' });
    };
    await checkLock();
    const disk = await loadProject(this.root);
    if (disk.diagnostics.some(d => d.severity === 'error')) throw Object.assign(new Error('외부 파일을 읽을 수 없습니다. 다시 불러온 뒤 확인해 주세요.'), { code: 'PROJECT_READ_ERROR' });
    const actual = new Map<string, string>();
    for (const doc of disk.documents) {
      const bytes = await readFile(await safeFile(this.root, doc.file));
      if (fileHash(new TextDecoder('utf-8', { fatal: true }).decode(bytes)) !== doc.version)
        throw Object.assign(new Error(`읽는 동안 변경되었습니다: ${doc.file}`), { code: 'PROJECT_BUSY' });
      actual.set(doc.file, fileHash(bytes));
    }
    await checkLock();
    const files = [...new Set([...this.baseline.keys(), ...actual.keys()])].filter(file => this.baseline.get(file) !== actual.get(file));
    if (files.length) throw new ProjectConflict(this.diskVersion(), projectVersion(this.root, actual), files);
  }

  draftVersion(): string {
    let version = versions.get(this.documents);
    if (version === undefined) versions.set(this.documents, version = `draft-v1:${fileHash(sourceKey(this.documents))}`);
    return version;
  }
  /** `shared`: the reply is serialized at once (Qt stdio protocol), so documents are not cloned. */
  view(options: { shared?: boolean } = {}): StudioView {
    // `documents` is cloned when first read: replies that are never inspected (edit/validate
    // return values the Qt service discards) cost nothing, and the captured array never changes.
    const documents = this.documents;
    if (options.shared) return { revision: this.revision, root: this.root, documents, draftVersion: this.draftVersion(), diskVersion: this.root ? this.diskVersion() : null,
      dirty: sourceKey(this.documents) !== this.original, undo: !!this.history.length, redo: !!this.future.length, diagnostics: this.diagnostics, readOnly: this.readOnly };
    let cloned: DraftDocument[] | null = null;
    const view = { revision: this.revision, root: this.root, documents, draftVersion: this.draftVersion(), diskVersion: this.root ? this.diskVersion() : null,
      dirty: sourceKey(this.documents) !== this.original, undo: !!this.history.length, redo: !!this.future.length,
      diagnostics: this.diagnostics, readOnly: this.readOnly } as StudioView;
    Object.defineProperty(view, 'documents', { enumerable: true, configurable: true, get: () => cloned ??= structuredClone(documents),
      set: (value: DraftDocument[]) => { cloned = value; } });
    return view;
  }
  reply(error: string | null = null, notice: string | null = null, options: { shared?: boolean } = {}): StudioReply { return { view: this.view(options), error, notice }; }
  current(revision: unknown): void { if (revision !== this.revision) throw new Error('편집 내용이 갱신되었습니다. 다시 시도해 주세요.'); }

  async open(root: string, readOnly = false): Promise<StudioReply> {
    const resolved = await realpath(root);
    const recovered = readOnly ? false : await recoverProject(resolved);
    const loaded = await loadProject(resolved);
    if (!loaded.documents.some(d => d.kind === 'project')) throw new Error('project.json을 읽을 수 없습니다. 기존 프로젝트를 유지합니다.');
    const baseline = new Map<string, string>();
    for (const doc of loaded.documents) {
      const bytes = await readFile(path.join(resolved, doc.file));
      if (fileHash(new TextDecoder('utf-8', { fatal: true }).decode(bytes)) !== doc.version) throw new Error(`읽는 동안 변경되었습니다: ${doc.file}`);
      baseline.set(doc.file, fileHash(bytes));
    }
    // The draft being replaced is discarded: its unsaved imported files go (see unsaved-assets.ts),
    // and so do the leftovers of editors that ended without closing (a crash, a killed process).
    if (this.root && !this.readOnly) await releaseUnsavedAssets(this.root).catch(() => []);
    if (!readOnly && this.root !== resolved) await releaseUnsavedAssets(resolved).catch(() => []);
    this.root = resolved; this.readOnly = readOnly;
    this.documents = loaded.documents.map(({ file, kind, data }) => ({ file, kind, data }));
    this.original = sourceKey(this.documents); this.baseline = baseline;
    this.originalDocuments = new Map(this.documents.map(d => [d.file, serialize(d.data)]));
    this.history = []; this.future = []; this.loaderDiagnostics = loaded.diagnostics;
    this.revision++;
    await this.validate(this.revision);
    return this.reply(null, recovered ? '중단된 저장을 이전 상태로 복구했습니다.' : null);
  }

  /** Ends the session: the saved project stays, files imported only for the unsaved draft are removed. Returns them. */
  async close(): Promise<string[]> {
    const root = this.root, readOnly = this.readOnly;
    if (!root || readOnly) return [];
    return releaseUnsavedAssets(root).catch(() => []);
  }

  edit(edit: Edit, revision: number): StudioReply {
    this.current(revision);
    const next = [...this.documents];  // documents are replaced, never mutated (see keys above)
    if (edit.type === 'undo' || edit.type === 'redo') {
      const from = edit.type === 'undo' ? this.history : this.future;
      const to = edit.type === 'undo' ? this.future : this.history;
      const previous = from.pop();
      if (previous) { to.push(this.documents); this.documents = previous; this.revision++; this.diagnostics = []; }
      return this.reply();
    }
    const edits = edit.type === 'batch' ? edit.edits : [edit];
    if (!Array.isArray(edits) || edits.length < 1 || edits.length > MAX_EDIT_BATCH)
      throw new Error(`일괄 편집은 1..${MAX_EDIT_BATCH}개 변경이어야 합니다.`);
    // Apply only to the detached candidate. History, revision and diagnostics
    // remain untouched until every primitive succeeds.
    for (const primitive of edits) {
      if (!primitive || typeof primitive !== 'object' || !['replace', 'add', 'delete', 'importScenario', 'create'].includes(primitive.type))
        throw new Error('일괄 편집에는 추가·수정·삭제·생성·시나리오 가져오기만 사용할 수 있습니다.');
      const edit = primitive;
      if (edit.type === 'replace') {
        const at = next.findIndex(d => d.file === edit.file);
        const doc = next[at];
        if (!doc) throw new Error('문서를 찾을 수 없습니다.');
        const text = serialize(edit.data);
        if (Buffer.byteLength(text) > maxDocumentBytes(doc.kind)) throw new Error(documentSizeMessage(doc.kind));
        const parsed = parseDocument(doc.file, doc.kind, text, { serialized: true });
        if (!parsed.document) throw new Error('문서 구조를 읽을 수 없습니다.');
        const replaced = { ...doc, data: parsed.document.data };
        texts.set(replaced, text);
        next[at] = replaced;
      } else if (edit.type === 'add') {
        if (!['activity', 'event', 'ending', 'character'].includes(edit.kind) || !/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(edit.id) || edit.id.length > 64) throw new Error('ID는 소문자 영문·숫자·하이픈으로 입력하세요.');
        const folders = { activity: 'activities', event: 'events', ending: 'endings', character: 'characters' };
        const file = `${folders[edit.kind]}/${edit.id}.json`;
        if (next.some(d => d.file === file || d.kind === edit.kind && (d.data as { id?: string })?.id === edit.id)) throw new Error('이미 존재하는 ID입니다.');
        const data: Record<string, Json> = { formatVersion: '0.1.0', id: edit.id, label: '새 콘텐츠' };
        if (edit.kind === 'character') { delete data.label; Object.assign(data, { name: '새 인물', portraits: {}, relationships: [] }); }
        else {
          data.requirements = { op: 'always' };
          if (edit.kind === 'activity') Object.assign(data, { costs: [], baseEffects: [] });
          else {
            Object.assign(data, { priority: 0, entryNodeId: 'intro', nodes: [
              { id: 'intro', type: 'dialogue', speakerId: null, text: '새로운 이야기를 입력하세요.', next: 'end' }, { id: 'end', type: 'end' }] });
            if (edit.kind === 'event') data.window = { fromPeriod: 0, throughPeriod: 0 };
          }
        }
        next.push({ file, kind: edit.kind, data });
      } else if (edit.type === 'importScenario') {
        if (!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(edit.id) || edit.id.length > 64) throw new Error('시나리오 ID 형식이 올바르지 않습니다.');
        const file = `scenarios/${edit.id}.json`;
        if (next.some(d => d.file === file || d.kind === 'scenario' && (d.data as { id?: string }).id === edit.id)) throw new Error('이미 존재하는 시나리오입니다.');
        if ((edit.data as { id?: string })?.id !== edit.id || Buffer.byteLength(serialize(edit.data)) > 2 * 1024 * 1024) throw new Error('시나리오 데이터가 올바르지 않습니다.');
        next.push({ file, kind: 'scenario', data: structuredClone(edit.data) });
      } else if (edit.type === 'create') {
        // Plugin documents have no engine template: the caller supplies the whole envelope.
        const kind = edit.file === PLUGIN_LOCK_FILE ? 'pluginLock' : typeof edit.file === 'string' && EXTENSION_FILE.test(edit.file) ? 'extension' : null;
        if (!kind) throw new Error('create는 plugins/lock.json 또는 plugins/<플러그인>/<종류>/<ID>.json 문서에만 사용할 수 있습니다.');
        if (next.some(d => d.file.toLowerCase() === edit.file.toLowerCase())) throw new Error(`이미 있는 문서입니다: ${edit.file}`);
        const text = serialize(edit.data);
        if (Buffer.byteLength(text) > maxDocumentBytes(kind)) throw new Error(documentSizeMessage(kind));
        const parsed = parseDocument(edit.file, kind, text, { serialized: true });
        if (!parsed.document) throw new Error('문서 구조를 읽을 수 없습니다.');
        const created: DraftDocument = { file: edit.file, kind, data: parsed.document.data };
        texts.set(created, text);
        next.push(created);
      } else if (edit.type === 'delete') {
        const at = next.findIndex(d => d.file === edit.file && ['activity', 'event', 'ending', 'character', 'scenario', 'extension', 'pluginLock'].includes(d.kind));
        if (at < 0) throw new Error('이 문서는 삭제할 수 없습니다.');
        next.splice(at, 1);
      } else throw new Error('알 수 없는 편집 명령입니다.');
    }
    if (sourceKey(next) === sourceKey(this.documents)) return this.reply();
    this.history.push(this.documents); if (this.history.length > 100) this.history.shift();
    this.future = []; this.documents = next; this.revision++; this.diagnostics = [];
    return this.reply();
  }

  loaded(): LoadedProject {
    if (!this.root) throw new Error('프로젝트를 열어 주세요.');
    const loaded: LoadedProject = { root: this.root, documents: [], diagnostics: [...this.loaderDiagnostics], ...(this.pluginStore ? { pluginStore: this.pluginStore } : {}) };
    for (const doc of this.documents) {
      // Fresh data per validation (validators get their own copy), text and locations cached/lazy.
      const parsed = parseDocument(doc.file, doc.kind, textOf(doc), { serialized: true });
      if (parsed.document) {
        // One (lazy) location map per document value: diagnostics of an untouched large document
        // never rebuild it on the next validation.
        const locations = locationsOf.get(doc) ?? parsed.document.locations;
        locationsOf.set(doc, locations);
        loaded.documents.push({ ...parsed.document, locations });
      }
      loaded.diagnostics.push(...parsed.diagnostics);
    }
    return loaded;
  }
  async validate(revision: number): Promise<StudioReply> {
    this.current(revision);
    const result = await validateProject(this.loaded());
    this.current(revision);
    this.diagnostics = result.diagnostics;
    return this.reply(null, result.valid ? '검증을 통과했습니다.' : '진단 목록에서 수정할 항목을 확인하세요.');
  }
  async snapshot(revision: number) {
    this.current(revision);
    const result = await buildSnapshot(this.loaded());
    this.current(revision);
    this.diagnostics = result.diagnostics;
    return result;
  }
  /** Store contents and this draft's plugin resolution (shared by Qt and MCP). */
  async plugins(): Promise<PluginStatus> {
    const lock = this.documents.find(d => d.kind === 'pluginLock')?.data ?? null;
    const used = this.documents.filter(d => d.kind === 'extension').map(d => String((d.data as { plugin?: unknown })?.plugin ?? ''));
    return pluginStatus(this.pluginStore, lock, used);
  }
  /** Runtime modules + documents for an export/preview of `content` (a snapshot of this draft at `revision`).
   *  `hostFeatures`: the target player's capabilities.json visualFeatures, so plugins can prepare
   *  what it draws (cutout packs atlases only for players with plugin-atlas-v1); absent = unknown. */
  async exportExtensions(revision: number, content: ContentSnapshot, target: 'windows' | 'web', hostFeatures?: readonly string[]): Promise<PreparedExtensions | null> {
    this.current(revision);
    const result = await exportBundle({ store: this.pluginStore, lock: this.documents.find(d => d.kind === 'pluginLock')?.data ?? null,
      documents: structuredClone(this.documents), visual: content.project.visual, target,
      assets: { root: this.root, assets: content.assets, hashes: content.assetHashes }, ...(hostFeatures ? { hostFeatures } : {}) });
    this.current(revision);
    return result;
  }
  async save(revision: number): Promise<StudioReply> {
    this.current(revision);
    if (!this.root || this.readOnly) throw new Error('기본 샘플은 읽기 전용입니다. 새 프로젝트로 복사한 뒤 저장하세요.');
    const candidate = this.loaded();
    const validation = await validateProject(candidate);
    this.diagnostics = validation.diagnostics;
    // Structural errors cannot be written. Semantic errors are allowed in an explicit draft save.
    if (candidate.diagnostics.some(d => d.severity === 'error') || validation.structuredDocuments.length !== this.documents.length) throw new Error('구조 오류를 먼저 수정해 주세요. 의미 오류가 있는 초안은 저장할 수 있습니다.');
    await this.assertDiskUnchanged();
    const all = new Set([...this.baseline.keys(), ...this.documents.map(d => d.file)]);
    const changes = [...all].flatMap(file => {
      const doc = this.documents.find(d => d.file === file);
      const text = doc ? serialize(doc.data) : null;
      const expected = this.baseline.get(file) ?? null;
      if (text !== null && this.originalDocuments.get(file) === text) return [];
      return text !== null && fileHash(text) === expected ? [] : [{ file, expected, text }];
    });
    await commitProject(this.root, changes, async () => {
      this.current(revision);
      await this.assertDiskUnchanged(true);
    });
    for (const change of changes) {
      if (change.text === null) this.baseline.delete(change.file);
      else this.baseline.set(change.file, fileHash(change.text));
    }
    this.originalDocuments = new Map(this.documents.map(d => [d.file, serialize(d.data)]));
    this.original = sourceKey(this.documents); this.revision++;
    return this.reply(null, this.diagnostics.some(d => d.severity === 'error') ? '초안을 저장했습니다. 실행 전 의미 오류를 수정하세요.' : '프로젝트를 저장했습니다.');
  }
}

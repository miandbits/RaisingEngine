import { AnalysisJobs } from '../analysis-service/jobs.js';
import { buildCatalog, searchCatalog, buildRelations, relationNeighborhood, type CatalogQuery } from '../analysis-service/catalog.js';
import type { AnalysisOptions } from '../analysis-service/model.js';
import { replayTrace } from '../analysis-service/runner.js';
import { traceScenario } from '../analysis-service/scenario.js';
import { readFile } from 'node:fs/promises';
import { StudioEditor, type Edit } from '../studio-service/editor.js';
import { importSprites, type SpriteImportInput } from '../studio-service/sprites.js';
import { ProjectConflict } from '../project-service/version.js';
import { runScenario } from '../runtime-service/scenario.js';
import { createSession, dispatchSession } from '../runtime-service/session.js';
import { evaluateCondition, evaluateNumber } from '../core/evaluation.js';
import { inspectEvents } from '../core/narrative.js';
import type { ContentSnapshot, RunState } from '../core/model.js';
import { callProjectPlugin, resolveProjectPlugins } from '../plugin-host/resolve.js';
import { readDocumentSchema } from '../plugin-host/service.js';
import { pluginStorePath } from '../plugin-host/store.js';

export class ToolFault extends Error {
  constructor(readonly code: string, message: string, readonly details: Record<string, unknown> = {}) { super(message); }
}

/** One on-disk project. A call owns its private draft; rejected batches never leak edits. */
export class McpWorkspace {
  private jobs: AnalysisJobs;
  readonly pluginStore: string;
  constructor(readonly root: string, readonly readOnly = false, workerUrl?: URL, pluginStore?: string) {
    this.jobs = new AnalysisJobs(workerUrl); this.pluginStore = pluginStorePath(pluginStore);
  }

  async importSprites(expectedVersion:string,input:SpriteImportInput){
    if(this.readOnly)throw new ToolFault('READ_ONLY','이 MCP 서버는 읽기 전용입니다.');
    const editor=await this.open(expectedVersion),before=editor.fileVersions();editor.readOnly=false;
    try{
      const spriteImport=await importSprites(editor,{...input,expectedVersion},editor.revision);
      await editor.validate(editor.revision);
      if(editor.diagnostics.some(d=>d.severity==='error'))throw new ToolFault('VALIDATION_FAILED','검증 오류가 있어 저장하지 않았습니다.',{diagnostics:editor.diagnostics});
      await editor.save(editor.revision);
      const after=editor.fileVersions();
      return {ok:true,previousVersion:expectedVersion,version:editor.diskVersion(),spriteImport,
        changes:Object.keys(after).filter(file=>before[file]!==after[file]).map(file=>({file,before:before[file]??null,after:after[file]})),diagnostics:editor.diagnostics};
    }catch(error){if(error instanceof ProjectConflict||error instanceof ToolFault||(error as NodeJS.ErrnoException).code)throw error;throw new ToolFault('CHANGE_REJECTED',error instanceof Error?error.message:String(error));}
  }

  async searchContent(expectedVersion: string, query: CatalogQuery, offset = 0, limit = 100) {
    const editor = await this.open(expectedVersion); const found = searchCatalog(buildCatalog(editor.documents, editor.diagnostics), query);
    return { ok: true, version: editor.diskVersion(), total: found.length, items: found.slice(offset, offset + limit).map(({ data, ...item }) => item) };
  }
  async relationships(expectedVersion: string, key: string, depth = 1) {
    const editor = await this.open(expectedVersion); const items = buildCatalog(editor.documents, editor.diagnostics);
    if (!items.some(i => i.key === key)) throw new ToolFault('INVALID_ARGUMENT', '콘텐츠 검색 결과의 key를 사용하세요.');
    return { ok: true, version: editor.diskVersion(), ...relationNeighborhood(buildRelations(items), key, depth) };
  }
  async startAnalysis(expectedVersion: string, options: AnalysisOptions) {
    if (this.readOnly) throw new ToolFault('READ_ONLY', '분석 결과 기록에는 쓰기 가능한 프로젝트가 필요합니다.');
    const editor = await this.open(expectedVersion); const snapshot = await editor.snapshot(editor.revision);
    if (!snapshot.ok) throw new ToolFault('VALIDATION_FAILED', '콘텐츠 검증 오류를 먼저 수정하세요.');
    await editor.assertDiskUnchanged();
    return { ok: true, job: await this.jobs.start(this.root, snapshot.snapshot, expectedVersion, null, options) };
  }
  async analysisStatus(id?: string) {
    const editor = await this.open();
    const summary = (job: import('../analysis-service/model.js').AnalysisJob) => ({ ...job, result: { ...job.result, traces: job.result.traces.map(({ commands, state, ...trace }) => ({ ...trace, commandCount: commands.length, phase: state.phase, endingId: state.endingId })) } });
    if (id) { const job = await this.jobs.get(this.root, id); return { ok: true, job: summary(job), stale: job.sourceVersion !== (job.sourceVersion.startsWith('draft-v1:') ? editor.draftVersion() : editor.diskVersion()) }; }
    const jobs = await this.jobs.list(this.root);
    return { ok: true, jobs: jobs.slice(0,20).map(summary), total: jobs.length, version: editor.diskVersion() };
  }
  async cancelAnalysis(id: string) { if (this.readOnly) throw new ToolFault('READ_ONLY', '읽기 전용 서버입니다.'); return { ok: true, job: await this.jobs.cancel(this.root, id) }; }
  async replayAnalysis(id: string, traceIndex: number, steps?: number) {
    const stored = await this.jobs.stored(this.root, id); const trace = stored.job.result.traces[traceIndex]; if (!trace) throw new ToolFault('INVALID_ARGUMENT', '재현 기록이 없습니다.');
    const result = replayTrace(stored.content, trace, steps);
    return { ok: true, fingerprint: stored.job.fingerprint, seed: trace.seed, commands: trace.commands, state: result.state };
  }
  async exportAnalysis(expectedVersion: string, id: string, traceIndex: number, scenarioId: string) {
    const editor = await this.open(expectedVersion); const snapshot = await editor.snapshot(editor.revision); const stored = await this.jobs.stored(this.root, id);
    if (!snapshot.ok || snapshot.snapshot.contentFingerprint !== stored.job.fingerprint) throw new ToolFault('VERSION_CONFLICT', '현재 콘텐츠에서 다시 검사하세요.');
    const trace = stored.job.result.traces[traceIndex]; if (!trace) throw new ToolFault('INVALID_ARGUMENT', '재현 기록이 없습니다.');
    const data = await traceScenario(stored.content, trace, scenarioId);
    return this.apply(expectedVersion, [{ type: 'importScenario', id: scenarioId, data }], '검증한 자동 플레이 경로를 회귀 시나리오로 등록');
  }

  private async open(expectedVersion?: string): Promise<StudioEditor> {
    const editor = new StudioEditor({ pluginStore: this.pluginStore });
    // Reads must not perform recovery. Startup/write recovery is an explicit host concern.
    await editor.open(this.root, true);
    await editor.assertDiskUnchanged();
    if (expectedVersion !== undefined && editor.diskVersion() !== expectedVersion)
      throw new ToolFault('VERSION_CONFLICT', '프로젝트가 변경되었습니다. 다시 조회하고 변경 내용을 검토하세요.', {
        expectedVersion, actualVersion: editor.diskVersion(), fileVersions: editor.fileVersions(),
      });
    return editor;
  }

  async readProject() {
    const editor = await this.open();
    const versions = editor.fileVersions();
    return { ok: true, version: editor.diskVersion(), readOnly: this.readOnly,
      documents: editor.documents.map(d => ({ file: d.file, kind: d.kind, version: versions[d.file],
        id: typeof d.data === 'object' && d.data !== null && !Array.isArray(d.data) ? d.data.id ?? null : null })),
      diagnostics: editor.diagnostics };
  }

  async readDocument(file: string, expectedVersion: string) {
    const editor = await this.open(expectedVersion);
    const doc = editor.documents.find(d => d.file === file);
    if (!doc) throw new ToolFault('DOCUMENT_NOT_FOUND', `등록된 문서가 아닙니다: ${file}`);
    return { ok: true, version: editor.diskVersion(), fileVersion: editor.fileVersions()[file], document: doc };
  }

  async readSchema(name: string, plugin?: string, type?: string) {
    const allowed = ['common', 'project', 'attributes', 'flags', 'character', 'activity', 'event', 'ending', 'assets', 'scenario', 'extension', 'plugin-lock'];
    if (!allowed.includes(name)) throw new ToolFault('INVALID_ARGUMENT', '알 수 없는 스키마입니다.');
    if (name === 'extension' && (plugin !== undefined || type !== undefined)) {
      // The `data` schema of a plugin document type, from the version this project pins.
      if (!plugin || !type) throw new ToolFault('INVALID_ARGUMENT', 'plugin과 type을 함께 지정하세요.');
      const editor = await this.open();
      const lock = editor.documents.find(d => d.kind === 'pluginLock')?.data ?? null;
      const resolved = (await resolveProjectPlugins(this.pluginStore, lock)).find(p => p.id === plugin);
      if (!resolved || resolved.state !== 'active') throw new ToolFault(resolved?.diagnostics[0]?.code ?? 'PLUGIN_NOT_LOCKED', resolved?.diagnostics[0]?.message ?? `플러그인 '${plugin}'가 이 프로젝트의 잠금 파일에 없습니다.`);
      try { return { ok: true, name, plugin, version: resolved.version, type, envelope: 'extension', schema: await readDocumentSchema({ directory: resolved.directory!, manifest: resolved.manifest! }, type) }; }
      catch (error) { throw new ToolFault((error as { code?: string }).code ?? 'PLUGIN_ERROR', error instanceof Error ? error.message : String(error)); }
    }
    const schema: unknown = JSON.parse(await readFile(new URL(`../schema/${name}.schema.json`, import.meta.url), 'utf8'));
    return { ok: true, name, schema };
  }

  /** Installed plugins and this project's lock resolution (same view as Qt replies' `plugins`). */
  async pluginStatus() {
    const editor = await this.open();
    return { ok: true, version: editor.diskVersion(), ...await editor.plugins() };
  }
  /** Read-only plugin service op (e.g. cutout `sample`, `validate`, `template`, `edit` previews). Results are applied with apply_project_changes. */
  async pluginCall(expectedVersion: string, plugin: string, op: string, payload: unknown) {
    const editor = await this.open(expectedVersion);
    const lock = editor.documents.find(d => d.kind === 'pluginLock')?.data ?? null;
    try { return { ok: true, version: editor.diskVersion(), plugin, op, result: await callProjectPlugin(this.pluginStore, lock, plugin, op, payload) as unknown }; }
    catch (error) { throw new ToolFault((error as { code?: string }).code ?? 'PLUGIN_ERROR', error instanceof Error ? error.message : String(error)); }
  }

  async validate(expectedVersion: string) {
    const editor = await this.open(expectedVersion);
    const result = await editor.snapshot(editor.revision);
    await editor.assertDiskUnchanged();
    return { ok: result.ok, version: editor.diskVersion(), diagnostics: result.diagnostics,
      ...(result.ok ? { contentFingerprint: result.snapshot.contentFingerprint } : { error: { code: 'VALIDATION_FAILED' } }) };
  }

  async apply(expectedVersion: string, edits: Exclude<Edit, { type: 'undo' } | { type: 'redo' }>[], reason: string, allowInvalidDraft = false) {
    if (this.readOnly) throw new ToolFault('READ_ONLY', '이 MCP 서버는 읽기 전용입니다.');
    const editor = await this.open(expectedVersion);
    const before = editor.fileVersions();
    editor.readOnly = false;
    try {
      for (const edit of edits) editor.edit(edit, editor.revision);
      await editor.validate(editor.revision);
      if (!allowInvalidDraft && editor.diagnostics.some(d => d.severity === 'error'))
        throw new ToolFault('VALIDATION_FAILED', '변경 묶음에 오류가 있어 저장하지 않았습니다.', { diagnostics: editor.diagnostics });
      await editor.save(editor.revision);
    } catch (error) {
      if (error instanceof ProjectConflict || error instanceof ToolFault || (error as NodeJS.ErrnoException).code) throw error;
      throw new ToolFault('CHANGE_REJECTED', error instanceof Error ? error.message : String(error), { diagnostics: editor.diagnostics });
    }
    const after = editor.fileVersions();
    return { ok: true, previousVersion: expectedVersion, version: editor.diskVersion(), reason,
      changes: [...new Set([...Object.keys(before), ...Object.keys(after)])].filter(file => before[file] !== after[file])
        .map(file => ({ file, before: before[file] ?? null, after: after[file] ?? null })), diagnostics: editor.diagnostics };
  }

  private async scenario(expectedVersion: string, scenarioId: string) {
    const editor = await this.open(expectedVersion);
    const snapshot = await editor.snapshot(editor.revision);
    if (!snapshot.ok) throw new ToolFault('VALIDATION_FAILED', '실행 콘텐츠 검증에 실패했습니다.', { diagnostics: snapshot.diagnostics });
    const document = editor.documents.find(d => d.kind === 'scenario' && (d.data as { id?: string }).id === scenarioId);
    if (!document) throw new ToolFault('SCENARIO_NOT_FOUND', `시나리오가 없습니다: ${scenarioId}`);
    const result = await runScenario(snapshot.snapshot, document.data);
    await editor.assertDiskUnchanged();
    return { content: snapshot.snapshot, result };
  }

  async runScenario(expectedVersion: string, scenarioId: string) {
    const { content, result } = await this.scenario(expectedVersion, scenarioId);
    return { ok: result.ok, version: expectedVersion, contentFingerprint: content.contentFingerprint,
      scenarioId: result.scenarioId, steps: result.steps, assertions: result.assertions,
      state: result.session?.state ?? null, ...(!result.ok ? { error: result.error } : {}) };
  }

  async explainEvents(expectedVersion: string, scenarioId: string, afterCommands: number) {
    const { content, result } = await this.scenario(expectedVersion, scenarioId);
    if (!result.session) throw new ToolFault('SCENARIO_FAILED', '시나리오 실행 상태를 만들지 못했습니다.', { result });
    if (afterCommands > result.session.commands.length) throw new ToolFault('INVALID_ARGUMENT', '실행한 코어 명령 수를 초과했습니다.', { maximum: result.session.commands.length });
    const initial = createSession(content, result.session.origin.seed, result.session.origin);
    if (!initial.ok) throw new ToolFault('SCENARIO_FAILED', initial.error.message);
    let session = initial.session;
    for (const command of result.session.commands.slice(0, afterCommands)) {
      const next = dispatchSession(content, session, command, session.state.stateRevision);
      if (!next.ok) throw new ToolFault('SCENARIO_FAILED', next.error.message);
      session = next.session;
    }
    return { ok: true, version: expectedVersion, contentFingerprint: content.contentFingerprint, scenarioId, afterCommands,
      phase: session.state.phase, periodIndex: session.state.periodIndex,
      decisionAppliesNow: session.state.phase === 'period-end',
      events: inspectEvents(content, session.state).map(decision => {
        const event = content.events.find(e => e.id === decision.eventId)!;
        return { ...decision, window: event.window, condition: explainCondition(content, session.state, event.requirements, `/events/${event.id}/requirements`) };
      }) };
  }
}

/** Explanations call the core evaluators, never a second implementation of game rules. */
function explainCondition(content: ContentSnapshot, state: RunState, condition: Parameters<typeof evaluateCondition>[2], pointer: string): Record<string, unknown> {
  const base = { pointer, op: condition.op };
  try {
    const passed = evaluateCondition(content, state, condition, pointer);
    switch (condition.op) {
      case 'compare': return { ...base, passed, comparator: condition.comparator,
        actual: evaluateNumber(content, state, condition.left), expected: evaluateNumber(content, state, condition.right) };
      case 'flag': return { ...base, passed, flagId: condition.flagId, actual: state.flags[condition.flagId], expected: condition.equals };
      case 'eventCompleted': return { ...base, passed, eventId: condition.eventId };
      case 'all': case 'any': return { ...base, passed, children: condition.conditions.map((c, i) => explainCondition(content, state, c, `${pointer}/conditions/${i}`)) };
      case 'not': return { ...base, passed, child: explainCondition(content, state, condition.condition, `${pointer}/condition`) };
      default: return { ...base, passed };
    }
  } catch (error) { return { ...base, error: error instanceof Error ? error.message : String(error) }; }
}

export function toolError(error: unknown): Record<string, unknown> {
  if (error instanceof ProjectConflict) return { ok: false, error: { code: error.code, message: error.message,
    expectedVersion: error.expectedVersion, actualVersion: error.actualVersion, files: error.files } };
  if (error instanceof ToolFault) return { ok: false, error: { code: error.code, message: error.message, ...error.details } };
  const code = (error as NodeJS.ErrnoException | null)?.code;
  return { ok: false, error: { code: code === 'EEXIST' ? 'PROJECT_BUSY' : code ?? 'PROJECT_ERROR',
    message: error instanceof Error ? error.message : String(error) } };
}

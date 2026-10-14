// Qt and Electron use the same authoring transactions, validator and exporter.
// stdout is a JSON-lines protocol; diagnostics belong on stderr.
import { createInterface } from 'node:readline';
import { mkdir, mkdtemp, realpath, access, readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { StudioEditor } from '../../dist/packages/studio-service/editor.js';
import { importPng } from '../../dist/packages/studio-service/images.js';
import { importSprites } from '../../dist/packages/studio-service/sprites.js';
import { importFont } from '../../dist/packages/studio-service/fonts.js';
import { extractMessages,contentFromDocuments } from '../../dist/packages/localization/catalog.js';
import { createTemplateProject, createProjectFromSource } from '../../dist/packages/studio-service/templates.js';
import { exportGame, hasPortraitTransforms } from '../../dist/packages/export-service/export.js';
import { RuleDebugSession, ruleImpact } from '../../dist/packages/rule-debug/index.js';
import {AnalysisJobs} from '../../dist/packages/analysis-service/jobs.js';
import { validateProject } from '../../dist/packages/validation/index.js';
import { installPackage, listInstalled, pluginStorePath, compareVersions, callProjectPlugin, PluginError } from '../../dist/packages/plugin-host/index.js';
import { createRun } from '../../dist/packages/core/run.js';
import { evaluateCondition } from '../../dist/packages/core/evaluation.js';
import { resolveScreenElements } from '../../dist/packages/visual/components.js';

const engineRoot = fileURLToPath(new URL('../..', import.meta.url));
const LOCK_FILE = 'plugins/lock.json';
export class QtStudioService {
  debugSession = null;
  analysisJobs = new AnalysisJobs();
  constructor({ runtimes = { web: path.join(engineRoot, 'dist/web'), windows: process.env.RAISING_NATIVE_PLAYER_RUNTIME || path.join(engineRoot, 'dist/native-player-app') }, pluginStore } = {}) {
    this.runtimes = runtimes;
    this.pluginStore = pluginStorePath(pluginStore);
    this.editor = new StudioEditor({ pluginStore: this.pluginStore });
  }
  /** `plugins` member of every reply: store contents + this draft's lock resolution. */
  async plugins() { return this.editor.plugins(); }
  /** Lock changes are ordinary draft edits (undo/redo, save, conflict checks like any document). */
  editLock(revision, change) {
    const lock = this.editor.documents.find(d => d.kind === 'pluginLock');
    const data = structuredClone(lock?.data ?? { formatVersion: '0.1.0', plugins: [] });
    if (!Array.isArray(data.plugins)) throw new Error('plugins/lock.json 구조가 올바르지 않습니다. 직접 수정해 주세요.');
    change(data.plugins);
    this.editor.edit(lock ? { type: 'replace', file: lock.file, data } : { type: 'create', file: LOCK_FILE, data }, revision);
  }
  async handle(request) {
    const { method, params = {} } = request;
    if(['analysisList','analysisStatus','analysisStop','analysisRuns','analysisRun','analysisReplay','analysisTree'].includes(method)){
      const jobs=this.analysisJobs;let analysis;
      if(method==='analysisList')analysis={jobs:(await jobs.list(params.root)).map(job=>({...job,result:{...job.result,routeGraph:undefined,traces:[]}}))};
      else if(method==='analysisStatus'){const job=await jobs.get(params.root,params.jobId,params.includeGraph!==false);analysis={job:params.includeGraph===false?{...job,result:{...job.result,routeGraph:undefined,traces:[]}}:job};}
      else if(method==='analysisStop')analysis={job:await jobs.cancel(params.root,params.jobId)};
      else if(method==='analysisRuns'){const runs=await jobs.runs(params.root,params.jobId,params);analysis={runs:params.includePaths?runs:{...runs,rows:runs.rows.map(({nodeIds,edgeIds,traceHash,...row})=>row)}};}
      else if(method==='analysisTree')analysis={tree:await jobs.tree(params.root,params.jobId,params)};
      else if(method==='analysisRun')analysis={run:await jobs.run(params.root,params.jobId,params.run)};
      else analysis={replay:await jobs.replay(params.root,params.jobId,params.run,params.step)};
      return {error:null,notice:null,analysis};
    }
    let extra = {};
    switch (method) {
      case 'conditionPreview': {
        const content=await this.debugSnapshot(params.revision);
        const created=createRun(content,42,{mode:'full',initialOverrides:{attributes:params.attributes??{},flags:{},relationships:{}}});
        if(!created.ok)throw new Error(created.error?.message??'미리보기 값이 올바르지 않습니다.');
        const phase=params.phase??'planning';
        if(!['planning','result','dialogue','choice','finished'].includes(phase))throw new Error('지원하지 않는 미리보기 상태입니다.');
        const hidden={};
        for(const screen of content.project.visual.screens)hidden[screen.id]=resolveScreenElements(content.project.visual,screen.id).filter(e=>
          e.hidden||((e.visibleWhen??'always')!=='always'&&e.visibleWhen!==phase)||(e.visibleCondition&&!evaluateCondition(content,created.state,e.visibleCondition))
        ).map(e=>e.id);
        extra={conditionPreview:{revision:params.revision,attributes:created.state.attributes,phase,hidden}};
        break;
      }
      case 'analysisStart': {
        const content=await this.debugSnapshot(params.revision);
        const job=await this.analysisJobs.start(this.editor.root,content,this.editor.draftVersion(),params.revision,{runs:5000,seed:42,strategy:'random',maxCommands:1000,timeoutMs:3600000,...params.options});
        extra={analysis:{job}};break;
      }
      case 'open': await this.editor.open(params.root); this.debugSession = null; break;
      case 'create':
        if(params.template==='existing')await createProjectFromSource(params.source,params.root);
        else await createTemplateProject(path.join(engineRoot, 'examples'), params.template??'academy-adventure', params.root);
        await this.editor.open(params.root); this.debugSession = null; break;
      case 'view': case 'pluginStatus': break;
      case 'pluginInstall': {
        if (typeof params.file !== 'string' || !path.isAbsolute(params.file)) throw new Error('설치할 .raiseplugin 파일의 절대 경로를 지정하세요.');
        const { plugin, alreadyInstalled } = await installPackage(this.pluginStore, params.file, { source: 'local' });
        extra = { pluginInstalled: { id: plugin.id, version: plugin.version, name: plugin.manifest.name, sha256: plugin.sha256, directory: plugin.directory, alreadyInstalled } };
        if (this.editor.root) await this.editor.validate(this.editor.revision);
        break;
      }
      case 'pluginEnable': {
        if (typeof params.id !== 'string') throw new Error('켤 플러그인 ID를 지정하세요.');
        const versions = (await listInstalled(this.pluginStore)).filter(p => p.id === params.id).map(p => p.version).sort(compareVersions);
        const current = this.editor.documents.find(d => d.kind === 'pluginLock')?.data?.plugins?.find?.(p => p?.id === params.id);
        // Keep the project's pin unless asked otherwise; a new pin takes the newest installed version.
        const version = params.version ?? (current && versions.includes(current.version) ? current.version : versions.at(-1));
        if (!version || !versions.includes(version)) throw new PluginError('PLUGIN_MISSING', `플러그인 '${params.id}'${params.version ? ` ${params.version}` : ''}이(가) 설치되어 있지 않습니다. 먼저 설치하세요.`);
        this.editLock(params.revision, plugins => {
          const entry = plugins.find(p => p?.id === params.id);
          if (entry) { entry.version = version; entry.enabled = true; } else plugins.push({ id: params.id, version, enabled: true });
        });
        await this.editor.validate(this.editor.revision); break;
      }
      case 'pluginDisable': {
        this.editLock(params.revision, plugins => {
          const entry = plugins.find(p => p?.id === params.id);
          if (!entry) throw new PluginError('PLUGIN_NOT_LOCKED', `플러그인 '${params.id}'가 이 프로젝트에서 사용 중이 아닙니다.`);
          entry.enabled = false;
        });
        await this.editor.validate(this.editor.revision); break;
      }
      case 'pluginCall': {
        if (typeof params.plugin !== 'string' || typeof params.op !== 'string') throw new Error('plugin과 op를 지정하세요.');
        const lock = this.editor.documents.find(d => d.kind === 'pluginLock')?.data ?? null;
        extra = { pluginResult: await callProjectPlugin(this.pluginStore, lock, params.plugin, params.op, params.payload ?? {}) }; break;
      }
      case 'importImage':
        extra = { importedAssetId: await importPng(this.editor, params.file, params.revision) };
        await this.editor.validate(this.editor.revision); break;
      case 'importSprites':
        extra = { spriteImport: await importSprites(this.editor, params, params.revision) };
        await this.editor.validate(this.editor.revision); break;
      case 'importFont':
        extra = { importedAssetId: await importFont(this.editor, params.file, params.revision) };
        await this.editor.validate(this.editor.revision); break;
      case 'edit':
        this.editor.edit(params.edit, params.revision);
        await this.editor.validate(this.editor.revision); break;
      case 'save': await this.editor.save(params.revision); break;
      case 'validate': await this.editor.validate(params.revision); break;
      case 'export': {
        const revision = params.revision;
        this.editor.current(revision);
        if (params.target !== 'windows' && params.target !== 'web') throw new Error('지원하지 않는 내보내기 대상입니다.');
        if (typeof params.destination !== 'string' || !path.isAbsolute(params.destination)) throw new Error('새 출력 폴더의 절대 경로를 지정하세요.');
        await this.editor.validate(revision);
        if (this.editor.diagnostics.some(d => d.severity === 'error')) throw new Error('내보내기 전에 문제 패널의 오류를 수정하세요.');
        await this.editor.assertDiskUnchanged();
        this.editor.current(revision);
        const built = await this.editor.snapshot(revision);
        if (!built.ok) throw new Error('내보내기 스냅샷을 만들 수 없습니다.');
        const extensions = await this.editor.exportExtensions(revision, built.snapshot, params.target);
        const exported = await exportGame({ content: built.snapshot, projectRoot: this.editor.root,
          destination: params.destination, target: params.target, runtimes: this.runtimes, extensions,
          beforePublish: async () => {
            await this.editor.assertDiskUnchanged(); this.editor.current(revision);
          } });
        extra = { exported }; break;
      }
      case 'debugStart': {
        const content = await this.debugSnapshot(params.revision);
        this.debugSession = new RuleDebugSession(content, params.revision, params.seed ?? 42);
        extra = { debug: this.debugSession.view() }; break;
      }
      case 'debugLoad': {
        const content = await this.debugSnapshot(params.revision);
        const candidate = new RuleDebugSession(content, params.revision);
        const loaded = await candidate.loadSave(params.text);
        await this.editor.assertDiskUnchanged();
        this.editor.current(params.revision);
        if (loaded.ok) { this.debugSession = candidate; extra = { debug: loaded }; }
        else extra = { debug: this.debugSession ? this.debugSession.view({ ok: false, error: loaded.error }) : { ok: false, error: loaded.error } };
        break;
      }
      case 'debugCommand': case 'debugInspect': {
        this.editor.current(params.revision);
        await this.editor.assertDiskUnchanged();
        this.editor.current(params.revision);
        const session = this.debugSession;
        if (!session || session.sessionId !== params.sessionId || session.projectRevision !== params.revision)
          throw new Error('디버그 실행이 없거나 프로젝트가 변경되었습니다. 새 디버그 실행을 시작하세요.');
        extra = { debug: method === 'debugCommand' ? session.command(params.command, params.expectedStateRevision) : session.inspect(params.subject) };
        break;
      }
      case 'debugImpact': {
        const content = await this.debugSnapshot(params.revision);
        extra = { debug: { impact: ruleImpact(content, params.subject), projectRevision: params.revision, contentFingerprint: content.contentFingerprint } }; break;
      }
      case 'preview': {
        // A renderer preview is an immutable draft export, never an implicit save.
        const revision = params.revision;
        this.editor.current(revision);
        await this.editor.validate(revision);
        if (this.editor.diagnostics.some(d => d.severity === 'error')) throw new Error('미리보기 전에 문제 패널의 오류를 수정하세요.');
        await this.editor.assertDiskUnchanged();
        this.editor.current(revision);
        const built = await this.editor.snapshot(revision);
        if (!built.ok) throw new Error('미리보기 스냅샷을 만들 수 없습니다.');
        const hasScene = Object.hasOwn(params, 'scene');
        if (hasScene) {
          const candidate = structuredClone(this.editor.loaded());
          const project = candidate.documents.find(document => document.kind === 'project');
          if (!project?.data.visual) throw new Error('장면 미리보기에는 화면 구성이 필요합니다.');
          project.data.visual.defaultScene = structuredClone(params.scene);
          const checked = await validateProject(candidate);
          if (!checked.valid) {
            const issue = checked.diagnostics.find(diagnostic => diagnostic.severity === 'error');
            throw new Error(`장면 미리보기 오류: ${issue?.file ?? ''}${issue?.pointer ?? ''} · ${issue?.message ?? '유효하지 않은 장면입니다.'}`);
          }
        }
        const requirePreview = async directory => {
          const capabilities = JSON.parse(await readFile(path.join(directory, 'capabilities.json'), 'utf8'));
          if (!Array.isArray(capabilities.visualFeatures) || !capabilities.visualFeatures.includes('renderer-preview-v1'))
            throw new Error('현재 플레이어는 렌더러 미리보기를 지원하지 않습니다. 내보내기용 플레이어를 다시 빌드해 주세요.');
          if (hasScene && !capabilities.visualFeatures.includes('scene-preview-v1'))
            throw new Error('현재 플레이어는 장면 미리보기를 지원하지 않습니다. 내보내기용 플레이어를 다시 빌드해 주세요.');
          if (hasScene && hasPortraitTransforms(params.scene) && !capabilities.visualFeatures.includes('scene-portrait-transform-v1'))
            throw new Error('현재 플레이어는 저장하지 않은 장면의 배율·위치 변환(scene-portrait-transform-v1)을 지원하지 않습니다. 내보내기용 플레이어를 다시 빌드해 주세요.');
        };
        await requirePreview(this.runtimes.windows);
        const cache = path.join(engineRoot, '.cache/qt-render-preview');
        await mkdir(cache, { recursive: true });
        const run = await mkdtemp(path.join(cache, 'run-'));
        const extensions = await this.editor.exportExtensions(revision, built.snapshot, 'windows');
        const exported = await exportGame({ content: built.snapshot, projectRoot: this.editor.root,
          destination: path.join(run, 'game'), target: 'windows', runtimes: this.runtimes, extensions });
        // A rebuilt runtime or external edit during export must not launch stale data.
        await requirePreview(exported.directory);
        await this.editor.assertDiskUnchanged();
        this.editor.current(revision);
        const sceneFile = hasScene ? path.join(run, 'preview-scene.json') : null;
        if (sceneFile) await writeFile(sceneFile, JSON.stringify(params.scene, null, 2), 'utf8');
        extra = { play: { preview: true, executable: path.join(exported.directory, 'RaisingPlayer.exe'),
          directory: exported.directory, save: path.join(run, 'save.json'), report: path.join(run, 'report.json'),
          revision, fingerprint: exported.contentFingerprint, ...(sceneFile ? { scene: sceneFile, previewSource: 'initial-state', gameplayValidated: false } : {}) } };
        break;
      }
      case 'play': {
        this.editor.current(params.revision);
        await this.editor.validate(params.revision);
        if (this.editor.diagnostics.some(d => d.severity === 'error')) throw new Error('실행 전에 문제 패널의 오류를 수정하세요.');
        await this.editor.save(params.revision);
        const built = await this.editor.snapshot(this.editor.revision);
        if (!built.ok) throw new Error('실행 스냅샷을 만들 수 없습니다.');
        const cache = path.join(engineRoot, '.cache/qt-preview');
        await mkdir(cache, { recursive: true });
        const run = await mkdtemp(path.join(cache, 'run-'));
        const extensions = await this.editor.exportExtensions(this.editor.revision, built.snapshot, 'windows');
        const exported = await exportGame({ content: built.snapshot, projectRoot: this.editor.root,
          destination: path.join(run, 'game'), target: 'windows',
          runtimes: this.runtimes, extensions });
        // Recheck before launch: an MCP save during export must not silently become this run.
        await this.editor.assertDiskUnchanged();
        extra = { play: { executable: path.join(exported.directory, 'RaisingPlayer.exe'),
          directory: exported.directory, save: path.join(run, 'save.json'), report: path.join(run, 'report.json'),
          revision: this.editor.revision, fingerprint: exported.contentFingerprint } };
        break;
      }
      default: throw new Error(`알 수 없는 요청: ${method}`);
    }
    return { ...this.editor.reply(), ...extra, assets: await this.assets(), localizationSources:extractMessages(contentFromDocuments(this.editor.documents)), plugins: await this.plugins() };
  }
  async debugSnapshot(revision) {
    this.editor.current(revision);
    await this.editor.validate(revision);
    if (this.editor.diagnostics.some(d => d.severity === 'error')) throw new Error('디버그 전에 문제 패널의 오류를 수정하세요.');
    await this.editor.assertDiskUnchanged();
    this.editor.current(revision);
    const built = await this.editor.snapshot(revision);
    if (!built.ok) throw new Error('디버그 스냅샷을 만들 수 없습니다.');
    await this.editor.assertDiskUnchanged();
    this.editor.current(revision);
    return built.snapshot;
  }
  async assets() {
    if (!this.editor.root) return [];
    const root = await realpath(this.editor.root);
    const rows = this.editor.documents.flatMap(d => d.data?.assets ?? []);
    const result = [];
    for (const asset of rows) {
      if (!['image','font'].includes(asset.kind)) continue;
      try {
        const file = await realpath(path.resolve(root, asset.path));
        const relative = path.relative(root, file);
        if (relative.startsWith('..') || path.isAbsolute(relative)) continue;
        await access(file); result.push({ ...asset, absolutePath: file });
      } catch { /* The shared validator reports missing assets. */ }
    }
    return result;
  }
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const service = new QtStudioService();
  const input = createInterface({ input: process.stdin, crlfDelay: Infinity });
  for await (const line of input) {
    let request;
    try {
      if (Buffer.byteLength(line) > 4 * 1024 * 1024) throw new Error('요청 크기가 너무 큽니다.');
      request = JSON.parse(line);
      const result = await service.handle(request);
      process.stdout.write(JSON.stringify({ id: request.id, ...result }) + '\n');
    } catch (error) {
      process.stdout.write(JSON.stringify({ id: request?.id ?? null, ...service.editor.reply(error.message), code: error.code ?? null, plugins: await service.plugins().catch(() => null) }) + '\n');
    }
  }
}

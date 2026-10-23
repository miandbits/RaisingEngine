import { saveMigration } from './save-migration.mjs';
import {GoalCiReports} from './goal-ci.mjs';
import {GitHubGoalReports} from './goal-github.mjs';
import {runGoalRegression} from './goal-regression.mjs';
import {changeGoalBaseline} from './goal-approvals.mjs';
import {resolveGoalImpact} from './goal-impact.mjs';
import {createBenchmarkProject} from './benchmarks.mjs';
import {runAssurance} from '../../dist/packages/analysis-service/assurance.js';
import {GoalSearchJobs} from './goal-search.mjs';
import {checkGoalImpacts,publishGoalImpacts,latestGoalImpact} from './goal-impact.mjs';
import {listGoalBaselines} from './goal-baselines.mjs';
import {GoalComparisons} from './goal-compare.mjs';
import {listGoalRoutes, importGoalRecording, saveGoalRoute, checkGoalRoutes} from './goal-routes.mjs';
// Qt and Electron use the same authoring transactions, validator and exporter.
// stdout is a JSON-lines protocol; diagnostics belong on stderr.
import { createInterface } from 'node:readline';
import { mkdir, mkdtemp, realpath, access, readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import os from 'node:os';
import { StudioEditor } from '../../dist/packages/studio-service/editor.js';
import { detachProject } from '../../dist/packages/project-service/snapshot.js';
import { importPng } from '../../dist/packages/studio-service/images.js';
import { importSprites } from '../../dist/packages/studio-service/sprites.js';
import { importPluginContent } from '../../dist/packages/studio-service/plugin-import.js';
import { importFont } from '../../dist/packages/studio-service/fonts.js';
import { extractMessages,contentFromDocuments } from '../../dist/packages/localization/catalog.js';
import { createTemplateProject, createProjectFromSource } from '../../dist/packages/studio-service/templates.js';
import { exportGame, hasPortraitTransforms } from '../../dist/packages/export-service/export.js';
import { authoringWorkspace, workspaceImpact } from '../../dist/packages/authoring-workspace/index.js';
import { RuleDebugSession, ruleImpact } from '../../dist/packages/rule-debug/index.js';
import {AnalysisJobs} from '../../dist/packages/analysis-service/jobs.js';
import { validateProject } from '../../dist/packages/validation/index.js';
import { installPackage, listInstalled, pluginStorePath, compareVersions, callProjectPlugin, PluginError } from '../../dist/packages/plugin-host/index.js';
import { createRun } from '../../dist/packages/core/run.js';
import { evaluateCondition } from '../../dist/packages/core/evaluation.js';
import { resolveScreenElements } from '../../dist/packages/visual/components.js';
import { authoringPreview } from './authoring-preview.mjs';

const engineRoot = fileURLToPath(new URL('../..', import.meta.url));
const LOCK_FILE = 'plugins/lock.json';
export class QtStudioService {
  debugSession = null;
  analysisJobs = new AnalysisJobs();
  goalSearchJobs = new GoalSearchJobs();
  goalComparisons = new GoalComparisons();
  goalCiReports = new GoalCiReports();
  goalGitHub = new GitHubGoalReports();
  /** shareViews: replies are serialized right away (the stdio protocol below), so the draft's
   *  documents are not cloned into each reply (in-process callers keep their own copies). */
  constructor({ runtimes = { web: process.env.RAISING_WEB_PLAYER_RUNTIME || path.join(engineRoot, 'dist/web'), windows: process.env.RAISING_NATIVE_PLAYER_RUNTIME || path.join(engineRoot, 'dist/native-player-app') }, pluginStore, shareViews = false } = {}) {
    this.runtimes = runtimes;
    this.shareViews = shareViews;
    this.pluginStore = pluginStorePath(pluginStore);
    this.defaultPluginStore = this.pluginStore;
    this.editor = new StudioEditor({ pluginStore: this.pluginStore });
  }
  /** The target player's capabilities.json visualFeatures (plugins prepare exports for them, e.g. atlases). */
  async playerFeatures(target) {
    const capabilities = await readFile(path.join(this.runtimes[target], 'capabilities.json'), 'utf8').then(JSON.parse).catch(() => ({}));
    return Array.isArray(capabilities.visualFeatures) ? capabilities.visualFeatures : [];
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
    if (method === 'saveMigrationPreview' || method === 'saveMigrationPublish') return saveMigration(method, params);
    if(method==='goalCompareStep')return {error:null,goalComparisonStep:await this.goalComparisons.step(this.editor,params)};
    if(method==='goalSearchStatus'||method==='goalSearchStop')return {error:null,goalSearch:method==='goalSearchStop'?this.goalSearchJobs.cancel(params.root,params.jobId):this.goalSearchJobs.public(this.goalSearchJobs.get(params.root,params.jobId))};
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
      case 'goalRecordPrepare': {
        const caps=await readFile(path.join(this.runtimes.windows,'capabilities.json'),'utf8').then(JSON.parse).catch(()=>({}));
        if(!caps.toolFeatures?.includes('goal-route-recording-v1'))throw new Error('이 플레이어는 목표 경로 기록을 지원하지 않습니다. 최신 Windows 플레이어 런타임을 선택하세요.');
        extra={recordingSupported:true};break;
      }
      case 'goalSearchStart': {
        const content=await this.debugSnapshot(params.revision);
        extra={goalSearch:await this.goalSearchJobs.start(this.editor.root,content,params.revision,params)};break;
      }
      case 'goalGitHubDefaults': {
        extra={goalGitHub:await this.goalGitHub.defaults(this.editor.root)};break;
      }
      case 'goalGitHubRuns': {
        this.editor.current(params.revision);
        extra={goalGitHub:await this.goalGitHub.list(this.editor.root,params)};break;
      }
      case 'goalGitHubReports': {
        this.editor.current(params.revision);
        extra={goalGitHub:await this.goalGitHub.reports(this.editor.root,params.listingId,params.runId)};break;
      }
      case 'goalGitHubOpen': {
        extra={goalCi:this.goalGitHub.open(this.editor,this.goalCiReports,params.revision,params.bundleId,params.reportId)};break;
      }
      case 'goalCiOpen': {
        extra={goalCi:await this.goalCiReports.open(this.editor,params.file,params.revision)};break;
      }
      case 'goalCiRecheck': {
        const content=await this.debugSnapshot(params.revision);
        extra=await this.goalCiReports.recheck(this.editor,content,params.revision,params.id);break;
      }
      case 'goalBaselineApprovePartial':
      case 'goalBaselineApprove':
      case 'goalBaselineRestore': {
        const content=await this.debugSnapshot(params.revision);
        const resolved=await resolveGoalImpact(this.editor.root,content,params.impactReportId,params.impactRow);
        if(['goalBaselineApprove','goalBaselineApprovePartial'].includes(method)&&resolved.row.status!=='changed')throw Error('목표에 도달한 변경 경로만 승인할 수 있습니다.');
        if(method==='goalBaselineRestore'&&!params.baselineId&&!params.restoreReview)throw Error('되돌릴 기준을 선택하세요.');
        const entry=await changeGoalBaseline(this.editor.root,{comparisonRuntime:resolved.row.comparisonRuntime,expectedRevision:resolved.report.selectionRevision,route:resolved.route,baseline:resolved.baseline,content,reportId:resolved.report.id,name:params.name,note:params.note,restoreId:method==='goalBaselineRestore'?params.baselineId:undefined,restoreReview:method==='goalBaselineRestore'?params.restoreReview:undefined,itemIds:method==='goalBaselineApprovePartial'?(params.itemIds??[]):undefined,validate:async()=>{this.editor.current(params.revision);await this.editor.assertDiskUnchanged();await resolveGoalImpact(this.editor.root,content,params.impactReportId,params.impactRow);}});
        extra={goalApproval:entry};
        try{const policy=resolved.report.runtimePolicy??'strict';const report=resolved.report.kind==='goal-regression'?await runGoalRegression(this.editor.root,{runtimePolicy:policy}):await checkGoalImpacts(this.editor.root,content,params.revision,{runtimePolicy:policy});report.testedRevision=params.revision;extra.goalImpact=await publishGoalImpacts(this.editor,content,params.revision,report);}
        catch(error){extra.goalImpact={rows:[],error:'기준 변경은 완료되었으나 재검사에 실패했습니다: '+error.message,stale:true,complete:false};}
        break;
      }
      case 'goalImpactLatest': {const content=await this.debugSnapshot(params.revision);extra={goalImpact:await latestGoalImpact(this.editor.root,content)};break;}
      case 'goalBaselineList': this.editor.current(params.revision);extra={goalBaselines:await listGoalBaselines(this.editor.root)};break;
      case 'goalBaselineSave': {
        const content=await this.debugSnapshot(params.revision);
        extra={goalBaseline:await this.goalComparisons.capture(this.editor,content,params)};break;
      }
      case 'goalCompareOpen': {
        const content=await this.debugSnapshot(params.revision);
        extra={goalComparison:await this.goalComparisons.open(this.editor,this.goalSearchJobs,content,params)};break;
      }
      case 'goalSearchRegister': {
        const content=await this.debugSnapshot(params.revision);
        extra={goalSearchRegistered:await this.goalSearchJobs.register(this.editor,content,params)};break;
      }
      case 'goalRoutesList': this.editor.current(params.revision); extra={goalRoutes:await listGoalRoutes(this.editor.root)};break;
      case 'goalRouteImport': {
        this.editor.current(params.revision);
        const route=await importGoalRecording(params.file,params.metadata);
        const content=await this.debugSnapshot(params.revision);
        if(route.gameId!==content.project.gameId)throw new Error('다른 게임의 기록입니다.');
        extra={goalRoute:route};break;
      }
      case 'goalRouteSave': {
        this.editor.current(params.revision);await this.editor.assertDiskUnchanged();
        if(params.route?.gameId!==this.editor.documents.find(d=>d.kind==='project')?.data.gameId)throw new Error('현재 게임의 목표 경로만 저장할 수 있습니다.');
        extra={goalRouteSaved:await saveGoalRoute(this.editor.root,params.route,params.expectedRevision??null)};break;
      }
      case 'goalRoutesRun': {
        const content=await this.debugSnapshot(params.revision);
        const report=await checkGoalRoutes(this.editor.root,content,params.ids);await this.editor.assertDiskUnchanged();
        extra={goalReport:{...report,testedRevision:params.revision}};break;
      }
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
      case 'createBenchmark': if(this.editor.root&&this.editor.view().dirty)throw new Error('현재 프로젝트의 변경을 먼저 저장하거나 되돌린 뒤 새 샘플을 만드세요.');await createBenchmarkProject(params.root,params);await this.editor.open(params.root);this.debugSession=null;break;
      case 'assuranceRun': {
        const content=await this.debugSnapshot(params.revision);
        const report=await runAssurance(content,{runs:params.runs??32,seed:params.seed??42,maxCommands:150,timeoutMs:60000});
        const folder=path.join(this.editor.root,'.studio','assurance');await mkdir(folder,{recursive:true});
        const file=path.join(folder,'report-'+Date.now()+'.json');await writeFile(file,JSON.stringify(report,null,2));
        extra={assurance:{...report,file}};break;
      }
      case 'analysisStart': {
        const content=await this.debugSnapshot(params.revision);
        const job=await this.analysisJobs.start(this.editor.root,content,this.editor.draftVersion(),params.revision,{runs:5000,seed:42,strategy:'random',maxCommands:1000,timeoutMs:3600000,...params.options});
        extra={analysis:{job}};break;
      }
      case 'open': {
        // A remembered local path is optional. A missing disk must not prevent opening content.
        const previousStore = this.pluginStore;
        const previousDiagnostics = this.editor.diagnostics;
        this.editor.pluginStore = this.defaultPluginStore;
        this.pluginStore = this.defaultPluginStore;
        if (params.pluginStore) {
          try { await this.editor.configurePluginStore(params.pluginStore, this.editor.revision); this.pluginStore = this.editor.pluginStore; }
          catch (error) { extra.environmentWarning = `저장된 플러그인 저장소를 사용할 수 없습니다: ${error.message}`; }
        }
        try { await this.editor.open(params.root); }
        catch (error) { this.editor.pluginStore = previousStore; this.pluginStore = previousStore; this.editor.diagnostics = previousDiagnostics; throw error; }
        this.debugSession = null; break;
      }
      case 'create': {
        if(params.template==='existing')await createProjectFromSource(params.source,params.root);
        else await createTemplateProject(path.join(engineRoot, 'examples'), params.template??'academy-adventure', params.root);
        const previousStore = this.pluginStore;
        this.editor.pluginStore = this.defaultPluginStore;
        this.pluginStore = this.defaultPluginStore;
        try { await this.editor.open(params.root); }
        catch (error) { this.editor.pluginStore = previousStore; this.pluginStore = previousStore; throw error; }
        this.debugSession = null; break;
      }
      case 'view': break;
      case 'pluginStatus': if (this.editor.root) await this.editor.validate(params.revision); break;
      case 'pluginStore': {
        await this.editor.configurePluginStore(params.directory, params.revision);
        this.pluginStore = this.editor.pluginStore;
        this.debugSession = null;
        extra.pluginStoreConfigured = this.pluginStore;
        break;
      }
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
      case 'pluginImport': {
        // Plugin import hook (CONTRACT.md §11): dryRun = preview, else images + documents as one edit.
        extra = { pluginImport: await importPluginContent(this.editor, { plugin: params.plugin, op: params.op, payload: params.payload ?? {}, dryRun: params.dryRun === true }, params.revision) };
        if (params.dryRun !== true) await this.editor.validate(this.editor.revision);
        break;
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
      case 'save': {
        await this.editor.save(params.revision);
        const hasRoutes=await access(path.join(this.editor.root,'playtests','routes')).then(()=>true,()=>false);
        if(hasRoutes){
          let content;
          try {content=await this.debugSnapshot(this.editor.revision);}catch(error){extra={goalReport:{ok:false,reports:[],error:error.message},goalImpact:{rows:[],error:error.message,complete:false,stale:true}};}
          if(content){
            try {if((await listGoalRoutes(this.editor.root)).length){const report=await checkGoalRoutes(this.editor.root,content);await this.editor.assertDiskUnchanged();extra.goalReport={...report,testedRevision:this.editor.revision};}}
            catch(error){extra.goalReport={ok:false,reports:[],error:error.message,testedRevision:this.editor.revision};}
            try {const report=await checkGoalImpacts(this.editor.root,content,this.editor.revision);extra.goalImpact=await publishGoalImpacts(this.editor,content,this.editor.revision,report);}
            catch(error){extra.goalImpact={rows:[],error:error.message,complete:false,stale:true,testedRevision:this.editor.revision};}
          }
        }
        break;
      }
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
        const extensions = await this.editor.exportExtensions(revision, built.snapshot, params.target, await this.playerFeatures(params.target));
        const exported = await exportGame({ content: built.snapshot, projectRoot: this.editor.root,
          destination: params.destination, target: params.target, runtimes: this.runtimes, extensions,
          beforePublish: async () => {
            await this.editor.assertDiskUnchanged(); this.editor.current(revision);
          } });
        extra = { exported }; break;
      }
      case 'authoringWorkspace': {
        this.editor.current(params.revision);
        const view=this.editor.view();
        const workspace=authoringWorkspace(view.documents);
        extra={workspace,...(params.key?{workspaceImpact:workspaceImpact(workspace,params.key,params.depth??4)}:{})};break;
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
        let previewState, replayedUpdatedContent = false;
        if (params.authoringContext?.savePath) {
          const session = new RuleDebugSession(built.snapshot, revision);
          const text = await readFile(params.authoringContext.savePath, 'utf8');
          let loaded = await session.loadSave(text);
          if (!loaded.ok && loaded.error?.code === 'CONTENT_MISMATCH' && params.authoringContext.canvasContext === true) {
            // Inspection only: revalidate the full schema, command replay AND exact saved state
            // against the current draft. Never rewrite a save or relax normal player loading.
            const candidate = JSON.parse(text);
            if(candidate.state.contentFingerprint !== candidate.contentFingerprint)throw new Error('미리보기 저장 불러오기: 저장 문서와 상태의 콘텐츠 지문이 다릅니다.');
            candidate.contentFingerprint = built.snapshot.contentFingerprint;
            candidate.state.contentFingerprint = built.snapshot.contentFingerprint;
            loaded = await session.loadSave(JSON.stringify(candidate));
            replayedUpdatedContent = loaded.ok;
          }
          if (!loaded.ok) throw new Error(`미리보기 저장 불러오기: ${loaded.error?.message ?? '저장 재현 검사 실패'}`);
          previewState = loaded.state;
        }
        const inspection = params.authoringContext ? authoringPreview(built.snapshot, params.authoringContext, previewState) : null;
        if (inspection && replayedUpdatedContent) {
          inspection.explanation.replayedUpdatedContent = true;
          inspection.explanation.notice += ' 콘텐츠 변경 후 전체 명령과 저장 상태의 일치를 재검사했습니다. 저장 파일을 변환하지 않았습니다.';
        }
        if (hasScene) {
          // Shares the (lazy) source locations: the scene check's diagnostics keep exact lines.
          const candidate = detachProject(this.editor.loaded());
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
          if (inspection && !capabilities.visualFeatures?.includes('authoring-preview-v1'))
            throw new Error('현재 플레이어는 제작 상태 미리보기(authoring-preview-v1)를 지원하지 않습니다. 플레이어를 다시 빌드하세요.');
          if (!Array.isArray(capabilities.visualFeatures) || !capabilities.visualFeatures.includes('renderer-preview-v1'))
            throw new Error('현재 플레이어는 렌더러 미리보기를 지원하지 않습니다. 내보내기용 플레이어를 다시 빌드해 주세요.');
          if (hasScene && !capabilities.visualFeatures.includes('scene-preview-v1'))
            throw new Error('현재 플레이어는 장면 미리보기를 지원하지 않습니다. 내보내기용 플레이어를 다시 빌드해 주세요.');
          if (hasScene && hasPortraitTransforms(params.scene) && !capabilities.visualFeatures.includes('scene-portrait-transform-v1'))
            throw new Error('현재 플레이어는 저장하지 않은 장면의 배율·위치 변환(scene-portrait-transform-v1)을 지원하지 않습니다. 내보내기용 플레이어를 다시 빌드해 주세요.');
        };
        await requirePreview(this.runtimes.windows);
        const cache = path.join(process.env.RAISING_STUDIO_CACHE || path.join(os.tmpdir(),'RaisingStudio'), 'qt-render-preview');
        await mkdir(cache, { recursive: true });
        const run = await mkdtemp(path.join(cache, 'run-'));
        // Play/preview runs skip export-only preparation (atlas packing): quick, and reports keep the part ids.
        const extensions = await this.editor.exportExtensions(revision, built.snapshot, 'windows');
        const exported = await exportGame({ content: built.snapshot, projectRoot: this.editor.root,
          destination: path.join(run, 'game'), target: 'windows', runtimes: this.runtimes, extensions });
        // A rebuilt runtime or external edit during export must not launch stale data.
        await requirePreview(exported.directory);
        await this.editor.assertDiskUnchanged();
        this.editor.current(revision);
        const sceneFile = hasScene ? path.join(run, 'preview-scene.json') : null;
        const inspectionFile = inspection ? path.join(run, 'inspection.json') : null;
        if (inspectionFile) await writeFile(inspectionFile, JSON.stringify(inspection), 'utf8');
        if (sceneFile) await writeFile(sceneFile, JSON.stringify(params.scene, null, 2), 'utf8');
        extra = { play: { preview: true, executable: path.join(exported.directory, 'RaisingPlayer.exe'),
          directory: exported.directory, save: path.join(run, 'save.json'), report: path.join(run, 'report.json'),
          revision, fingerprint: exported.contentFingerprint, ...(sceneFile ? { scene: sceneFile, previewSource: 'initial-state', gameplayValidated: false } : {}),
          ...(inspection ? {inspection:inspectionFile,screenId:inspection.screenId,explanation:inspection.explanation,previewSource:'authoring-state',gameplayValidated:false} : {}) } };
        break;
      }
      case 'play': {
        const capabilities=await readFile(path.join(this.runtimes.windows,'capabilities.json'),'utf8').then(JSON.parse).catch(()=>({}));
        this.editor.current(params.revision);
        await this.editor.validate(params.revision);
        if (this.editor.diagnostics.some(d => d.severity === 'error')) throw new Error('실행 전에 문제 패널의 오류를 수정하세요.');
        await this.editor.save(params.revision);
        const built = await this.editor.snapshot(this.editor.revision);
        if (!built.ok) throw new Error('실행 스냅샷을 만들 수 없습니다.');
        const cache = path.join(process.env.RAISING_STUDIO_CACHE || path.join(os.tmpdir(),'RaisingStudio'), 'qt-preview');
        await mkdir(cache, { recursive: true });
        const run = await mkdtemp(path.join(cache, 'run-'));
        const extensions = await this.editor.exportExtensions(this.editor.revision, built.snapshot, 'windows');
        const exported = await exportGame({ content: built.snapshot, projectRoot: this.editor.root,
          destination: path.join(run, 'game'), target: 'windows',
          runtimes: this.runtimes, extensions });
        // Recheck before launch: an MCP save during export must not silently become this run.
        await this.editor.assertDiskUnchanged();
        extra = { play: { executable: path.join(exported.directory, 'RaisingPlayer.exe'),
          directory: exported.directory, save: path.join(run, 'save.json'), report: path.join(run, 'report.json'), recording: capabilities.toolFeatures?.includes('goal-route-recording-v1')?path.join(run, 'recording.json'):null,
          revision: this.editor.revision, fingerprint: exported.contentFingerprint } };
        break;
      }
      default: throw new Error(`알 수 없는 요청: ${method}`);
    }
    return { ...this.editor.reply(null, null, { shared: this.shareViews }), ...extra, assets: await this.assets(), localizationSources:extractMessages(contentFromDocuments(this.editor.documents)), plugins: await this.plugins() };
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
    // Checked concurrently (a large character has hundreds of part images), order kept.
    const checked = await Promise.all(rows.map(async asset => {
      if (!['image','font'].includes(asset.kind)) return null;
      try {
        const file = await realpath(path.resolve(root, asset.path));
        const relative = path.relative(root, file);
        if (relative.startsWith('..') || path.isAbsolute(relative)) return null;
        await access(file); return { ...asset, absolutePath: file };
      } catch { return null; /* The shared validator reports missing assets. */ }
    }));
    return checked.filter(Boolean);
  }
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const service = new QtStudioService({ shareViews: true });
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
  // The editor closed its end (quit, also after 「저장 안 함」): files imported for the unsaved draft go.
  await service.editor.close();
}

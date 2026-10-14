import { Worker } from 'node:worker_threads';
import { randomUUID,createHash } from 'node:crypto';
import { mkdir, readFile, readdir, realpath, writeFile, rename, unlink } from 'node:fs/promises';
import { setTimeout as delay } from 'node:timers/promises';
import path from 'node:path';
import type { ContentSnapshot } from '../core/model.js';
import { emptyResult, validateOptions, type AnalysisJob, type AnalysisOptions, type StoredAnalysis } from './model.js';
import type {Trace} from './model.js';
import type {RunSummary} from './routes.js';
import {freeze} from '../core/util.js';
import {createRun,dispatchCommand} from '../native-bridge/backend.js';
import {applyCommands} from './runner.js';
import {readArchive,type StoredRun} from './archive.js';
import {loadRouteTree} from './route-tree.js';

export async function atomicJson(file: string, value: unknown) {
  const temp = `${file}.${randomUUID()}.tmp`;
  await writeFile(temp, JSON.stringify(value), { flag: 'wx' });
  try {
    for (let attempt = 0; ; attempt++) {
      try { await rename(temp, file); return; }
      catch (error) {
        // Windows readers and antivirus scans can briefly prevent replacing an otherwise writable file.
        if (attempt >= 6 || !['EPERM', 'EACCES', 'EBUSY'].includes((error as NodeJS.ErrnoException).code ?? '')) throw error;
        await delay(Math.min(25 * 2 ** attempt, 400));
      }
    }
  } finally { await unlink(temp).catch(() => {}); }
}
export class AnalysisJobs {
  private workers = new Map<string, Worker>();
  constructor(private workerUrl = new URL('./worker.js', import.meta.url)) {}
  private async directory(root: string, create = false) {
    const resolved = await realpath(root); const studio = path.join(resolved, '.studio');
    if (create) await mkdir(studio, { recursive: true });
    const checked = await realpath(studio); if (path.dirname(checked).toLowerCase() !== resolved.toLowerCase()) throw new Error('프로젝트 밖 분석 경로는 허용하지 않습니다.');
    const dir = path.join(checked, 'analysis'); if (create) await mkdir(dir, { recursive: true });
    if ((await realpath(dir)).toLowerCase() !== dir.toLowerCase()) throw new Error('분석 경로의 링크는 허용하지 않습니다.');
    return dir;
  }
  private async file(root: string, id: string) {
    if (!/^[a-f0-9-]{36}$/.test(id)) throw new Error('올바르지 않은 작업 ID입니다.');
    return path.join(await this.directory(root), `${id}.json`);
  }
  async start(root: string, content: ContentSnapshot, sourceVersion: string, revision: number | null, input: AnalysisOptions) {
    const options = validateOptions(input, content); const dir = await this.directory(root, true);
    const running = (await this.list(root)).filter(j => j.status === 'running'); if (running.length >= 2) throw new Error('동시 분석 작업은 2개까지 실행할 수 있습니다.');
    const id = randomUUID(); const job: AnalysisJob = { id, root: await realpath(root), sourceVersion, revision, fingerprint: content.contentFingerprint,
      effectiveWorkers: options.target || options.strategy === 'explore' ? 1 : Math.min(options.workers ?? 1, options.runs), createdAt: new Date().toISOString(), status: 'running', options, result: emptyResult(options.runs), error: null };
    job.snapshotHash=createHash('sha256').update(JSON.stringify(content)).digest('hex');
    await atomicJson(path.join(dir, `${id}.snapshot.json`), content);
    await atomicJson(path.join(dir, `${id}.json`), { ...job, ownerPid: process.pid });
    const worker = new Worker(this.workerUrl, { workerData: { job, content, dir }, execArgv: [], env: { ...process.env, NODE_OPTIONS: '' } });
    this.workers.set(id, worker);
    const failed = async (error: string) => { const current = await this.get(root, id); if (current.status === 'running') await atomicJson(path.join(dir, `${id}.json`), { ...current, status: 'error', error }); };
    worker.on('error', e => { void failed(e.message).catch(() => {}); });
    worker.on('exit', code => { this.workers.delete(id); if (code !== 0) void failed(`분석 작업 프로세스 종료: ${code}`).catch(() => {}); });
    worker.unref(); return job;
  }
  async get(root: string, id: string,includeGraph=true): Promise<AnalysisJob> {
    const file=await this.file(root,id);let light:AnalysisJob&{ownerPid?:number}|null=null;
    try{light=JSON.parse(await readFile(file.replace(/\.json$/,'.progress.json'),'utf8'));}catch(e){if((e as NodeJS.ErrnoException).code!=='ENOENT')throw e;}
    const data=!includeGraph&&light?light:JSON.parse(await readFile(file,'utf8')) as AnalysisJob & { ownerPid?: number };
    if(includeGraph&&light&&light.result.completed>=data.result.completed){const {routeGraph,traces}=data.result;Object.assign(data,light);data.result={...light.result,...(routeGraph?{routeGraph}:{}),traces};}
    if (data.status === 'running' && data.ownerPid) {
      try { process.kill(data.ownerPid, 0); } catch (error) { if ((error as NodeJS.ErrnoException).code === 'ESRCH') {
        data.status = 'error'; data.error = '분석 호스트가 종료되었습니다. 완료된 실행 결과는 유지됩니다.';
        const archive=await readArchive(file.replace(/\.json$/,''));if(archive.rows.length>data.result.completed){data.result.completed=archive.rows.length;data.result.outcomes={};for(const row of archive.rows)data.result.outcomes[row.outcome]=(data.result.outcomes[row.outcome]??0)+1;}
      } }
    }
    return data;
  }
  async list(root: string): Promise<AnalysisJob[]> {
    let dir: string; try { dir = await this.directory(root); } catch (e) { if ((e as NodeJS.ErrnoException).code === 'ENOENT') return []; throw e; }
    const files = (await readdir(dir)).filter(f => /^[a-f0-9-]{36}\.json$/.test(f));
    const jobs = await Promise.all(files.map(f => this.get(root, f.slice(0, -5),false)));
    return jobs.sort((a, b) => b.createdAt.localeCompare(a.createdAt));
  }
  async cancel(root: string, id: string) { const job = await this.get(root, id); if (job.status === 'running') await writeFile(`${await this.file(root, id)}.cancel`, 'cancel'); return job; }
  async stored(root: string, id: string, includeGraph = true): Promise<StoredAnalysis> {
    const job = await this.get(root, id,includeGraph);const text=await readFile((await this.file(root,id)).replace(/\.json$/,'.snapshot.json'),'utf8');
    if(job.snapshotHash&&createHash('sha256').update(text).digest('hex')!==job.snapshotHash)throw new Error('분석 스냅샷 파일이 변경되었습니다.');
    const content = JSON.parse(text) as ContentSnapshot;
    if (content.contentFingerprint !== job.fingerprint) throw new Error('분석 스냅샷이 기록과 다릅니다.');
    freeze(content);
    return { job, content };
  }
  async runs(root:string,id:string,options:{offset?:number;limit?:number;nodeId?:string;edgeId?:string;outcome?:string;endingId?:string;attributeId?:string;min?:number;max?:number;routePrefix?:string[]}={}){
    const offset=options.offset??0,limit=options.limit??100;
    if(!Number.isSafeInteger(offset)||offset<0||!Number.isSafeInteger(limit)||limit<1||limit>500)throw new Error('목록 범위가 올바르지 않습니다.');
    if(options.min!==undefined&&!Number.isFinite(options.min)||options.max!==undefined&&!Number.isFinite(options.max))throw new Error('능력치 범위가 올바르지 않습니다.');
    const job=await this.get(root,id,false),archive=await readArchive((await this.file(root,id)).replace(/\.json$/,''));
    const prefixRuns=options.routePrefix===undefined?null:(await loadRouteTree((await this.file(root,id)).replace(/\.json$/,''),archive.rows,job.fingerprint,job.snapshotHash)).runs(options.routePrefix);
    const node=options.nodeId?archive.nodes.indexOf(options.nodeId):-1,edge=options.edgeId?archive.edges.indexOf(options.edgeId):-1;
    const rows:StoredRun[]=[];
    for(const row of archive.rows){
      if(prefixRuns&&!prefixRuns.has(row.run))continue;
      if(options.nodeId&&!row.nodeIds.includes(row.encoding?node:options.nodeId)||options.edgeId&&!row.edgeIds.includes(row.encoding?edge:options.edgeId)||options.outcome&&row.outcome!==options.outcome||options.endingId&&row.endingId!==options.endingId)continue;
      if(options.attributeId){const value=row.attributes?.[options.attributeId];if(value===undefined||options.min!==undefined&&value<options.min||options.max!==undefined&&value>options.max)continue;}rows.push(row);}
    return {total:rows.length,offset,limit,rows:rows.slice(offset,offset+limit).map(archive.decode),fingerprint:job.fingerprint};
  }
  async tree(root:string,id:string,options:{prefix?:string[];offset?:number;limit?:number}={}){
    const job=await this.get(root,id,false),file=(await this.file(root,id)).replace(/\.json$/,''),archive=await readArchive(file);
    const tree=await loadRouteTree(file,archive.rows,job.fingerprint,job.snapshotHash);return {...tree.page(options.prefix??[],options.offset??0,options.limit??40),fingerprint:job.fingerprint};
  }
  async run(root:string,id:string,run:number):Promise<Trace>{
    const job=await this.get(root,id,false);if(!Number.isSafeInteger(run)||run<0||run>=job.options.runs)throw new Error('실행 회차가 범위를 벗어납니다.');
    const dir=(await this.file(root,id)).replace(/\.json$/,'.runs');if((await realpath(dir)).toLowerCase()!==dir.toLowerCase())throw new Error('분석 기록 경로의 링크는 허용하지 않습니다.');
    const text=await readFile(path.join(dir,`${run}.json`),'utf8');
    const archive=await readArchive((await this.file(root,id)).replace(/\.json$/,''));const summary=archive.rows.find(row=>row.run===run);
    if(!summary||summary.traceHash&&createHash('sha256').update(text).digest('hex')!==summary.traceHash)throw new Error('분석 실행 기록 파일이 변경되었습니다.');
    const trace=JSON.parse(text) as Trace;
    if(trace.run!==run||trace.seed!==((job.options.seed+run)>>>0)||trace.state.contentFingerprint!==job.fingerprint)throw new Error('실행 기록과 분석 작업이 일치하지 않습니다.');return trace;
  }
  async replay(root:string,id:string,run:number,step:number){
    const {job,content}=await this.stored(root,id,false),trace=await this.run(root,id,run);
    if(!Number.isSafeInteger(step)||step<0||step>trace.commands.length)throw new Error('재생 명령 위치가 범위를 벗어납니다.');
    const first=createRun(content,trace.seed,{mode:'full'});if(!first.ok)throw new Error(first.error.message);
    const prefix=applyCommands(content,first.state,trace.commands.slice(0,Math.max(0,step-1)));if(!prefix)throw new Error('기록된 명령을 재현하지 못했습니다.');
    const beforeState=prefix.state;let state=beforeState,transition=null;
    if(step){const next=dispatchCommand(content,beforeState,trace.commands[step-1]!,beforeState.stateRevision);if(!next.ok)throw new Error(next.error.message);state=next.state;transition=next.transition;}
    if(step===trace.commands.length&&JSON.stringify(state)!==JSON.stringify(trace.state))throw new Error('기록된 최종 상태와 재생 결과가 다릅니다.');
    return {run,seed:trace.seed,fingerprint:job.fingerprint,step,total:trace.commands.length,state,beforeState,transition};
  }
}

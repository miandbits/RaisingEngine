import {fork,execFile} from 'node:child_process';
import {promisify} from 'node:util';
import {randomUUID,createHash} from 'node:crypto';
import {mkdir,realpath,readFile,readdir,writeFile,unlink,stat,lstat,open} from 'node:fs/promises';
import path from 'node:path';
import {listGoalRoutes,readGoalRouteBytes} from './goal-routes.mjs';
import {archiveGoalImpacts} from './goal-impact.mjs';
const hash=value=>createHash('sha256').update(typeof value==='string'?value:JSON.stringify(value)).digest('hex');
const uuid=/^[a-f0-9-]{36}$/;
const MAX_INPUT=64*1024*1024, RETAIN=8;
export function batchLimits(input={}){
  if(!input||typeof input!=='object'||Array.isArray(input)||Object.keys(input).some(k=>!['routeTimeoutMs','batchTimeoutMs'].includes(k)))throw Error('Invalid route batch limits.');
  const value={routeTimeoutMs:120000,batchTimeoutMs:900000,...input};
  for(const [key,max] of [['routeTimeoutMs',300000],['batchTimeoutMs',3600000]])if(!Number.isSafeInteger(value[key])||value[key]<1000||value[key]>max)throw Error(`Invalid ${key}: use 1,000–${max} milliseconds.`);
  return value;
}
async function directory(root){const base=await realpath(root),studio=path.join(base,'.studio');await mkdir(studio,{recursive:true});if(await realpath(studio)!==studio)throw Error('Route batch storage must stay inside the project.');const dir=path.join(studio,'route-batches');await mkdir(dir,{recursive:true});if(await realpath(dir)!==dir)throw Error('Route batch storage cannot be a link.');return dir;}
const identity=entries=>entries.map(e=>({id:e.route?.id??path.basename(e.file,'.json'),revision:e.revision??null,sourceHash:e.sourceHash,error:e.error??null}));
async function capture(root,ids){const entries=await listGoalRoutes(root,{ids,maxBytes:MAX_INPUT});for(const entry of entries){const real=await realpath(entry.file);if(real!==entry.file)throw Error('Route batch inputs cannot be links.');if((await stat(real)).size>4*1024*1024){entry.sourceHash='oversized';continue;}const bytes=await readGoalRouteBytes(real);if(bytes.length>4*1024*1024)throw Error('Route changed while preparing the batch.');entry.sourceHash=hash(bytes.toString('utf8'));if(entry.revision&&entry.sourceHash!==entry.revision)throw Error('Route changed while preparing the batch.');}return entries;}

// Owned reports always begin with these three fields. Read only their fixed
// header for retention; a valid failure may include a multi-megabyte command.
async function ownedReport(file,name){
  if(!(await lstat(file)).isFile())return false;
  const handle=await open(file,'r');
  try{const bytes=Buffer.alloc(512);const {bytesRead}=await handle.read(bytes,0,bytes.length,0);const match=/^\{\s*"version"\s*:\s*1,\s*"kind"\s*:\s*"goal-route-batch",\s*"id"\s*:\s*"([a-f0-9-]{36})",/.exec(bytes.subarray(0,bytesRead).toString('utf8'));return !!match&&name.endsWith('-'+match[1]+'.json');}
  finally{await handle.close();}
}
async function retireProcess(child){
  if(!child.pid)return;
  // Every batch has its own process group. A hard timeout must retire a native
  // child even when it is stuck inside a command and cannot observe closed stdin.
  if(process.platform==='win32'){
    try{await promisify(execFile)(path.join(process.env.SystemRoot??'C:\\Windows','System32','taskkill.exe'),['/PID',String(child.pid),'/T','/F'],{windowsHide:true,timeout:5000});}
    catch(error){throw Error('Could not verify termination of the owned route process tree: '+error.message);}
  }else{try{process.kill(-child.pid,'SIGKILL');}catch(error){if(error.code!=='ESRCH')throw error;}}
  if(child.exitCode===null&&child.signalCode===null)await new Promise(resolve=>{const timer=setTimeout(resolve,2000);child.once('exit',()=>{clearTimeout(timer);resolve();});});
  if(child.exitCode===null&&child.signalCode===null)throw Error('The owned route process did not exit after termination.');
}
export class GoalRouteBatches {
  jobs=new Map(); starting=false;
  async prepare(root,params={}){
    const base=await realpath(root),ids=params.ids??[];
    if(!Array.isArray(ids)||ids.length>128||ids.some(id=>typeof id!=='string'||!/^[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}$/.test(id))||new Set(ids).size!==ids.length)throw Error('Choose distinct valid route IDs.');
    return {root:base,entries:await capture(base,ids)};
  }
  async start(root,content,sourceVersion,revision,params={},guard=async()=>{},prepared=null){
    if(this.starting||[...this.jobs.values()].some(j=>j.status==='running'||j.status==='cancelling'))throw Error('A route batch is already running. Wait or cancel it first.');
    this.starting=true;
    try{
      const limits=batchLimits(params.limits),base=await realpath(root);
      const ids=params.ids??[];
      if(!Array.isArray(ids)||ids.length>128||ids.some(id=>typeof id!=='string'||!/^[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}$/.test(id))||new Set(ids).size!==ids.length)throw Error('Choose distinct valid route IDs.');
      const entries=prepared?structuredClone(prepared.entries):await capture(base,ids);
      if(prepared&&prepared.root!==base)throw Error('Captured routes belong to another project.');
      if(!entries.length||ids.length&&entries.length!==ids.length)throw Error('One or more selected routes are missing. Refresh the route list.');
      if(params.routeRevisions!==undefined){const expected=params.routeRevisions;if(!expected||typeof expected!=='object'||Array.isArray(expected)||Object.keys(expected).length!==entries.length||entries.some(e=>expected[e.route?.id??path.basename(e.file,'.json')]!==(e.revision??null)))throw Error('Routes changed. Refresh the route list before starting.');}
      const impact=prepared?.impact??null;
      const inputs=JSON.stringify({content,entries,...(impact?{impact}:{})});if(Buffer.byteLength(inputs)>MAX_INPUT)throw Error('A route batch snapshot and routes must fit within 64 MiB. Select fewer routes.');
      const dir=await directory(base);await guard();
      // Bound only this feature's owned archives; unrelated automatic reports are retained.
      const files=[];
      for(const name of (await readdir(dir)).filter(f=>/^\d+-[a-f0-9-]{36}\.json$/.test(f)).sort()){
        if(await ownedReport(path.join(dir,name),name))files.push(name);
      }
      for(const file of files.slice(0,Math.max(0,files.length-RETAIN+1))){await unlink(path.join(dir,file));const input=path.join(dir,file.replace(/\.json$/,'.inputs.json'));const info=await lstat(input).catch(e=>{if(e.code!=='ENOENT')throw e;return null;});if(info?.isFile())await unlink(input);}
      while(this.jobs.size>=RETAIN){const old=[...this.jobs.values()].find(j=>!['running','cancelling'].includes(j.status));if(!old)break;this.jobs.delete(old.id);}
      const id=randomUUID(),stem=Date.now()+'-'+id,file=path.join(dir,stem+'.json');
      const job={version:1,kind:'goal-route-batch',id,root:base,revision,testedRevision:revision,sourceVersion,contentFingerprint:content.contentFingerprint,snapshotHash:hash(content),inputHash:hash(inputs),routeSelection:identity(entries),limits,status:'running',ok:false,createdAt:new Date().toISOString(),activeIndex:null,completed:0,reports:entries.map(e=>({id:e.route?.id??path.basename(e.file,'.json'),name:e.route?.name??path.basename(e.file),routeRevision:e.revision??null,status:'not-run',ok:false})),file};
      const inputFile=file.replace(/\.json$/,'.inputs.json');
      await writeFile(inputFile,inputs,{flag:'wx'});
      try{await guard();const fresh=new Map(identity(prepared?entries:await capture(base,ids)).map(e=>[e.id,e]));if(job.routeSelection.some(e=>JSON.stringify(fresh.get(e.id))!==JSON.stringify(e)))throw Error('Routes changed while preparing the batch. Refresh and try again.');}catch(error){await unlink(inputFile).catch(()=>{});throw error;}
      let worker;try{worker=fork(new URL('./goal-route-batch-worker.mjs',import.meta.url),[],{detached:process.platform!=='win32',windowsHide:true,serialization:'advanced',stdio:['ignore','ignore','pipe','ipc'],execArgv:['--max-old-space-size=512'],env:{...process.env,NODE_OPTIONS:''}});}catch(error){await unlink(inputFile).catch(()=>{});throw error;}
      job.worker=worker;job.completion=new Promise(resolve=>{job.resolveCompletion=resolve;});this.jobs.set(id,job);
      const finish=async(reason)=>{
        if(job.finished)return;job.finished=true;clearTimeout(job.timer);clearTimeout(job.grace);clearTimeout(job.routeTimer);
        if(job.activeIndex!==null&&job.reports[job.activeIndex].status==='running')Object.assign(job.reports[job.activeIndex],{status:reason==='cancelled'?'cancelled':reason==='timed-out'?'timed-out':'failed',failure:{step:0,code:reason==='cancelled'?'CANCELLED':reason==='timed-out'?'TIMEOUT':'WORKER_ERROR',message:job.error??'The route did not complete.'}});
        job.activeIndex=null;job.completed=job.reports.filter(r=>!['not-run','running'].includes(r.status)).length;
        const terminalStatus=reason??(job.reports.every(r=>r.ok)?'completed':job.reports.some(r=>r.status==='timed-out')?'timed-out':'failed');
        const terminalOk=terminalStatus==='completed'&&job.reports.every(r=>r.ok);job.finishedAt=new Date().toISOString();
        let completionError=null;
        if(job.goalImpact?.id){try{job.goalImpact=await archiveGoalImpacts(base,job.goalImpact);}catch(error){job.goalImpact={...job.goalImpact,stale:true,complete:false,error:'Could not save comparison report: '+error.message};}}
        try{await retireProcess(worker);}catch(error){completionError=String(error.message);}
        if(completionError)job.error=completionError;
        job.persistence=writeFile(file,JSON.stringify({...this.public(job),status:completionError?'failed':terminalStatus,ok:!completionError&&terminalOk},null,2),{flag:'wx'}).catch(e=>{completionError='Could not save batch report: '+e.message;});
        await job.persistence;
        job.status=completionError?'failed':terminalStatus;job.ok=!completionError&&terminalOk;if(completionError)job.error=completionError;
        job.resolveCompletion();job.worker=null;job.finish=null;job.resolveCompletion=null;
      };
      job.finish=finish;
      worker.on('message',message=>{if(job.finished)return;if(message.fatal){job.error=message.fatal;void finish(job.stopReason??'failed');return;}if(message.active!==undefined){job.activeIndex=message.active;job.reports[message.active].status='running';clearTimeout(job.routeTimer);job.routeTimer=setTimeout(()=>{job.error='A route exceeded its deadline without yielding. Remaining routes were not run.';void finish('timed-out');},limits.routeTimeoutMs+2000);job.routeTimer.unref();}
        if(message.stage)job.stage=message.stage;
        if(message.goalImpact)job.goalImpact=message.goalImpact;
        if(message.progress)Object.assign(job.reports[message.index],message.progress);
        if(message.report){clearTimeout(job.routeTimer);if(job.stopReason==='timed-out'&&message.report.failure?.code==='CANCELLED')message.report={...message.report,status:'timed-out',failure:{...message.report.failure,code:'TIMEOUT',message:'The whole batch deadline was reached.'}};const old=job.reports[message.index];job.reports[message.index]={...old,...message.report};job.completed++;job.activeIndex=null;}
        if(message.done)void finish(job.stopReason??message.reason);
      });
      worker.stderr.on('data',data=>{job.workerError=((job.workerError??'')+data.toString()).slice(-4096);});
      worker.on('error',error=>{job.error=error.message;void finish(job.stopReason??'failed');});
      worker.on('exit',code=>{if(!job.finished){job.error=`Route worker exited without a result (${code}). `+(job.workerError??'');void finish(job.stopReason??'failed');}});
      try{worker.send({content,entries,limits,...(impact?{impact}:{})},error=>{if(error&&!job.finished){job.error=error.message;void finish('failed');}});}catch(error){job.error=error.message;void finish('failed');}
      job.timer=setTimeout(()=>this.stopJob(job,'timed-out'),limits.batchTimeoutMs);job.timer.unref();
      return this.public(job);
    }finally{this.starting=false;}
  }
  public(job){const {worker,workerError,timer,routeTimer,grace,finish,finished,persistence,completion,resolveCompletion,routeCheckAt,routeCheck,routeCheckComplete,routesChanged,routeCheckError,...view}=job;return structuredClone(view);}
  get(root,id){const job=this.jobs.get(id);if(!uuid.test(id??'')||!job||job.root!==root)throw Error('This route batch does not belong to the current project.');return job;}
  stopJob(job,reason='cancelled'){if(job.finished||!['running','cancelling'].includes(job.status))return;job.stopReason??=reason;job.status='cancelling';if(job.worker?.connected)job.worker.send({cancel:true},()=>{});if(!job.grace){job.grace=setTimeout(()=>void job.finish(job.stopReason),2000);job.grace.unref();}}
  stop(root,id){const job=this.get(root,id);this.stopJob(job);return this.public(job);}
  async status(root,id,sourceVersion){
    const job=this.get(root,id);
    // Hash content, not only mtimes. Poll at most once every two seconds and once
    // again on completion, so a large route collection does not monopolize I/O.
    const check=()=>{const finishedAtStart=!!job.finished;job.routeCheck=(async()=>{try{const current=await capture(root,job.routeSelection.map(e=>e.id)),revisions=new Map(identity(current).map(e=>[e.id,e]));job.routesChanged=job.routeSelection.some(e=>JSON.stringify(revisions.get(e.id))!==JSON.stringify(e));job.routeCheckError=null;}catch(error){job.routesChanged=true;job.routeCheckError=String(error.message).slice(0,4096);}job.routeCheckAt=Date.now();job.routeCheckComplete=finishedAtStart;})().finally(()=>{job.routeCheck=null;});};
    if(!job.routeCheck&&(Date.now()-(job.routeCheckAt??0)>=2000||job.finished&&!job.routeCheckComplete))check();
    if(job.finished){if(job.routeCheck)await job.routeCheck;if(!job.routeCheckComplete){check();await job.routeCheck;}}
    return {...this.public(job),stale:job.sourceVersion!==sourceVersion||!!job.routesChanged,routesChanged:!!job.routesChanged,...(job.routeCheckError?{routeCheckError:job.routeCheckError}:{})};
  }
  async close(){this.beforeClose?.();for(const job of this.jobs.values())this.stopJob(job);await Promise.all([...this.jobs.values()].map(job=>job.completion));await this.afterClose?.();}
}

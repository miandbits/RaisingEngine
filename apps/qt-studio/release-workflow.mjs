import {realpath} from 'node:fs/promises';
import {randomUUID} from 'node:crypto';
import {fork,execFile} from 'node:child_process';
import {promisify} from 'node:util';
import path from 'node:path';
import {privateDirectory,atomicJSON,digest} from './project-files.mjs';
import {boundedBytes,releaseManifest,sampleBytes} from './release-workflow-checks.mjs';
import {acquireAuthoringLock} from './authoring-lock.mjs';

const targets={web:'Web',macos:'macOS',windows:'Windows',ios:'iOS'};
const stamp=()=>new Date().toISOString();
const entryVersion=entry=>digest(JSON.stringify({...entry,entryRevision:undefined}));
const label=(value,fallback)=>{const text=value??fallback;if(typeof text!=='string'||!text.trim()||text.length>160)throw Error('RELEASE_NAME: Use a name of 1–160 characters.');return text.trim();};
const pending=()=>({status:'pending',message:'Not checked.'});
const alive=pid=>{if(!Number.isInteger(pid)||pid<=0)return false;try{process.kill(pid,0);return true;}catch(error){return error.code!=='ESRCH';}};
async function retireProcess(child){
 if(!child?.pid)return;
 if(process.platform==='win32'){try{await promisify(execFile)(path.join(process.env.SystemRoot??'C:\\Windows','System32','taskkill.exe'),['/PID',String(child.pid),'/T','/F'],{windowsHide:true,timeout:5000});}catch(error){throw Error('Could not verify retirement of the owned release process tree: '+error.message);}}
 else try{process.kill(-child.pid,'SIGKILL');}catch(error){if(error.code!=='ESRCH')throw error;}
 if(child.exitCode===null&&child.signalCode===null)await new Promise(resolve=>{const timer=setTimeout(resolve,2000);child.once('exit',()=>{clearTimeout(timer);resolve();});});
 if(child.exitCode===null&&child.signalCode===null)throw Error('The owned release worker did not stop.');
}
function distributionRows(target){return [
 {id:'playthrough',label:'Interactive load → progress → save → relaunch on this target'},
 {id:'device',label:target==='ios'?'Physical iPhone / iPad acceptance':'Acceptance on supported target hardware'},
 {id:'accessibility',label:'Accessibility and assistive technology review'},
 {id:'rights',label:'License, notices and asset rights review'},
 {id:'branding',label:'App name, icon and release branding review'},
 {id:'distribution',label:target==='web'?'Hosting and delivery configuration review':target==='ios'?'Signing, provisioning and store distribution review':'Signing, installer and delivery review'},
 ].map(row=>({...row,status:'pending',kind:'manual',note:''}));}

/** Private, local acceptance evidence. No samples or ledger paths enter Game.zip. */
export class ReleaseWorkflow {
 jobs=new Map();workers=new Map();settlements=new Set();
 root(editor){editor.current(editor.revision);if(!editor.root)throw Error('RELEASE_NO_PROJECT: Open a project first.');return editor.root;}
 async storage(root){return path.join(await privateDirectory(root,'release-workflow'),'ledger.json');}
 async read(root){const file=await this.storage(root);let value;try{value=JSON.parse((await boundedBytes(file)).toString('utf8'));}catch(e){if(e.code==='ENOENT')return {version:1,releases:[]};throw e;}
  if(value.version!==1||!Array.isArray(value.releases)||value.releases.length>200)throw Error('RELEASE_LEDGER: Invalid local release ledger.');
  for(const e of value.releases)if(!/^[a-f0-9]{64}$/.test(e.id)||entryVersion(e)!==e.entryRevision||e.identity?.manifestSha256!==e.id||!Array.isArray(e.cases)||e.cases.length>200)throw Error('RELEASE_LEDGER: Release record changed outside the workflow.');return value;
 }
 async locked(root,action){const directory=await privateDirectory(root,'release-workflow'),release=await acquireAuthoringLock(path.join(directory,'writers'));
  try{const ledger=await this.read(root),result=await action(ledger);for(const entry of ledger.releases)entry.entryRevision=entryVersion(entry);if(Buffer.byteLength(JSON.stringify(ledger))>16*1024*1024)throw Error('RELEASE_LEDGER_LIMIT: Private evidence metadata exceeds 16 MiB. Keep a private backup before starting another project ledger.');await atomicJSON(path.join(directory,'ledger.json'),ledger);return result;}
  finally{await release();}
 }
 async list(editor){const root=this.root(editor),ledger=await this.read(root),releases=structuredClone(ledger.releases);
  for(const entry of releases){for(const check of [...Object.values(entry.checks),...entry.cases.map(c=>c.check)])if(check.status==='running'&&!this.jobs.has(check.jobId)){if(alive(check.ownerPid))check.message='Another editor is checking this release.';else{check.status='stale';check.message='The previous process stopped before this check completed. Run it again.';}}}
  return {version:1,storage:await this.storage(root),scope:'Private local ledger. Original saves are referenced, never included in game archives. Back up this ledger and the referenced originals privately.',releases,jobs:[...this.jobs.values()].filter(j=>j.projectRoot===root).map(j=>this.publicJob(j)),readiness:'Evidence only. No automatic distribution, legal, physical-device or commercial-readiness approval.'};
 }
 current(ledger,params){const entry=ledger.releases.find(e=>e.id===params.releaseId);if(!entry)throw Error('RELEASE_NOT_FOUND: Select a registered release.');if(params.entryRevision!==entry.entryRevision)throw Error('RELEASE_ENTRY_STALE: Refresh this release before changing its evidence.');if([...this.jobs.values()].some(j=>j.releaseId===entry.id&&j.status==='running')||[...Object.values(entry.checks),...entry.cases.map(c=>c.check)].some(c=>c.status==='running'&&alive(c.ownerPid)))throw Error('RELEASE_BUSY: Wait for or cancel this release check first.');return entry;}
 async register(editor,params){const root=this.root(editor),selected=await releaseManifest(params.directory),game=editor.documents.find(d=>d.kind==='project')?.data;
  if(selected.manifest.gameId&&selected.manifest.gameId!==game.gameId)throw Error('RELEASE_GAME: This export belongs to another game.');
  const result=await this.locked(root,ledger=>{const existing=ledger.releases.find(e=>e.id===selected.sha256);if(existing){
   if(existing.directory!==selected.directory){this.current(ledger,{releaseId:existing.id,entryRevision:existing.entryRevision});existing.previousLocations??=[];existing.previousLocations.push({directory:existing.directory,changedAt:stamp()});existing.directory=selected.directory;existing.integrityKnownStale=true;
    for(const check of [...Object.values(existing.checks),...existing.cases.map(c=>c.check)])if(check.status!=='pending')Object.assign(check,{previousStatus:check.status,status:'stale',invalidatedAt:stamp(),message:'The release was explicitly registered at a new location. Verify this copy before reusing its acceptance evidence.'});
   }return existing;
  }
   if(ledger.releases.length>=200)throw Error('RELEASE_LIMIT: This project already records 200 releases.');
   const m=selected.manifest,entry={id:selected.sha256,name:label(params.name,`${m.version} · ${targets[m.target]}`),directory:selected.directory,gameId:game.gameId,registeredAt:stamp(),identity:{version:m.version,target:m.target,contentFingerprint:m.contentFingerprint,...(m.sourceContentFingerprint?{sourceContentFingerprint:m.sourceContentFingerprint}:{}),engineVersion:m.engineVersion??'unknown',engineIdentity:m.engineIdentity??{source:{commit:null,origin:'not-recorded'},note:'This historical release did not record engine source identity.'},manifestSha256:selected.sha256},cases:[],checks:{release:pending(),recovery:pending()},distribution:distributionRows(m.target)};
   ledger.releases.push(entry);return entry;
  });return structuredClone(result);
 }
 async addCase(editor,params){const root=this.root(editor);if(typeof params.file!=='string'||!path.isAbsolute(params.file))throw Error('RELEASE_SAVE_FILE: Select an original save file.');
  const file=await realpath(params.file),sha256=digest(await sampleBytes(file));let sourceProject;if(params.sourceProject){if(typeof params.sourceProject!=='string'||!path.isAbsolute(params.sourceProject))throw Error('RELEASE_SOURCE: Select the previous source project folder.');sourceProject=await realpath(params.sourceProject);}
  const kind=params.kind??'original';if(!['original','migrated'].includes(kind))throw Error('RELEASE_CASE_KIND: Choose an original or designer-labelled migrated sample.');
  const result=await this.locked(root,ledger=>{const entry=this.current(ledger,params);if(entry.cases.length>=200)throw Error('RELEASE_LIMIT: This release already records 200 save cases.');const name=label(params.name);if(entry.cases.some(c=>c.name===name))throw Error('RELEASE_CASE_NAME: Use a distinct case name.');entry.cases.push({id:randomUUID(),name,file,sha256,kind,...(sourceProject?{sourceProject}:{}),registeredAt:stamp(),check:pending(),provenance:kind==='migrated'?'Designer-labelled converted sample; conversion provenance is not inferred.':'Original file reference; verification rejects changed bytes.'});return entry;});return structuredClone(result);
 }
 publicJob(job){const {projectRoot,finishing,...out}=job;return structuredClone(out);}
 async start(editor,params){const root=this.root(editor),kind=params.kind;if(!['release','save','recovery'].includes(kind))throw Error('RELEASE_JOB_KIND: Choose release, save or recovery.');if(params.destination&&(typeof params.destination!=='string'||!path.isAbsolute(params.destination)))throw Error('RELEASE_RESTORE_DESTINATION: Choose a new absolute destination folder.');
  const id=randomUUID(),job={id,releaseId:params.releaseId,projectRoot:root,kind,status:'running',startedAt:stamp(),message:'Starting background verification.',progress:{done:0,total:0},...(params.caseId?{caseId:params.caseId}:{}),...(params.destination?{destination:params.destination}:{})};
  const entry=await this.locked(root,ledger=>{const e=this.current(ledger,params);if(kind==='save'&&(!e.cases.length||params.caseId&&!e.cases.some(c=>c.id===params.caseId)))throw Error('RELEASE_SAVE_CASE: Add or select a named save case.');const state={status:'running',jobId:id,ownerPid:process.pid,message:job.message};
   if(kind==='save'){for(const c of e.cases)if(!params.caseId||c.id===params.caseId)c.check={...c.check,...state};}
   else e.checks[kind==='release'?'release':'recovery']=state;return e;});
  this.jobs.set(id,job);
  try{const worker=fork(new URL('./release-workflow-worker.mjs',import.meta.url),[],{detached:process.platform!=='win32',windowsHide:true,serialization:'advanced',stdio:['ignore','ignore','pipe','ipc'],execArgv:['--max-old-space-size=512'],env:{...process.env,NODE_OPTIONS:''}});this.workers.set(id,worker);worker.stderr.resume();
   worker.on('message',message=>{if(job.status!=='running')return;if(message.progress){job.progress=message.progress;job.message=message.progress.message;}else if(message.result)this.complete(job,entry.entryRevision,message.result);else if(message.error)this.complete(job,entry.entryRevision,null,message.error.message);});
   worker.on('error',error=>{if(job.status==='running')this.complete(job,entry.entryRevision,null,error.message);});
   worker.on('exit',()=>{if(job.status==='running'&&!job.finishing)this.complete(job,entry.entryRevision,null,'Background check stopped without a result.');else if(job.status!=='running')this.workers.delete(id);});
   worker.send({entry:structuredClone(entry),kind,caseId:params.caseId,destination:params.destination,projectRoot:root},error=>{if(error)this.complete(job,entry.entryRevision,null,error.message);});
  }catch(error){await this.finish(job,entry.entryRevision,null,error.message);}
  return this.publicJob(job);
 }
 complete(...args){const job=args[0],promise=this.finish(...args);this.settlements.add(promise);void promise.catch(error=>{job.status='failed';job.message=error.message;}).finally(()=>this.settlements.delete(promise));}
 async finish(job,revision,result,error){if(job.status!=='running'||job.finishing)return;job.finishing=true;
  let terminal;try{await retireProcess(this.workers.get(job.id));this.workers.delete(job.id);await this.locked(job.projectRoot,ledger=>{const e=ledger.releases.find(e=>e.id===job.releaseId);if(!e||e.entryRevision!==revision){terminal={status:'stale',message:'Release evidence changed while checking; results were not accepted.'};return;}
   if(error){const status=/STALE/.test(error)?'stale':'failed',check={status,checkedAt:stamp(),message:error};
    if(error.startsWith('RELEASE_INTEGRITY:')){e.integrityKnownStale=true;e.checks.release=check;for(const dependent of [e.checks.recovery,...e.cases.map(c=>c.check)])if(dependent.status!=='pending')Object.assign(dependent,{previousStatus:dependent.status,status:'stale',invalidatedAt:stamp(),message:'The selected release no longer verifies. Recheck this evidence after restoring its exact bytes.'});}
    if(/RELEASE_(STALE|ARCHIVE|BUNDLE|GAME|MANIFEST|FILE)/.test(error))e.checks.release=check;
    if(job.kind==='save'){for(const c of e.cases)if(!job.caseId||c.id===job.caseId)c.check={...c.check,...check};}
    else e.checks[job.kind==='release'?'release':'recovery']=check;terminal={status,message:error};}
   else{e.integrityKnownStale=false;e.checks.release=result.release;if(result.cases)for(const c of result.cases)e.cases.find(item=>item.id===c.id).check=c.check;if(result.recovery)e.checks.recovery=result.recovery;terminal={status:'completed',message:'Check completed. Review individual results and remaining manual acceptance.',result};}
  });Object.assign(job,terminal);}finally{job.finishedAt=stamp();delete job.finishing;}
 }
 async status(editor,params){const root=this.root(editor),job=this.jobs.get(params.jobId);if(!job||job.projectRoot!==root)throw Error('RELEASE_JOB_NOT_FOUND: This job belongs to another or closed project.');return this.publicJob(job);}
 async cancel(editor,params){this.root(editor);const job=this.jobs.get(params.jobId);if(!job||job.projectRoot!==editor.root)throw Error('RELEASE_JOB_NOT_FOUND: Select a job in this project.');return this.cancelJob(job);}
 async cancelJob(job){if(job.status!=='running')return this.publicJob(job);if(job.finishing)throw Error('RELEASE_BUSY: The check is recording its result.');job.status='cancelled';job.message='Check cancelled; no acceptance result recorded. A recovery folder already published before cancellation is preserved for inspection.';job.finishedAt=stamp();try{await retireProcess(this.workers.get(job.id));}catch(error){job.status='failed';job.message=error.message;}this.workers.delete(job.id);
  await this.locked(job.projectRoot,ledger=>{const e=ledger.releases.find(e=>e.id===job.releaseId);if(!e)return;for(const c of [...Object.values(e.checks),...e.cases.map(c=>c.check)])if(c.jobId===job.id)Object.assign(c,{status:'cancelled',message:job.message,checkedAt:stamp()});});return this.publicJob(job);
 }
 async distribution(editor,params){const root=this.root(editor);if(!['pending','passed','failed','not-applicable'].includes(params.status)||typeof params.note!=='string'||params.note.length>4000||params.status!=='pending'&&!params.note.trim())throw Error('RELEASE_ATTESTATION: Record a reason or evidence note for manual acceptance.');
  const result=await this.locked(root,async ledger=>{const e=this.current(ledger,params),selected=await releaseManifest(e.directory);if(selected.sha256!==e.id)throw Error('RELEASE_STALE: The registered release receipt changed.');const row=e.distribution.find(r=>r.id===params.checkId);if(!row)throw Error('RELEASE_CHECK: Select a distribution check.');Object.assign(row,{status:params.status,note:params.note.trim(),updatedAt:stamp(),kind:'manual'});return e;});return structuredClone(result);
 }
 async close(){await Promise.all([...this.jobs.values()].filter(j=>j.status==='running'&&!j.finishing).map(j=>this.cancelJob(j)));await Promise.allSettled([...this.settlements]);}
}

import {randomUUID,createHash} from 'node:crypto';
import path from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
import {goalBaselineInputStamp} from './goal-impact.mjs';

const busy=job=>['running','cancelling'].includes(job.status);
const digest=value=>createHash('sha256').update(JSON.stringify(value)).digest('hex');

/** One coalescing saved-content queue, sharing the manual batch supervisor.
 * Saving captures inputs; replay and comparison never execute on the service's
 * event loop. A newer save supersedes only automatic work, never a manual run. */
export class SavedGoalChecks {
  latest=null; pending=null; draining=null; epoch=0;
  constructor(batches){
    this.batches=batches;
    batches.beforeClose=()=>{this.epoch++;this.stop();};
    batches.afterClose=async()=>{await this.draining;};
  }
  async enqueue(root,content,sourceVersion,revision){
    const prepared=await this.batches.prepare(root);
    if(!prepared.entries.length){this.stop();this.latest=null;return null;}
    const baselineStamp=await goalBaselineInputStamp(root);
    const identity=digest({sourceVersion,routes:prepared.entries.map(e=>[e.file,e.sourceHash]),baselineStamp});
    // Repeated saves of exactly the same inputs do not restart useful work.
    const impact=this.latest?.goalImpact,reusableComparison=!impact?.error&&!impact?.archiveError&&impact?.archived!==false&&!impact?.counts?.error&&!impact?.counts?.incomplete;
    if(this.latest?.root===root&&this.latest.identity===identity&&(['queued','running'].includes(this.latest.status)||this.latest.status==='completed'&&this.batches.jobs.has(this.latest.batchId)&&reusableComparison))return this.public(this.latest);
    this.stop();
    const record={id:randomUUID(),root,origin:'save',status:'queued',stage:'routes',revision,testedRevision:revision,sourceVersion,baselineStamp,contentFingerprint:content.contentFingerprint,createdAt:new Date().toISOString(),ok:false,completed:0,stale:false,limits:{routeTimeoutMs:120000,batchTimeoutMs:900000},reports:prepared.entries.map(e=>({id:e.route?.id??path.basename(e.file,'.json'),name:e.route?.name??path.basename(e.file),routeRevision:e.revision??null,status:'not-run',ok:false})),identity,content,prepared:{...prepared,impact:{root,revision,baselineStamp}},epoch:this.epoch};
    this.latest=record;this.pending=record;
    this.kick();
    return this.public(record);
  }
  failed(root,sourceVersion,revision,error){
    this.stop();
    this.latest={id:randomUUID(),root,origin:'save',status:'failed',revision,testedRevision:revision,sourceVersion,ok:false,completed:0,reports:[],error:'Project saved, but background checks could not start: '+error.message,stale:false};
    return this.public(this.latest);
  }
  public(record){
    if(!record)return null;
    const {identity,content,prepared,epoch,...view}=record;
    return structuredClone(view);
  }
  kick(){
    if(this.draining)return;
    this.draining=this.drain().finally(()=>{this.draining=null;if(this.pending)this.kick();});
  }
  async drain(){
    while(this.pending){
      const record=this.pending;this.pending=null;
      // The manual supervisor can also be capturing inputs before a job exists.
      while(this.batches.starting&&record.epoch===this.epoch&&record===this.latest&&record.status==='queued')await delay(20);
      const active=[...this.batches.jobs.values()].filter(busy);
      if(active.length)await Promise.all(active.map(job=>job.completion));
      if(record.epoch!==this.epoch||record!==this.latest||record.status!=='queued')continue;
      try{
        const job=await this.batches.start(record.root,record.content,record.sourceVersion,record.revision,{limits:record.limits},async()=>{
          if(record.epoch!==this.epoch||record!==this.latest||record.status!=='queued')throw Error('Saved check superseded before launch.');
        },record.prepared);
        record.batchId=job.id;
        // Cancellation/project replacement can arrive after start's last guard
        // while it is handing its worker back. Retire that newly owned worker.
        if(record.epoch!==this.epoch||record!==this.latest||record.status!=='queued'){
          this.batches.stop(record.root,job.id);record.status='cancelling';
        }else record.status='running';
        delete record.content;delete record.prepared;
        await this.batches.get(record.root,job.id).completion;
        const finished=this.batches.public(this.batches.get(record.root,job.id));
        Object.assign(record,finished,{id:record.id,batchId:job.id,origin:'save'});
      }catch(error){record.status=record.status==='cancelled'?'cancelled':'failed';record.ok=false;record.error=String(error.message);delete record.content;delete record.prepared;}
    }
  }
  stop(expectedId){
    const record=this.latest;
    if(!record)return null;
    // A deferred UI cancellation owns the check that was visible when clicked.
    // A newer save may have replaced it while another bridge request completed.
    if(expectedId!==undefined&&record.id!==expectedId)return this.public(record);
    if(record.status==='queued'){record.status='cancelled';record.ok=false;delete record.content;delete record.prepared;if(this.pending===record)this.pending=null;}
    else if(record.batchId&&busy(record)){this.batches.stop(record.root,record.batchId);record.status='cancelling';}
    return this.public(record);
  }
  async status(root,sourceVersion){
    const record=this.latest;
    if(!record||record.root!==root)return null;
    let result=this.public(record);
    if(record.batchId&&this.batches.jobs.has(record.batchId)){
      const batch=await this.batches.status(root,record.batchId,sourceVersion);
      result={...result,...batch,id:record.id,batchId:batch.id,origin:'save'};
    }else if(record.batchId){
      result.historyExpired=true;result.stale=true;
      result.error='This check is outside the retained batch history. Save again to create a fresh report.';
    }
    if(record.baselineStamp){
      try{result.baselinesChanged=await goalBaselineInputStamp(root)!==record.baselineStamp;}
      catch(error){result.baselinesChanged=true;result.baselineCheckError=String(error.message);}
    }
    result.stale=!!result.stale||record.sourceVersion!==sourceVersion||!!result.goalImpact?.stale||!!result.baselinesChanged;
    return result;
  }
}

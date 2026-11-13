import {randomUUID} from 'node:crypto';
import {mkdir,realpath,lstat,readdir,readFile,writeFile,rename,unlink,rmdir} from 'node:fs/promises';
import path from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';

const busy=()=>Error('Another editor is saving player priorities. Refresh and try again.');
const alive=pid=>{try{process.kill(pid,0);return true;}catch(error){return error.code!=='ESRCH';}};
const absent=error=>{if(error.code!=='ENOENT')throw error;};
const transientLeaseRead=error=>['ENOENT','EPERM','EACCES','EBUSY'].includes(error.code);
async function removeLease(directory){
 for(const name of ['ticket.json','ticket.tmp'])await unlink(path.join(directory,name)).catch(absent);
 await rmdir(directory).catch(error=>{if(!['ENOENT','ENOTEMPTY'].includes(error.code))throw error;});
}
/** A short, cooperative local-project writer lock. Each process owns a unique
 * lease, so reclaiming a dead process never unlinks a newer writer's lock.
 * Ticket selection follows the bakery protocol: a preparing live participant
 * blocks entry; later participants choose a ticket above the current owner.
 * Brief asynchronous waits let another writer finish; a busy UI can retry.
 */
export async function acquireAuthoringLock(directory){
 await mkdir(directory,{recursive:true});if(await realpath(directory)!==directory)throw Error('Authoring lock storage cannot be linked.');
 const id=process.pid+'_'+randomUUID(),own=path.join(directory,id);await mkdir(own);
 async function participants(){
  const entries=await readdir(directory,{withFileTypes:true}),result=[];
  // Reclaim dead leases before enforcing the live-writer bound. A crash backlog
  // must not prevent its own cleanup; very large backlogs retire in bounded batches.
  for(const entry of entries.slice(0,4096)){
   if(entry.name===id)continue;
   const match=/^([1-9][0-9]*)_([a-f0-9-]{36})$/.exec(entry.name);
   if(!match||!entry.isDirectory()||entry.isSymbolicLink())throw Error('Authoring lock storage has an unexpected entry; existing files were preserved.');
   const pid=Number(match[1]),folder=path.join(directory,entry.name);
   if(!Number.isSafeInteger(pid))throw Error('Invalid authoring lock owner.');
   if(!alive(pid)){await removeLease(folder);continue;}
   let ticket=null,unreadable=false;
   try {
    const file=path.join(folder,'ticket.json'),info=await lstat(file);
    if(!info.isFile()||info.isSymbolicLink()||info.size>1024)throw Error('Invalid authoring lock ticket.');
    const value=JSON.parse(await readFile(file,'utf8'));
    if(value?.version!==1||!Number.isSafeInteger(value.number)||value.number<1)throw Error('Invalid authoring lock ticket.');
    ticket=value.number;
   }catch(error){
    if(!transientLeaseRead(error))throw error;
    // Windows may deny opening a ticket while its owner deletes it. Treat an
    // unreadable live lease as preparing (blocking), never as permission to enter.
    // A directory confirmed gone can be retired; the next scan retries the rest.
    const current=await lstat(folder).catch(error=>{if(error.code==='ENOENT')return null;if(transientLeaseRead(error))return 'unreadable';throw error;});
    if(!current)continue;
    unreadable=error.code!=='ENOENT'||current==='unreadable';
    if(current!=='unreadable'&&(!current.isDirectory()||current.isSymbolicLink()))throw Error('Invalid authoring lock owner directory.');
   }
   result.push({id:entry.name,ticket,unreadable});
  }
  if(entries.length>4096)throw Error('Old authoring locks were cleaned up. Refresh and try again.');
  if(result.length>128)throw Error('Too many active authoring writers. Close other editors and try again.');
  return result;
 }
 try {
  const before=await participants();
  // An unreadable published ticket may belong to an owner already inside its
  // critical section. Never choose a lower number by treating that ticket as 0.
  if(before.some(p=>p.unreadable))throw busy();
  const ticket=Math.max(0,...before.map(p=>p.ticket??0))+1;
  if(!Number.isSafeInteger(ticket))throw Error('Authoring lock ticket range exhausted.');
  await writeFile(path.join(own,'ticket.tmp'),JSON.stringify({version:1,number:ticket}),{flag:'wx'});
  await rename(path.join(own,'ticket.tmp'),path.join(own,'ticket.json'));
  for(let attempt=0;attempt<11;attempt++){
   const others=await participants();
   if(!others.some(other=>other.ticket===null||other.ticket<ticket||other.ticket===ticket&&other.id<id))return ()=>removeLease(own);
   if(attempt===10)throw busy();await delay(20);
  }
 }catch(error){await removeLease(own);throw error;}
}

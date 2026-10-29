import {createHash} from 'node:crypto';
import {createReadStream,linkSync,lstatSync,realpathSync} from 'node:fs';
import {mkdir,realpath,readdir,writeFile,unlink,lstat} from 'node:fs/promises';
import path from 'node:path';
import {validateGoalBaseline} from '../../dist/packages/analysis-service/goal-history.js';

const MAX_BYTES=64*1024*1024;
const digest=value=>createHash('sha256').update(value).digest('hex');
const sameFile=(a,b)=>a.dev===b.dev&&a.ino===b.ino&&a.size===b.size&&a.mtimeMs===b.mtimeMs&&a.ctimeMs===b.ctimeMs;

/** Worker-only: serialize and validate the heavy trace outside the editor. */
export async function stageGoalBaseline(file,input){
  const baseline=validateGoalBaseline(input),text=JSON.stringify(baseline),bytes=Buffer.byteLength(text);
  if(bytes>MAX_BYTES)throw Error('기준 기록이 64 MiB를 넘습니다. 더 짧은 경로로 나누세요.');
  await writeFile(file,text,{flag:'wx'});
  return {id:baseline.id,name:baseline.name,createdAt:baseline.createdAt,fingerprint:baseline.content.contentFingerprint,runtime:baseline.runtime,bytes,sha256:digest(text),reached:baseline.trace.reached,commands:baseline.trace.route.commands.length};
}

/** Service-only: verify the staged bytes, then publish in one guarded,
 * no-overwrite operation. The last guard and filesystem identity checks have no
 * async gap before publication, so a newer editor request cannot interleave. */
export async function publishStagedGoalBaseline(root,file,metadata,guard){
  if(!metadata||!/^[0-9a-f-]{36}$/.test(metadata.id??'')||typeof metadata.name!=='string'||!metadata.name.trim()||metadata.name.length>120||!Number.isSafeInteger(metadata.bytes)||metadata.bytes<1||metadata.bytes>MAX_BYTES||!/^[0-9a-f]{64}$/.test(metadata.sha256??''))throw Error('Invalid prepared baseline metadata.');
  const base=await realpath(root),source=await realpath(file);
  if(source!==file||!source.startsWith(path.join(base,'.studio','route-batches')+path.sep))throw Error('Prepared baseline must stay in the owned batch directory.');
  const before=await lstat(file);if(!before.isFile()||before.size!==metadata.bytes)throw Error('Prepared baseline file changed.');
  const hash=createHash('sha256');for await(const chunk of createReadStream(file))hash.update(chunk);
  if(hash.digest('hex')!==metadata.sha256||!sameFile(before,await lstat(file)))throw Error('Prepared baseline integrity check failed.');
  let dir=base;
  for(const part of ['playtests','baselines']){dir=path.join(dir,part);await mkdir(dir,{recursive:true});if(await realpath(dir)!==dir)throw Error('Baseline storage cannot be a link.');}
  const lock=path.join(dir,'.write-lock');await writeFile(lock,'',{flag:'wx'}).catch(()=>{throw Error('Another editor is saving a baseline. Retry after it finishes.');});
  const destination=path.join(dir,metadata.id+'.json');
  try{
    if((await readdir(dir)).filter(name=>name.endsWith('.json')).length>=128)throw Error('기준 기록은 프로젝트당 최대 128개입니다.');
    await guard();
    if(realpathSync(dir)!==dir||realpathSync(path.dirname(file))!==path.dirname(file)||!sameFile(before,lstatSync(file))||!lstatSync(file).isFile())throw Error('Baseline storage changed before publication.');
    // link, unlike rename on POSIX, fails if the destination already exists.
    linkSync(file,destination);
    await unlink(file);
    return {id:metadata.id,name:metadata.name,createdAt:metadata.createdAt,file:destination,fingerprint:metadata.fingerprint,runtime:metadata.runtime,bytes:metadata.bytes};
  }finally{await unlink(lock);}
}

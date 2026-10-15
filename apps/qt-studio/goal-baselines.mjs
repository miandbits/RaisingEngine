import {createHash,randomUUID} from 'node:crypto';
import {mkdir,readdir,readFile,writeFile,rename,unlink,realpath,stat} from 'node:fs/promises';
import path from 'node:path';
import {nativeExecutable} from '../../dist/packages/native-bridge/index.js';
import {validateGoalBaseline} from '../../dist/packages/analysis-service/goal-history.js';

const validId=id=>typeof id==='string'&&/^[0-9a-f-]{36}$/.test(id);
const MAX_BYTES=64*1024*1024;
function inside(root,file){const relative=path.relative(root,file);if(relative.startsWith('..')||path.isAbsolute(relative))throw Error('기준 기록은 프로젝트 안에 있어야 합니다.');}
async function folder(root){const base=await realpath(root);let dir=base;for(const part of ['playtests','baselines']){dir=path.join(dir,part);await mkdir(dir,{recursive:true});inside(base,await realpath(dir));}return dir;}
export async function runtimeIdentity(){const binary=await readFile(nativeExecutable());return 'cpp-sha256:'+createHash('sha256').update(binary).digest('hex');}
export async function readGoalBaseline(root,id){
  if(!validId(id))throw Error('잘못된 기준 기록 ID입니다.');
  const dir=await folder(root),file=path.join(dir,id+'.json');inside(dir,await realpath(file));
  if((await stat(file)).size>MAX_BYTES)throw Error('기준 기록이 64 MiB 제한을 넘습니다.');
  const baseline=validateGoalBaseline(JSON.parse(await readFile(file,'utf8')));
  if(baseline.id!==id)throw Error('기준 기록의 파일명과 ID가 다릅니다.');
  return baseline;
}
export async function listGoalBaselines(root){
  const dir=await folder(root),files=(await readdir(dir)).filter(f=>validId(f.slice(0,-5))&&f.endsWith('.json')).sort();
  if(files.length>128)throw Error('기준 기록은 프로젝트당 최대 128개입니다.');
  const result=[];
  for(const file of files){const id=file.slice(0,-5);try{const b=await readGoalBaseline(root,id);result.push({id,name:b.name,createdAt:b.createdAt,routeId:b.trace.route.id,routeName:b.trace.route.name,fingerprint:b.content.contentFingerprint,steps:b.trace.steps.length-1,reached:b.trace.reached});}catch(e){result.push({id,error:e.message});}}
  return result.sort((a,b)=>(b.createdAt??'').localeCompare(a.createdAt??''));
}
export async function saveGoalBaseline(root,input){
  const baseline=validateGoalBaseline(input),dir=await folder(root),file=path.join(dir,baseline.id+'.json');
  const text=JSON.stringify(baseline);if(Buffer.byteLength(text)>MAX_BYTES)throw Error('기준 기록이 64 MiB 제한을 넘습니다.');
  // Independent UUID files never replace an existing baseline, even across editor windows.
  const lock=path.join(dir,'.write-lock');await writeFile(lock,'',{flag:'wx'}).catch(()=>{throw Error('다른 창이 기준 기록을 저장 중입니다. 잠시 후 다시 시도하세요.');});
  const temp=path.join(dir,'.'+randomUUID()+'.tmp');
  try{
    if((await readdir(dir)).filter(f=>f.endsWith('.json')).length>=128)throw Error('기준 기록은 프로젝트당 최대 128개입니다.');
    try{await stat(file);throw Error('이미 존재하는 기준 기록은 덮어쓰지 않습니다.');}catch(e){if(e.code!=='ENOENT')throw e;}
    await writeFile(temp,text,{flag:'wx'});await rename(temp,file);
    return {id:baseline.id,name:baseline.name,createdAt:baseline.createdAt,file,fingerprint:baseline.content.contentFingerprint};
  }finally{await unlink(temp).catch(()=>{});await unlink(lock);}
}

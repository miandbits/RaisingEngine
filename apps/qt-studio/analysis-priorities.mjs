import {createHash,randomUUID} from 'node:crypto';
import {mkdir,realpath,lstat,open} from 'node:fs/promises';
import path from 'node:path';
import {acquireAuthoringLock} from './authoring-lock.mjs';
import {atomicJson} from '../../dist/packages/analysis-service/jobs.js';
import {validatePriorityProfile} from '../../dist/packages/analysis-service/priorities.js';

const MAX_BYTES=1024*1024,MAX_PROFILES=64;
const hash=text=>createHash('sha256').update(text).digest('hex');
const record=v=>v!==null&&typeof v==='object'&&!Array.isArray(v);
const validId=id=>typeof id==='string'&&/^[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}$/.test(id);
function nameOf(name){if(typeof name!=='string'||!name.trim()||name.trim().length>120||/[\u0000-\u001f\u007f]/.test(name))throw Error('Enter a profile name of 1–120 characters without control characters.');return name.trim();}
function exactKeys(value,keys){return record(value)&&Object.keys(value).every(key=>keys.includes(key));}
async function location(root,create=false){
 const base=await realpath(root),dir=path.join(base,'playtests');
 if(create)await mkdir(dir,{recursive:true});
 try{if(await realpath(dir)!==dir)throw Error('Player-priority storage must stay inside the project without linked folders.');}
 catch(error){if(error.code==='ENOENT'&&!create)return null;throw error;}
 return {dir,file:path.join(dir,'player-priorities.json')};
}
async function readStore(root,gameId){
 if(typeof gameId!=='string'||!gameId.trim())throw Error('Give the project a valid game identity before saving player priorities.');
 const loc=await location(root);let text;
 if(loc){try{
   const info=await lstat(loc.file);if(!info.isFile()||info.isSymbolicLink())throw Error('Player-priority storage must be an ordinary project file.');
   const handle=await open(loc.file,'r');try{if((await handle.stat()).size>MAX_BYTES)throw Error('Player-priority storage exceeds 1 MiB.');const bytes=Buffer.alloc(MAX_BYTES+1);let used=0;while(used<bytes.length){const n=(await handle.read(bytes,used,bytes.length-used,null)).bytesRead;if(!n)break;used+=n;}if(used>MAX_BYTES)throw Error('Player-priority storage exceeds 1 MiB.');text=bytes.subarray(0,used).toString('utf8');}finally{await handle.close();}
  }catch(error){if(error.code!=='ENOENT')throw error;}}
 if(text===undefined)return {data:{version:1,gameId,profiles:[]},revision:null};
 let data;try{data=JSON.parse(text);}catch{throw Error('Player-priority storage is damaged. The existing file was preserved.');}
 if(!exactKeys(data,['version','gameId','profiles'])||data.version!==1||typeof data.gameId!=='string'||!Array.isArray(data.profiles)||data.profiles.length>MAX_PROFILES)throw Error('Player-priority storage has an unsupported format. The existing file was preserved.');
 if(data.gameId!==gameId)throw Error('Saved player priorities belong to a different game identity. Restore the original game identity or preserve and move its playtests/player-priorities.json before creating new profiles.');
 const ids=new Set();for(const item of data.profiles){if(!exactKeys(item,['id','name','profile'])||!validId(item.id)||ids.has(item.id)||!record(item.profile))throw Error('Player-priority storage contains invalid or duplicate records. The existing file was preserved.');nameOf(item.name);ids.add(item.id);}
 return {data,revision:hash(text)};
}
function body(profile,content){const checked=validatePriorityProfile(profile,content);const {id,name,...value}=checked;return value;}
function publicStore(stored,content){return {version:1,gameId:stored.data.gameId,revision:stored.revision,profiles:stored.data.profiles.map(item=>{let issues=[];try{body(item.profile,content);}catch(error){issues=[error.message];}return {...structuredClone(item),issues};})};}
export async function listPriorityProfiles(root,content){return publicStore(await readStore(root,content.project.gameId),content);}
async function mutate(root,content,expectedRevision,edit,guard){
 if(expectedRevision!==null&&(typeof expectedRevision!=='string'||!/^[a-f0-9]{64}$/.test(expectedRevision)))throw Error('Refresh player priorities before saving.');
 const loc=await location(root,true),release=await acquireAuthoringLock(path.join(loc.dir,'.player-priority-writers'));
 try{
  const stored=await readStore(root,content.project.gameId);if(stored.revision!==expectedRevision)throw Error('Player priorities changed in another editor. Refresh before saving; no profile was overwritten.');
  await guard?.();const data=structuredClone(stored.data);edit(data);if(Buffer.byteLength(JSON.stringify(data),'utf8')>MAX_BYTES)throw Error('Player-priority storage exceeds 1 MiB.');
  await guard?.();await atomicJson(loc.file,data);return publicStore(await readStore(root,content.project.gameId),content);
 }finally{await release();}
}
export async function savePriorityProfile(root,content,{profilesRevision,id,name,profile},guard){
 if(id!==undefined&&!validId(id))throw Error('Select an existing player-priority profile.');
 const item={id:id??randomUUID(),name:nameOf(name),profile:body(profile,content)};
 const result=await mutate(root,content,profilesRevision,data=>{const index=data.profiles.findIndex(p=>p.id===item.id);if(id!==undefined&&index<0)throw Error('The selected player-priority profile no longer exists. Refresh the list.');if(index<0){if(data.profiles.length>=MAX_PROFILES)throw Error('A project can store at most 64 player-priority profiles.');data.profiles.push(item);}else data.profiles[index]=item;},guard);
 return {...result,savedId:item.id};
}
export async function deletePriorityProfile(root,content,{profilesRevision,id},guard){
 if(!validId(id))throw Error('Select an existing player-priority profile.');
 return mutate(root,content,profilesRevision,data=>{const index=data.profiles.findIndex(p=>p.id===id);if(index<0)throw Error('The selected player-priority profile no longer exists. Refresh the list.');data.profiles.splice(index,1);},guard);
}
export async function resolvePriorityProfile(root,content,{priorityProfileId,profilesRevision}){
 const stored=await readStore(root,content.project.gameId);if(stored.revision!==profilesRevision)throw Error('Player priorities changed. Refresh and review the selected profile before running.');
 const item=stored.data.profiles.find(p=>p.id===priorityProfileId);if(!item)throw Error('Select a saved player-priority profile before running.');
 return validatePriorityProfile({...item.profile,id:item.id,name:item.name},content);
}

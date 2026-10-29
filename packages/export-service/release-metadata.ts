import {createHash} from 'node:crypto';
import {lstat,readFile,realpath,mkdir,writeFile,open} from 'node:fs/promises';
import path from 'node:path';
import {decodePng} from '../media/png.js';
import {loadProject} from '../project-service/loader.js';
import type {AssetManifest} from '../schema/generated/assets.js';
export type ReleaseMetadata = NonNullable<AssetManifest['release']>;
export interface ReleaseFile {path:string;sha256:string;bytes:Buffer;id?:string;label?:string}
export interface PreparedRelease {metadata:ReleaseMetadata;files:ReleaseFile[]}
export const RELEASE_LIMITS=Object.freeze({noticeBytes:1024*1024,totalNoticeBytes:16*1024*1024,iconBytes:20*1024*1024,notices:64});
export const releaseHash=(bytes:Uint8Array)=>createHash('sha256').update(bytes).digest('hex');
const filePattern=/^assets\/(?:[A-Za-z0-9_-]+\/)*[A-Za-z0-9_-]+\.[A-Za-z0-9]+$/;
/** No symlink components, traversal, device names, or case-folded collisions. */
export function releasePath(file:unknown):string {
 if(typeof file!=='string'||!filePattern.test(file)||file.split('/').some(p=>/^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|$)/i.test(p)))throw Error('Release files require a safe project-relative assets path.');return file;
}
export async function readReleaseFile(root:string,file:string,max:number):Promise<Buffer>{
 releasePath(file);const base=await realpath(root);let current=base;
 for(const [i,part] of file.split('/').entries()){current=path.join(current,part);const st=await lstat(current);if(st.isSymbolicLink()||(i<file.split('/').length-1?!st.isDirectory():!st.isFile()))throw Error('Release files cannot use symbolic links or non-regular files: '+file);if(i===file.split('/').length-1&&st.size>max)throw Error('Release file exceeds its size limit: '+file);}
 const handle=await open(current,'r');try{const st=await handle.stat();if(!st.isFile()||st.size>max||await realpath(current)!==current)throw Error('Release file changed or exceeds its size limit: '+file);const bytes=Buffer.alloc(max+1);let used=0;while(used<bytes.length){const {bytesRead}=await handle.read(bytes,used,bytes.length-used,null);if(!bytesRead)break;used+=bytesRead;}if(used>max)throw Error('Release file exceeds its size limit: '+file);return bytes.subarray(0,used);}finally{await handle.close();}
}
export function validateNotice(bytes:Buffer):void {
 if(!bytes.length||bytes.length>RELEASE_LIMITS.noticeBytes)throw Error('Choose a nonempty UTF-8 notice up to 1 MiB.');
 try {const text=new TextDecoder('utf-8',{fatal:true}).decode(bytes);if(text.includes('\0'))throw Error('NUL');}catch{throw Error('Notices must be UTF-8 text. Original bytes, including BOM and line endings, are preserved.');}
}
export function validateReleaseIcon(bytes:Buffer):void {
 if(bytes.length>RELEASE_LIMITS.iconBytes)throw Error('Application icon exceeds 20 MiB.');
 if(bytes.length<24||bytes.readUInt32BE(16)!==1024||bytes.readUInt32BE(20)!==1024)throw Error('Choose an opaque 1024 × 1024 PNG application icon.');
 const image=decodePng(bytes);if(image.width!==1024||image.height!==1024)throw Error('Choose an opaque 1024 × 1024 PNG application icon.');
 for(let i=3;i<image.pixels.length;i+=4)if(image.pixels[i]!==255)throw Error('Application icons must be opaque for iOS compatibility.');
}
export async function savedReleaseMetadata(root:string):Promise<ReleaseMetadata|undefined>{
 const loaded=await loadProject(root);if(loaded.diagnostics.some(d=>d.file==='assets/manifest.json'&&d.severity==='error'&&d.code!=='DOCUMENT_MISSING'))throw Error('The saved asset manifest cannot be read safely.');return (loaded.documents.find(d=>d.kind==='assets')?.data as AssetManifest|undefined)?.release;
}
export async function prepareRelease(root:string,input:ReleaseMetadata|undefined):Promise<PreparedRelease>{
 const metadata=JSON.parse(JSON.stringify(input??{})) as ReleaseMetadata,files:ReleaseFile[]=[],names=new Set<string>(),ids=new Set<string>();let total=0;
 if(!metadata||typeof metadata!=='object'||Array.isArray(metadata)||Object.keys(metadata).some(k=>!['displayName','icon','notices'].includes(k)))throw Error('Invalid release declarations.');
 if(metadata.displayName!==undefined&&(typeof metadata.displayName!=='string'||!metadata.displayName.trim()||metadata.displayName.length>80||/[\u0000-\u001f\u007f]/.test(metadata.displayName)))throw Error('Application name must contain 1–80 printable characters.');
 if(metadata.notices!==undefined&&(!Array.isArray(metadata.notices)||metadata.notices.length>RELEASE_LIMITS.notices))throw Error('A release supports at most 64 notices.');
 const add=async(ref:{path:string;sha256:string;label?:string},icon:boolean,id?:string)=>{
  if(!ref||Object.keys(ref).some(k=>!(icon?['path','sha256']:['path','sha256','id','label']).includes(k))||!/^[0-9a-f]{64}$/.test(ref.sha256))throw Error('Release file requires its imported SHA-256.');
  const file=releasePath(ref.path),key=file.normalize('NFC').toLowerCase();if(names.has(key))throw Error('Duplicate or colliding release file: '+file);names.add(key);
  const bytes=await readReleaseFile(root,file,icon?RELEASE_LIMITS.iconBytes:RELEASE_LIMITS.noticeBytes);
  if(releaseHash(bytes)!==ref.sha256)throw Error('Declared release file changed. Import the intended replacement: '+file);
  if(icon){if(path.extname(file).toLowerCase()!=='.png')throw Error('Application icon must have a .png extension.');validateReleaseIcon(bytes);}else{validateNotice(bytes);total+=bytes.length;if(total>RELEASE_LIMITS.totalNoticeBytes)throw Error('Total notice size exceeds 16 MiB.');}
  files.push({path:file,sha256:ref.sha256,bytes,...(id?{id}:{}),...(!icon&&ref.label?{label:ref.label}:{})});
 };
 if(metadata.icon)await add(metadata.icon,true);
 for(const notice of metadata.notices??[]){if(notice.label!==undefined&&(typeof notice.label!=='string'||!notice.label.trim()||notice.label.length>120||/[\u0000-\u001f\u007f]/.test(notice.label)))throw Error('Notice labels must contain 1–120 printable characters.');if(typeof notice.id!=='string'||notice.id.length>64||!/^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/.test(notice.id)||ids.has(notice.id))throw Error('Notice IDs must be unique lowercase names.');ids.add(notice.id);await add(notice,false,notice.id);}
 return {metadata,files};
}
export async function verifyPreparedRelease(root:string,prepared:PreparedRelease):Promise<void>{
 const actual=await prepareRelease(root,prepared.metadata);
 if(actual.files.length!==prepared.files.length||actual.files.some((f,i)=>f.path!==prepared.files[i]?.path||f.sha256!==prepared.files[i]?.sha256||!f.bytes.equals(prepared.files[i]!.bytes)))throw Error('Prepared release bytes changed before publication.');
}
export async function packageNotices(data:string,prepared:PreparedRelease):Promise<void>{
 const notices=prepared.files.filter(f=>f.id);if(!notices.length)return;
 const directory=path.join(data,'licenses/project');await mkdir(directory,{recursive:true});
 const index=[];for(const file of notices){const name=file.id+'.txt';await writeFile(path.join(directory,name),file.bytes,{flag:'wx'});index.push({id:file.id,...(file.label?{label:file.label}:{}),file:name,source:file.path,sha256:file.sha256,bytes:file.bytes.length});}
 await writeFile(path.join(directory,'index.json'),JSON.stringify({formatVersion:1,notices:index},null,2)+'\n',{flag:'wx'});
}

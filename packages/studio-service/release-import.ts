import {lstat,readFile,realpath,mkdir,writeFile,open} from 'node:fs/promises';
import path from 'node:path';
import type {Json} from '../validation/model.js';
import type {StudioEditor} from './editor.js';
import {noteUnsavedAsset} from './unsaved-assets.js';
import {releaseHash,validateNotice,validateReleaseIcon,prepareRelease,RELEASE_LIMITS,type ReleaseMetadata} from '../export-service/release-metadata.js';
/** Imports immutable packaging files without inventing licensing terms or gameplay assets. */
export async function importReleaseFile(editor:StudioEditor,source:string,kind:'icon'|'notice',revision:number):Promise<{path:string;sha256:string;id?:string;label?:string}>{
 editor.current(revision);if(!editor.root||editor.readOnly)throw Error('Open an editable project.');
 if(!['icon','notice'].includes(kind)||typeof source!=='string'||!path.isAbsolute(source))throw Error('Choose a local release file.');
 const label=path.basename(source);if(kind==='notice'&&(!label.trim()||label.length>120||/[\u0000-\u001f\u007f]/.test(label)))throw Error('Choose a notice filename with 1–120 printable characters.');
 const limit=kind==='icon'?RELEASE_LIMITS.iconBytes:RELEASE_LIMITS.noticeBytes,st=await lstat(source);if(!st.isFile()||st.size>limit)throw Error('Release source must be a regular file within its size limit.');
 const handle=await open(source,'r');let bytes:Buffer;try{const actual=await handle.stat();if(!actual.isFile()||actual.size>limit)throw Error('Release source exceeds its size limit.');const buffer=Buffer.alloc(limit+1);let used=0;while(used<buffer.length){const {bytesRead}=await handle.read(buffer,used,buffer.length-used,null);if(!bytesRead)break;used+=bytesRead;}if(used>limit)throw Error('Release source exceeds its size limit.');bytes=buffer.subarray(0,used);}finally{await handle.close();}if(kind==='icon'){if(path.extname(source).toLowerCase()!=='.png')throw Error('Choose a PNG application icon.');validateReleaseIcon(bytes);}else validateNotice(bytes);
 const root=await realpath(editor.root),assets=path.join(root,'assets');if(await realpath(assets)!==assets)throw Error('Release assets cannot use a symbolic link.');
 const folder=path.join(assets,'release');await mkdir(folder,{recursive:true});if(await realpath(folder)!==folder)throw Error('Release assets cannot use a symbolic link.');
 const sha256=releaseHash(bytes),id=(kind==='icon'?'icon-':'notice-')+sha256.slice(0,24),relative='assets/release/'+id+(kind==='icon'?'.png':'.txt'),destination=path.join(root,relative);editor.current(revision);
 let created=false;try{await writeFile(destination,bytes,{flag:'wx'});created=true;}catch(error){if((error as NodeJS.ErrnoException).code!=='EEXIST')throw error;if((await lstat(destination)).isSymbolicLink()||!(await readFile(destination)).equals(bytes))throw Error('Imported release filename collides with another file.');}
 await noteUnsavedAsset(root,destination,bytes,created);editor.current(revision);return {path:relative,sha256,...(kind==='notice'?{id,label}:{})};
}
export async function applyProjectReleaseSettings(editor:StudioEditor,params:{project:Json;release:ReleaseMetadata;iconFile?:string;noticeFiles?:string[]},revision:number):Promise<void>{
 editor.current(revision);await editor.assertDiskUnchanged();const manifest=editor.documents.find(d=>d.kind==='assets'),project=editor.documents.find(d=>d.kind==='project');if(!manifest||!project)throw Error('Project and asset manifest are required.');
 const release=structuredClone(params.release??{});if(params.iconFile)release.icon=await importReleaseFile(editor,params.iconFile,'icon',revision);
 if(params.noticeFiles!==undefined&&(!Array.isArray(params.noticeFiles)||params.noticeFiles.length>64))throw Error('Choose at most 64 notice files.');
 for(const source of params.noticeFiles??[]){const imported=await importReleaseFile(editor,source,'notice',revision);release.notices??=[];if(!release.notices.some(n=>n.path===imported.path))release.notices.push({...imported,id:imported.id!});}
 await prepareRelease(editor.root!,release);editor.current(revision);await editor.assertDiskUnchanged();editor.current(revision);
 const data={...(manifest.data as Record<string,Json>)};if(Object.keys(release).length)data.release=release;else delete data.release;
 editor.edit({type:'batch',edits:[{type:'replace',file:project.file,data:params.project},{type:'replace',file:manifest.file,data}]},revision);
}

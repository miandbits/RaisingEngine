import yauzl from 'yauzl';
import {readFile,lstat,mkdtemp,rm,realpath,mkdir} from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import {digest,relativeFile,materializeProject} from './project-files.mjs';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {validateStructure} from '../../dist/packages/validation/structure.js';
import {prepareRelease} from '../../dist/packages/export-service/release-metadata.js';
const MAX_FILE=256*1024*1024,MAX_TOTAL=1024*1024*1024,MAX_FILES=50000;
const table=Array.from({length:256},(_,v)=>{for(let i=0;i<8;i++)v=(v>>>1)^((v&1)?0xedb88320:0);return v>>>0;});
function crc(bytes){let c=0xffffffff;for(const b of bytes)c=(c>>>8)^table[(c^b)&255];return (c^0xffffffff)>>>0;}
/** Untrusted ZIP names and bytes never become filesystem paths until every entry is checked. */
export async function readProjectBackup(file,{expectedSha256}={}){
 const st=await lstat(file);if(!st.isFile()||st.size>MAX_TOTAL)throw Error('Choose a regular project ZIP of at most 1 GiB.');
 const bytes=await readFile(file);if(bytes.length>MAX_TOTAL)throw Error('Project backup exceeds 1 GiB.');const sha256=digest(bytes);if(expectedSha256&&sha256!==expectedSha256)throw Error('Backup changed since verification. Select and verify it again.');
 const files=await new Promise((resolve,reject)=>yauzl.fromBuffer(bytes,{lazyEntries:true,strictFileNames:true,validateEntrySizes:true},(error,zip)=>{
  if(error)return reject(error);const out=new Map(),names=new Set();let total=0,failed=false;
  const fail=e=>{if(failed)return;failed=true;zip.close();reject(e);};zip.on('error',fail);zip.on('end',()=>{if(!failed)resolve(out);});
  zip.on('entry',entry=>{try{
   const name=relativeFile(entry.fileName),key=name.normalize('NFC').toLowerCase(),mode=(entry.externalFileAttributes>>>16)&0xf000;
   if(names.has(key)||names.size>=MAX_FILES||mode&&mode!==0x8000||entry.generalPurposeBitFlag&1||![0,8].includes(entry.compressionMethod)||entry.uncompressedSize>MAX_FILE||total+entry.uncompressedSize>MAX_TOTAL||name.split('/').some(p=>/^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|$)/i.test(p)))throw Error('Unsafe, colliding, or oversized backup entry: '+name);
   names.add(key);total+=entry.uncompressedSize;zip.openReadStream(entry,(error,stream)=>{if(error)return fail(error);const chunks=[];let size=0;stream.on('error',fail);stream.on('data',b=>{size+=b.length;if(size>entry.uncompressedSize||size>MAX_FILE){stream.destroy();fail(Error('Backup entry expanded beyond its declared size.'));return;}chunks.push(b);});stream.on('end',()=>{if(failed)return;const data=Buffer.concat(chunks);if(data.length!==entry.uncompressedSize||crc(data)!==entry.crc32)return fail(Error('Backup CRC mismatch: '+name));out.set(name,data);zip.readEntry();});});
  }catch(e){fail(e);}});zip.readEntry();
 }));
 const raw=files.get('RELEASE.json');if(!raw||raw.length>16*1024*1024)throw Error('Project backup requires a bounded RELEASE.json receipt.');
 const manifest=JSON.parse(raw);if(manifest.formatVersion!==1||!Array.isArray(manifest.sourceFiles)||manifest.sourceFiles.length>MAX_FILES)throw Error('Invalid project backup receipt.');
 const expected=new Set(['RELEASE.json']);for(const entry of manifest.sourceFiles){const name=relativeFile(entry.file);if(expected.has(name)||!/^[0-9a-f]{64}$/.test(entry.sha256)||!files.has(name)||digest(files.get(name))!==entry.sha256)throw Error('Backup source checksum mismatch: '+name);expected.add(name);}
 if(expected.size!==files.size||[...files.keys()].some(name=>!expected.has(name)))throw Error('Backup contains undeclared files.');
 return {files,manifest,sha256};
}
export async function verifyRestoredProject(directory,backup){
 for(const [file,bytes] of backup.files){const actual=await readFile(path.join(directory,file));if(!actual.equals(bytes))throw Error('Restored bytes differ: '+file);}
 const loaded=await loadProject(directory);if(loaded.diagnostics.some(d=>d.severity==='error'))throw Error('Restored project cannot be reopened: '+loaded.diagnostics.map(d=>d.message).join('; '));
 for(const doc of loaded.documents){const errors=await validateStructure(doc);if(errors.some(d=>d.severity==='error'))throw Error('Restored document is invalid: '+doc.file);}
 await prepareRelease(directory,loaded.documents.find(d=>d.kind==='assets')?.data?.release);
 const plugins=loaded.documents.some(d=>['pluginLock','extension'].includes(d.kind));let contentFingerprint=null;
 // Arbitrary backup plugins are preserved, never executed as part of verification.
 if(!plugins){const built=await buildSnapshot(loaded);if(!built.ok)throw Error('Restored project validation failed: '+built.diagnostics.map(d=>d.message).join('; '));contentFingerprint=built.snapshot.contentFingerprint;if(contentFingerprint!==(backup.manifest.sourceContentFingerprint??backup.manifest.contentFingerprint))throw Error('Restored project gameplay fingerprint differs from its release receipt.');}
 return {ok:true,archiveSha256:backup.sha256,sourceFilesVerified:backup.files.size-1,documentsReopened:loaded.documents.length,contentFingerprint,contentFingerprintVerified:!plugins,pluginValidation:plugins?'Not executed. Review and trust the restored plugin store before running plugin validation.':'not-needed'};
}
export async function verifyProjectBackup(file,options={}){
 const backup=await readProjectBackup(file,options),temporary=await mkdtemp(path.join(os.tmpdir(),'raising-backup-verify-'));
 try {const directory=await materializeProject(backup.files,path.join(temporary,'project'));return {...await verifyRestoredProject(directory,backup),version:backup.manifest.version};}finally{await rm(temporary,{recursive:true,force:true});}
}
export async function restoreProjectBackup(file,{destination,expectedSha256,originalRoot}={}){
 if(!expectedSha256)throw Error('Verify this backup before restoring it.');
 if(typeof destination!=='string'||!path.isAbsolute(destination))throw Error('Choose a new absolute project folder.');
 await mkdir(path.dirname(destination),{recursive:true});const target=path.join(await realpath(path.dirname(destination)),path.basename(destination));if(originalRoot){const base=await realpath(originalRoot),relative=path.relative(base,target);if(!relative||!relative.startsWith('..'+path.sep)&&relative!=='..'&&!path.isAbsolute(relative))throw Error('Restore outside the open project into a new folder.');}
 const backup=await readProjectBackup(file,{expectedSha256});
 // Validate in owned temporary storage before publishing any user destination.
 const temporary=await mkdtemp(path.join(os.tmpdir(),'raising-backup-restore-'));try{const draft=await materializeProject(backup.files,path.join(temporary,'project'));await verifyRestoredProject(draft,backup);const directory=await materializeProject(backup.files,target);return {directory,verification:await verifyRestoredProject(directory,backup)};}finally{await rm(temporary,{recursive:true,force:true});}
}

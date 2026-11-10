import {open,readFile,realpath} from 'node:fs/promises';
import path from 'node:path';
import {createHash} from 'node:crypto';
import yauzl from 'yauzl';
import {digest,regularFile,relativeFile} from './project-files.mjs';
import {readSampleSaveBytes} from './update-compatibility.mjs';
import {readProjectBackup,verifyProjectBackup,restoreProjectBackup} from './backup-restore.mjs';
import {loadProject,buildSnapshot} from '../../dist/packages/project-service/index.js';
import {decodeSave} from '../../dist/packages/runtime-service/save.js';

export async function boundedBytes(file,limit=16*1024*1024){
 const h=await open(file,'r');try{const check=async()=>{const s=await h.stat();if(!s.isFile()||s.size>limit)throw Error('RELEASE_FILE_SIZE: Choose a bounded regular file.');};await check();const buffer=Buffer.alloc(limit+1);let n=0;while(n<buffer.length){const r=await h.read(buffer,n,buffer.length-n,n);if(!r.bytesRead)break;n+=r.bytesRead;}if(n>limit)throw Error('RELEASE_FILE_SIZE: File grew beyond the limit.');await check();return buffer.subarray(0,n);}finally{await h.close();}
}
export async function sampleBytes(file){const h=await open(file,'r');try{return await readSampleSaveBytes(h);}finally{await h.close();}}
export async function releaseManifest(directory){
 if(typeof directory!=='string'||!path.isAbsolute(directory))throw Error('RELEASE_DIRECTORY: Choose a versioned export folder.');
 const root=await realpath(directory),bytes=await boundedBytes(await regularFile(root,'RELEASE.json')),manifest=JSON.parse(bytes.toString('utf8'));
 if(manifest.formatVersion!==1||!['web','macos','windows','ios'].includes(manifest.target)||typeof manifest.version!=='string'||!/^\d{1,4}\.\d{1,4}\.\d{1,4}(?:-[a-zA-Z0-9.-]{1,40})?$/.test(manifest.version)||!/^sha256:[a-f0-9]{64}$/.test(manifest.contentFingerprint)||!Array.isArray(manifest.files)||!manifest.files.length||manifest.files.length>50000||!Array.isArray(manifest.sourceFiles)||manifest.sourceFiles.length>50000)throw Error('RELEASE_MANIFEST: Select an export with a valid RELEASE.json.');
 for(const list of [manifest.files,manifest.sourceFiles]){const seen=new Set();for(const f of list){relativeFile(f.file);if(seen.has(f.file)||!/^[a-f0-9]{64}$/.test(f.sha256))throw Error('RELEASE_MANIFEST: Invalid or duplicate file checksum.');seen.add(f.file);}}
 for(const kind of ['game','project']){const f=manifest.archives?.[kind];if(!f||!/^[a-f0-9]{64}$/.test(f.sha256))throw Error('RELEASE_ARCHIVES: Export with game and project archives enabled.');relativeFile(f.file);}
 return {directory:root,manifest,sha256:digest(bytes)};
}
const bundleFile=target=>target==='macos'?'RaisingPlayer.app/Contents/Resources/game.json':target==='ios'?'game/game.json':'game.json';
const crcTable=Array.from({length:256},(_,v)=>{for(let i=0;i<8;i++)v=(v>>>1)^((v&1)?0xedb88320:0);return v>>>0;});
async function fileHash(file){const h=await open(file,'r');try{if(!(await h.stat()).isFile())throw Error('RELEASE_FILE: Expected a regular file.');const hash=createHash('sha256'),buffer=Buffer.alloc(1024*1024);let n=0;while(true){const r=await h.read(buffer,0,buffer.length,n);if(!r.bytesRead)break;n+=r.bytesRead;hash.update(buffer.subarray(0,r.bytesRead));}return hash.digest('hex');}finally{await h.close();}}
async function verifyGameZip(file,files){
 const expected=new Map(files.map(f=>[f.file,f.sha256])),seen=new Set();let count=0,total=0;
 await new Promise((resolve,reject)=>yauzl.open(file,{lazyEntries:true,strictFileNames:true,validateEntrySizes:true},(error,zip)=>{
  if(error)return reject(error);let failed=false;const fail=e=>{if(failed)return;failed=true;zip.close();reject(e);};zip.on('error',fail);
  zip.on('entry',e=>{try{relativeFile(e.fileName);const key=e.fileName.normalize('NFC').toLowerCase(),mode=(e.externalFileAttributes>>>16)&0xf000;
   if(seen.has(key)||mode&&mode!==0x8000||!expected.has(e.fileName)||e.uncompressedSize>1024*1024*1024||total+e.uncompressedSize>4*1024*1024*1024||e.generalPurposeBitFlag&1||![0,8].includes(e.compressionMethod))throw Error('RELEASE_ARCHIVE: Unsafe, colliding, nonregular or oversized game ZIP entry.');seen.add(key);total+=e.uncompressedSize;
   zip.openReadStream(e,(error,stream)=>{if(error)return fail(error);const h=createHash('sha256');let size=0,crc=0xffffffff;stream.on('error',fail);stream.on('data',b=>{size+=b.length;if(size>e.uncompressedSize){stream.destroy();fail(Error('RELEASE_ARCHIVE: Entry grew beyond its declared size.'));return;}h.update(b);for(const byte of b)crc=(crc>>>8)^crcTable[(crc^byte)&255];});stream.on('end',()=>{if(failed)return;if(size!==e.uncompressedSize||((crc^0xffffffff)>>>0)!==e.crc32||h.digest('hex')!==expected.get(e.fileName))return fail(Error('RELEASE_ARCHIVE: Game ZIP member checksum or CRC differs: '+e.fileName));expected.delete(e.fileName);count++;zip.readEntry();});});}catch(e){fail(e);}});
  zip.on('end',()=>{if(failed)return;expected.size?fail(Error('RELEASE_ARCHIVE: Game ZIP is missing declared files.')):resolve();});zip.readEntry();
 }));return count;
}
export async function verifyRelease(entry,progress=()=>{}){
 const selected=await releaseManifest(entry.directory);if(selected.sha256!==entry.id)throw Error('RELEASE_STALE: RELEASE.json changed. Register the new export separately.');
 const m=selected.manifest;let n=0;for(const f of m.files){if(await fileHash(await regularFile(selected.directory,f.file))!==f.sha256)throw Error('RELEASE_STALE: Exported file changed: '+f.file);progress({message:'Checking exported files',done:++n,total:m.files.length});}
 for(const f of Object.values(m.archives))if(await fileHash(await regularFile(selected.directory,f.file))!==f.sha256)throw Error('RELEASE_STALE: Archive changed: '+f.file);
 await readProjectBackup(await regularFile(selected.directory,m.archives.project.file),{expectedSha256:m.archives.project.sha256,expectedRelease:{...m,gameId:entry.gameId}});
 const members=await verifyGameZip(await regularFile(selected.directory,m.archives.game.file),m.files),file=bundleFile(m.target),declared=m.files.find(f=>f.file===file);if(!declared)throw Error('RELEASE_BUNDLE: The manifest does not declare its game bundle.');
 const bytes=await boundedBytes(await regularFile(selected.directory,file),64*1024*1024);if(digest(bytes)!==declared.sha256)throw Error('RELEASE_STALE: Game bundle changed during verification.');const bundle=JSON.parse(bytes.toString('utf8'));
 if(bundle.content?.contentFingerprint!==m.contentFingerprint||bundle.content?.project?.gameId!==entry.gameId)throw Error('RELEASE_GAME: Exported content does not match this project and receipt.');
 return {manifest:m,content:bundle.content,receipt:{status:'passed',checkedAt:new Date().toISOString(),manifestSha256:entry.id,filesVerified:m.files.length,gameZipMembersVerified:members,archiveHashes:m.archives,message:'Declared player files and both archives match this release; distribution and device acceptance remain separate.'}};
}
async function savedSource(root){const loaded=await loadProject(root),built=await buildSnapshot(loaded);if(!built.ok)throw Error('RELEASE_SOURCE_INVALID: Previous source project does not validate.');return {content:built.snapshot,documentVersion:digest(JSON.stringify(loaded.documents.map(d=>[d.file,d.version]).sort((a,b)=>a[0].localeCompare(b[0]))))};}
async function resolvedLocation(file){let parent=path.dirname(file),tail=path.basename(file);while(true){try{return path.join(await realpath(parent),tail);}catch(e){if(e.code!=='ENOENT')throw e;tail=path.join(path.basename(parent),tail);const next=path.dirname(parent);if(next===parent)throw e;parent=next;}}}
export async function runReleaseWorkflowJob({entry,kind,caseId,destination,projectRoot},progress=()=>{}){
 const integrity=async callback=>{try{return await verifyRelease(entry,callback);}catch(error){throw Error('RELEASE_INTEGRITY: '+error.message);}};
 const before=await integrity(progress),result={release:before.receipt};
 if(kind==='save'){
  const cases=caseId?entry.cases.filter(c=>c.id===caseId):entry.cases;if(!cases.length)throw Error('RELEASE_SAVE_CASE: Add or select a save case.');result.cases=[];
  for(const [index,c]of cases.entries()){
   try{
   progress({message:'Checking save: '+c.name,done:index,total:cases.length});const bytes=await sampleBytes(c.file);if(digest(bytes)!==c.sha256)throw Error('RELEASE_SAVE_STALE: Original save changed: '+c.name);
   const text=new TextDecoder('utf-8',{fatal:true}).decode(bytes),direct=await decodeSave(before.content,text);let source;
   const check={status:direct.ok?'passed':'failed',checkedAt:new Date().toISOString(),sampleSha256:c.sha256,caseKind:c.kind,targetFingerprint:entry.identity.contentFingerprint,directLoad:direct.ok,message:direct.ok?'Strict replay and saved presentation validate against this exact release. Interactive load, progression, save and relaunch still require a recorded playtest.':direct.error.code,...(direct.ok?{commands:direct.session.commands.length,hasPresentation:!!direct.playerPresentation}:{error:direct.error})};
   if(!direct.ok&&direct.error.code==='CONTENT_MISMATCH'){
    check.status='migration-required';check.message='Different content identity. Use explicit reviewed save migration; this check does not approve conversion.';
    if(c.sourceProject){source=await savedSource(c.sourceProject);if(source.content.project.gameId!==entry.gameId)throw Error('RELEASE_GAME: Previous source belongs to another game.');const old=await decodeSave(source.content,text);check.sourceFingerprint=source.content.contentFingerprint;check.sourceDocumentVersion=source.documentVersion;check.sourceValidated=old.ok;if(!old.ok){check.status='failed';check.error=old.error;check.message='The original save does not validate against the selected previous source.';}if(c.check?.sourceDocumentVersion&&(c.check.sourceDocumentVersion!==source.documentVersion||c.check.sourceFingerprint!==source.content.contentFingerprint))throw Error('RELEASE_SOURCE_STALE: Previous source changed. Register a new case for the changed source.');}
   }
   if(digest(await sampleBytes(c.file))!==c.sha256)throw Error('RELEASE_SAVE_STALE: Save changed during replay: '+c.name);
   if(source){const after=await savedSource(c.sourceProject);if(after.documentVersion!==source.documentVersion||after.content.contentFingerprint!==source.content.contentFingerprint)throw Error('RELEASE_SOURCE_STALE: Previous source changed during replay.');}
   result.cases.push({id:c.id,check});
   }catch(error){const {jobId,...prior}=c.check??{};result.cases.push({id:c.id,check:{...prior,status:/STALE/.test(error.message)?'stale':'failed',checkedAt:new Date().toISOString(),sampleSha256:c.sha256,caseKind:c.kind,directLoad:false,message:error.message}});}
  }
 }else if(kind==='recovery'){
  const archive=await regularFile(entry.directory,before.manifest.archives.project.file),sha=before.manifest.archives.project.sha256;
  const expectedRelease={...before.manifest,gameId:entry.gameId},verification=await verifyProjectBackup(archive,{expectedSha256:sha,expectedRelease});let restored;
  if(destination){
   // Never publish recovery inside the selected immutable release or a previous
   // project used by a save case, even when it differs from the open project.
   const resolved=await resolvedLocation(destination);for(const base of [entry.directory,...entry.cases.map(c=>c.sourceProject).filter(Boolean)]){const canonical=await realpath(base).catch(error=>{if(error.code==='ENOENT')return resolvedLocation(base);throw error;}),relative=path.relative(canonical,resolved);if(!relative||!relative.startsWith('..'+path.sep)&&relative!=='..'&&!path.isAbsolute(relative))throw Error('RELEASE_RECOVERY_LOCATION: Restore outside the release and previous source folders.');}
   restored=await restoreProjectBackup(archive,{destination,expectedSha256:sha,expectedRelease,originalRoot:projectRoot});
  }
  result.recovery={status:'passed',checkedAt:new Date().toISOString(),verification,sourceContentFingerprint:verification.contentFingerprint,runtimeContentFingerprint:entry.identity.contentFingerprint,sourceMatchesRuntime:verification.contentFingerprint===entry.identity.contentFingerprint,...(restored?{directory:restored.directory,restored:true,restoredVerification:restored.verification}:{restored:false}),message:restored?'Backup restored to a new folder and reopened with verified source identity. Check sourceMatchesRuntime before using it as a save-migration target.':'Backup bytes and reopen validation passed; a named restoration destination has not been recorded.'};
 }else if(kind!=='release')throw Error('RELEASE_JOB_KIND: Choose a supported check.');
 // Any changed receipt, player, archive or source invalidates the whole result.
 await integrity(()=>{});return result;
}

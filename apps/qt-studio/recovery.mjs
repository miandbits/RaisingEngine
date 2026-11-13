import {randomUUID} from 'node:crypto';
import {readFile,writeFile,mkdir,readdir,lstat,realpath,rm} from 'node:fs/promises';
import path from 'node:path';
import {privateDirectory,atomicJSON,projectFiles,materializeProject,digest,relativeFile} from './project-files.mjs';
import {FirstGameProgress} from './first-game.mjs';

const idPattern=/^[a-f0-9-]{36}$/;
const maxJournal=32*1024*1024;
export class DraftRecovery {
  id=randomUUID();last=null;status=null;blobs=new Set();
  async folder(root,id=this.id) {
    if(!idPattern.test(id))throw Error('Invalid recovery identifier.');
    const parent=await privateDirectory(root,'recovery'),folder=path.join(parent,id);
    await mkdir(folder,{recursive:true});if(await realpath(folder)!==folder)throw Error('Recovery cannot use symbolic links.');return folder;
  }
  async checkpoint(editor) {
    if(!editor.root||editor.readOnly)return;
    const version=editor.draftVersion(),key=editor.root+version;
    if(!editor.view({shared:true}).dirty) {await this.clearOwn(editor.root);this.status=null;this.last=key;return;}
    if(this.last===key)return;
    const folder=await this.folder(editor.root),files=await projectFiles(editor),entries=[];
    for(const [file,bytes] of files) {
      const hash=digest(bytes),blob=path.join(folder,hash);
      if(!this.blobs.has(blob)) {
        try {await writeFile(blob,bytes,{flag:'wx'});}catch(e){
          if(e.code!=='EEXIST')throw e;
          const stat=await lstat(blob);
          if(!stat.isFile()||stat.size!==bytes.length||digest(await readFile(blob))!==hash)throw Error('Recovery data changed unexpectedly. Save your project before continuing.');
        }
        this.blobs.add(blob);
      }
      entries.push({file,sha256:hash,size:bytes.length});
    }
    const value={formatVersion:1,id:this.id,root:editor.root,baseVersion:editor.diskVersion(),draftVersion:version,updatedAt:new Date().toISOString(),title:editor.documents.find(d=>d.kind==='project')?.data?.title??'Untitled',files:entries};
    const progress=await new FirstGameProgress().read(editor).catch(()=>null);
    if(progress)value.firstGame={version:1,baseline:progress.baseline,played:!!progress.played};
    if(Buffer.byteLength(JSON.stringify(value))>maxJournal)throw Error('Recovery index exceeds its size limit. Save the project before continuing.');
    await atomicJSON(path.join(folder,'draft.json'),value);this.last=key;this.status={savedAt:value.updatedAt};
  }
  async read(root,id) {
    const folder=await this.folder(root,id),file=path.join(folder,'draft.json'),stat=await lstat(file);
    if(!stat.isFile()||stat.size>maxJournal)throw Error('Invalid recovery index.');
    const bytes=await readFile(file),value=JSON.parse(bytes);
    if(value.formatVersion!==1||value.id!==id||value.root!==await realpath(root)||!Array.isArray(value.files)||value.files.length>50000)throw Error('Recovery does not belong to this project.');
    return {folder,value,revision:digest(bytes)};
  }
  async list(root) {
    const parent=await privateDirectory(root,'recovery'),items=[];
    for(const entry of await readdir(parent,{withFileTypes:true})) {
      if(!entry.isDirectory()||!idPattern.test(entry.name))continue;
      try {const {value,revision}=await this.read(root,entry.name);items.push({id:entry.name,revision,title:value.title,updatedAt:value.updatedAt,current:entry.name===this.id});}
      catch(error){items.push({id:entry.name,error:error.message});}
    }
    return items.sort((a,b)=>(b.updatedAt??'').localeCompare(a.updatedAt??''));
  }
  async restore(root,{id,revision,destination}) {
    const {folder,value,revision:actual}=await this.read(root,id);if(actual!==revision)throw Error('Recovery changed. Refresh the list before restoring.');
    if(typeof destination!=='string'||!path.isAbsolute(destination))throw Error('Choose an absolute path for a new project folder.');
    const target=path.join(await realpath(path.dirname(destination)),path.basename(destination)),base=await realpath(root);
    const relative=path.relative(base,target);
    if(!relative||(!relative.startsWith('..'+path.sep)&&relative!=='..'&&!path.isAbsolute(relative)))throw Error('Restore outside the original project into a new folder.');
    const files=new Map();
    for(const entry of value.files) {
      relativeFile(entry.file);if(!/^[a-f0-9]{64}$/.test(entry.sha256)||!Number.isSafeInteger(entry.size)||entry.size<0)throw Error('Invalid recovery file.');
      const blob=path.join(folder,entry.sha256),stat=await lstat(blob);
      if(!stat.isFile()||stat.size!==entry.size)throw Error('Recovery file is missing or changed: '+entry.file);
      const bytes=await readFile(blob);if(digest(bytes)!==entry.sha256)throw Error('Recovery checksum mismatch: '+entry.file);
      if(files.has(entry.file))throw Error('Duplicate recovery file.');files.set(entry.file,bytes);
    }
    const firstGame=value.firstGame;
    if(firstGame&&(firstGame.version!==1||!firstGame.baseline||Buffer.byteLength(JSON.stringify(firstGame))>65536))throw Error('Invalid recovery checklist.');
    return materializeProject(files,target,{firstGame});
  }
  async discard(root,{id,revision}) {
    const {folder,revision:actual}=await this.read(root,id);if(actual!==revision)throw Error('Recovery changed. Refresh the list first.');
    await rm(folder,{recursive:true});if(id===this.id){this.last=null;this.status=null;}
  }
  async clearOwn(root) {
    // Never remove another editor session's recovery on Save or Discard.
    if(!root)return;
    const candidate=path.join(root,'.studio/recovery',this.id);
    if(!await lstat(candidate).catch(e=>{if(e.code==='ENOENT')return null;throw e;}))return;
    const parent=await privateDirectory(root,'recovery'),folder=path.join(parent,this.id);
    const stat=await lstat(folder).catch(e=>{if(e.code==='ENOENT')return null;throw e;});
    if(stat?.isSymbolicLink())throw Error('Recovery cannot use symbolic links.');
    if(stat)await rm(folder,{recursive:true});
    this.blobs.clear();
  }
}

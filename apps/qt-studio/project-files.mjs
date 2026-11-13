import {prepareRelease} from '../../dist/packages/export-service/release-metadata.js';
import {createHash,randomUUID} from 'node:crypto';
import {readFile,writeFile,open,mkdir,lstat,realpath,readdir,rename,rm} from 'node:fs/promises';
import path from 'node:path';

export const digest = bytes => createHash('sha256').update(bytes).digest('hex');
export function relativeFile(file) {
  if(typeof file!=='string'||!file||file.includes('\\')||file.includes(':')||file.includes('\0')||file.split('/').some(p=>!p||p==='.'||p==='..')||file.startsWith('/')||['.studio','.git','.env'].includes(file.split('/')[0]))throw Error('Invalid project file: '+file);
  return file;
}
export function contained(root,file) {
  const relative=path.relative(root,file);
  if(relative==='..'||relative.startsWith('..'+path.sep)||path.isAbsolute(relative))throw Error('File must remain inside its project.');
  return file;
}
export async function regularFile(root,file) {
  const full=path.join(root,relativeFile(file));contained(root,await realpath(full));
  if(!(await lstat(full)).isFile())throw Error('Expected a regular file: '+file);
  return full;
}
export async function privateDirectory(root,leaf) {
  const base=await realpath(root),studio=path.join(base,'.studio');await mkdir(studio,{recursive:true});
  if(await realpath(studio)!==studio)throw Error('Project metadata cannot be a symbolic link.');
  const directory=path.join(studio,leaf);await mkdir(directory,{recursive:true});
  if(await realpath(directory)!==directory)throw Error('Project metadata cannot be a symbolic link.');
  return directory;
}
export async function atomicJSON(file,value) {
  const temporary=file+'.'+randomUUID()+'.tmp';
  try {await writeFile(temporary,JSON.stringify(value),{flag:'wx'});await rename(temporary,file);}
  finally {await rm(temporary,{force:true});}
}
export async function projectFiles(editor) {
  const root=await realpath(editor.root),files=new Map();
  for(const doc of editor.documents)files.set(relativeFile(doc.file),Buffer.from(JSON.stringify(doc.data,null,2)+'\n'));
  const documents=new Set(files.keys());
  const assets=editor.documents.find(d=>d.kind==='assets')?.data?.assets??[];
  for(const asset of assets) {
    const name=relativeFile(asset.path);
    if(documents.has(name))throw Error('An asset overlaps a project document: '+name);
    // Several asset IDs may intentionally refer to the same source file.
    if(!files.has(name))files.set(name,await readFile(await regularFile(root,name)));
  }
  const release=await prepareRelease(root,editor.documents.find(d=>d.kind==='assets')?.data?.release);
  for(const file of release.files){if(documents.has(file.path)||files.has(file.path))throw Error('Release file overlaps another project resource: '+file.path);files.set(file.path,file.bytes);}
  // Named routes and approved expectations are source, unlike local saves/caches.
  for(const group of ['routes','baselines']) {
    const directory=path.join(root,'playtests',group);
    for(const entry of await readdir(directory,{withFileTypes:true}).catch(e=>{if(e.code==='ENOENT')return [];throw e;})) {
      if(!entry.name.endsWith('.json'))continue;
      const name='playtests/'+group+'/'+entry.name;files.set(name,await readFile(await regularFile(root,name)));
    }
  }
  // Player priorities are authoring source too; local analysis runs stay private.
  const priorities='playtests/player-priorities.json';
  try {
    const full=await regularFile(root,priorities),handle=await open(full,'r');
    try {
      const limit=1024*1024,bytes=Buffer.alloc(limit+1);let used=0;
      if((await handle.stat()).size>limit)throw Error('Player-priority storage exceeds 1 MiB.');
      while(used<bytes.length){const n=(await handle.read(bytes,used,bytes.length-used,null)).bytesRead;if(!n)break;used+=n;}
      if(used>limit)throw Error('Player-priority storage exceeds 1 MiB.');
      files.set(priorities,bytes.subarray(0,used));
    } finally {await handle.close();}
  } catch(error) {if(error.code!=='ENOENT')throw error;}
  return files;
}
export async function materializeProject(files,destination,{firstGame}={}) {
  if(!path.isAbsolute(destination))throw Error('Choose an absolute path for a new project folder.');
  await mkdir(path.dirname(destination),{recursive:true});
  const parent=await realpath(path.dirname(destination)),target=path.join(parent,path.basename(destination));
  if(await lstat(target).catch(e=>{if(e.code==='ENOENT')return null;throw e;}))throw Error('The destination already exists. Choose a new folder.');
  const stage=path.join(parent,'.raising-project-'+randomUUID());await mkdir(stage);
  try {
    const names=new Set();
    for(const [file,bytes] of files) {
      const name=relativeFile(file),key=name.normalize('NFC').toLowerCase();
      if(names.has(key))throw Error('Project contains colliding filenames: '+name);names.add(key);
      const full=path.join(stage,name);await mkdir(path.dirname(full),{recursive:true});await writeFile(full,bytes,{flag:'wx'});
    }
    if(!files.has('project.json'))throw Error('Recovery has no project document.');
    if(firstGame) {
      await mkdir(path.join(stage,'.studio'));
      await atomicJSON(path.join(stage,'.studio/first-game.json'),firstGame);
    }
    if(await lstat(target).catch(e=>{if(e.code==='ENOENT')return null;throw e;}))throw Error('The destination was created by another operation.');
    await rename(stage,target);return target;
  } catch(error) {await rm(stage,{recursive:true,force:true});throw error;}
}

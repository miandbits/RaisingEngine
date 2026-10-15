import {execFileSync} from 'node:child_process';
import {readFile, stat, mkdir, writeFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {fileURLToPath} from 'node:url';
import path from 'node:path';

const root=fileURLToPath(new URL('..',import.meta.url));
const args=process.argv.slice(2);
if(args.length && (args.length!==2 || args[0]!=='--out')) throw Error('Usage: node scripts/repository-audit.mjs [--out report.json]');
const files=execFileSync('git',['ls-files','-z'],{cwd:root,encoding:'utf8',maxBuffer:16*1024*1024}).split('\0').filter(Boolean);
const groups=new Map(), references=new Map(), duplicates=new Map();
const generated=[], sensitiveNames=[], missing=[];let bytes=0;
for(const file of files){
  const absolute=path.join(root,file);let info;
  try{info=await stat(absolute);}catch(e){if(e.code==='ENOENT'){missing.push(file);continue;}throw e;}
  if(!info.isFile())continue;
  bytes+=info.size;
  const top=file.split('/')[0],group=groups.get(top)??{files:0,bytes:0};group.files++;group.bytes+=info.size;groups.set(top,group);
  const ref=file.match(/^projects\/((?:pm2|magical)[^/]*)\//);
  if(ref){const group=references.get(ref[1])??{files:0,bytes:0};group.files++;group.bytes+=info.size;references.set(ref[1],group);}
  if(/(^|\/)(node_modules|dist|release|\.cache|\.studio|\.toolchains)\//.test(file)||file.startsWith('docs/evidence/'))generated.push(file);
  if(/(^|\/)\.env(?:\.|$)/.test(file)&&!/(?:example|sample)$/.test(file)||/\.(?:p12|pfx|keystore|pem|key)$/.test(file)||/(^|\/)(?:id_rsa|id_ed25519)$/.test(file))sensitiveNames.push(file);
  if(info.size>=1024 && /\.(?:png|jpe?g|webp|ttf|otf|woff2?|wav|ogg|mp3|mkv|mp4)$/i.test(file)){
    const hash=createHash('sha256').update(await readFile(absolute)).digest('hex');
    const group=duplicates.get(hash)??{sha256:hash,bytes:info.size,files:[]};group.files.push(file);duplicates.set(hash,group);
  }
}
const duplicateAssets=[...duplicates.values()].filter(g=>g.files.length>1).map(g=>({...g,redundantBytes:g.bytes*(g.files.length-1)})).sort((a,b)=>b.redundantBytes-a.redundantBytes);
const report={version:1,scope:'tracked working-tree files only; not a release/security/license approval',
  trackedFiles:files.length,bytes,rootLicense:files.find(f=>/^(?:LICENSE|COPYING)(?:\.[^/]+)?$/i.test(f))??null,
  groups:[...groups].map(([name,g])=>({name,...g})).sort((a,b)=>b.bytes-a.bytes),
  referenceProjects:[...references].map(([name,g])=>({name,...g})),generated,sensitiveNames,missing,duplicateAssets};
const mib=n=>(n/1048576).toFixed(1)+' MiB';
console.log(`Tracked files: ${files.length}; size: ${mib(bytes)}`);
console.log(`Engine license file: ${report.rootLicense??'MISSING — owner decision required'}`);
console.log(`Known reference-project folders: ${references.size}; review redistribution before release`);
console.log(`Tracked generated/evidence files: ${generated.length}; sensitive filename candidates: ${sensitiveNames.length}; missing files: ${missing.length}`);
console.log(`Duplicate binary asset groups: ${duplicateAssets.length}; repeated bytes: ${mib(duplicateAssets.reduce((n,g)=>n+g.redundantBytes,0))}`);
for(const g of report.groups.slice(0,8))console.log(`  ${g.name}: ${g.files} files, ${mib(g.bytes)}`);
console.log('No files removed. Asset rights, secrets in contents and Git history are NOT evaluated.');
if(args.length){const output=path.resolve(args[1]);if(files.some(f=>path.resolve(root,f)===output))throw Error('Choose an untracked report path; refusing to overwrite a tracked file.');await mkdir(path.dirname(output),{recursive:true});await writeFile(output,JSON.stringify(report,null,2)+'\n',{flag:'wx'});console.log('Inventory report written (existing files are never overwritten).');}

import {execFileSync} from 'node:child_process';
import {readFile,access} from 'node:fs/promises';
import path from 'node:path';
import {createHash} from 'node:crypto';
const root=path.resolve('.');
const files=execFileSync('git',['ls-files','-z'],{encoding:'utf8'}).split('\0').filter(Boolean);
const errors=[];
for(const file of files){
  if(!file.includes('/')&&file.endsWith('.cmd'))errors.push('Root experiment launcher: '+file);
  if(file.startsWith('apps/studio/')||file.startsWith('apps/player/main/')||file.startsWith('apps/player/preload/'))errors.push('Electron shell outside legacy: '+file);
  if(file==='apps/web/generated/save-validator.mjs')errors.push('Generated validator must not be tracked');
}
const pkg=JSON.parse(await readFile('package.json','utf8'));
for(const [name,command] of Object.entries(pkg.scripts))if(!name.startsWith('legacy:')&&/electron-vite|\belectron\s/.test(command))errors.push('Implicit Electron command: '+name);
const manifest=JSON.parse(await readFile('tests/fixtures/media-manifest.json','utf8'));
for(const item of manifest.files){
  if(files.includes(item.target))errors.push('Duplicate fixture media is tracked: '+item.target);
  if(!files.includes(item.source))errors.push('Fixture source must be tracked: '+item.source);
  const hash=createHash('sha256').update(await readFile(item.source)).digest('hex');
  if(hash!==item.sha256)errors.push('Fixture source changed: '+item.source);
}
// Curated entry points must not send a new user to missing local files.
for(const doc of ['README.md','docs/README.md','docs/manual/README.md','docs/development/README.md','examples/README.md','CONTRIBUTING.md']){
  const text=await readFile(doc,'utf8');
  for(const match of text.matchAll(/\]\(([^)]+)\)/g)){
    const dest=match[1].split('#')[0].replace(/^<|>$/g,'');
    if(!dest||/^[a-z]+:|^\//i.test(dest))continue;
    const file=path.resolve(path.dirname(doc),dest);
    try{await access(file);}catch{errors.push(`${doc}: missing link ${dest}`);}
  }
}
console.log(JSON.stringify({trackedFiles:files.length,fixtureCopies:manifest.files.length,errors},null,2));
if(errors.length)process.exitCode=1;

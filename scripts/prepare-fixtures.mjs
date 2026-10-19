import {readFile,writeFile,mkdir,lstat,realpath} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const repository=fileURLToPath(new URL('..',import.meta.url));
const digest=bytes=>createHash('sha256').update(bytes).digest('hex');
function inside(root,file){const relative=path.relative(root,file);return relative!==''&&!relative.startsWith('..'+path.sep)&&relative!=='..'&&!path.isAbsolute(relative);}
export async function prepareFixtures(root=repository,manifest){
  root=await realpath(root);
  manifest??=JSON.parse(await readFile(path.join(root,'tests/fixtures/media-manifest.json'),'utf8'));
  let created=0;
  for(const item of manifest.files){
    if(!item.target.startsWith('tests/fixtures/projects/'))throw Error('Fixture target must be inside tests/fixtures/projects');
    const source=path.resolve(root,item.source),target=path.resolve(root,item.target);
    if(!inside(root,source)||!inside(root,target)||!inside(root,await realpath(source)))throw Error('Fixture path escapes repository');
    const bytes=await readFile(source);
    if(digest(bytes)!==item.sha256)throw Error(`Fixture source hash mismatch: ${item.source}`);
    const existing=await lstat(target).catch(e=>{if(e.code==='ENOENT')return null;throw e;});
    if(existing){
      if(!inside(root,await realpath(target)))throw Error('Fixture path escapes repository');
      if(existing.isSymbolicLink()||!existing.isFile()||digest(await readFile(target))!==item.sha256)throw Error(`Fixture has local changes; preserved: ${item.target}`);
      continue;
    }
    let ancestor=path.dirname(target);
    while(!(await lstat(ancestor).catch(e=>{if(e.code==='ENOENT')return null;throw e;})))ancestor=path.dirname(ancestor);
    const actual=await realpath(ancestor);
    if(actual!==root&&!inside(root,actual))throw Error('Fixture directory escapes repository');
    await mkdir(path.dirname(target),{recursive:true});
    await writeFile(target,bytes,{flag:'wx'});created++;
  }
  return {files:manifest.files.length,created};
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url))console.log(JSON.stringify(await prepareFixtures()));

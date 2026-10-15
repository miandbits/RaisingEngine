import {execFileSync} from 'node:child_process';
import {readFile} from 'node:fs/promises';
import {fileURLToPath} from 'node:url';
import {createHash} from 'node:crypto';
import path from 'node:path';

export async function inspectPublicContent(root,files,policy){
  const violations=[];const hashes=new Set(policy.excludedAssetSha256);
  for(const file of files){
    const normalized=file.replaceAll('\\','/');
    if(/^(projects|internal|\.cache|dist|release)\//.test(normalized)||normalized.startsWith('docs/evidence/')||policy.privateTools.includes(normalized)){
      violations.push({file,reason:'local-only path is tracked'});continue;
    }
    const bytes=await readFile(path.join(root,file));
    if(hashes.has(createHash('sha256').update(bytes).digest('hex')))violations.push({file,reason:'matches excluded reference-game asset'});
  }
  return violations;
}

const root=fileURLToPath(new URL('..',import.meta.url));
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url)){
  const policy=JSON.parse(await readFile(path.join(root,'config/repository-content.json'),'utf8'));
  const files=execFileSync('git',['ls-files','-z'],{cwd:root,encoding:'utf8',maxBuffer:16*1024*1024}).split('\0').filter(Boolean);
  const violations=await inspectPublicContent(root,files,policy);
  console.log(JSON.stringify({trackedFiles:files.length,excludedAssetHashes:policy.excludedAssetSha256.length,violations},null,2));
  if(violations.length)process.exitCode=1;
  else console.log('Current index/worktree boundary passed. Git history and unknown asset provenance are NOT certified.');
}

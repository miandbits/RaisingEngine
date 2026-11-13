import {readFile,mkdir,writeFile,rename,unlink} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {resolve,join} from 'node:path';
import {fetchVerifiedBytes} from './lib/verified-download.mjs';
const manifest=JSON.parse(await readFile('config/qt-corresponding-source.json','utf8'));
const output=resolve(process.env.QT_SOURCE_ARCHIVES||'.cache/preview-release/qt-sources');
await mkdir(output,{recursive:true});
for(const source of manifest.archives){
  const file=join(output,source.file);
  let data=await readFile(file).catch(e=>{if(e.code==='ENOENT')return null;throw e;});
  const existed=data!==null;
  if(!data){
    data=await fetchVerifiedBytes(manifest.downloadBase+source.file,source.sha256,{
      onRetry:({attempt,attempts,error})=>console.warn(`Retrying ${source.file} (${attempt}/${attempts}): ${error.message}`),
    });
  }
  const hash=createHash('sha256').update(data).digest('hex');
  if(hash!==source.sha256)throw Error(`Corresponding-source hash mismatch: ${source.file}`);
  if(existed)continue;
  // No overwrite of a file whose bytes have not been verified.
  const temp=file+'.download';await writeFile(temp,data);await rename(temp,file).catch(async e=>{await unlink(temp);throw e;});
}
console.log(JSON.stringify({ok:true,output,archives:manifest.archives.length}));

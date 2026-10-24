import {mkdir,readFile,writeFile,rename} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {createHash} from 'node:crypto';

// Match the exact Qt binaries used by package-qt-studio; never package an
// unverified download as the corresponding source.
const config=JSON.parse(await readFile('config/qt-corresponding-source.json','utf8'));
const out=resolve(process.env.QT_SOURCE_ARCHIVES||'.cache/preview-release/qt-sources');
await mkdir(out,{recursive:true});
const hash=bytes=>createHash('sha256').update(bytes).digest('hex');
for(const archive of config.archives){
  const file=join(out,archive.file);
  if(await readFile(file).then(bytes=>hash(bytes)===archive.sha256).catch(()=>false))continue;
  const response=await fetch(new URL(archive.file,config.downloadBase),{signal:AbortSignal.timeout(180000)});
  if(!response.ok)throw Error(`Qt source download failed: ${response.status} ${archive.file}`);
  const bytes=Buffer.from(await response.arrayBuffer());
  if(hash(bytes)!==archive.sha256)throw Error('Qt source hash mismatch: '+archive.file);
  await writeFile(file+'.download',bytes);await rename(file+'.download',file);
  console.log('Verified Qt source: '+archive.file);
}

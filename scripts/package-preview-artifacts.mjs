import {readFile,writeFile,mkdir,access,readdir} from 'node:fs/promises';
import {dirname,basename,join,relative} from 'node:path';
import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {previewArtifactMetadata,windowsPreviewCandidate} from './lib/preview-artifact-metadata.mjs';

const release=JSON.parse(await readFile('config/preview-release.json','utf8'));
const {bundle,installer,version,portableName}=await windowsPreviewCandidate();
const manifestBytes=await readFile(join(bundle,'package-manifest.json')),manifest=JSON.parse(manifestBytes);
if(manifest.releaseVersion!==version)throw Error('Bundle version does not match preview candidate');
const digest=bytes=>createHash('sha256').update(bytes).digest('hex');
for(const file of manifest.files){
  const hash=createHash('sha256').update(await readFile(join(bundle,file.path))).digest('hex');
  if(hash!==file.sha256)throw Error('Modified bundle file: '+file.path);
}
// ZIP must not accidentally include editor history or files added after the audit.
const expected=new Set([...manifest.files.map(f=>f.path),'package-manifest.json']);
async function verifyInventory(dir){for(const entry of await readdir(dir,{withFileTypes:true})){
  const file=join(dir,entry.name),name=relative(bundle,file).replaceAll('\\','/');
  if(entry.isSymbolicLink())throw Error('Symlink in bundle: '+name);
  if(entry.isDirectory())await verifyInventory(file);
  else if(!expected.has(name))throw Error('Unreviewed extra payload: '+name);
}}
await verifyInventory(bundle);
const installerBytes=await readFile(installer),installerReceipt=JSON.parse(await readFile(installer+'.json','utf8'));
const packageManifestSha256=digest(manifestBytes),installerSha256=digest(installerBytes);
if(installerReceipt.releaseVersion!==version||installerReceipt.packageManifestSha256!==packageManifestSha256
  ||installerReceipt.sha256!==installerSha256||installerReceipt.bytes!==installerBytes.length){
  throw Error('Installer receipt does not match the installer and audited package manifest');
}
const out=dirname(installer),zip=join(out,portableName);
for(const file of [zip,join(out,'SHA256SUMS.txt'),join(out,'release-artifacts.json')]){
  try{await access(file);throw Error('Release artifact already exists: '+file);}catch(error){if(error.code!=='ENOENT')throw error;}
}
await mkdir(out,{recursive:true});
// Windows tar/libarchive writes ZIP without a PowerShell quoting boundary.
const archive=spawnSync('tar.exe',['-a','-cf',zip,'-C',dirname(bundle),basename(bundle)],{encoding:'utf8',windowsHide:true});
if(archive.status!==0)throw Error(archive.stderr||'Could not create portable ZIP');
const archiveBytes=await readFile(zip);
const files=[{file:basename(installer),bytes:installerBytes.length,sha256:installerSha256},
  {file:basename(zip),bytes:archiveBytes.length,sha256:digest(archiveBytes)}];
await writeFile(join(out,'SHA256SUMS.txt'),files.map(f=>`${f.sha256}  ${f.file}`).join('\n')+'\n');
await writeFile(join(out,'release-artifacts.json'),JSON.stringify({...previewArtifactMetadata(release,manifest,files),packageManifestSha256},null,2)+'\n');
console.log(JSON.stringify({ok:true,version,files}));

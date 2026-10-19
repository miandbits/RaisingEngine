import {readFile,writeFile,mkdir,access} from 'node:fs/promises';
import {resolve,dirname,basename,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';

const release=JSON.parse(await readFile('config/preview-release.json','utf8'));
if(!/^[0-9A-Za-z.-]+$/.test(release.version))throw Error('Invalid release version');
const bundle=resolve(process.env.QT_PACKAGE_OUT||'release/qt-preview/RaisingStudio');
const installer=resolve(process.env.QT_INSTALLER_OUT||'release/qt-preview/RaisingStudio-Qt-Preview-0.1.0-x64.exe');
const manifest=JSON.parse(await readFile(join(bundle,'package-manifest.json'),'utf8'));
if(manifest.releaseVersion!==release.version)throw Error('Bundle version does not match release config');
for(const file of manifest.files){
  const hash=createHash('sha256').update(await readFile(join(bundle,file.path))).digest('hex');
  if(hash!==file.sha256)throw Error('Modified bundle file: '+file.path);
}
await access(installer);
const out=dirname(installer),zip=join(out,`RaisingEngine-${release.version}-windows-x64-portable.zip`);
try{await access(zip);throw Error('Portable archive already exists: '+zip);}catch(error){if(error.code!=='ENOENT')throw error;}
await mkdir(out,{recursive:true});
// Windows tar/libarchive writes ZIP without a PowerShell quoting boundary.
const archive=spawnSync('tar.exe',['-a','-cf',zip,'-C',dirname(bundle),basename(bundle)],{encoding:'utf8',windowsHide:true});
if(archive.status!==0)throw Error(archive.stderr||'Could not create portable ZIP');
const files=[];
for(const file of [installer,zip]){const bytes=await readFile(file);files.push({file:basename(file),bytes:bytes.length,sha256:createHash('sha256').update(bytes).digest('hex')});}
await writeFile(join(out,'SHA256SUMS.txt'),files.map(f=>`${f.sha256}  ${f.file}`).join('\n')+'\n');
await writeFile(join(out,'release-artifacts.json'),JSON.stringify({version:release.version,sourceCommit:manifest.sourceCommit,sourceTreeDirty:manifest.sourceTreeDirty,signed:false,publicReleaseReady:release.publicReleaseReady,remainingReleaseChecks:release.remainingReleaseChecks,files},null,2)+'\n');
console.log(JSON.stringify({ok:true,version:release.version,files}));

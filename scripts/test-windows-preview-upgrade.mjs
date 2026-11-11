import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {spawnSync} from 'node:child_process';
import {access,mkdir,mkdtemp,readFile,writeFile} from 'node:fs/promises';
import {join,resolve} from 'node:path';
import {windowsPreviewCandidate} from './lib/preview-artifact-metadata.mjs';

assert.equal(process.platform,'win32');
const candidate=await windowsPreviewCandidate();
const published=JSON.parse(await readFile('config/preview-publication-validation.json','utf8'));
assert.notEqual(candidate.version,published.version);
const priorAsset=published.files.find(file=>file.file===`RaisingEngine-${published.version}-windows-x64.exe`);
assert.ok(priorAsset,'Missing published installer pin');

const out=resolve('.cache/windows-preview-upgrade');
await mkdir(out,{recursive:true});
const root=await mkdtemp(join(out,'run-'));
const priorInstall=join(root,'Published preview');
const priorInstaller=join(root,priorAsset.file);
const url=`https://github.com/miandbits/RaisingEngine/releases/download/v${published.version}/${priorAsset.file}`;
const sha256=bytes=>createHash('sha256').update(bytes).digest('hex');
const run=(exe,args,{timeout=120000,env=process.env,verbatim=false}={})=>{
  const result=spawnSync(exe,args,{env,encoding:'utf8',windowsHide:true,windowsVerbatimArguments:verbatim,timeout});
  assert.equal(result.status,0,JSON.stringify({exe,args,status:result.status,stdout:result.stdout,stderr:result.stderr,error:result.error?.message}));
  return result.stdout.trim();
};

run('curl.exe',['--fail','--silent','--show-error','--location','--output',priorInstaller,url]);
const installerBytes=await readFile(priorInstaller);
assert.equal(installerBytes.length,priorAsset.bytes);
assert.equal(sha256(installerBytes),priorAsset.sha256);
run(priorInstaller,['/S',`/D=${priorInstall}`],{verbatim:true});
const registryKey=`HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\RaisingEngine-${published.version}`;
const shortcut=join(process.env.APPDATA,'Microsoft/Windows/Start Menu/Programs',`RaisingEngine ${published.version}`,'Raising Studio.lnk');
const priorEditor=join(priorInstall,'RaisingStudioQt.exe');
const priorManifest=join(priorInstall,'package-manifest.json');
const retainedFile=join(priorInstall,'creator-notes.txt');
try {
  await writeFile(retainedFile,'A previous preview user file');
  const files=[priorEditor,priorManifest,shortcut,retainedFile];
  const before=await Promise.all(files.map(async file=>sha256(await readFile(file))));
  const manifest=JSON.parse(await readFile(priorManifest,'utf8'));
  assert.equal(manifest.releaseVersion,published.version);
  assert.equal(manifest.sourceCommit,published.sourceCommit);
  const registration=run('reg.exe',['query',registryKey]);
  assert.ok(registration.includes(priorInstall),'Published preview is registered at the expected location');
  const pilot=spawnSync(process.execPath,['scripts/test-qt-package.mjs','--creator-pilot'],{
    env:process.env,windowsHide:true,stdio:'inherit',timeout:900000,
  });
  assert.equal(pilot.status,0,pilot.error?.message||'Installed candidate pilot failed');
  const proof=JSON.parse(await readFile('.cache/qt-release/latest.json','utf8'));
  assert.equal(proof.ok,true);
  assert.equal(proof.portable,false);
  assert.equal(proof.overwriteRejected,true);
  assert.equal(proof.uninstallPreservedUnknownFiles,true);
  assert.equal(run('reg.exe',['query',registryKey]),registration);
  const after=await Promise.all(files.map(async file=>sha256(await readFile(file))));
  assert.deepEqual(after,before);
  const result={ok:true,priorReleaseVersion:published.version,priorReleaseSource:published.sourceCommit,
    priorInstallerSha256:priorAsset.sha256,priorEditorSha256:before[0],candidateReleaseVersion:candidate.version,
    priorInstallationAndRegistrationPreserved:true,candidatePilot:proof};
  await writeFile(join(root,'result.json'),JSON.stringify(result,null,2)+'\n');
  console.log(JSON.stringify(result));
} finally {
  await access(join(priorInstall,'Uninstall.exe'));
  run(join(priorInstall,'Uninstall.exe'),['/S',`_?=${priorInstall}`],{verbatim:true});
}

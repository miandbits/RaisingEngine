import {readFile,lstat,realpath} from 'node:fs/promises';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import path from 'node:path';

export const cocoaRepair = Object.freeze({
  qtVersion:'6.12.0',arch:'arm64',minimumMacOS:'14.4',
  commit:'b74d6cb3c5e8418602494ae549b60839d88ef858',
  qtArchiveSha256:'a951bd163c7b80fc6b8c88d7668fb56abf91c152373e13c10666763238131307',
  patchSha256:'e0158ec9584d0577896ae9112670c3d8212a71c34bf5b3e40969df4a0b424f24',
  moltenArchiveSha256:'6864db532f1dbbdb621a8d0ec13f24edae318fd9269dd3dd0cdff791334bb1cb',
  vulkanArchiveSha256:'4a078be12bef21cfebc09d878b77a63cff9d68f899254a0b00d0e37ef73e7f7e',
});
export const sha256=bytes=>createHash('sha256').update(bytes).digest('hex');
const hash=value=>typeof value==='string'&&/^[a-f0-9]{64}$/.test(value);
const object=value=>value!==null&&typeof value==='object'&&!Array.isArray(value);
const fail=message=>{throw Error('Qt Cocoa repair: '+message);};
function relativeFile(value) {
  return typeof value==='string'&&value.length>0&&value.length<1024&&!value.includes('\\')&&!value.includes('\0')&&!path.posix.isAbsolute(value)&&value.split('/').every(part=>part&&part!=='.'&&part!=='..');
}
export function validateCocoaRepairManifest(value) {
  if(!object(value)||value.schemaVersion!==1)fail('unsupported or malformed manifest');
  for(const key of ['qtVersion','arch','minimumMacOS'])if(value[key]!==cocoaRepair[key])fail('unsupported '+key);
  if(value.patchCommit!==cocoaRepair.commit)fail('only the reviewed official Qt backport is supported');
  if(value.nativeRegression?.passed!==true||value.nativeRegression?.case!=='tableViewSelectionAfterModelChange'||!hash(value.nativeRegression?.logSha256)||!relativeFile(value.nativeRegression?.logFile))fail('verified native accessibility regression is required');
  for(const key of ['plugin','sourceKit'])if(!object(value[key])||!relativeFile(value[key].file)||!hash(value[key].sha256))fail('invalid '+key+' file identity');
  if(!object(value.frameworks)||Object.keys(value.frameworks).sort().join(',')!=='QtCore,QtGui,QtWidgets')fail('exact Qt framework identities are required');
  for(const [name,identity] of Object.entries(value.frameworks))if(!object(identity)||!hash(identity.sha256)||typeof identity.uuid!=='string'||!/^[A-Fa-f0-9-]{36}$/.test(identity.uuid))fail('invalid framework identity: '+name);
  return value;
}
async function ownedFile(root,relative,limit) {
  const file=path.join(root,relative),info=await lstat(file);
  if(!info.isFile()||info.isSymbolicLink()||info.size>limit)fail('expected a bounded regular file: '+relative);
  const actual=await realpath(file);
  if(!actual.startsWith(root+path.sep))fail('file resolves outside the repair directory: '+relative);
  return {file:actual,bytes:await readFile(actual)};
}
export function verifyCocoaSourceKit(archive,run=(tool,args)=>execFileSync(tool,args,{maxBuffer:128*1024*1024})) {
  const entries=run('/usr/bin/tar',['-tzf',archive]).toString().split('\n').filter(Boolean);
  if(entries.some(name=>!name.startsWith('qt-cocoa-source-kit/')||name.split('/').includes('..')))fail('unsafe source-kit archive entry');
  const expected={
    'downloads/qtbase-everywhere-src-6.12.0.tar.xz':cocoaRepair.qtArchiveSha256,
    ['downloads/'+cocoaRepair.commit+'.patch']:cocoaRepair.patchSha256,
    'downloads/MoltenVK-v1.4.2.tar.gz':cocoaRepair.moltenArchiveSha256,
    'downloads/Vulkan-Headers-1.4.363.0.tar.gz':cocoaRepair.vulkanArchiveSha256,
    'licenses/LGPL-3.0-only.txt':'da7eabb7bafdf7d3ae5e9f223aa5bdc1eece45ac569dc21b3b037520b4464768',
    'licenses/GPL-3.0-only.txt':'8ceb4b9ee5adedde47b31e975c1d90c73ad27b6b165a1dcd80c7c545eb65b903',
  };
  for(const [file,expectedHash] of Object.entries(expected)) {
    const entry='qt-cocoa-source-kit/'+file;
    if(entries.filter(name=>name===entry).length!==1||sha256(run('/usr/bin/tar',['-xOf',archive,entry]))!==expectedHash)fail('source/license mismatch: '+file);
  }
  for(const file of ['README.md','rebuild.py','driver/CMakeLists.txt','installed-kit.json','source-checksums.json']) {
    const entry='qt-cocoa-source-kit/'+file;
    if(entries.filter(name=>name===entry).length!==1||run('/usr/bin/tar',['-xOf',archive,entry]).length<20)fail('missing rebuild material: '+file);
  }
}
export async function prepareCocoaRepair(manifestFile,{qt,libraryRoot,platform=process.platform,arch=process.arch}={}) {
  if(!manifestFile)return undefined;
  if(platform!=='darwin'||arch!==cocoaRepair.arch)fail('this repair is verified only for macOS arm64');
  const manifestInfo=await lstat(manifestFile);
  if(!manifestInfo.isFile()||manifestInfo.isSymbolicLink()||manifestInfo.size>65536)fail('manifest must be a bounded regular file');
  const manifestPath=await realpath(manifestFile),root=path.dirname(manifestPath);
  let manifest;try{manifest=validateCocoaRepairManifest(JSON.parse(await readFile(manifestPath,'utf8')));}catch(error){fail(error.message);}
  const plugin=await ownedFile(root,manifest.plugin.file,32*1024*1024),sourceKit=await ownedFile(root,manifest.sourceKit.file,128*1024*1024);
  if(sha256(plugin.bytes)!==manifest.plugin.sha256||sha256(sourceKit.bytes)!==manifest.sourceKit.sha256)fail('plugin or corresponding-source archive checksum mismatch');
  const regression=await ownedFile(root,manifest.nativeRegression.logFile,4*1024*1024);
  if(sha256(regression.bytes)!==manifest.nativeRegression.logSha256||!regression.bytes.toString().includes('PASS   : tst_QAccessibilityMac::tableViewSelectionAfterModelChange()')||!/Totals: \d+ passed, 0 failed, 0 skipped/.test(regression.bytes.toString()))fail('native regression evidence mismatch');
  const run=(tool,args)=>execFileSync(tool,args,{encoding:'utf8',maxBuffer:16*1024*1024}).trim();
  if(run(path.join(qt,'bin/qtpaths'),['--qt-version'])!==cocoaRepair.qtVersion)fail('Qt kit version mismatch');
  const metadata=JSON.parse(run(path.join(qt,'bin/qtplugininfo'),['--full-json',plugin.file]));
  if(metadata.version!==396288||metadata.debug!==false||metadata.className!=='QCocoaIntegrationPlugin'||metadata.MetaData?.Keys?.join(',')!=='cocoa')fail('plugin metadata does not match release Qt 6.12.0 Cocoa');
  if(run('/usr/bin/lipo',['-archs',plugin.file])!=='arm64')fail('plugin architecture mismatch');
  const pluginUUID=run('/usr/bin/dwarfdump',['--uuid',plugin.file]).match(/UUID: ([A-Fa-f0-9-]{36}) \(arm64\)/)?.[1];
  if(!pluginUUID)fail('plugin UUID missing');
  if(!/\bminos 14\.4\b/.test(run('/usr/bin/otool',['-l',plugin.file])))fail('plugin deployment target mismatch');
  for(const [name,identity] of Object.entries(manifest.frameworks)) {
    const file=path.join(libraryRoot,name+'.framework/Versions/A',name);
    if(sha256(await readFile(file))!==identity.sha256)fail('private ABI kit hash mismatch: '+name);
    if(!run('/usr/bin/dwarfdump',['--uuid',file]).includes(identity.uuid))fail('framework UUID mismatch: '+name);
  }
  verifyCocoaSourceKit(sourceKit.file);
  return {manifest,plugin:plugin.file,pluginUUID,sourceKit:sourceKit.file,notice:
    'This app contains Qt 6.12.0 Cocoa with official backport '+cocoaRepair.commit+' (QTBUG-149612). Accessibility remains enabled. The original Qt source, exact patch, licenses and rebuild recipe are in source-kit.tar.gz beside this notice. This is a locally built dependency repair, not a released Qt update. The plugin remains a separately replaceable dynamic library; replacing it requires re-signing the copied app. See the source kit README.\n'};
}

export async function deployedCocoaRepair(repair,app) {
  if(!repair)return undefined;
  const contents=path.join(app,'Contents'),plugin=path.join(contents,'PlugIns/platforms/libqcocoa.dylib');
  const deployedUUID=execFileSync('/usr/bin/dwarfdump',['--uuid',plugin],{encoding:'utf8'}).match(/UUID: ([A-Fa-f0-9-]{36}) \(arm64\)/)?.[1];
  if(deployedUUID!==repair.pluginUUID)fail('deployed plugin UUID mismatch; the validated repair was replaced');
  for(const [name,identity] of Object.entries(repair.manifest.frameworks)) {
    const file=path.join(contents,'Frameworks',name+'.framework/Versions/A',name);
    const uuids=execFileSync('/usr/bin/dwarfdump',['--uuid',file],{encoding:'utf8'});
    if(!uuids.includes(identity.uuid))fail('deployed framework UUID mismatch: '+name);
  }
  const dependencies=execFileSync('/usr/bin/otool',['-L',plugin],{encoding:'utf8'}).split('\n').slice(1).map(line=>line.trim().split(' ')[0]).filter(Boolean);
  for(const dependency of dependencies) {
    if(dependency.startsWith('/System/Library/')||dependency.startsWith('/usr/lib/'))continue;
    const target=dependency.startsWith('@loader_path/')?path.resolve(path.dirname(plugin),dependency.slice(13)):dependency.startsWith('@rpath/')?path.join(contents,'Frameworks',dependency.slice(7)):undefined;
    if(!target||!(await realpath(target)).startsWith((await realpath(app))+path.sep))fail('external deployed plugin dependency: '+dependency);
  }
  return {patchCommit:cocoaRepair.commit,qtVersion:cocoaRepair.qtVersion,arch:cocoaRepair.arch,
    pluginUUID:repair.pluginUUID,originalPluginSha256:repair.manifest.plugin.sha256,relocatedPluginSha256:sha256(await readFile(plugin)),
    sourceKitSha256:repair.manifest.sourceKit.sha256,sourceKit:'licenses/qt-cocoa-repair/source-kit.tar.gz',
    nativeRegressionLogSha256:repair.manifest.nativeRegression.logSha256,frameworks:repair.manifest.frameworks};
}

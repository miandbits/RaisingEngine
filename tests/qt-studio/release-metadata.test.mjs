import test from 'node:test';
import {nativePlayerCapabilities} from '../../scripts/lib/native-player-capabilities.mjs';
import assert from 'node:assert/strict';
import {mkdtemp,mkdir,readFile,writeFile,symlink,rm,cp} from 'node:fs/promises';
import {existsSync} from 'node:fs';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {projectFiles,digest} from '../../apps/qt-studio/project-files.mjs';
import {verifyProjectBackup,restoreProjectBackup,readProjectBackup} from '../../apps/qt-studio/backup-restore.mjs';
import {writeZip,treeFiles} from '../../apps/qt-studio/release-zip.mjs';
import {prepareRelease,releaseHash} from '../../dist/packages/export-service/release-metadata.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {decodePng,encodeRgbaPng} from '../../dist/packages/media/png.js';
import {prepareIOSProject,configureIOSProject,runAppleTool,verifyIOSBundleMetadata} from '../../scripts/lib/ios-project.mjs';
import {NtExecutable,NtExecutableResource,Resource} from 'resedit';
const call=(s,method,params={})=>s.handle({method,params:{revision:s.editor.revision,...params}});
const getRelease=s=>s.editor.documents.find(d=>d.kind==='assets').data.release;
async function fixture(){
 await mkdir('.cache/release-metadata',{recursive:true});const work=await mkdtemp(path.resolve('.cache/release-metadata/run-')),web=path.join(work,'web-runtime');await mkdir(web);await writeFile(path.join(web,'index.html'),'<html><head><title>Player</title></head><body>Game</body></html>');
 const caps=nativePlayerCapabilities(process.platform);await writeFile(path.join(web,'capabilities.json'),JSON.stringify(caps));
 const s=new QtStudioService({runtimes:{web}});await call(s,'create',{root:path.join(work,'game'),template:'my-first-raising-game'});
 const notice=path.join(work,'Original.txt'),original=Buffer.from('\ufeffCopyright Example Author\r\nPermission to share this synthetic fixture.\r\n');await writeFile(notice,original);
 const pixels=Buffer.alloc(1024*1024*4);for(let y=0;y<1024;y++)for(let x=0;x<1024;x++){const i=(y*1024+x)*4;pixels[i]=x%256;pixels[i+1]=y%256;pixels[i+2]=127;pixels[i+3]=255;}const icon=path.join(work,'master.png');await writeFile(icon,encodeRgbaPng({width:1024,height:1024,pixels}));return {s,work,web,notice,original,icon};
}
async function settings(f,extra={}){return call(f.s,'projectReleaseSettings',{project:structuredClone(f.s.editor.documents.find(d=>d.kind==='project').data),release:{displayName:'Garden & Sky'},iconFile:f.icon,noticeFiles:[f.notice],...extra});}
test('release settings import original bytes, undo together, survive save/reopen and preserve gameplay fingerprint',async()=>{
 const f=await fixture(),s=f.s,baseline=(await s.editor.snapshot(s.editor.revision)).snapshot.contentFingerprint;await settings(f);const release=structuredClone(getRelease(s));assert.equal(release.notices.length,1);assert.equal(release.notices[0].label,'Original.txt');assert.deepEqual(await readFile(path.join(s.editor.root,release.notices[0].path)),f.original);assert.equal(release.notices[0].sha256,digest(f.original));assert.equal((await s.editor.snapshot(s.editor.revision)).snapshot.contentFingerprint,baseline);
 const files=await projectFiles(s.editor);assert.deepEqual(files.get(release.notices[0].path),f.original);assert.ok(files.has(release.icon.path));const recoveries=await s.recovery.list(s.editor.root);assert.ok(recoveries.length);
 await call(s,'edit',{edit:{type:'undo'}});assert.equal(getRelease(s),undefined);await call(s,'edit',{edit:{type:'redo'}});assert.deepEqual(getRelease(s),release);await call(s,'save');const root=s.editor.root;await s.editor.close();assert.ok(existsSync(path.join(root,release.notices[0].path)));await call(s,'open',{root});assert.deepEqual(getRelease(s),release);assert.equal((await s.editor.snapshot(s.editor.revision)).snapshot.contentFingerprint,baseline);
 await assert.rejects(s.handle({method:'projectReleaseSettings',params:{revision:s.editor.revision-1,project:{},release:{}}}),/revision|수정|변경|다시/);
});
test('versioned Web export packages notices and real icons; backup restores exact sources into a verified new project',async()=>{
 const f=await fixture();await settings(f);const destination=path.join(f.work,'published');await call(f.s,'export',{target:'web',destination,releaseVersion:'1.2.3'});const declaration=getRelease(f.s).notices[0];assert.deepEqual(await readFile(path.join(destination,'licenses/project',declaration.id+'.txt')),f.original);const index=JSON.parse(await readFile(path.join(destination,'licenses/project/index.json')));assert.equal(index.notices[0].sha256,digest(f.original));assert.equal(index.notices[0].label,'Original.txt');assert.equal(JSON.parse(await readFile(path.join(destination,'manifest.webmanifest'))).name,'Garden & Sky');assert.match(await readFile(path.join(destination,'index.html'),'utf8'),/<title>Garden &amp; Sky<\/title>/);for(const size of [32,192,512])assert.equal(decodePng(await readFile(path.join(destination,'branding/icon-'+size+'.png'))).width,size);
 const zip=path.join(destination,'Project-backup.zip'),verification=await verifyProjectBackup(zip);assert.equal(verification.contentFingerprintVerified,true);const receipt=JSON.parse(await readFile(path.join(destination,'RELEASE.json')));assert.equal(receipt.backupVerification.archiveSha256,digest(await readFile(zip)));const restored=await restoreProjectBackup(zip,{destination:path.join(f.work,'restored'),expectedSha256:verification.archiveSha256,originalRoot:f.s.editor.root});assert.equal(restored.verification.contentFingerprint,verification.contentFingerprint);assert.deepEqual(await readFile(path.join(restored.directory,declaration.path)),f.original);
 await assert.rejects(restoreProjectBackup(zip,{destination:restored.directory,expectedSha256:verification.archiveSha256}),/already exists/);await assert.rejects(restoreProjectBackup(zip,{destination:path.join(f.s.editor.root,'nested'),expectedSha256:verification.archiveSha256,originalRoot:f.s.editor.root}),/outside/);await assert.rejects(restoreProjectBackup(zip,{destination:path.join(f.work,'bad'),expectedSha256:'0'.repeat(64)}),/changed/);
 await writeFile(path.join(f.work,'proof.json'),JSON.stringify({ok:true,declaration,verification,restored:restored.verification},null,2));
});
test('release validation rejects stale, unsafe, oversized and colliding declarations without publishing partial exports',async()=>{
 const f=await fixture();await settings(f);const release=getRelease(f.s),root=f.s.editor.root;const snapshot=(await f.s.editor.snapshot(f.s.editor.revision)).snapshot;const prepared=await prepareRelease(root,release),file=path.join(root,release.notices[0].path);
 await assert.rejects(prepareRelease(root,{notices:[{...release.notices[0],label:'bad\nlabel'}]}),/labels/);await assert.rejects(prepareRelease(root,{notices:[...release.notices,{...release.notices[0],id:'another'}]}),/colliding/);await assert.rejects(prepareRelease(root,{notices:[{...release.notices[0],path:'../outside.txt'}]}),/safe/);await assert.rejects(prepareRelease(root,{notices:[{...release.notices[0],path:'assets/CON.txt'}]}),/safe/);
 await writeFile(file,'changed');await assert.rejects(prepareRelease(root,release),/changed/);await assert.rejects(projectFiles(f.s.editor),/changed/);await writeFile(file,f.original);
 const destination=path.join(f.work,'stale-export');await assert.rejects(exportGame({content:snapshot,projectRoot:root,destination,target:'web',runtimes:{web:f.web},release:prepared,prepareBundle:()=>writeFile(file,'changed after preparation')}),/changed/);assert.equal(existsSync(destination),false);await writeFile(file,f.original);
 const big=path.join(f.work,'large.txt');await writeFile(big,Buffer.alloc(1024*1024+1,65));const revision=f.s.editor.revision;await assert.rejects(settings(f,{noticeFiles:[big]}),/size|MiB/);assert.equal(f.s.editor.revision,revision);
 await rm(file);await symlink(f.notice,file);await assert.rejects(prepareRelease(root,release),/symbolic/);assert.equal((await readFile(f.notice)).equals(f.original),true);
});
test('Windows and macOS exports embed actual icon resources and application names; signed PE mutation is refused',async()=>{
 const f=await fixture();await settings(f);const release=await prepareRelease(f.s.editor.root,getRelease(f.s)),content=(await f.s.editor.snapshot(f.s.editor.revision)).snapshot,runtimes={web:f.web};
 for(const target of ['windows','macos']){const runtime=path.join(f.work,target);await mkdir(runtime);await cp(path.join(f.web,'capabilities.json'),path.join(runtime,'capabilities.json'));if(target==='windows')await writeFile(path.join(runtime,'RaisingPlayer.exe'),Buffer.from(NtExecutable.createEmpty(false,false).generate()));else{await mkdir(path.join(runtime,'RaisingPlayer.app/Contents/MacOS'),{recursive:true});await writeFile(path.join(runtime,'RaisingPlayer.app/Contents/MacOS/RaisingPlayer'),'fixture');await writeFile(path.join(runtime,'RaisingPlayer.app/Contents/Info.plist'),'<?xml version="1.0"?><plist version="1.0"><dict><key>CFBundleName</key><string>RaisingPlayer</string></dict></plist>');}runtimes[target]=runtime;
  const destination=path.join(f.work,'export-'+target);await exportGame({content,projectRoot:f.s.editor.root,destination,target,runtimes,release});if(target==='windows'){const resource=NtExecutableResource.from(NtExecutable.from(await readFile(path.join(destination,'RaisingPlayer.exe'))));const icons=Resource.IconGroupEntry.fromEntries(resource.entries)[0].getIconItemsFromEntries(resource.entries);assert.deepEqual(icons.map(i=>i.width||256),[16,32,48,64,128,256]);assert.equal(Resource.VersionInfo.fromEntries(resource.entries)[0].getStringValues({lang:1033,codepage:1200}).ProductName,'Garden & Sky');}else{const resources=path.join(destination,'RaisingPlayer.app/Contents/Resources'),bytes=await readFile(path.join(resources,'ReleaseIcon.icns'));assert.equal(bytes.toString('ascii',0,4),'icns');assert.equal(bytes.readUInt32BE(4),bytes.length);let offset=8;const sizes=[];while(offset<bytes.length){const size=bytes.readUInt32BE(offset+4);sizes.push(decodePng(bytes.subarray(offset+8,offset+size)).width);offset+=size;}assert.deepEqual(sizes,[16,32,64,128,256,512,1024]);assert.match(await readFile(path.join(destination,'RaisingPlayer.app/Contents/Info.plist'),'utf8'),/Garden &amp; Sky/);assert.deepEqual(await readFile(path.join(resources,'licenses/project',getRelease(f.s).notices[0].id+'.txt')),f.original);}}
 const signedFile=path.join(runtimes.windows,'RaisingPlayer.exe'),signed=await readFile(signedFile),pe=signed.readUInt32LE(0x3c),security=pe+24+112+4*8;signed.writeUInt32LE(signed.length,security);signed.writeUInt32LE(8,security+4);await writeFile(signedFile,Buffer.concat([signed,Buffer.alloc(8)]));await assert.rejects(exportGame({content,projectRoot:f.s.editor.root,destination:path.join(f.work,'signed'),target:'windows',runtimes,release}),/unsigned PE|Signed runtimes/);assert.equal(existsSync(path.join(f.work,'signed')),false);
});
test('iOS source packaging wires a real AppIcon catalog, literal application name and preserved notices',async()=>{
 const f=await fixture(),displayName=' Garden $(NAME) ${NAME} @HOME@ & "Sky"; C:\\path ';
 await settings(f,{release:{displayName}});
 const release=await prepareRelease(f.s.editor.root,getRelease(f.s)),content=(await f.s.editor.snapshot(f.s.editor.revision)).snapshot,engine=path.join(f.work,'engine');
 for(const entry of ['include','src','vendor','player','mobile/ios','player/vendor/SDL-ios'])await mkdir(path.join(engine,'native',entry),{recursive:true});
 await writeFile(path.join(engine,'native/CMakeLists.txt'),'');
 // Use the real target-directory hook in a tiny child target: this checks actual
 // Xcode resource registration, post-build argument safety, and signing order.
 const playerCmake=(await readFile('native/player/CMakeLists.txt','utf8')).replaceAll('\r\n','\n');
 const nameHook=playerCmake.match(/  if\(RAISING_IOS_RELEASE_NAME_SCRIPT\)\n[\s\S]*?  endif\(\)/)?.[0];
 assert.ok(nameHook,'The real native target must own the literal-name post-build command.');
 await writeFile(path.join(engine,'native/player/main.cpp'),'int main(){return 0;}\n');
 await writeFile(path.join(engine,'native/player/CMakeLists.txt'),'add_executable(raising-player MACOSX_BUNDLE main.cpp)\nset_target_properties(raising-player PROPERTIES OUTPUT_NAME RaisingPlayer MACOSX_BUNDLE_GUI_IDENTIFIER "${RAISING_APP_ID}" MACOSX_BUNDLE_SHORT_VERSION_STRING "${RAISING_IOS_VERSION}" MACOSX_BUNDLE_BUNDLE_VERSION "${RAISING_IOS_BUILD_NUMBER}" XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${RAISING_APP_ID}" XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")\n'+nameHook+'\n');
 await cp('native/mobile/ios/Info.plist.in',path.join(engine,'native/mobile/ios/Info.plist.in'));
 await writeFile(path.join(engine,'native/player/vendor/SDL-ios/LICENSE.txt'),'SDL fixture license');await writeFile(path.join(engine,'LICENSE'),'Engine fixture license');
 const destination=path.join(f.work,'ios');await exportGame({content,projectRoot:f.s.editor.root,destination,target:'ios',runtimes:{ios:f.web,web:f.web},release,prepareBundle:dir=>prepareIOSProject(dir,{root:engine})});
 assert.equal(await readFile(path.join(destination,'Application-DisplayName.txt'),'utf8'),displayName);
 const cmake=await readFile(path.join(destination,'CMakeLists.txt'),'utf8'),script=await readFile(path.join(destination,'ApplyReleaseName.cmake'),'utf8');
 assert.equal(cmake.includes(displayName),false);assert.equal(script.includes(displayName),false);assert.doesNotMatch(cmake,/INFOPLIST_EXPAND_BUILD_SETTINGS/);
 assert.match(script,/file\(READ/);assert.match(script,/COMMAND_ERROR_IS_FATAL ANY/);assert.match(cmake,/ASSETCATALOG_COMPILER_APPICON_NAME AppIcon/);assert.match(cmake,/TARGET_DIRECTORY raising-player PROPERTIES MACOSX_PACKAGE_LOCATION Resources/);
 const catalog=path.join(destination,'branding/AppIcon.xcassets'),entries=JSON.parse(await readFile(path.join(catalog,'AppIcon.appiconset/Contents.json'))).images;
 for(const entry of entries){const decoded=decodePng(await readFile(path.join(catalog,'AppIcon.appiconset',entry.filename)));assert.equal(decoded.width,Number(entry.size.split('x')[0])*Number(entry.scale[0]));}
 assert.deepEqual(await readFile(path.join(destination,'game/licenses/project',getRelease(f.s).notices[0].id+'.txt')),f.original);
 if(process.platform==='darwin'&&existsSync('/usr/bin/xcrun')){
  const output=path.join(f.work,'compiled-icons');await mkdir(output);try{execFileSync('/usr/bin/xcrun',['--find','actool'],{stdio:'pipe'});}catch{return;}
  execFileSync('/usr/bin/xcrun',['actool',catalog,'--compile',output,'--platform','iphonesimulator','--minimum-deployment-target','16.0','--app-icon','AppIcon','--output-partial-info-plist',path.join(output,'Info.plist'),'--target-device','iphone','--target-device','ipad'],{stdio:'pipe'});assert.ok(existsSync(path.join(output,'Assets.car')));
  assert.match(execFileSync('/usr/bin/plutil',['-extract','CFBundleIcons.CFBundlePrimaryIcon.CFBundleIconName','raw','-o','-',path.join(output,'Info.plist')],{encoding:'utf8'}),/AppIcon/);
  await configureIOSProject(destination,{sdk:'iphonesimulator'});
  await runAppleTool(process.env.CMAKE??'cmake',['--build','build-iphonesimulator','--config','Release','--target','raising-player','--parallel','2','--','CODE_SIGNING_ALLOWED=YES','CODE_SIGN_IDENTITY=-'],destination,{logFile:path.join(f.work,'icon-integration-build.log')});
  const built=await verifyIOSBundleMetadata(destination,{sdk:'iphonesimulator'});assert.equal(built.appIconVerified,true);assert.equal(built.displayName,displayName);
  const plist=JSON.parse(execFileSync('/usr/bin/plutil',['-convert','json','-o','-',built.plist],{encoding:'utf8'}));assert.equal(plist.CFBundleName,displayName);assert.equal(plist.MinimumOSVersion,'16.0');for(const [key,value]of Object.entries(plist))if((key.startsWith('DT')||key==='BuildMachineOSBuild')&&typeof value==='string')assert.doesNotMatch(value,/\$[({]/,key);
  execFileSync('/usr/bin/codesign',['--verify','--strict',path.dirname(built.plist)],{stdio:'pipe'});
  await writeFile(path.join(f.work,'icon-integration-proof.json'),JSON.stringify({...built,plist,adHocSignatureVerifiedAfterNameStamp:true},null,2));
 }
});
test('backup reader rejects undeclared files, corrupt CRC and unsafe archive entries before materialization',async()=>{
 const f=await fixture();await settings(f);const destination=path.join(f.work,'export');await call(f.s,'export',{target:'web',destination,releaseVersion:'1.0.0'});const archive=path.join(destination,'Project-backup.zip'),backup=await readProjectBackup(archive),source=path.join(f.work,'tamper');await mkdir(source);for(const [file,bytes] of backup.files){await mkdir(path.dirname(path.join(source,file)),{recursive:true});await writeFile(path.join(source,file),bytes);}await writeFile(path.join(source,'extra.txt'),'undeclared');const tampered=path.join(f.work,'extra.zip');await writeZip(source,await treeFiles(source),tampered);await assert.rejects(readProjectBackup(tampered),/undeclared/);
 const bytes=await readFile(archive);bytes[30+bytes.readUInt16LE(26)+bytes.readUInt16LE(28)]^=1;const corrupt=path.join(f.work,'corrupt.zip');await writeFile(corrupt,bytes);await assert.rejects(readProjectBackup(corrupt),/CRC|checksum/);
 const unsafe=Buffer.from(await readFile(archive));const central=unsafe.indexOf(Buffer.from([0x50,0x4b,0x01,0x02]));unsafe.writeUInt32LE((0xa1ff<<16)>>>0,central+38);const linked=path.join(f.work,'symlink.zip');await writeFile(linked,unsafe);await assert.rejects(readProjectBackup(linked),/Unsafe/);
});

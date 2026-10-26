import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,writeFile,readFile,readdir,symlink,rename,lstat,readlink} from 'node:fs/promises';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {spawnSync} from 'node:child_process';
import {prepareIOSProject,configureIOSProject,validateIOSVersions,verifyIOSBundleMetadata} from '../../scripts/lib/ios-project.mjs';

async function fixture() {
  await mkdir('.cache/ios-package-tests',{recursive:true});
  const root=await mkdtemp(path.resolve('.cache/ios-package-tests/run-'));
  for(const file of ['LICENSE','native/CMakeLists.txt','native/include/core.hpp','native/src/core.cpp','native/vendor/json.hpp','native/player/main.cpp','native/player/surface.hpp','native/player/surface-markup.inc','native/player/vendor/SDL-ios/LICENSE.txt','native/mobile/ios/Info.plist.in']) {
    await mkdir(path.dirname(path.join(root,file)),{recursive:true});await writeFile(path.join(root,file),file);
  }
  const out=path.join(root,'export');await mkdir(path.join(out,'assets'),{recursive:true});await mkdir(path.join(out,'fonts'));
  await writeFile(path.join(out,'game.json'),JSON.stringify({formatVersion:'1',content:{contentFingerprint:'sha256:test'},assets:{}}));
  return {root,out};
}

test('iOS export is relocatable, preserves resources and licenses, and defaults to unsigned Xcode presets',async()=>{
  const {root,out}=await fixture();
  await prepareIOSProject(out,{root,applicationId:'org.example.garden'});
  assert.equal(JSON.parse(await readFile(path.join(out,'game/game.json'),'utf8')).content.contentFingerprint,'sha256:test');
  assert.equal(await readFile(path.join(out,'game/licenses/SDL.txt'),'utf8'),'native/player/vendor/SDL-ios/LICENSE.txt');
  for(const name of ['surface.hpp','surface-markup.inc'])assert.equal(await readFile(path.join(out,'engine/native/player',name),'utf8'),'native/player/'+name,'Relocated iOS source includes the reusable surface header/markup');
  const cmake=await readFile(path.join(out,'CMakeLists.txt'),'utf8');assert.ok(cmake.includes('${CMAKE_CURRENT_SOURCE_DIR}/game'));assert.ok(!cmake.includes(root));
  const presets=JSON.parse(await readFile(path.join(out,'CMakePresets.json'),'utf8')).configurePresets;
  assert.deepEqual(presets.map(p=>p.name),['iphonesimulator','iphoneos']);
  assert.ok(presets.every(p=>p.cacheVariables.CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED==='NO'));
  const metadata=JSON.parse(await readFile(path.join(out,'ios-build.json'),'utf8'));
  assert.equal(metadata.applicationId,'org.example.garden');assert.equal(metadata.releaseVersion,'0.1.0');assert.equal(metadata.buildNumber,'1');
});

test('iOS preparation rejects invalid identifiers and source symlinks',async()=>{
  const {root,out}=await fixture();const before=await readdir(out);
  await assert.rejects(prepareIOSProject(out,{root,applicationId:'../invalid'}),/application ID/);assert.deepEqual(await readdir(out),before);
  await symlink(path.join(root,'LICENSE'),path.join(root,'native/include/external'));
  await assert.rejects(prepareIOSProject(out,{root}),/symbolic link/);
});

// The runtime retains legacy orientation hints; the app manifest permits reading-layout rotation.
test('iOS app manifest permits portrait and landscape reading layouts on phone and tablet',async()=>{
  const plist=await readFile('native/mobile/ios/Info.plist.in','utf8');
  for(const key of ['UISupportedInterfaceOrientations','UISupportedInterfaceOrientations~ipad']){
    const array=plist.split(`<key>${key}</key>`)[1]?.split('</array>')[0];
    assert.ok(array, key);
    for(const orientation of ['Portrait','LandscapeLeft','LandscapeRight'])assert.ok(array.includes(`UIInterfaceOrientation${orientation}</string>`),`${key}: ${orientation}`);
  }
});


test('post-publish Xcode configuration preserves hashed metadata; standalone configuration records status',{skip:process.platform!=='darwin'},async t=>{
  const {root,out}=await fixture();await prepareIOSProject(out,{root,applicationId:'org.example.garden'});
  const metadataFile=path.join(out,'ios-build.json');
  // Include formatting in the sealed bytes: JSON-equivalent rewriting is still a release mutation.
  await writeFile(metadataFile,(await readFile(metadataFile,'utf8'))+'\n');
  const sealed=await readFile(metadataFile),digest=bytes=>createHash('sha256').update(bytes).digest('hex');
  const manifest={files:[{file:'ios-build.json',sha256:digest(sealed)}]};
  await writeFile(path.join(out,'RELEASE.json'),JSON.stringify(manifest));
  const cmake=path.join(root,'fake cmake.command');
  await writeFile(cmake,'#!/bin/sh\nset -eu\n[ "$1" = "--preset" ]\n[ "$2" = "iphonesimulator" ]\nmkdir -p "build-$2/RaisingGame.xcodeproj"\necho "Generated Xcode fixture"\n',{mode:0o755});
  const previous=process.env.CMAKE;process.env.CMAKE=cmake;
  t.after(()=>{if(previous===undefined)delete process.env.CMAKE;else process.env.CMAKE=previous;});
  const generated=await configureIOSProject(out,{recordStatus:false});
  assert.equal(generated,path.join(out,'build-iphonesimulator','RaisingGame.xcodeproj'));
  assert.ok((await readdir(path.dirname(generated))).includes('RaisingGame.xcodeproj'));
  assert.deepEqual(await readFile(metadataFile),sealed,'post-publish configuration preserves the exact released bytes');
  const published=JSON.parse(await readFile(path.join(out,'RELEASE.json'),'utf8'));
  for(const file of published.files)assert.equal(digest(await readFile(path.join(out,file.file))),file.sha256,'untouched exported release still verifies');
  const standalone=path.join(root,'standalone');await mkdir(standalone);await writeFile(path.join(standalone,'ios-build.json'),sealed);
  assert.equal(await configureIOSProject(standalone),path.join(standalone,'build-iphonesimulator','RaisingGame.xcodeproj'));
  assert.deepEqual(JSON.parse(await readFile(path.join(standalone,'ios-build.json'),'utf8')),{...JSON.parse(sealed.toString()),status:'xcode-project-generated',sdk:'iphonesimulator',signed:false},'standalone callers retain normal status recording');
  assert.deepEqual(await readFile(metadataFile),sealed);
});


test('iOS export preserves the selected release version and independent build number',async()=>{
  const {root,out}=await fixture();await prepareIOSProject(out,{root,version:'2.7.3',buildNumber:'42.1'});
  const metadata=JSON.parse(await readFile(path.join(out,'ios-build.json'),'utf8'));
  assert.equal(metadata.releaseVersion,'2.7.3');assert.equal(metadata.buildNumber,'42.1');
  const cmake=await readFile(path.join(out,'CMakeLists.txt'),'utf8');
  assert.ok(cmake.includes('set(RAISING_IOS_VERSION "2.7.3" CACHE STRING'));assert.ok(cmake.includes('set(RAISING_IOS_BUILD_NUMBER "42.1" CACHE STRING'));
  const help=await readFile(path.join(out,'IOS-README.txt'),'utf8');assert.match(help,/Release version: 2\.7\.3/);assert.match(help,/Build number: 42\.1/);
});

test('iOS version validation rejects malformed and unsafe metadata before changing export resources',async()=>{
  const {root,out}=await fixture(),before=await readdir(out),game=await readFile(path.join(out,'game.json'));
  assert.deepEqual(validateIOSVersions(),{version:'0.1.0',buildNumber:'1'});
  for(const version of ['0.1','1.2.3-beta','1.2.3.4','1.2.-1','1.2.3\n','1.2.3"','1000000000.0.0',null,1]){
    await assert.rejects(prepareIOSProject(out,{root,version}),/iOS release version/);assert.deepEqual(await readdir(out),before);assert.deepEqual(await readFile(path.join(out,'game.json')),game);
  }
  for(const buildNumber of ['', '1.2.3.4', '-1', '1b1', '1\n', '$(touch x)', '1000000000', null, 2]){
    await assert.rejects(prepareIOSProject(out,{root,buildNumber}),/iOS build number/);assert.deepEqual(await readdir(out),before);assert.deepEqual(await readFile(path.join(out,'game.json')),game);
  }
  for(const buildNumber of ['0','2','2.0','2.3.4','999999999.999999999.999999999'])assert.equal(validateIOSVersions({buildNumber}).buildNumber,buildNumber);
});


test('built iOS app metadata is verified independently for each SDK',{skip:process.platform!=='darwin'},async()=>{
  const {root,out}=await fixture();await prepareIOSProject(out,{root,version:'2.7.3',buildNumber:'42.1'});
  for(const sdk of ['iphonesimulator','iphoneos']){
    const app=path.join(out,'build-'+sdk,'native-player','Release-'+sdk,'RaisingPlayer.app');await mkdir(app,{recursive:true});
    const plist=path.join(app,'Info.plist');
    const content=build=>`<?xml version="1.0" encoding="UTF-8"?><plist version="1.0"><dict><key>CFBundleIdentifier</key><string>org.raisingengine.firstgame</string><key>CFBundleShortVersionString</key><string>2.7.3</string><key>CFBundleVersion</key><string>${build}</string></dict></plist>`;
    await writeFile(plist,content('42.1'));const sealed=await readFile(path.join(out,'ios-build.json'));
    assert.deepEqual(await verifyIOSBundleMetadata(out,{sdk}),{sdk,plist,applicationId:'org.raisingengine.firstgame',releaseVersion:'2.7.3',buildNumber:'42.1'});
    await writeFile(plist,content('1'));await assert.rejects(verifyIOSBundleMetadata(out,{sdk}),/CFBundleVersion differs/);
    assert.deepEqual(await readFile(path.join(out,'ios-build.json')),sealed,'inspection never rewrites release metadata');
  }
});

async function relocationTools(root,t){
 const bin=path.join(root,'tool shims');await mkdir(bin);
 const cmake=path.join(bin,'cmake with spaces.command'),trace=path.join(root,'configure-arguments.txt');
 await writeFile(cmake,`#!/bin/zsh
set -eu
printf '%s\\n' "$@" >> "$RAISING_RELOCATION_TRACE"
[[ "$1" == --preset ]]
sdk="$2"
if [[ "\${RAISING_RELOCATION_FAIL:-}" == yes ]]; then exit 7; fi
mkdir -p "build-$sdk/RaisingGame.xcodeproj"
printf 'CMAKE_HOME_DIRECTORY:INTERNAL=%s\\nCMAKE_CACHEFILE_DIR:INTERNAL=%s\\n' "$PWD" "$PWD/build-$sdk" > "build-$sdk/CMakeCache.txt"
`,{mode:0o755});
 await writeFile(path.join(bin,'open'),'#!/bin/sh\nprintf "%s\\n" "$@" > "$RAISING_RELOCATION_OPEN"\n',{mode:0o755});
 const keys=['CMAKE','PATH','RAISING_RELOCATION_TRACE','RAISING_RELOCATION_OPEN','RAISING_RELOCATION_FAIL'],previous=Object.fromEntries(keys.map(k=>[k,process.env[k]]));
 Object.assign(process.env,{CMAKE:cmake,PATH:bin+path.delimiter+process.env.PATH,RAISING_RELOCATION_TRACE:trace,RAISING_RELOCATION_OPEN:path.join(root,'opened.txt')});delete process.env.RAISING_RELOCATION_FAIL;
 t.after(()=>{for(const k of keys)if(previous[k]===undefined)delete process.env[k];else process.env[k]=previous[k];});
 return {trace,open:path.join(root,'opened.txt')};
}
async function buildEntries(root){
 const rows=[];async function walk(relative){const full=path.join(root,relative),s=await lstat(full),row={path:relative,mode:s.mode&0o7777};if(s.isSymbolicLink())rows.push({...row,type:'link',target:await readlink(full)});else if(s.isDirectory()){rows.push({...row,type:'directory'});for(const name of (await readdir(full)).sort())await walk(path.join(relative,name));}else rows.push({...row,type:'file',sha256:createHash('sha256').update(await readFile(full)).digest('hex')});}await walk('');return rows;
}
const relocatedFolders=async out=>(await readdir(out)).filter(x=>x.startsWith('.raising-moved-')).sort();
async function runRelocation(mode,out,{sdk='iphonesimulator',team}={}){
 if(mode==='service')return configureIOSProject(out,{sdk,team,recordStatus:false});
 const r=spawnSync('/bin/zsh',[path.join(out,'Generate Xcode project.command'),sdk,team??''],{env:process.env,encoding:'utf8'});
 if(r.status!==0)throw Error(r.stdout+'\n'+r.stderr);
 return path.join(out,'build-'+sdk,'RaisingGame.xcodeproj');
}

test('iOS launcher and service preserve relocated caches and keep same-location builds incremental',{skip:process.platform!=='darwin'},async t=>{
 const toolsRoot=await fixture(),tools=await relocationTools(toolsRoot.root,t);
 for(const mode of ['service','launcher'])for(const mismatch of ['source','build']){
  const {root,out:initial}=await fixture();await prepareIOSProject(initial,{root});
  const out=path.join(root,"moved project 'quoted' $literal "+mode+'-'+mismatch);await rename(initial,out);
  const build=path.join(out,'build-iphonesimulator');await mkdir(path.join(build,'custom settings'),{recursive:true});
  const metadata=await readFile(path.join(out,'ios-build.json'));
  const cache=`CMAKE_HOME_DIRECTORY:INTERNAL=${mismatch==='source'?path.join(root,'previous source'):out}\nCMAKE_CACHEFILE_DIR:INTERNAL=${mismatch==='build'?path.join(root,'previous build'):build}\n`;
  await writeFile(path.join(build,'CMakeCache.txt'),cache);await writeFile(path.join(build,'custom settings','local-signing.txt'),Buffer.from([0,255,42,13,10]),{mode:0o751});await symlink('custom settings/local-signing.txt',path.join(build,'local-settings-link'));
  const before=await buildEntries(build);
  await runRelocation(mode,out,{team:'A1B2C3D4E5'});
  const backups=await relocatedFolders(out);assert.equal(backups.length,1);
  assert.deepEqual(await buildEntries(path.join(out,backups[0],'build-iphonesimulator')),before,'Every old build byte, mode and link is preserved');
  assert.equal(await readFile(path.join(build,'CMakeCache.txt'),'utf8'),`CMAKE_HOME_DIRECTORY:INTERNAL=${out}\nCMAKE_CACHEFILE_DIR:INTERNAL=${build}\n`);
  const inode=(await lstat(build)).ino;await writeFile(path.join(build,'incremental-sentinel'),'Retain same-location local build');
  await runRelocation(mode,out);assert.equal((await lstat(build)).ino,inode);assert.deepEqual(await relocatedFolders(out),backups);assert.equal(await readFile(path.join(build,'incremental-sentinel'),'utf8'),'Retain same-location local build');
  await writeFile(path.join(build,'CMakeCache.txt'),cache);const second=await buildEntries(build);await runRelocation(mode,out);
  const all=await relocatedFolders(out);assert.equal(all.length,2,'A second relocation gets a distinct backup');
  const next=all.find(x=>x!==backups[0]);assert.deepEqual(await buildEntries(path.join(out,next,'build-iphonesimulator')),second);assert.deepEqual(await buildEntries(path.join(out,backups[0],'build-iphonesimulator')),before,'Earlier backups are never overwritten');
  assert.deepEqual(await readFile(path.join(out,'ios-build.json')),metadata,'Published release metadata stays byte-identical');
 }
 const args=await readFile(tools.trace,'utf8');assert.match(args,/-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=A1B2C3D4E5/);assert.ok(!args.includes('--fresh'),'CMake3.21.1 support does not require the newer --fresh option');assert.equal((await readFile(tools.open,'utf8')).trim(),'build-iphonesimulator/RaisingGame.xcodeproj');
});

test('iOS relocation refuses build/cache links and unidentified caches without changing their data',{skip:process.platform!=='darwin'},async t=>{
 const toolsRoot=await fixture(),tools=await relocationTools(toolsRoot.root,t);
 for(const mode of ['service','launcher'])for(const kind of ['build-link','cache-link','build-file','unidentified-cache']){
  const {root,out}=await fixture();await prepareIOSProject(out,{root});const external=path.join(root,'outside');await mkdir(external);await writeFile(path.join(external,'sentinel'),'Never alter outside data');
  const build=path.join(out,'build-iphonesimulator');
  if(kind==='build-link')await symlink(external,build);
  else if(kind==='build-file')await writeFile(build,'Existing file is not a build directory');
  else{await mkdir(build);if(kind==='cache-link')await symlink(path.join(external,'sentinel'),path.join(build,'CMakeCache.txt'));else await writeFile(path.join(build,'CMakeCache.txt'),'Unidentified cache must not be discarded');}
  const before=await buildEntries(out),outside=await buildEntries(external),trace=await readFile(tools.trace,'utf8').catch(()=>null);
  await assert.rejects(runRelocation(mode,out),/symbolic link|source\/build identity/);
  // Service diagnostics add only their owned log; compare all prior entries.
  const after=(await buildEntries(out)).filter(x=>x.path!=='xcode-configure.log');assert.deepEqual(after,before);assert.deepEqual(await buildEntries(external),outside);assert.equal(await readFile(tools.trace,'utf8').catch(()=>null),trace,'Unsafe paths never reach CMake');assert.deepEqual(await relocatedFolders(out),[]);
 }
});

test('iOS moved device cache remains preserved even when fresh configuration fails',{skip:process.platform!=='darwin'},async t=>{
 const {root,out}=await fixture();await prepareIOSProject(out,{root});await relocationTools(root,t);
 const build=path.join(out,'build-iphoneos');await mkdir(build);await writeFile(path.join(build,'CMakeCache.txt'),'CMAKE_HOME_DIRECTORY:INTERNAL=/old/project\nCMAKE_CACHEFILE_DIR:INTERNAL=/old/project/build-iphoneos\n');await writeFile(path.join(build,'signed-archive-sentinel'),'Preserve a prior local artifact even on failure');const before=await buildEntries(build);
 process.env.RAISING_RELOCATION_FAIL='yes';await assert.rejects(configureIOSProject(out,{sdk:'iphoneos',recordStatus:false}),/failed \(7\)/);
 const backups=await relocatedFolders(out);assert.equal(backups.length,1);assert.deepEqual(await buildEntries(path.join(out,backups[0],'build-iphoneos')),before);assert.equal(await lstat(build).catch(()=>null),null);
});

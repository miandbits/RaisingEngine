import {mkdir,readFile,writeFile,cp,readdir} from 'node:fs/promises';
import {existsSync} from 'node:fs';
import {execFileSync,spawnSync} from 'node:child_process';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {cocoaRepair,sha256,prepareCocoaRepair} from './lib/qt-cocoa-repair.mjs';

const args=process.argv.slice(2);
if(args.length!==2||args[0]!=='--out')throw Error('Usage: node scripts/build-qt-cocoa-repair.mjs --out NEW_DIRECTORY (opens disposable native AX test windows)');
if(process.platform!=='darwin'||process.arch!=='arm64')throw Error('This narrow repair is verified only on macOS arm64.');
const root=fileURLToPath(new URL('..',import.meta.url)),out=path.resolve(args[1]);
if(existsSync(out))throw Error('Choose a new output directory; existing files are preserved.');
const qt=process.env.QT_ROOT??'/opt/homebrew/opt/qt';
const run=(tool,argv)=>execFileSync(tool,argv,{encoding:'utf8',maxBuffer:16*1024*1024}).trim();
if(run(path.join(qt,'bin/qtpaths'),['--qt-version'])!==cocoaRepair.qtVersion)throw Error('An exact Qt6.12.0 kit is required.');
const libs=run(path.join(qt,'bin/qtpaths'),['--query','QT_INSTALL_LIBS']);
const frameworks={},installedBinaries=[];
for(const name of ['QtCore','QtGui','QtWidgets']) {
  const file=path.join(libs,name+'.framework/Versions/A',name);
  if(run('/usr/bin/lipo',['-archs',file])!=='arm64')throw Error('Expected arm64 framework: '+name);
  const uuid=run('/usr/bin/dwarfdump',['--uuid',file]).match(/UUID: ([A-Fa-f0-9-]{36}) \(arm64\)/)?.[1];
  if(!uuid)throw Error('Missing framework UUID: '+name);
  frameworks[name]={sha256:sha256(await readFile(file)),uuid};
  installedBinaries.push({path:name+'.framework/Versions/A/'+name,...frameworks[name]});
}
await mkdir(out,{recursive:true});
const kit=path.join(out,'qt-cocoa-source-kit');
for(const dir of ['downloads','driver','licenses'])await mkdir(path.join(kit,dir),{recursive:true});
const downloads=[
  ['qtbase-everywhere-src-6.12.0.tar.xz','https://download.qt.io/official_releases/qt/6.12/6.12.0/submodules/qtbase-everywhere-src-6.12.0.tar.xz',cocoaRepair.qtArchiveSha256],
  [cocoaRepair.commit+'.patch','https://github.com/qt/qtbase/commit/'+cocoaRepair.commit+'.patch',cocoaRepair.patchSha256],
  ['MoltenVK-v1.4.2.tar.gz','https://github.com/KhronosGroup/MoltenVK/archive/refs/tags/v1.4.2.tar.gz',cocoaRepair.moltenArchiveSha256],
  ['Vulkan-Headers-1.4.363.0.tar.gz','https://github.com/KhronosGroup/Vulkan-Headers/archive/refs/tags/vulkan-sdk-1.4.363.0.tar.gz',cocoaRepair.vulkanArchiveSha256],
];
for(const [file,url,expected] of downloads) {
  const response=await fetch(url,{signal:AbortSignal.timeout(120000)});
  if(!response.ok)throw Error('Source download failed: '+url);
  const bytes=Buffer.from(await response.arrayBuffer());
  if(sha256(bytes)!==expected)throw Error('Source checksum mismatch: '+file);
  await writeFile(path.join(kit,'downloads',file),bytes);
}
for(const name of ['LGPL-3.0-only.txt','GPL-3.0-only.txt']) {
  const bytes=execFileSync('/usr/bin/tar',['-xOf',path.join(kit,'downloads/qtbase-everywhere-src-6.12.0.tar.xz'),'qtbase-everywhere-src-6.12.0/LICENSES/'+name]);
  await writeFile(path.join(kit,'licenses',name),bytes);
}
await cp(path.join(root,'config/qt-cocoa-repair/CMakeLists.txt'),path.join(kit,'driver/CMakeLists.txt'));
await cp(path.join(root,'scripts/lib/qt-cocoa-rebuild.py'),path.join(kit,'rebuild.py'));
await cp(path.join(root,'docs/development/qt-cocoa-accessibility.md'),path.join(kit,'README.md'));
await writeFile(path.join(kit,'installed-kit.json'),JSON.stringify({qtVersion:cocoaRepair.qtVersion,architecture:'arm64',installedBinaries},null,2)+'\n');
await writeFile(path.join(kit,'sources.json'),JSON.stringify(downloads.map(([file,url,sha256])=>({file,url,sha256})),null,2)+'\n');
const checksums={};
async function scan(dir) {for(const entry of await readdir(dir,{withFileTypes:true})) {const file=path.join(dir,entry.name);if(entry.isDirectory())await scan(file);else if(entry.isFile())checksums[path.relative(kit,file).split(path.sep).join('/')]=sha256(await readFile(file));else throw Error('Unexpected source-kit link');}}
await scan(kit);await writeFile(path.join(kit,'source-checksums.json'),JSON.stringify(checksums,null,2)+'\n');
const build=spawnSync('/usr/bin/python3',[path.join(kit,'rebuild.py'),'--work-dir',path.join(out,'work'),'--qt-prefix',qt,'--cmake',process.env.CMAKE??'/opt/homebrew/bin/cmake','--ninja',process.env.NINJA??'/opt/homebrew/bin/ninja'],{encoding:'utf8',timeout:600000});
await writeFile(path.join(out,'rebuild.log'),build.stdout+build.stderr);
if(build.status!==0)throw Error('Isolated plugin build failed; inspect '+path.join(out,'rebuild.log'));
const plugin=path.join(out,'work/build/plugins/platforms/libqcocoa.dylib'),test=path.join(out,'work/build/tst_qaccessibilitymac');
const testEnv={...process.env,QT_QPA_PLATFORM:'cocoa',QT_DEBUG_PLUGINS:'1',QT_PLUGIN_PATH:path.join(out,'work/build/plugins'),QT_QPA_PLATFORM_PLUGIN_PATH:path.dirname(plugin)};
const tested=spawnSync(test,['tableViewSelectionAfterModelChange','tableViewTest','treeViewTest','hierarchyTest','lineEditTest','checkBoxTest','-v1'],{env:testEnv,encoding:'utf8',timeout:90000});
const log=tested.stdout+tested.stderr;await writeFile(path.join(out,'native-regression.log'),log);
if(tested.status!==0||!log.includes(plugin+'" loaded library'))throw Error('Native AX verification failed or used a different plugin; see native-regression.log. No package manifest was created.');
execFileSync('/usr/bin/tar',['-czf',path.join(out,'source-kit.tar.gz'),'-C',out,'qt-cocoa-source-kit']);
const manifest={schemaVersion:1,qtVersion:cocoaRepair.qtVersion,arch:'arm64',minimumMacOS:cocoaRepair.minimumMacOS,patchCommit:cocoaRepair.commit,
  plugin:{file:'work/build/plugins/platforms/libqcocoa.dylib',sha256:sha256(await readFile(plugin))},
  sourceKit:{file:'source-kit.tar.gz',sha256:sha256(await readFile(path.join(out,'source-kit.tar.gz')))},frameworks,
  nativeRegression:{passed:true,case:'tableViewSelectionAfterModelChange',logFile:'native-regression.log',logSha256:sha256(Buffer.from(log))}};
const manifestFile=path.join(out,'repair-manifest.json');await writeFile(manifestFile,JSON.stringify(manifest,null,2)+'\n');
await prepareCocoaRepair(manifestFile,{qt,libraryRoot:libs});
console.log('Verified opt-in Cocoa repair: '+manifestFile);

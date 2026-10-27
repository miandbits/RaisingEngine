import {spawnSync,execFileSync} from 'node:child_process';
import {readFile,mkdir,mkdtemp,readdir,realpath} from 'node:fs/promises';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import assert from 'node:assert/strict';

if(process.platform!=='darwin'||!process.argv[2])throw Error('Usage: node scripts/test-macos-package.mjs PATH_TO_EDITOR.app [--ios]');
const app=await realpath(process.argv[2]),contents=path.join(app,'Contents'),engine=path.join(contents,'Resources/engine');
const executable=path.join(contents,'MacOS/RaisingStudioQt');
async function binaries(directory) {
  const files=[];
  for(const entry of await readdir(directory,{withFileTypes:true})) {
    const file=path.join(directory,entry.name);if(entry.isSymbolicLink())continue;
    if(entry.isDirectory())files.push(...await binaries(file));
    else {const magic=(await readFile(file)).subarray(0,4).toString('hex');if(['cffaedfe','cefaedfe','cafebabe','bebafeca'].includes(magic))files.push(file);}
  }
  return files;
}
const native=JSON.parse(await readFile(path.join(engine,'dist/native/runtime.json'),'utf8'));
const files=[executable,path.join(engine,'node'),path.join(engine,'dist/native',native.executable),path.join(engine,'dist/native-player-app/RaisingPlayer.app/Contents/MacOS/RaisingPlayer'),...await binaries(path.join(contents,'Frameworks')),...await binaries(path.join(contents,'PlugIns'))];
for(const file of files) {
  const ownId=execFileSync('/usr/bin/otool',['-D',file],{encoding:'utf8'}).split('\n')[1]?.trim();
  const dependencies=execFileSync('/usr/bin/otool',['-L',file],{encoding:'utf8'}).split('\n').slice(1).map(s=>s.trim().split(' ')[0]).filter(Boolean);
  for(const dependency of dependencies) {
    if(dependency===ownId)continue;
    if(dependency.startsWith('/usr/lib/')||dependency.startsWith('/System/Library/'))continue;
    const resolved=dependency.startsWith('@rpath/')?path.join(contents,'Frameworks',dependency.slice(7)):dependency.startsWith('@loader_path/')?path.resolve(path.dirname(file),dependency.slice(13)):dependency.startsWith('@executable_path/')?path.resolve(contents,'MacOS',dependency.slice(17)):dependency;
    const actual=await realpath(resolved);assert.ok(actual.startsWith(app+path.sep),`External dependency: ${file}: ${dependency}`);
  }
}
execFileSync('/usr/bin/codesign',['--verify','--deep','--strict',app]);
const env=Object.fromEntries(Object.entries(process.env).filter(([key])=>!key.startsWith('RAISING_')&&!key.startsWith('QT_')&&!key.startsWith('DYLD_')&&!['NODE_OPTIONS','NODE_PATH'].includes(key)));env.PATH='/usr/bin:/bin';
const help=spawnSync(executable,['--help'],{env,encoding:'utf8',timeout:30000,cwd:'/tmp'});assert.equal(help.status,0,help.stderr);
await mkdir('.cache/first-game-package',{recursive:true});const out=await mkdtemp(path.resolve('.cache/first-game-package/run-'));
const source=`import {QtStudioService} from ${JSON.stringify(pathToFileURL(path.join(engine,'apps/qt-studio/service.mjs')).href)};
import {spawnSync,execFileSync} from 'node:child_process';
import {readFile,cp,access} from 'node:fs/promises';
const service=new QtStudioService();
await service.handle({method:'create',params:{root:${JSON.stringify(path.join(out,'game'))},template:'my-first-raising-game'}});
const result=await service.handle({method:'play',params:{revision:service.editor.revision}});
const p=result.play;const run=spawnSync(p.executable,['--bundle',p.directory,'--frames','3','--hidden','--report',p.report],{encoding:'utf8',env:process.env,timeout:30000});
if(run.status!==0)throw Error(run.stderr);
const exported=await service.handle({method:'export',params:{revision:service.editor.revision,target:'macos',releaseVersion:'0.1.0',destination:${JSON.stringify(path.join(out,'shipped'))}}});
const manifest=JSON.parse(await readFile(exported.exported.directory+'/RELEASE.json','utf8'));
if(!manifest.dependencies.dependencies?.length||!manifest.archives?.game?.sha256)throw Error('Release manifest is incomplete');
const gameApp=exported.exported.directory+'/RaisingPlayer.app';
await access(gameApp+'/Contents/Resources/fonts/NotoSansJP.ttf');
await access(gameApp+'/Contents/MacOS/fonts').then(()=>{throw Error('Legacy resources remain in MacOS');},error=>{if(error.code!=='ENOENT')throw error;});
const copiedApp=exported.exported.directory+'-copied.app',unzipped=exported.exported.directory+'-unzipped';
await cp(gameApp,copiedApp,{recursive:true});
execFileSync('/usr/bin/unzip',['-q',exported.exported.directory+'/Game.zip','-d',unzipped]);
for(const candidate of [gameApp,copiedApp,unzipped+'/RaisingPlayer.app'])execFileSync('/usr/bin/codesign',['--verify','--deep','--strict',candidate]);
const shipped=spawnSync(${JSON.stringify(path.join(out,'shipped/RaisingPlayer.app/Contents/MacOS/RaisingPlayer'))},['--frames','3','--hidden','--report',${JSON.stringify(path.join(out,'shipped-report.json'))}],{encoding:'utf8',env:process.env,timeout:30000,cwd:'/tmp'});
if(shipped.status!==0)throw Error(shipped.stderr);
let ios;
if(${process.argv.includes('--ios')}) {
  ios=(await service.handle({method:'export',params:{revision:service.editor.revision,target:'ios',releaseVersion:'0.1.0',destination:${JSON.stringify(path.join(out,'ios'))}}})).exported;
  if(!ios.xcodeProject||ios.warning)throw Error(ios.warning??'Xcode project missing');
}
console.log(JSON.stringify({project:service.editor.root,exported:exported.exported,macSignatures:{original:true,ordinaryCopy:true,gameArchive:true},ios}));`;
const run=spawnSync(path.join(engine,'node'),['--input-type=module','-e',source],{env,encoding:'utf8',timeout:process.argv.includes('--ios')?900000:90000,cwd:'/tmp'});assert.equal(run.status,0,run.stderr);
await writeResult();
async function writeResult(){const {writeFile}=await import('node:fs/promises');await writeFile(path.join(out,'result.json'),JSON.stringify({ok:true,app,checkedBinaries:files.length,result:JSON.parse(run.stdout)},null,2));console.log('Standalone package proof: '+out);}

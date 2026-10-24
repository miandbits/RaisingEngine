import {cp,mkdir,readFile,writeFile,access,realpath} from 'node:fs/promises';
import {existsSync} from 'node:fs';
import {execFileSync,spawnSync} from 'node:child_process';
import {createRequire} from 'node:module';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {studioTemplates} from '../dist/packages/studio-service/templates.js';
import {replaceReleaseMedia,auditReleaseMedia} from './release-sample-media.mjs';

if(process.platform!=='darwin')throw Error('Build the macOS editor package on a Mac.');
const root=fileURLToPath(new URL('..',import.meta.url));
const out=path.resolve(process.env.MACOS_STUDIO_OUT??'dist/macos/RaisingStudio.app');
if(existsSync(out))throw Error('Choose a new MACOS_STUDIO_OUT; existing apps are never overwritten.');
const nodeRoot=path.resolve(process.env.MACOS_NODE_ROOT??`.cache/macos-node/node-v22.14.0-darwin-${process.arch}`);
const node=path.join(nodeRoot,'bin/node');await access(path.join(nodeRoot,'LICENSE'));
const nodeInfo=JSON.parse(execFileSync(node,['-p','JSON.stringify({version:process.version,arch:process.arch})'],{encoding:'utf8'}));
const [nodeMajor,nodeMinor]=nodeInfo.version.slice(1).split('.').map(Number);
if(nodeInfo.arch!==process.arch||nodeMajor<22||(nodeMajor===22&&nodeMinor<12))throw Error('Use an official Node 22.12+ macOS archive matching this build architecture.');
const linked=execFileSync('/usr/bin/otool',['-L',node],{encoding:'utf8'}).split('\n').slice(1).map(s=>s.trim().split(' ')[0]).filter(Boolean);
if(linked.some(s=>!s.startsWith('/usr/lib/')&&!s.startsWith('/System/Library/')))throw Error('The bundled Node must be standalone; Homebrew Node requires external libraries. Use an official nodejs.org archive.');
const qt=process.env.QT_ROOT??['/opt/homebrew/opt/qt','/usr/local/opt/qt'].find(p=>existsSync(path.join(p,'bin/macdeployqt')));
if(!qt)throw Error('Set QT_ROOT to a Qt 6 macOS installation.');
await cp(path.resolve(process.env.QT_STUDIO_OUT??'dist/qt-studio','RaisingStudioQt.app'),out,{recursive:true,force:false,errorOnExist:true});
const query=key=>execFileSync(path.join(qt,'bin/qtpaths'),['--query',key],{encoding:'utf8'}).trim();
const pluginRoot=query('QT_INSTALL_PLUGINS'),libraryRoot=query('QT_INSTALL_LIBS');
const pluginPaths=[];
for(const name of ['platforms/libqcocoa.dylib','imageformats/libqjpeg.dylib','imageformats/libqwebp.dylib','imageformats/libqsvg.dylib']) {
  const source=path.join(pluginRoot,name);if(!existsSync(source))throw Error('Missing Qt plugin: '+source);
  const destination=path.join(out,'Contents/PlugIns',name);await mkdir(path.dirname(destination),{recursive:true});await cp(source,destination,{dereference:true});pluginPaths.push(destination);
}
const deployed=spawnSync(path.join(qt,'bin/macdeployqt'),[out,'-always-overwrite','-no-strip','-no-plugins','-no-codesign','-libpath='+libraryRoot,...pluginPaths.map(p=>'-executable='+p)],{encoding:'utf8'});
if(deployed.error)throw deployed.error;
if(deployed.status||/ERROR:/.test(deployed.stdout+deployed.stderr))throw Error('Qt deployment failed:\n'+deployed.stdout+deployed.stderr);
const resources=path.join(out,'Contents/Resources');
await writeFile(path.join(resources,'qt.conf'),'[Paths]\nPlugins=PlugIns\n');
const copy=async(from,to)=>{
  await mkdir(path.dirname(path.join(resources,to)),{recursive:true});
  await cp(path.resolve(root,from),path.join(resources,to),{recursive:true,force:false,errorOnExist:true,filter:source=>!path.relative(path.resolve(root,from),source).split(path.sep).some(s=>['.git','.studio','.cache','Xcode'].includes(s))});
};
await copy('apps/qt-studio','engine/apps/qt-studio');
await copy('config/native-dependencies.json','engine/config/native-dependencies.json');
await copy('scripts/lib/ios-project.mjs','engine/scripts/lib/ios-project.mjs');
await copy('dist/packages','engine/dist/packages');
await copy('packages/schema','engine/packages/schema');
await copy('packages/plugin-host','engine/packages/plugin-host');
await copy(node,'engine/node');
await copy(path.join(nodeRoot,'LICENSE'),'licenses/node-LICENSE.txt');
await writeFile(path.join(resources,'engine/package.json'),JSON.stringify({name:'raising-studio-services',version:'0.1.0',type:'module',private:true}));
const runtime=JSON.parse(await readFile(path.join(root,'dist/native/runtime.json'),'utf8'));
await copy('dist/native/runtime.json','engine/dist/native/runtime.json');
await copy('dist/native/'+runtime.executable,'engine/dist/native/'+runtime.executable);
const player=path.resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-app');
await copy(player,'engine/dist/native-player-app');
const web=path.resolve(process.env.RAISING_WEB_PLAYER_RUNTIME??'dist/web');
if(existsSync(path.join(web,'index.html')))await copy(web,'engine/dist/web');
for(const id of studioTemplates)await copy('examples/'+id,'engine/examples/'+id);
await copy('examples/rule-extension-lab','engine/examples/rule-extension-lab');
const mediaPolicy=JSON.parse(await readFile(path.join(root,'config/release-sample-media.json'),'utf8'));
await replaceReleaseMedia(resources,mediaPolicy);await auditReleaseMedia(resources,mediaPolicy);
// Include pinned native sources so iOS export works without a repository checkout.
for(const entry of ['CMakeLists.txt','include','src','vendor','player','mobile/ios'])await copy('native/'+entry,'engine/native/'+entry);
for(const file of ['LICENSE','THIRD_PARTY_NOTICES.md'])await copy(file,'engine/'+file);
await copy('licenses','licenses/repository');
await copy('docs/getting-started/first-game.md','FIRST-GAME.md');
const dependencies=new Map();
async function dependency(name,from=path.join(root,'package.json')) {
  const require=createRequire(from);let manifest;
  try {manifest=require.resolve(name+'/package.json');}
  catch {let dir=path.dirname(require.resolve(name));while(true){const p=path.join(dir,'package.json');if(existsSync(p)&&JSON.parse(await readFile(p,'utf8')).name===name){manifest=p;break;}const parent=path.dirname(dir);if(parent===dir)throw Error('Missing dependency: '+name);dir=parent;}}
  const p=JSON.parse(await readFile(manifest,'utf8'));
  if(dependencies.has(name)){if(dependencies.get(name)!==p.version)throw Error('Conflicting dependency: '+name);return;}
  dependencies.set(name,p.version);await copy(path.dirname(manifest),'engine/node_modules/'+name);
  for(const child of Object.keys(p.dependencies??{}))await dependency(child,manifest);
}
for(const name of ['ajv','jsonc-parser','yauzl'])await dependency(name);
await writeFile(path.join(resources,'package-manifest.json'),JSON.stringify({version:1,platform:'darwin',arch:process.arch,node:nodeInfo.version,templates:studioTemplates,dependencies:Object.fromEntries(dependencies),publicReleaseReady:false,notes:'Local development bundle. Distribution still requires Qt corresponding-source/license review, Developer ID signing, notarization and a clean-machine test.'},null,2));
execFileSync('/usr/bin/codesign',['--force','--deep','--sign','-',out],{stdio:'inherit'});
execFileSync('/usr/bin/codesign',['--verify','--deep','--strict',out],{stdio:'inherit'});
console.log(`macOS editor app: ${await realpath(out)}`);

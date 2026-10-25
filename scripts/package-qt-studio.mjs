import {cp,mkdir,readFile,writeFile,readdir,lstat,access} from 'node:fs/promises';
import {resolve,join,dirname,relative,basename} from 'node:path';
import {createHash} from 'node:crypto';
import {createRequire} from 'node:module';
import {execFileSync} from 'node:child_process';
import {replaceReleaseMedia,auditReleaseMedia} from './release-sample-media.mjs';
import {studioTemplates} from '../dist/packages/studio-service/templates.js';

const root=resolve('.'),out=resolve(process.env.QT_PACKAGE_OUT||'release/qt-preview/RaisingStudio');
const qt=resolve(process.env.QT_STUDIO_OUT||'dist/qt-studio');
const windows=resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME||'dist/native-player-app');
const web=resolve(process.env.RAISING_WEB_PLAYER_RUNTIME||'dist/web');
const release=JSON.parse(await readFile('config/preview-release.json','utf8'));
const qtSources=JSON.parse(await readFile('config/qt-corresponding-source.json','utf8'));
const sourceDir=resolve(process.env.QT_SOURCE_ARCHIVES||'.cache/preview-release/qt-sources');
for(const source of qtSources.archives){
 const bytes=await readFile(join(sourceDir,source.file));
 if(createHash('sha256').update(bytes).digest('hex')!==source.sha256)throw Error('Qt source hash mismatch: '+source.file);
}
try{await access(out);throw Error('Choose a new QT_PACKAGE_OUT; existing packages are never overwritten.');}catch(e){if(e.code!=='ENOENT')throw e;}
const policy=JSON.parse(await readFile('config/repository-content.json','utf8'));
const templates=studioTemplates;
for(const id of templates)if(!policy.publicSamples.some(allowed=>'examples/'+id===allowed||('examples/'+id).startsWith(allowed+'/')))throw Error('Template is not approved: '+id);
const copy=async(from,to)=>{await mkdir(dirname(join(out,to)),{recursive:true});await cp(from,join(out,to),{recursive:true,errorOnExist:true,force:false,filter:s=>!relative(from,s).split(/[\\/]/).some(p=>['.studio','.git','node_modules','.cache'].includes(p))});};
await access(`licenses/node/LICENSE-${process.version}.txt`);
await mkdir(dirname(out),{recursive:true});await mkdir(out,{recursive:false});
// Copy only deployable Qt files, never CMake objects, test executables or build caches.
for(const entry of await readdir(qt,{withFileTypes:true})){
 // Qt Widgets uses raster painting; these optional shader compilers are not used.
 if(['D3Dcompiler_47.dll','dxcompiler.dll','dxil.dll','Qt6Test.dll'].includes(entry.name))continue;
 if(entry.name==='platforms'){await copy(join(qt,'platforms/qwindows.dll'),'platforms/qwindows.dll');continue;}
 if(entry.name==='RaisingStudioQt.exe'||entry.name==='qt.conf'||entry.name.endsWith('.dll')||['platforms','styles','imageformats','iconengines','networkinformation','tls','translations'].includes(entry.name))await copy(join(qt,entry.name),entry.name);
}
await writeFile(join(out,'qt.conf'),'[Paths]\nPlugins=.\n');
await copy('native/studio/fonts','fonts');
await copy('apps/qt-studio','engine/apps/qt-studio');
await copy('config/native-dependencies.json','engine/config/native-dependencies.json');
await copy('scripts/lib/ios-project.mjs','engine/scripts/lib/ios-project.mjs');
await copy('dist/packages','engine/dist/packages');
await copy('packages/schema','engine/packages/schema');
await copy('packages/plugin-host','engine/packages/plugin-host');
await copy(process.execPath,'engine/node.exe');
await writeFile(join(out,'engine/package.json'),JSON.stringify({name:'raising-studio-service',version:'0.1.0',type:'module',private:true,license:'MIT'}));
const runtime=JSON.parse(await readFile('dist/native/runtime.json','utf8'));
await copy('dist/native/runtime.json','engine/dist/native/runtime.json');
await copy(join('dist/native',runtime.executable),'engine/dist/native/'+runtime.executable);
const addon=JSON.parse(await readFile('dist/native/addon.json','utf8').catch(()=>'{}'));
if(addon.executable===runtime.executable){await copy('dist/native/addon.json','engine/dist/native/addon.json');await copy(join('dist/native',addon.filename),'engine/dist/native/'+addon.filename);}
await copy(windows,'engine/dist/native-player-app');await copy(web,'engine/dist/web');
for(const id of templates)await copy('examples/'+id,'engine/examples/'+id);
await copy('examples/rule-extension-lab','engine/examples/rule-extension-lab');
const mediaPolicy=JSON.parse(await readFile('config/release-sample-media.json','utf8'));
await replaceReleaseMedia(out,mediaPolicy);
await auditReleaseMedia(out,mediaPolicy);
await copy('scripts/release-sample-media.mjs','licenses/sample-media-generator.mjs');
await copy('docs/development/release-sample-assets.md','licenses/SAMPLE-ASSETS.md');
await copy('LICENSE','LICENSE');await copy('licenses','licenses');await copy('THIRD_PARTY_NOTICES.md','THIRD_PARTY_NOTICES.md');
for(const source of qtSources.archives)await copy(join(sourceDir,source.file),'licenses/qt/sources/'+source.file);
await copy('config/qt-corresponding-source.json','licenses/qt/corresponding-source.json');
await copy('docs/development/preview-license-review.md','licenses/README.md');
await copy('docs/getting-started/preview','guides');
await copy('docs/getting-started/first-game.md','FIRST-GAME.md');
for(const doc of ['getting-started/first-game.md','development/platform-support.md','development/platform-capabilities.json','development/public-assets.md','development/qt-preview-release.md'])await copy('docs/'+doc,'engine/docs/'+doc);
await copy('LICENSE','engine/LICENSE');await copy('THIRD_PARTY_NOTICES.md','engine/THIRD_PARTY_NOTICES.md');
await copy(join(windows,'licenses'),'licenses/player');await copy(join(windows,'fonts/OFL.txt'),'licenses/NotoSansKR-OFL.txt');
await copy(join(windows,'fonts/OFL-NotoSansJP.txt'),'licenses/NotoSansJP-OFL.txt');
await copy('native/vendor/LICENSE.MIT','licenses/nlohmann-json.txt');
await copy('native/player/assets/OFL.txt','engine/dist/web/licenses/NotoSansKR-OFL.txt');
await copy('native/vendor/LICENSE.MIT','engine/dist/web/licenses/nlohmann-json.txt');
const qtDocs=resolve(process.env.QT_DOCS||'C:/Qt/Docs/Qt-6.10.1');
await copy(resolve(process.env.QT_COMPILER||'C:/Qt/Tools/mingw1310_64/bin','../licenses'),'licenses/mingw');
for(const module of ['qtcore','qtgui','qtwidgets','qtsvg','qtimageformats','qtnetwork','qtdoc']){
 for(const file of await readdir(join(qtDocs,module)).catch(()=>[]))if(/attribution|licenses-used|lgpl\.html|gpl\.html/.test(file))await copy(join(qtDocs,module,file),'licenses/qt/notices/'+module+'/'+file);
}
const dependencies=new Map();
async function dependency(name,from=join(root,'package.json')){
 const require=createRequire(from);let file;
 try{file=require.resolve(name+'/package.json');}catch{let dir=dirname(require.resolve(name));while(true){const candidate=join(dir,'package.json');try{if(JSON.parse(await readFile(candidate,'utf8')).name===name){file=candidate;break;}}catch{}const parent=dirname(dir);if(parent===dir)throw Error('Missing dependency manifest: '+name);dir=parent;}}
 const p=JSON.parse(await readFile(file,'utf8'));
 if(dependencies.has(name)){if(dependencies.get(name).version!==p.version)throw Error('Conflicting packaged versions: '+name);return;}
 dependencies.set(name,{name,version:p.version,license:p.license});
 await copy(dirname(file),'engine/node_modules/'+name);
 for(const child of Object.keys(p.dependencies||{}))await dependency(child,file);
}
for(const name of ['ajv','jsonc-parser','yauzl'])await dependency(name);
// Retain licenses for JS code embedded in the Web runtime as well as Node services.
for(const name of ['react','react-dom','scheduler']){
 const p=join(root,'node_modules',name);await copy(join(p,'LICENSE'),'engine/dist/web/licenses/'+name+'.txt');
}
const blocked=new Set(policy.excludedAssetSha256),files=[];
async function inventory(dir){for(const e of await readdir(dir,{withFileTypes:true})){
 const file=join(dir,e.name);if((await lstat(file)).isSymbolicLink())throw Error('Symlink in package: '+file);
 if(e.isDirectory())await inventory(file);else{const bytes=await readFile(file),sha256=createHash('sha256').update(bytes).digest('hex');if(blocked.has(sha256))throw Error('Private reference asset in package: '+file);files.push({path:relative(out,file).replaceAll('\\','/'),bytes:bytes.length,sha256});}
}}
await inventory(out);files.sort((a,b)=>a.path.localeCompare(b.path));
const manifest={version:1,product:'Raising Studio Qt',releaseVersion:release.version,channel:release.channel,publicReleaseReady:false,editorLanguages:release.editorLanguages,editorTranslationStatus:release.editorTranslationStatus,sourceCommit:execFileSync('git',['rev-parse','HEAD'],{encoding:'utf8'}).trim(),sourceTreeDirty:!!execFileSync('git',['status','--porcelain'],{encoding:'utf8'}).trim(),node:process.version,qt:'6.10.1',qtCorrespondingSource:qtSources.archives,templates,dependencies:[...dependencies.values()],files};
await writeFile(join(out,'package-manifest.json'),JSON.stringify(manifest,null,2));
console.log(JSON.stringify({out,files:files.length,bytes:files.reduce((n,f)=>n+f.bytes,0),publicReleaseReady:false}));

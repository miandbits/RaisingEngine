import {readFile,writeFile,lstat,mkdir,rm,realpath} from 'node:fs/promises';
import {createReadStream} from 'node:fs';
import {createServer} from 'node:http';
import {randomUUID} from 'node:crypto';
import path from 'node:path';
import {requiredPlayerFeatures,missingPlayerFeatures} from '../../dist/packages/export-service/compatibility.js';
import {contentFromDocuments} from '../../dist/packages/localization/catalog.js';
import {playerExecutable} from '../../dist/packages/export-service/targets.js';
import {projectFiles,materializeProject,privateDirectory,atomicJSON,digest,contained,regularFile} from './project-files.mjs';
import {treeFiles,writeZip} from './release-zip.mjs';
import {listInstalled,findModule,RUNTIME_PLATFORMS} from '../../dist/packages/plugin-host/index.js';
import {pluginElements} from '../../dist/packages/plugin-host/bundle.js';

const targets=['macos','windows','web','ios'];
const names={macos:'macOS',windows:'Windows',web:'Web',ios:'iOS'};
const featureName=id=>id.replace(/-v\d+$/,'').replaceAll('-',' ');
export class GameReleases {
  previews=new Map();exports=new Map();preferences=new Map();
  async preference(editor,target) {
    if(!editor.root)return null;
    if(target!==undefined) {
      if(!targets.includes(target))throw Error('Choose a supported release target.');
      const dir=await privateDirectory(editor.root,'release');await atomicJSON(path.join(dir,'settings.json'),{version:1,target});this.preferences.set(editor.root,target);
    }
    if(!this.preferences.has(editor.root)) {
      let value;
      try {const directory=await privateDirectory(editor.root,'release'),file=path.join(directory,'settings.json');const stat=await lstat(file);if(!stat.isFile()||stat.size>4096)throw Error('Invalid release settings.');value=JSON.parse(await readFile(file,'utf8'));}
      catch(e){if(e.code!=='ENOENT')throw e;}
      this.preferences.set(editor.root,targets.includes(value?.target)?value.target:process.platform==='darwin'?'macos':'windows');
    }
    return this.preferences.get(editor.root);
  }
  async compatibility(editor,runtimes) {
    if(!editor.root)return null;
    const target=await this.preference(editor),rows=[];
    const content={activities:[],events:[],endings:[],...contentFromDocuments(editor.documents)};
    let required;try{required=requiredPlayerFeatures(content);}catch{return {target,rows:[],summary:'Correct the project structure in Issues before checking platforms.'};}
    const assets=editor.documents.find(d=>d.kind==='assets')?.data?.assets??[];
    const used=[...new Set(pluginElements(content.project?.visual).map(e=>e.element.extension?.plugin))];
    const pins=editor.documents.find(d=>d.kind==='pluginLock')?.data?.plugins??[];
    const installed=used.length?await listInstalled(editor.pluginStore):[];
    for(const target of targets) {
      const issues=[];let available=false;
      try {
        if(!runtimes[target])throw Error('Runtime is not included in this editor build.');
        const root=await realpath(runtimes[target]),caps=JSON.parse(await readFile(path.join(root,'capabilities.json'),'utf8'));
        if(caps.formatVersion!=='1'||!Array.isArray(caps.visualFeatures))throw Error('Runtime capabilities are invalid.');
        const entry=target==='ios'?'capabilities.json':target==='web'?'index.html':playerExecutable(target,'');
        await regularFile(root,entry);available=true;
        for(const id of missingPlayerFeatures(required,caps.visualFeatures))issues.push({code:'UNSUPPORTED_FEATURE',feature:id,message:'Unsupported: '+featureName(id)});
        for(const id of used) {
          const pin=pins.find(p=>p.id===id),plugin=installed.find(p=>p.id===id&&p.version===pin?.version);
          if(!pin?.enabled||!plugin||['macos','ios'].includes(target)||!findModule(plugin.manifest,'runtime',RUNTIME_PLATFORMS[target])||(plugin.manifest.exportTargets&&!plugin.manifest.exportTargets.includes(target)))issues.push({code:'PLUGIN_RUNTIME_MISSING',message:`${plugin?.manifest.name??id} has no enabled ${names[target]} runtime.`});
        }
        for(const asset of assets) {
          const allowed=asset.kind==='image'?caps.imageExtensions:asset.kind==='font'?caps.fontExtensions:caps.audioExtensions;
          if(!allowed?.includes(path.extname(asset.path??'').toLowerCase()))issues.push({code:'UNSUPPORTED_ASSET',message:`Unsupported ${asset.kind}: ${asset.name??asset.label??asset.id} (${path.extname(asset.path??'')})`});
        }
      }catch(error){issues.push({code:'RUNTIME_UNAVAILABLE',message:error.code==='ENOENT'?'Runtime is not included in this editor build.':error.message});}
      rows.push({target,label:names[target],available,compatible:available&&!issues.length,issues});
    }
    const selected=rows.find(r=>r.target===target);
    return {target,rows,summary:selected.compatible?`${names[target]}: current core features are supported. ${used.length?'Plugin-specific requirements are checked during export. ':''}Validate and playtest before sharing.`:`${names[target]}: ${selected.issues.map(i=>i.message).join(' · ')}`};
  }
  async prepare(directory,{editor,target,version,content,contentFingerprint,engineRoot}) {
    if(typeof version!=='string'||!/^\d{1,4}\.\d{1,4}\.\d{1,4}(?:-[a-zA-Z0-9.-]{1,40})?$/.test(version))throw Error('Use a release version such as 0.1.0.');
    const files=await treeFiles(directory),source=path.join(directory,'.release-source');
    const sourceFiles=await projectFiles(editor);
    const pins=editor.documents.find(d=>d.kind==='pluginLock')?.data?.plugins??[],pluginDependencies=[];
    if(pins.length) {
      const installed=await listInstalled(editor.pluginStore);
      const registry=[];
      for(const pin of pins) {
        const plugin=installed.find(p=>p.id===pin.id&&p.version===pin.version);
        if(!plugin)throw Error(`Install the pinned plugin ${pin.id} ${pin.version} before creating a complete project backup, or export without archives.`);
        const root=await realpath(plugin.directory),prefix=`release-dependencies/plugin-store/installed/${plugin.id}/${plugin.version}/`;
        for(const name of ['plugin.json',...Object.keys(plugin.manifest.files)]) {
          const bytes=await readFile(await regularFile(root,name));
          if(name!=='plugin.json'&&digest(bytes)!==plugin.manifest.files[name])throw Error('A pinned plugin file changed: '+name);
          sourceFiles.set(prefix+name,bytes);
        }
        const {id,version,sha256,installedAt,source}=plugin;registry.push({id,version,sha256,installedAt,source});pluginDependencies.push({id,version,packageSha256:sha256,manifestSha256:digest(sourceFiles.get(prefix+'plugin.json'))});
      }
      sourceFiles.set('release-dependencies/plugin-store/registry.json',Buffer.from(JSON.stringify({version:1,installed:registry})));
      sourceFiles.set('release-dependencies/README.txt',Buffer.from('In the editor Plugin Manager, choose this backup\'s release-dependencies/plugin-store folder as the plugin store. These are the exact installed files; do not modify pinned versions.\n'));
    }
    for(const asset of content.assets)if(digest(sourceFiles.get(asset.path)??Buffer.alloc(0))!==content.assetHashes[asset.id])throw Error('An asset changed while preparing the release backup: '+asset.id);
    await materializeProject(sourceFiles,source);
    const dependencyFile=path.join(engineRoot,'config/native-dependencies.json');
    const dependencies=JSON.parse(await readFile(dependencyFile,'utf8').catch(e=>{if(e.code==='ENOENT')return '{}';throw e;}));
    const hashes=[];for(const file of files)hashes.push({file,sha256:digest(await readFile(path.join(directory,file)))});
    const manifest={formatVersion:1,version,target,contentFingerprint,createdAt:new Date().toISOString(),engineVersion:'0.1.0',node:process.version,host:process.platform,architecture:process.arch,dependencies,pluginDependencies,files:hashes,
      sourceFiles:[...sourceFiles].map(([file,bytes])=>({file,sha256:digest(bytes)})),
      saveCompatibility:'Keep this exact release and project backup. Changed game rules/content can invalidate player saves; use explicit save migration and replay verification before promising compatibility.',
      reproduction:'The game ZIP preserves the exact exported player and content bytes. The project ZIP preserves editable draft documents, referenced assets and named test routes. Rebuilding engine binaries requires the matching toolchain; byte-identical recompilation is not claimed.'};
    await writeFile(path.join(source,'RELEASE.json'),JSON.stringify(manifest,null,2));
    await writeZip(source,await treeFiles(source),path.join(directory,'Project-backup.zip'));await rm(source,{recursive:true});
    await writeZip(directory,files,path.join(directory,'Game.zip'));
    manifest.archives={game:{file:'Game.zip',sha256:digest(await readFile(path.join(directory,'Game.zip')))},project:{file:'Project-backup.zip',sha256:digest(await readFile(path.join(directory,'Project-backup.zip')))}};
    await writeFile(path.join(directory,'RELEASE.json'),JSON.stringify(manifest,null,2));
    return manifest;
  }
  register(result){const token=randomUUID();this.exports.set(token,{...result});return token;}
  async launch(token) {
    const exported=this.exports.get(token);if(!exported)throw Error('Export this game in the current editor before launching it.');
    const root=await realpath(exported.directory);
    if(exported.target==='ios'){const project=path.join(root,'build-iphonesimulator/RaisingGame.xcodeproj');return {kind:'folder',directory:await lstat(project).then(()=>project).catch(()=>root)};}
    if(exported.target!=='web') {
      if((exported.target==='macos')!==(process.platform==='darwin')||(exported.target==='windows'&&process.platform!=='win32'))throw Error('This game must be launched on its target operating system.');
      return {kind:'native',executable:playerExecutable(exported.target,root),directory:root};
    }
    if(this.previews.has(token))return {kind:'web',url:this.previews.get(token).url};
    const files=new Set(await treeFiles(root)),mime={'.html':'text/html; charset=utf-8','.js':'text/javascript; charset=utf-8','.mjs':'text/javascript; charset=utf-8','.json':'application/json','.wasm':'application/wasm','.css':'text/css','.png':'image/png','.jpg':'image/jpeg','.webp':'image/webp','.svg':'image/svg+xml','.ttf':'font/ttf','.ogg':'audio/ogg','.mp3':'audio/mpeg'};
    const server=createServer(async(req,res)=>{
      try {
        const host=req.headers.host;if(host!==`127.0.0.1:${server.address().port}`)throw Error('Invalid host');
        if(!['GET','HEAD'].includes(req.method)){res.writeHead(405);res.end();return;}
        const name=decodeURIComponent(new URL(req.url,'http://'+host).pathname).slice(1)||'index.html';
        if(!files.has(name)||name.startsWith('.')||['Game.zip','Project-backup.zip','RELEASE.json'].includes(name))throw Error('Not found');
        const file=await regularFile(root,name);contained(root,file);
        res.writeHead(200,{'Content-Type':mime[path.extname(name)]??'application/octet-stream','Cache-Control':'no-store','X-Content-Type-Options':'nosniff','Cross-Origin-Opener-Policy':'same-origin','Cross-Origin-Embedder-Policy':'require-corp'});
        if(req.method==='HEAD')res.end();else createReadStream(file).on('error',()=>res.destroy()).pipe(res);
      }catch {if(!res.headersSent)res.writeHead(404);res.end();}
    });
    await new Promise((resolve,reject)=>{server.once('error',reject);server.listen(0,'127.0.0.1',resolve);});server.unref();
    const url=`http://127.0.0.1:${server.address().port}/`;this.previews.set(token,{server,url});return {kind:'web',url};
  }
  async close(){for(const {server} of this.previews.values()){server.closeAllConnections();await new Promise(resolve=>server.close(resolve));}this.previews.clear();}
}

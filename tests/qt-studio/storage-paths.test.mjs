import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,mkdir,writeFile,readFile,cp,symlink,readdir,chmod,rm,realpath} from 'node:fs/promises';
import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';
import {packagedStudioPluginStore} from '../../apps/qt-studio/storage-paths.mjs';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {writeZip} from '../../dist/packages/plugin-host/index.js';

const root=fileURLToPath(new URL('../../',import.meta.url));
const temporary=async t=>{const p=await realpath(await mkdtemp(path.join(os.tmpdir(),'raising-storage-')));t.after(()=>rm(p,{recursive:true,force:true}));return p;};
const manifest=(p,name)=>writeFile(path.join(p,'package.json'),JSON.stringify({name,type:'module',private:true}));

test('installed Qt defaults are per-user on macOS and Windows; other hosts stay unchanged',async t=>{
  const p=await temporary(t);
  await manifest(p,'raising-studio-services');
  assert.equal(packagedStudioPluginStore(p,{platform:'darwin',home:'/Users/Designer',environment:{}}),'/Users/Designer/Library/Application Support/RaisingEngine/Raising Studio/plugin-store');
  await manifest(p,'raising-studio-service');
  assert.equal(packagedStudioPluginStore(p,{platform:'win32',home:'C:\\Users\\Designer',environment:{LOCALAPPDATA:'D:\\Local Data'}}),'D:\\Local Data\\RaisingEngine\\Raising Studio\\plugin-store');
  for(const environment of [{},{LOCALAPPDATA:'relative-path'}])assert.equal(packagedStudioPluginStore(p,{platform:'win32',home:'C:\\Users\\Designer',environment}),'C:\\Users\\Designer\\AppData\\Local\\RaisingEngine\\Raising Studio\\plugin-store');
  assert.equal(packagedStudioPluginStore(p,{platform:'linux',home:'/home/designer',environment:{}}),undefined);
  assert.deepEqual(await readdir(p),['package.json'],'resolving a default does not create storage');
});

test('repository, missing/malformed metadata and mismatched platform markers do not change library defaults',async t=>{
  const p=await temporary(t),options={platform:'darwin',home:'/Users/Designer',environment:{}};
  assert.equal(packagedStudioPluginStore(p,options),undefined);
  for(const name of ['raising-engine','raising-studio-service','other']){await manifest(p,name);assert.equal(packagedStudioPluginStore(p,options),undefined);}
  await writeFile(path.join(p,'package.json'),'{');assert.equal(packagedStudioPluginStore(p,options),undefined);
  assert.equal(packagedStudioPluginStore(root,options),undefined);
});

test('Qt service preserves explicit and environment store precedence',async t=>{
  const p=await temporary(t),before=process.env.RAISING_PLUGIN_STORE;
  t.after(()=>{if(before===undefined)delete process.env.RAISING_PLUGIN_STORE;else process.env.RAISING_PLUGIN_STORE=before;});
  process.env.RAISING_PLUGIN_STORE=path.join(p,'environment');
  assert.equal(new QtStudioService().pluginStore,path.join(p,'environment'));
  assert.equal(new QtStudioService({pluginStore:path.join(p,'explicit')}).pluginStore,path.join(p,'explicit'));
  delete process.env.RAISING_PLUGIN_STORE;
  assert.equal(new QtStudioService().pluginStore,path.join(root,'plugin-store'));
});

test('packaged service installs outside read-only resources and retains remembered project overrides',{skip:!['darwin','win32'].includes(process.platform)},async t=>{
  const p=await realpath(await mkdtemp(path.join(os.tmpdir(),'raising-storage-'))),engine=path.join(p,'installed-engine'),home=path.join(p,'user');
  const resources=process.env.RAISING_STORAGE_TEST_RESOURCES||root;
  t.after(async()=>{
    const writable=async d=>{await chmod(d,0o755);for(const e of await readdir(d,{withFileTypes:true})){if(e.isSymbolicLink())continue;const f=path.join(d,e.name);if(e.isDirectory())await writable(f);else await chmod(f,0o644);}};
    await writable(p);await rm(p,{recursive:true,force:true});
  });
  await mkdir(engine);await mkdir(home);
  await cp(path.join(root,'apps/qt-studio'),path.join(engine,'apps/qt-studio'),{recursive:true});
  for(const name of ['dist','scripts','examples','node_modules'])await symlink(path.join(resources,name),path.join(engine,name),process.platform==='win32'?'junction':'dir');
  await manifest(engine,process.platform==='darwin'?'raising-studio-services':'raising-studio-service');
  const ownFiles=async dir=>{const result=[];for(const e of await readdir(dir,{withFileTypes:true})){const f=path.join(dir,e.name);if(e.isSymbolicLink())continue;if(e.isDirectory())result.push(...await ownFiles(f));else result.push(f);}return result;};
  const files=await ownFiles(engine),hashes=await Promise.all(files.map(async f=>[path.relative(engine,f),createHash('sha256').update(await readFile(f)).digest('hex')]));
  if(process.platform!=='win32'){
    const dirs=[engine,path.join(engine,'apps'),path.join(engine,'apps/qt-studio')];
    for(const f of files)await chmod(f,0o444);for(const d of dirs)await chmod(d,0o555);
  }
  const bytes=Buffer.from('{"type":"object"}'),archive=path.join(p,'probe.raiseplugin');
  const plugin={manifestVersion:1,id:'storage-probe',name:'Storage probe',version:'1.0.0',engineApi:{min:1,max:1},documentTypes:[{type:'thing',label:'Thing',schema:'schema/thing.json'}],elementTypes:[],modules:[],files:{'schema/thing.json':createHash('sha256').update(bytes).digest('hex')}};
  await writeFile(archive,writeZip([{name:'plugin.json',data:Buffer.from(JSON.stringify(plugin))},{name:'schema/thing.json',data:bytes}]));
  const environment=Object.fromEntries(Object.entries(process.env).filter(([k])=>!k.startsWith('RAISING_')&&!['NODE_OPTIONS','NODE_PATH'].includes(k)));
  Object.assign(environment,{HOME:home,USERPROFILE:home,LOCALAPPDATA:path.join(home,'Local')});
  const expected=packagedStudioPluginStore(engine,{home,environment}),project=path.join(p,'game'),alternate=path.join(p,'remembered');
  await mkdir(alternate);await writeFile(path.join(alternate,'registry.json'),'{"version":1,"installed":[]}');
  const source=`import assert from 'node:assert/strict';import {QtStudioService} from ${JSON.stringify(pathToFileURL(path.join(engine,'apps/qt-studio/service.mjs')).href)};
const service=new QtStudioService();assert.equal(service.pluginStore,${JSON.stringify(expected)});
await service.handle({method:'create',params:{root:${JSON.stringify(project)},template:'my-first-raising-game'}});
const installed=await service.handle({method:'pluginInstall',params:{file:${JSON.stringify(archive)}}});assert.equal(installed.pluginInstalled.id,'storage-probe');
assert.ok(installed.pluginInstalled.directory.startsWith(${JSON.stringify(expected)}));
await service.handle({method:'open',params:{root:${JSON.stringify(project)},pluginStore:${JSON.stringify(alternate)}}});assert.equal(service.pluginStore,${JSON.stringify(alternate)});
console.log(JSON.stringify({store:${JSON.stringify(expected)},installed:installed.pluginInstalled.directory,remembered:service.pluginStore}));`;
  const child=spawnSync(process.env.RAISING_STORAGE_TEST_NODE||process.execPath,['--input-type=module','-e',source],{cwd:p,env:environment,encoding:'utf8',timeout:30000});
  assert.equal(child.status,0,child.stderr||child.stdout);
  assert.deepEqual(await Promise.all((await ownFiles(engine)).map(async f=>[path.relative(engine,f),createHash('sha256').update(await readFile(f)).digest('hex')])),hashes,'installed resources remain byte-identical and no store is created inside them');
  assert.equal(JSON.parse(await readFile(path.join(expected,'registry.json'),'utf8')).installed[0].id,'storage-probe');
});

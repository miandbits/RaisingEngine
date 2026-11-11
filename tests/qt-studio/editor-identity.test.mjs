import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp,mkdir,writeFile,readFile,rename,rm,chmod} from 'node:fs/promises';
import {execFileSync,spawnSync} from 'node:child_process';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {readEditorIdentity,createPackagedEditorIdentity,sourceEditorLaunch} from '../../apps/qt-studio/editor-identity.mjs';
import {previewArtifactMetadata} from '../../scripts/lib/preview-artifact-metadata.mjs';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';

const repository=fileURLToPath(new URL('../../',import.meta.url));
async function fixture(t){
 const root=await mkdtemp(path.join(os.tmpdir(),'raising-identity-'));t.after(()=>rm(root,{recursive:true,force:true}));
 await mkdir(path.join(root,'config'));await mkdir(path.join(root,'dist/native'),{recursive:true});
 await writeFile(path.join(root,'package.json'),JSON.stringify({name:'raising-engine',version:'2.3.4',type:'module'}));
 await writeFile(path.join(root,'config/preview-release.json'),JSON.stringify({version:'0.1.0-preview.3',channel:'unsigned-preview',platform:'windows',architecture:'x64',publicReleaseReady:true}));
 await writeFile(path.join(root,'dist/native/runtime.json'),JSON.stringify({runtime:'raising-native',coreVersion:'0.2.0',sourceRevision:'fixture-core-inputs'}));
 const git=(...args)=>execFileSync('git',['-C',root,...args],{encoding:'utf8',stdio:['ignore','pipe','pipe']}).trim();
 git('init');git('add','.');git('-c','user.name=Identity fixture','-c','user.email=fixture@example.invalid','commit','-m','Synthetic identity fixture');
 return {root,git};
}

test('source identity separates package version, published Windows preview and exact dirty Git provenance',async t=>{
 const {root,git}=await fixture(t),identity=await readEditorIdentity(root);
 assert.equal(identity.engineVersion,'2.3.4');assert.deepEqual(identity.publicPreview,{version:'0.1.0-preview.3',channel:'unsigned-preview',platform:'windows',architecture:'x64'});
 assert.equal(identity.distribution.kind,'source');assert.equal(identity.distribution.releaseVersion,null);
 assert.deepEqual(identity.source,{commit:git('rev-parse','HEAD'),tree:git('rev-parse','HEAD^{tree}'),dirty:false,origin:'git',context:'service-and-packaging-source',binaryVerified:false});
 assert.deepEqual(identity.nativeCore,{coreVersion:'0.2.0',sourceRevision:'fixture-core-inputs'});assert.equal(identity.launch.command,'npm run studio');
 assert.equal(JSON.stringify(identity).includes(root),false,'Portable provenance must not leak local paths');
 await writeFile(path.join(root,'uncommitted.txt'),'Uncommitted authoring change');assert.equal((await readEditorIdentity(root)).source.dirty,true);
});

test('packaged source receipt survives relocation and never adopts the enclosing checkout HEAD',async t=>{
 const {root,git}=await fixture(t),identity=await createPackagedEditorIdentity(root,{platform:'darwin',arch:'arm64'}),installed=path.join(root,'installed');
 await mkdir(installed);await writeFile(path.join(installed,'package.json'),JSON.stringify({name:'raising-studio-services',version:identity.engineVersion}));
 await writeFile(path.join(installed,'editor-identity.json'),JSON.stringify(identity));
 git('add','.');git('-c','user.name=Identity fixture','-c','user.email=fixture@example.invalid','commit','-m','Enclosing repository moved on');
 const moved=path.join(root,'moved editor');await rename(installed,moved);
 const actual=await readEditorIdentity(moved);assert.deepEqual(actual.source,identity.source);assert.notEqual(actual.source.commit,git('rev-parse','HEAD'));
 assert.equal(actual.distribution.channel,'development');assert.equal(actual.distribution.releaseVersion,null);assert.equal(actual.launch.entryPoint,'Contents/MacOS/RaisingStudioQt');
 assert.equal(actual.source.binaryVerified,false);assert.deepEqual(actual.buildHost,identity.host);
});

test('Windows unsigned preview policy is explicit and cannot mark an Apple package as that preview',async t=>{
 const {root}=await fixture(t),identity=await createPackagedEditorIdentity(root,{platform:'win32',arch:'x64',releaseVersion:'0.1.0-preview.3',channel:'unsigned-preview'});
 assert.deepEqual(identity.distribution,{kind:'package',platform:'win32',arch:'x64',releaseVersion:'0.1.0-preview.3',channel:'unsigned-preview'});
 assert.equal(identity.engineVersion,'2.3.4');assert.equal(identity.launch.entryPoint,'RaisingStudioQt.exe');assert.equal(identity.publicReleaseReady,undefined);
 await assert.rejects(createPackagedEditorIdentity(root,{platform:'darwin',channel:'unsigned-preview'}),/scoped to Windows/);
});

test('historical Windows publication approval cannot certify newly packaged artifacts',()=>{
 const policy={version:'0.1.0-preview.3',channel:'unsigned-preview',platform:'windows',architecture:'x64',publicReleaseReady:true,remainingReleaseChecks:[],signingPolicy:{ownerDecision:'unsigned-preview',status:'deferred'}};
 const manifest={releaseVersion:'0.1.0-preview.4',sourceCommit:'a'.repeat(40),sourceTreeDirty:false,editorIdentity:{engineVersion:'0.1.0',source:{tree:'b'.repeat(40)}}},files=[{file:'installer.exe',bytes:12,sha256:'b'.repeat(64)}],before=structuredClone(policy);
 const receipt=previewArtifactMetadata(policy,manifest,files);
 assert.equal(receipt.version,'0.1.0-preview.4');assert.equal(receipt.sourceTree,'b'.repeat(40));assert.equal(receipt.historicalPublicationPolicy.version,'0.1.0-preview.3');
 assert.equal(receipt.publicReleaseReady,false);assert.equal(receipt.publicationStatus,'not-evaluated');assert.ok(receipt.remainingReleaseChecks.length);
 assert.equal(receipt.historicalPublicationPolicy.publicReleaseReady,true);assert.equal(receipt.historicalPublicationPolicy.platform,'windows');assert.deepEqual(receipt.historicalPublicationPolicy.signingPolicy,policy.signingPolicy);
 assert.deepEqual(receipt.files,files);assert.equal(receipt.sourceCommit,manifest.sourceCommit);assert.deepEqual(policy,before);assert.equal(receipt.signed,false);
 assert.throws(()=>previewArtifactMetadata({...policy,platform:'macos'},manifest,files),/explicitly identify Windows/);
});

test('legacy, corrupt and source-archive metadata keep unknown provenance instead of borrowing parent Git',async t=>{
 const {root}=await fixture(t),legacy=path.join(root,'legacy'),archive=path.join(root,'source-archive');await mkdir(legacy);await mkdir(archive);
 await writeFile(path.join(legacy,'package.json'),JSON.stringify({name:'raising-studio-service',version:'0.1.0'}));
 await writeFile(path.join(archive,'package.json'),JSON.stringify({name:'raising-engine',version:'2.3.4'}));
 for(const dir of [legacy,archive]){const identity=await readEditorIdentity(dir);assert.equal(identity.source.commit,null);assert.equal(identity.source.dirty,null);assert.equal(identity.source.origin,'unavailable');assert.ok(identity.warnings.length);}
 await writeFile(path.join(legacy,'editor-identity.json'),'{broken');assert.equal((await readEditorIdentity(legacy)).source.commit,null);
 assert.equal((await readEditorIdentity(legacy)).distribution.channel,'unknown');
 const mismatched=await createPackagedEditorIdentity(root,{platform:'win32'});await writeFile(path.join(legacy,'editor-identity.json'),JSON.stringify(mismatched));assert.equal((await readEditorIdentity(legacy)).source.commit,null,'A stale identity version cannot override the installed package metadata');
 const bad=await createPackagedEditorIdentity(root,{platform:'win32'});bad.source.binaryVerified=true;await writeFile(path.join(legacy,'editor-identity.json'),JSON.stringify(bad));assert.equal((await readEditorIdentity(legacy)).source.commit,null,'Unknown binary provenance cannot be promoted by a malformed receipt');
});

test('identity CLI prints the selected engine metadata without requiring a project or mutating the receipt',async t=>{
 const {root}=await fixture(t),identity=await createPackagedEditorIdentity(root,{platform:process.platform}),engine=path.join(root,'packaged-engine');await mkdir(engine);
 await writeFile(path.join(engine,'package.json'),JSON.stringify({name:'raising-studio-services',version:identity.engineVersion}));
 const file=path.join(engine,'editor-identity.json'),bytes=JSON.stringify(identity);await writeFile(file,bytes);
 const result=spawnSync(process.execPath,[path.join(repository,'scripts/editor-identity.mjs'),'--root',engine],{encoding:'utf8'});assert.equal(result.status,0,result.stderr);
 assert.deepEqual(JSON.parse(result.stdout).source,identity.source);assert.equal(await readFile(file,'utf8'),bytes);
});

test('editor identity service is available from the launcher without opening or changing a project',async()=>{
 const service=new QtStudioService(),reply=await service.handle({method:'editorIdentity'});
 assert.equal(reply.error,null);assert.equal(reply.editorIdentity.product,'Raising Studio');assert.equal(reply.editorIdentity.engineVersion,JSON.parse(await readFile(path.join(repository,'package.json'))).version);
 assert.equal(reply.editorIdentity.launch.command,'npm run studio');assert.equal(service.editor.root,null);assert.equal(reply.view,undefined);
});

test('current source launch resolves Qt outputs and forwards literal arguments on all desktop platforms',()=>{
 const arguments_=['--project','a file with spaces','--ui-language','en','literal $(unchanged)'];
 for(const [platform,root,output,expected]of [['win32','C:\\Source root','D:\\Build output','D:\\Build output\\RaisingStudioQt.exe'],['darwin','/Source root','/Build output','/Build output/RaisingStudioQt.app/Contents/MacOS/RaisingStudioQt'],['linux','/Source root','custom-output','/Source root/custom-output/RaisingStudioQt']]){
  const result=sourceEditorLaunch(root,{platform,output,args:arguments_});assert.equal(result.executable,expected);assert.deepEqual(result.args,['--engine-root',root,...arguments_]);assert.equal(result.cwd,root);
 }
});

test('platform inventory reads explicit targets and does not label the host native runtime as another platform',async t=>{
 const {root}=await fixture(t),native=path.join(root,'native'),web=path.join(root,'web');await mkdir(native);await mkdir(web);
 const caps={runtime:'host-fixture',visualFeatures:['fixture-feature']};await writeFile(path.join(native,'capabilities.json'),JSON.stringify(caps));await writeFile(path.join(web,'capabilities.json'),JSON.stringify({...caps,runtime:'web-fixture'}));
 const env={...process.env,RAISING_NATIVE_PLAYER_RUNTIME:native,RAISING_WEB_PLAYER_RUNTIME:web,PLATFORM_REPORT:path.join(root,'report.json')};delete env.RAISING_WINDOWS_PLAYER_RUNTIME;delete env.RAISING_MACOS_PLAYER_RUNTIME;
 const result=spawnSync(process.execPath,[path.join(repository,'scripts/report-platform-support.mjs')],{cwd:repository,env,encoding:'utf8'});assert.equal(result.status,0,result.stderr);
 const report=JSON.parse(await readFile(env.PLATFORM_REPORT));assert.match(report.meaning,/performs no acceptance tests/);assert.equal(report.editorIdentity.product,'Raising Studio');assert.equal(report.runtimes.web.runtime,'web-fixture');
 if(process.platform==='darwin'){assert.equal(report.runtimes.macos.root,native);assert.notEqual(report.runtimes.windows.root,native);}
 else if(process.platform==='win32'){assert.equal(report.runtimes.windows.root,native);assert.notEqual(report.runtimes.macos.root,native);}
 else{assert.notEqual(report.runtimes.windows.root,native);assert.notEqual(report.runtimes.macos.root,native);}
});

test('npm studio entry launches the chosen current Qt output with the requested project and Node', {skip:process.platform==='win32'?'Windows executable launch is covered by packaged creator CI; this fixture uses a POSIX executable script.':false},async t=>{
 const {root}=await fixture(t),output=path.join(root,'Qt output'),spec=sourceEditorLaunch(repository,{output}),file=spec.executable;
 await mkdir(path.dirname(file),{recursive:true});await writeFile(file,'#!'+process.execPath+'\nconsole.log(JSON.stringify({args:process.argv.slice(2),cwd:process.cwd(),node:process.env.RAISING_NODE}));\n');await chmod(file,0o755);
 const args=['--project',path.join(root,'project with spaces'),'--ui-language','en'];
 const result=spawnSync(process.execPath,[path.join(repository,'scripts/start-qt-studio.mjs'),...args],{encoding:'utf8',env:{...process.env,QT_STUDIO_OUT:output,RAISING_NODE:process.execPath}});assert.equal(result.status,0,result.stderr);
 const actual=JSON.parse(result.stdout);assert.deepEqual(actual.args,['--engine-root',repository.replace(/[\\/]$/,''),...args]);assert.equal(actual.cwd,repository.replace(/[\\/]$/,''));assert.equal(actual.node,process.execPath);
});

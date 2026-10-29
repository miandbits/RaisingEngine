import test from 'node:test';
import assert from 'node:assert/strict';
import {access,cp,mkdir,mkdtemp,readFile,readdir,writeFile} from 'node:fs/promises';
import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';

const hash=bytes=>createHash('sha256').update(bytes).digest('hex');
const absent=async file=>access(file).then(()=>false,error=>{if(error.code==='ENOENT')return true;throw error;});
test('macOS game resources retain signatures and font loading after ordinary copy and release ZIP extraction', {skip:process.platform!=='darwin',timeout:180000}, async t=>{
  await mkdir('.cache/macos-resource-layout',{recursive:true});
  const out=await mkdtemp(path.resolve('.cache/macos-resource-layout/service-')),service=new QtStudioService();
  t.after(()=>service.releases.close());
  const call=(method,params={})=>service.handle({method,params:{revision:service.editor.revision,...params}});
  const runtime=service.runtimes.macos,runtimeApp=path.join(runtime,'RaisingPlayer.app');
  assert.ok(await absent(path.join(runtimeApp,'Contents/MacOS/fonts')),'Rebuild the native runtime to remove its legacy executable-adjacent fonts');
  const resources={};
  for(const name of await readdir(path.join(runtimeApp,'Contents/Resources/fonts')))resources[name]=hash(await readFile(path.join(runtimeApp,'Contents/Resources/fonts',name)));
  assert.ok(resources['NotoSansKR.ttf']&&resources['NotoSansJP.ttf']&&resources['NotoSansJP-provenance.json']&&resources['OFL-NotoSansJP.txt']);
  const before=hash(await readFile(path.join(runtimeApp,'Contents/MacOS/RaisingPlayer')));
  await call('create',{root:path.join(out,'project'),template:'my-first-raising-game'});
  const destination=path.join(out,'export'),exported=(await call('export',{target:'macos',destination,releaseVersion:'0.1.0'})).exported;
  const original=path.join(destination,'RaisingPlayer.app'),copied=path.join(out,'copied.app'),unzipped=path.join(out,'unzipped');
  await cp(original,copied,{recursive:true});
  const commands=[];
  function run(command,args,timeout=30000){const r=spawnSync(command,args,{encoding:'utf8',timeout,cwd:'/tmp'});commands.push({command,args,status:r.status,stdout:r.stdout,stderr:r.stderr});assert.equal(r.status,0,r.stderr||r.error?.message);return r;}
  run('/usr/bin/unzip',['-q',exported.gameArchive,'-d',unzipped]);
  for(const [label,app]of [['original',original],['copied',copied],['archive',path.join(unzipped,'RaisingPlayer.app')]]){
    assert.ok(await absent(path.join(app,'Contents/MacOS/fonts')));
    for(const [name,sha]of Object.entries(resources))assert.equal(hash(await readFile(path.join(app,'Contents/Resources/fonts',name))),sha,`${label} ${name}`);
    run('/usr/bin/codesign',['--verify','--deep','--strict',app]);
    // SDL resolves a bundled application's fonts from Resources even outside its
    // working directory. Missing either fallback font makes startup fail.
    const report=path.join(out,label+'-report.json');
    run(path.join(app,'Contents/MacOS/RaisingPlayer'),['--frames','3','--hidden','--locale','ja','--report',report]);
    assert.equal(JSON.parse(await readFile(report)).final.state,null);
  }
  assert.equal(hash(await readFile(path.join(runtimeApp,'Contents/MacOS/RaisingPlayer'))),before);
  // An older template can sign successfully in place and still be uncopyable.
  // Refuse it before creating the archive or publishing any output.
  const legacy=path.join(out,'legacy-runtime');await cp(runtime,legacy,{recursive:true});
  await mkdir(path.join(legacy,'RaisingPlayer.app/Contents/MacOS/fonts'),{recursive:true});
  await writeFile(path.join(legacy,'RaisingPlayer.app/Contents/MacOS/fonts/stale-resource.txt'),'legacy generated resource');
  service.runtimes={...service.runtimes,macos:legacy};
  const rejected=path.join(out,'rejected');
  await assert.rejects(()=>call('export',{target:'macos',destination:rejected,releaseVersion:'0.2.0'}),/Rebuild the macOS player runtime/);
  assert.ok(await absent(rejected));assert.deepEqual((await readdir(out)).filter(name=>name.startsWith('.raising-export-')),[]);
  await writeFile(path.join(out,'proof.json'),JSON.stringify({ok:true,runtime,runtimeExecutableSha256:before,resourceHashes:resources,originalCopiedAndArchivedSignatures:true,japaneseFontStartup:true,staleRuntimeRejectedWithoutPublication:true,commands},null,2));
  console.log(JSON.stringify({evidence:out}));
});

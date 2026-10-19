import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile,cp,unlink,access} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {createHash} from 'node:crypto';
await mkdir('.cache/qt-release',{recursive:true});
const root=await mkdtemp(resolve('.cache/qt-release/run-')),installed=join(root,'Installed Preview'),evidence=join(root,'evidence');
const portable=process.argv.includes('--portable');
await mkdir(evidence);const installer=resolve(process.env.QT_INSTALLER_OUT||'release/qt-preview/RaisingStudio-Qt-Preview-0.1.0-x64.exe');
const run=(exe,args,env=process.env,timeout=120000)=>{const r=spawnSync(exe,args,{env,windowsHide:true,windowsVerbatimArguments:args.some(a=>a.startsWith('/D=')||a.startsWith('_?=')),encoding:'utf8',timeout});assert.equal(r.status,0,JSON.stringify({exe,status:r.status,error:r.error?.message,stdout:r.stdout?.slice(-2000),stderr:r.stderr?.slice(-2000)}));return r;};
if(portable)await cp(resolve(process.env.QT_PACKAGE_OUT||'release/qt-preview/RaisingStudio'),installed,{recursive:true,errorOnExist:true});
else run(installer,['/S',`/D=${installed}`]);await access(join(installed,'RaisingStudioQt.exe'));
if(!portable){
const retry=spawnSync(installer,['/S',`/D=${installed}`],{windowsHide:true,windowsVerbatimArguments:true,encoding:'utf8',timeout:30000});assert.equal(retry.status,2,'existing install directory must not be overwritten');
}
const manifest=JSON.parse(await readFile(join(installed,'package-manifest.json'),'utf8'));
const shortcut=join(process.env.APPDATA,'Microsoft/Windows/Start Menu/Programs',`RaisingEngine ${manifest.releaseVersion}`,'Raising Studio.lnk');
const registryKey=`HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\RaisingEngine-${manifest.releaseVersion}`;
if(!portable){await access(shortcut);const registration=run('reg.exe',['query',registryKey,'/v','InstallLocation']);assert.ok(registration.stdout.includes(installed),'Installed apps entry points to installed folder');}
for(const f of manifest.files)assert.equal(createHash('sha256').update(await readFile(join(installed,f.path))).digest('hex'),f.sha256,f.path);
const env={...process.env,PATH:process.env.SystemRoot+'\\System32',QT_QPA_PLATFORM:'offscreen',RAISING_QT_TEST:'1',RAISING_RELEASE_EVIDENCE:evidence,RAISING_TEST_UI_LANGUAGE:process.env.RAISING_TEST_UI_LANGUAGE||'ko',RAISING_EDITOR_SETTINGS:join(evidence,'editor-settings.ini')};
for(const name of Object.keys(env))if((name.startsWith('RAISING_')&&!['RAISING_QT_TEST','RAISING_RELEASE_EVIDENCE','RAISING_TEST_UI_LANGUAGE','RAISING_EDITOR_SETTINGS'].includes(name))||['QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','NODE_PATH','NODE_OPTIONS'].includes(name))delete env[name];
// Only the test driver and QtTest DLL are temporary; the editor/service/player use installed files.
const driver=join(installed,'release-workflow-test.exe'),testDll=join(installed,'Qt6Test.dll'),offscreen=join(installed,'platforms/qoffscreen.dll');
await cp(resolve(process.env.QT_STUDIO_OUT||'dist/qt-studio','qt-studio-test.exe'),driver);
await cp(resolve(process.env.QT_ROOT||'C:/Qt/6.10.1/mingw_64','bin/Qt6Test.dll'),testDll);
await cp(resolve(process.env.QT_ROOT||'C:/Qt/6.10.1/mingw_64','plugins/platforms/qoffscreen.dll'),offscreen);
try{run(join(installed,'RaisingStudioQt.exe'),['--help'],env);run(driver,['releasePackageWorkflow','-o',join(evidence,'qt-tests.txt')+',txt'],env,180000);}finally{await unlink(driver);await unlink(testDll);await unlink(offscreen);}
const workflow=JSON.parse(await readFile(join(evidence,'qt-workflow.json'),'utf8'));assert.ok(workflow.ok);
const game=join(evidence,'windows'),steps=join(evidence,'native-steps.json'),report=join(evidence,'native-report.json'),save=join(evidence,'play-save.json');
await writeFile(steps,JSON.stringify([{click:'start'},{checkpoint:'started'},{click:'visit-city'},{checkpoint:'before-save'},{key:'F5'},{checkpoint:'saved'},{click:'visit-market'},{checkpoint:'market'},{key:'F9'},{checkpoint:'restored',screenshot:join(evidence,'restored.png')}]));
run(join(game,'RaisingPlayer.exe'),['--bundle',game,'--save',save,'--hidden','--automation',steps,'--report',report],env);
const played=JSON.parse(await readFile(report,'utf8'));assert.ok(played.ok,JSON.stringify(played));
assert.equal(played.steps.find(s=>s.command.checkpoint==='before-save').ui.state.gameplay.locationId,'city');
assert.equal(played.steps.find(s=>s.command.checkpoint==='market').ui.state.gameplay.locationId,'market');
assert.deepEqual(played.steps.find(s=>s.command.checkpoint==='before-save').ui.state,played.final.state);
if(!portable){
await writeFile(join(installed,'user-keep.txt'),'User-created file survives uninstall');
// Uninstall deletes only the packaged file list and leaves projects, saves and unknown files.
run(join(installed,'Uninstall.exe'),['/S',`_?=${installed}`],env);
for(let i=0;i<50;i++){try{await access(join(installed,'RaisingStudioQt.exe'));await new Promise(r=>setTimeout(r,100));}catch{break;}}
await assert.rejects(access(join(installed,'RaisingStudioQt.exe')));
await assert.rejects(access(shortcut));
assert.notEqual(spawnSync('reg.exe',['query',registryKey],{windowsHide:true}).status,0,'Uninstall removes its registration');
assert.equal(await readFile(join(installed,'user-keep.txt'),'utf8'),'User-created file survives uninstall');await access(join(workflow.project,'project.json'));await access(save);
}
const result={ok:true,portable,root,evidence,project:workflow.project,web:join(evidence,'web'),windows:game,installedManifestFiles:manifest.files.length,systemNodeAndQtPathsRemoved:true,createdEditedExportedInQt:true,windowsSaveExact:true,overwriteRejected:portable?'not-tested':true,uninstallPreservedUnknownFiles:portable?'not-tested':true};
await writeFile('.cache/qt-release/latest.json',JSON.stringify(result,null,2));console.log(JSON.stringify(result));

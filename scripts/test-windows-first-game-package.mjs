import assert from 'node:assert/strict';
import {copyFile,mkdir,unlink,readFile,writeFile} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';

assert.equal(process.platform,'win32','This gate needs a real Windows runner.');
const app=resolve(process.argv[2]),engine=join(app,'engine');
const qt=resolve(process.env.QT_ROOT||'C:/Qt/6.10.1/mingw_64');
const driver=join(app,'qt-first-game-test.exe'),dll=join(app,'Qt6Test.dll'),offscreen=join(app,'platforms/qoffscreen.dll');
await copyFile(resolve(process.env.QT_STUDIO_OUT||'dist/qt-studio','qt-first-game-test.exe'),driver);
await copyFile(join(qt,'bin/Qt6Test.dll'),dll);
await copyFile(join(qt,'plugins/platforms/qoffscreen.dll'),offscreen);
const env={...process.env,PATH:process.env.SystemRoot+'\\System32',QT_STUDIO_OUT:app,
  RAISING_NODE:join(engine,'node.exe'),RAISING_ENGINE_ROOT:engine,QT_QPA_PLATFORM:'offscreen'};
for(const name of ['QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','NODE_PATH','NODE_OPTIONS','RAISING_NATIVE_PLAYER_RUNTIME','RAISING_WEB_PLAYER_RUNTIME'])delete env[name];
try{
  // The helper prepares route data; the real Qt windows use only packaged
  // services, templates and player binaries, with developer PATH removed.
  const result=spawnSync(join(engine,'node.exe'),['scripts/test-first-game-ui.mjs'],{env,stdio:'inherit',timeout:240000});
  assert.equal(result.status,0,result.error?.message||'Packaged first-game journey failed');
  const proof=JSON.parse(await readFile('.cache/first-game-ui/latest.json','utf8'));
  await mkdir('.cache/windows-first-game-package',{recursive:true});
  await writeFile('.cache/windows-first-game-package/result.json',JSON.stringify({ok:true,app,proof,systemNodeAndQtPathsRemoved:true},null,2));
}finally{for(const file of [driver,dll,offscreen])await unlink(file);}

import { spawnSync } from 'node:child_process';
import { existsSync, mkdirSync, copyFileSync } from 'node:fs';
import path from 'node:path';
const qt = process.env.QT_ROOT ?? 'C:/Qt/6.10.1/mingw_64';
const compiler = process.env.QT_COMPILER ?? 'C:/Qt/Tools/mingw1310_64/bin';
const ninja = process.env.NINJA ?? 'C:/Qt/Tools/Ninja/ninja.exe';
const output = path.resolve(process.env.QT_STUDIO_OUT ?? 'dist/qt-studio');
const env = { ...process.env, PATH: `${compiler};${qt}/bin;${process.env.PATH}` };
function run(command,args){const r=spawnSync(command,args,{env,stdio:'inherit',windowsHide:true});if(r.error)throw r.error;if(r.status)process.exit(r.status);}
if(!existsSync(`${qt}/lib/cmake/Qt6/Qt6Config.cmake`))throw new Error('Set QT_ROOT to an installed Qt 6 desktop kit.');
run('cmake',['-S','native/studio','-B',output,'-G','Ninja',`-DCMAKE_MAKE_PROGRAM=${ninja}`,`-DCMAKE_CXX_COMPILER=${compiler}/g++.exe`,`-DCMAKE_PREFIX_PATH=${qt}`,'-DCMAKE_BUILD_TYPE=Release']);
run('cmake',['--build',output,'--parallel',process.env.BUILD_JOBS ?? '4']);
run(`${qt}/bin/windeployqt.exe`,['--release','--no-translations','--no-opengl-sw','--no-system-d3d-compiler','--no-system-dxc-compiler',`${output}/RaisingStudioQt.exe`]);
mkdirSync(`${output}/translations`, {recursive:true});
for (const locale of ['ko','en','ja']) copyFileSync(`${qt}/translations/qtbase_${locale}.qm`, `${output}/translations/qtbase_${locale}.qm`);
mkdirSync(`${output}/fonts`, {recursive:true});
for (const file of ['NotoSansJP.ttf','OFL-NotoSansJP.txt']) copyFileSync(`native/studio/fonts/${file}`, `${output}/fonts/${file}`);
console.log(`Qt editor: ${output}/RaisingStudioQt.exe`);

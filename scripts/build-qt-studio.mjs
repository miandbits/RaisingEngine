import { spawnSync, execFileSync } from 'node:child_process';
import { existsSync, mkdirSync, copyFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
const root = fileURLToPath(new URL('..', import.meta.url));
const windows = process.platform === 'win32';
const mac = process.platform === 'darwin';
const qt = process.env.QT_ROOT ?? (windows ? 'C:/Qt/6.10.1/mingw_64'
  : ['/opt/homebrew/opt/qt', '/usr/local/opt/qt'].find(p => existsSync(`${p}/lib/cmake/Qt6/Qt6Config.cmake`)));
if (!qt || !existsSync(`${qt}/lib/cmake/Qt6/Qt6Config.cmake`))
  throw new Error('Set QT_ROOT to an installed Qt 6 desktop kit (macOS: brew install qt cmake ninja).');
const compiler = process.env.QT_COMPILER ?? 'C:/Qt/Tools/mingw1310_64/bin';
const ninja = process.env.NINJA ?? (windows ? 'C:/Qt/Tools/Ninja/ninja.exe' : 'ninja');
const output = path.resolve(root, process.env.QT_STUDIO_OUT ?? 'dist/qt-studio');
const env = { ...process.env, PATH: [windows ? compiler : '', `${qt}/bin`, process.env.PATH].filter(Boolean).join(path.delimiter) };
function run(command, args) {
  const result = spawnSync(command, args, { cwd: root, env, stdio: 'inherit', windowsHide: true });
  if (result.error) throw result.error;
  if (result.status !== 0) throw new Error(`${command} failed (${result.status})`);
}
const options = windows ? [`-DCMAKE_CXX_COMPILER=${compiler}/g++.exe`] : [];
run(process.env.CMAKE ?? 'cmake', ['-S', 'native/studio', '-B', output, '-G', 'Ninja',
  `-DCMAKE_MAKE_PROGRAM=${ninja}`, `-DCMAKE_PREFIX_PATH=${qt}`, '-DCMAKE_BUILD_TYPE=Release', ...options]);
run(process.env.CMAKE ?? 'cmake', ['--build', output, '--parallel', process.env.BUILD_JOBS ?? '2',
  ...(process.env.QT_BUILD_TARGETS ? ['--target', ...process.env.QT_BUILD_TARGETS.split(',')] : [])]);
const executable = path.join(output, mac ? 'RaisingStudioQt.app/Contents/MacOS/RaisingStudioQt' : windows ? 'RaisingStudioQt.exe' : 'RaisingStudioQt');
if (!existsSync(executable)) { console.log(`Qt test targets built: ${output}`); process.exit(0); }
if (windows) run(`${qt}/bin/windeployqt.exe`, ['--release', '--no-translations', '--no-opengl-sw', '--no-system-d3d-compiler', '--no-system-dxc-compiler', executable]);
const resources = mac ? path.join(output, 'RaisingStudioQt.app/Contents/Resources') : output;
for (const directory of ['translations', 'fonts']) mkdirSync(path.join(resources, directory), { recursive: true });
const translations = mac ? execFileSync(`${qt}/bin/qtpaths`, ['--query', 'QT_INSTALL_TRANSLATIONS'], {encoding:'utf8'}).trim() : `${qt}/translations`;
for (const locale of ['ko', 'en', 'ja']) {
  const source = `${translations}/qtbase_${locale}.qm`;
  if (existsSync(source)) copyFileSync(source, path.join(resources, 'translations', `qtbase_${locale}.qm`));
}
for (const file of ['NotoSansJP.ttf', 'OFL-NotoSansJP.txt']) copyFileSync(path.join(root, 'native/studio/fonts', file), path.join(resources, 'fonts', file));
console.log(`Qt editor: ${executable}`);

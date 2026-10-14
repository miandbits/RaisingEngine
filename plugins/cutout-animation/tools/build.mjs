// Build the cutout animation plugin modules into dist/cutout (never shared output dirs).
//   node plugins/cutout-animation/tools/build.mjs [win] [wasm] [studio] [test]
// Defaults to "win wasm test". Env: QT_ROOT, QT_COMPILER, NINJA, EMSDK.
import { spawnSync } from 'node:child_process';
import { existsSync, mkdirSync } from 'node:fs';
import path from 'node:path';

const pluginRoot = path.resolve(import.meta.dirname, '..');
const engineRoot = path.resolve(pluginRoot, '../..');
const out = path.join(engineRoot, 'dist/cutout');
const steps = process.argv.slice(2).length ? process.argv.slice(2) : ['win', 'wasm', 'test'];
const qtRoot = process.env.QT_ROOT ?? 'C:/Qt/6.10.1/mingw_64';
const compiler = process.env.QT_COMPILER ?? 'C:/Qt/Tools/mingw1310_64/bin';
const ninja = process.env.NINJA ?? 'C:/Qt/Tools/Ninja/ninja.exe';
const winEnv = { ...process.env, PATH: `${compiler}${path.delimiter}${path.dirname(ninja)}${path.delimiter}${process.env.PATH}` };

function run(cmd, args, env = winEnv) {
  const r = spawnSync(cmd, args, { cwd: engineRoot, stdio: 'inherit', env });
  if (r.error) throw r.error;
  if (r.status !== 0) process.exit(r.status ?? 1);
}
mkdirSync(out, { recursive: true });

if (steps.includes('win') || steps.includes('studio') || steps.includes('test')) {
  const build = path.join(out, 'build-win');
  const studio = steps.includes('studio') ? 'ON' : 'OFF';
  run('cmake', ['-S', pluginRoot, '-B', build, '-G', 'Ninja', `-DCMAKE_MAKE_PROGRAM=${ninja}`, '-DCMAKE_BUILD_TYPE=Release',
    `-DCMAKE_CXX_COMPILER=${compiler}/g++.exe`, `-DCMAKE_PREFIX_PATH=${qtRoot}`, `-DCUTOUT_BUILD_STUDIO=${studio}`]);
  run('cmake', ['--build', build, '--parallel', '4']);
  if (steps.includes('test')) run(path.join(build, 'cutout-core-tests.exe'), []);
}
if (steps.includes('wasm')) {
  const sdk = process.env.EMSDK ?? path.join(engineRoot, '.toolchains/emsdk');
  const toolchain = path.join(sdk, 'upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake');
  if (!existsSync(toolchain)) throw new Error('Emscripten SDK not found. Set EMSDK.');
  const env = { ...process.env, NODE_OPTIONS: '', EMSDK: sdk, EM_CONFIG: path.join(sdk, '.emscripten') };
  const build = path.join(out, 'build-wasm');
  run('cmake', ['-S', pluginRoot, '-B', build, '-G', 'MinGW Makefiles', '-DCMAKE_BUILD_TYPE=Release', `-DCMAKE_TOOLCHAIN_FILE=${toolchain}`], env);
  run('cmake', ['--build', build, '--target', 'cutout-runtime', '--parallel', '2'], env);
}
console.log(`cutout plugin modules: ${out}`);

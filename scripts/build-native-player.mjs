import { nativePlayerCapabilities } from './lib/native-player-capabilities.mjs';
import { spawnSync } from 'node:child_process';
import { existsSync } from 'node:fs';
import { mkdir, copyFile, cp, readFile, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
import path from 'node:path';
import { prepareNativeDependencies } from './prepare-native-dependencies.mjs';
const root = fileURLToPath(new URL('..', import.meta.url));
await prepareNativeDependencies(root);
const output = path.resolve(root, process.env.NATIVE_PLAYER_BUILD ?? 'dist/native-player');
const artifact = path.resolve(root, process.env.NATIVE_PLAYER_OUT ?? 'dist/native-player-app');
const cmake = process.env.CMAKE ?? 'cmake';
const mingw = process.env.QT_COMPILER ?? 'C:/Program Files/mingw64/bin';
const env = { ...process.env, PATH: process.platform === 'win32' ? `${mingw}${path.delimiter}${process.env.PATH}` : process.env.PATH, NODE_OPTIONS: '' };
function run(args) { const result = spawnSync(cmake, args, { cwd: root, env, stdio: 'inherit' }); if (result.error) throw result.error; if (result.status) process.exit(result.status); }
const options = process.platform === 'win32' && existsSync(path.join(mingw, 'g++.exe')) ? ['-G', 'MinGW Makefiles', `-DCMAKE_C_COMPILER=${mingw}/gcc.exe`, `-DCMAKE_CXX_COMPILER=${mingw}/g++.exe`] : [];
run(['-S', 'native/player', '-B', output, ...options, '-DCMAKE_BUILD_TYPE=Release']);
run(['--build', output, '--target', 'raising-player', '--parallel', process.env.BUILD_JOBS ?? '4']);
await mkdir(path.join(artifact, 'licenses'), { recursive: true });
const filename = process.platform === 'win32' ? 'RaisingPlayer.exe' : 'RaisingPlayer';
const mac = process.platform === 'darwin';
const relativeExecutable = mac ? 'RaisingPlayer.app/Contents/MacOS/RaisingPlayer' : filename;
if (mac) await cp(path.join(output, 'RaisingPlayer.app'), path.join(artifact, 'RaisingPlayer.app'), { recursive: true });
else await copyFile(path.join(output, filename), path.join(artifact, filename));
// Fonts rarely change; skipping identical files keeps a running player/studio from blocking the copy (EBUSY).
const same = async (a, b) => existsSync(b) && (await readFile(a)).equals(await readFile(b));
await cp(path.join(output, mac ? 'RaisingPlayer.app/Contents/Resources/fonts' : 'fonts'), path.join(artifact, 'fonts'), { recursive: true, filter: async (source, destination) => !(await same(source, destination).catch(() => false)) });
for (const [source, destination] of [
  ['SDL/LICENSE.txt','SDL.txt'], ['SDL_image/LICENSE.txt','SDL_image.txt'], ['RmlUi/LICENSE.txt','RmlUi.txt'], ['freetype/docs/FTL.TXT','Freetype.txt'], ['miniaudio/LICENSE','miniaudio.txt'], ['SDL_image/external/libwebp/COPYING','libwebp.txt'], ['SDL_image/external/libwebp/PATENTS','libwebp-PATENTS.txt'],
]) await copyFile(path.join(root, 'native/player/vendor', source), path.join(artifact, 'licenses', destination));
await copyFile(path.join(root, 'native/vendor/LICENSE.MIT'), path.join(artifact, 'licenses/nlohmann-json.txt'));
await copyFile(path.join(root,'native/player/vendor/RmlUi/Include/RmlUi/Core/Containers/LICENSE.txt'),path.join(artifact,'licenses/RmlUi-containers.txt'));
const stbSource=await readFile(path.join(root,'native/player/vendor/SDL_image/src/stb_image.h'),'utf8');
const stbLicense=stbSource.slice(stbSource.lastIndexOf('This software is available under 2 licenses'));
if(!stbLicense.includes('Copyright (c) 2017 Sean Barrett'))throw Error('SDL_image stb notice not found');
await writeFile(path.join(artifact,'licenses/stb_image.txt'),stbLicense);
await writeFile(path.join(artifact, 'build.json'), JSON.stringify({ runtime: 'cpp-sdl3-rmlui', executable: relativeExecutable, platform: process.platform, arch: process.arch,
  sha256: createHash('sha256').update(await readFile(path.join(artifact, relativeExecutable))).digest('hex'),
  dependencies: { SDL:'3.2.24', SDL_image:'3.2.4', RmlUi:'6.1', FreeType:'2.13.3', miniaudio:'0.11.23', libwebp:'1.6.0' } }, null, 2));
await writeFile(path.join(artifact, 'capabilities.json'), JSON.stringify(nativePlayerCapabilities(process.platform),null,2));
if (mac) await cp(path.join(artifact, "licenses"), path.join(artifact, "RaisingPlayer.app/Contents/Resources/licenses"), {recursive:true});
console.log(`Native graphical player: ${path.join(artifact, relativeExecutable)}`);

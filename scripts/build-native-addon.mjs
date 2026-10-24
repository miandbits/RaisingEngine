import { spawnSync } from 'node:child_process';
import { mkdirSync, readFileSync, writeFileSync, renameSync, existsSync, copyFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { resolve, dirname } from 'node:path';
const root = resolve(import.meta.dirname, '..'), output = resolve(root, 'dist/native');
const runtime = JSON.parse(readFileSync(resolve(output, 'runtime.json'), 'utf8'));
const names = ['CMakeLists.txt', 'include/raising/runtime.hpp','include/raising/calendar.hpp','include/raising/json-calendar.hpp', 'include/raising/relationship-history.hpp', 'src/runtime.cpp','src/user-fields.inc', 'src/gameplay.inc', 'src/generic.inc','src/roster.inc', 'src/character-state.inc','src/custom-states.inc','src/progression.inc','src/unified-time.inc', 'src/view.inc', 'src/tactics.inc','src/systems.inc', 'src/systems-view.inc', 'src/protocol.inc', 'src/main.cpp', 'src/wasm.cpp', 'vendor/json.hpp',
  'addon/CMakeLists.txt', 'addon/addon.cpp', ...['node_api.h', 'node_api_types.h', 'js_native_api.h', 'js_native_api_types.h'].map(x => `addon/vendor/${x}`)];
const files = names.map(name => [name, readFileSync(resolve(root, 'native', name))]);
const coreHash = createHash('sha256').update('native-release-isolated-v2:' + process.platform);
for (const [name, bytes] of files.filter(([name]) => !name.startsWith('addon/'))) coreHash.update(name).update(bytes);
if (coreHash.digest('hex').slice(0, 16) !== runtime.sourceRevision) throw new Error('Build the current C++ runtime first: node scripts/build-native.mjs');
const hash = createHash('sha256').update('napi8-host-resolve-v1:' + process.platform + ':' + process.arch);
for (const [name, bytes] of files) hash.update(name).update(bytes);
const revision = hash.digest('hex').slice(0, 16), stem = `raising-addon-${revision}`, filename = `${stem}.node`;
const manifest = { filename, executable: runtime.executable, sourceRevision: revision, napi: 8, platform: process.platform, arch: process.arch };
function publish() { const temp = resolve(output, `addon-${process.pid}.json`); writeFileSync(temp, JSON.stringify(manifest, null, 2)); renameSync(temp, resolve(output, 'addon.json')); }
if (existsSync(resolve(output, filename))) { publish(); console.log(`Node-API addon current: ${filename}`); process.exit(0); }
// Sources and output paths are immutable once published; loaded Windows DLLs
// can remain in existing editors while another build publishes a new module.
const source = resolve(root, '.cache/addon-sources', revision), build = resolve(root, '.cache/addon-build', revision, String(process.pid));
for (const [name, bytes] of files) { const path = resolve(source, name); mkdirSync(dirname(path), { recursive: true }); writeFileSync(path, bytes); }
mkdirSync(build, { recursive: true });
function run(cmd, args) { const r = spawnSync(cmd, args, { cwd: root, stdio: 'inherit' }); if (r.error) throw r.error; if (r.status !== 0) process.exit(r.status ?? 1); }
run('cmake', ['-S', resolve(source, 'addon'), '-B', build, ...(process.platform === 'win32' ? ['-G', 'MinGW Makefiles'] : []), '-DCMAKE_BUILD_TYPE=Release', `-DRAISING_ADDON_NAME=${stem}`]);
run('cmake', ['--build', build, '--target', 'raising-addon', '--parallel', '2']);
// COPYFILE_EXCL keeps a module already loaded by another process untouched.
try { copyFileSync(resolve(build, filename), resolve(output, filename), 1); } catch (error) { if (error.code !== 'EEXIST') throw error; }
publish(); console.log(`Node-API C++ runtime: ${resolve(output, filename)}`);

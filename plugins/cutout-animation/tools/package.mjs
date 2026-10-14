// Assemble dist/plugins/cutout-animation-<version>.raiseplugin from sources + built modules.
//   node plugins/cutout-animation/tools/package.mjs [--out <dir>] [--no-authoring]
// Needs `tsc` output (dist/packages/plugin-host) and `tools/build.mjs win wasm` output.
// Env: CUTOUT_STUDIO_DLL overrides the authoring module location.
import { createHash } from 'node:crypto';
import { existsSync, readdirSync, statSync } from 'node:fs';
import { mkdir, readFile, readdir, rename, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const pluginRoot = path.resolve(import.meta.dirname, '..');
const engineRoot = path.resolve(pluginRoot, '../..');
const args = process.argv.slice(2);
const outIndex = args.indexOf('--out');
const outDir = path.resolve(outIndex >= 0 ? args[outIndex + 1] : path.join(engineRoot, 'dist/plugins'));
const host = path.join(engineRoot, 'dist/packages/plugin-host/index.js');
if (!existsSync(host)) throw new Error('dist/packages/plugin-host가 없습니다. 먼저 npx tsc -p tsconfig.json을 실행하세요.');
const { writeZip, inspectBytes, parseManifest } = await import(pathToFileURL(host).href);

/** Find a built file under a folder (Qt/CMake layouts differ between builds). */
function findFile(root, name) {
  if (!existsSync(root)) return null;
  for (const entry of readdirSync(root, { withFileTypes: true })) {
    const full = path.join(root, entry.name);
    if (entry.isFile() && entry.name.toLowerCase() === name) return full;
    if (entry.isDirectory() && entry.name !== 'CMakeFiles') { const found = findFile(full, name); if (found) return found; }
  }
  return null;
}
const built = {
  'runtime/windows/cutout-runtime.dll': path.join(engineRoot, 'dist/cutout/build-win/cutout-runtime.dll'),
  'runtime/web/cutout-runtime.mjs': path.join(engineRoot, 'dist/cutout/build-wasm/cutout-runtime.mjs'),
  'runtime/web/cutout-runtime.wasm': path.join(engineRoot, 'dist/cutout/build-wasm/cutout-runtime.wasm'),
};
const studio = args.includes('--no-authoring') ? null : process.env.CUTOUT_STUDIO_DLL || findFile(path.join(engineRoot, 'dist/cutout/build-win'), 'cutout-studio.dll');
if (studio && existsSync(studio)) built['authoring/windows/cutout-studio.dll'] = studio;

const files = new Map(); // package path -> bytes
async function addTree(folder, prefix) {
  const absolute = path.join(pluginRoot, folder);
  if (!existsSync(absolute)) return;
  for (const entry of (await readdir(absolute, { withFileTypes: true })).sort((a, b) => a.name < b.name ? -1 : 1)) {
    const relative = `${prefix}/${entry.name}`;
    if (entry.isDirectory()) await addTree(`${folder}/${entry.name}`, relative);
    else if (entry.isFile()) files.set(relative, await readFile(path.join(absolute, entry.name)));
  }
}
for (const folder of ['schema', 'docs', 'service', 'templates']) await addTree(folder, folder);
for (const [target, source] of Object.entries(built)) {
  if (!existsSync(source) || !statSync(source).isFile()) {
    if (target.startsWith('authoring/')) continue;
    throw new Error(`빌드된 모듈이 없습니다: ${source}. node plugins/cutout-animation/tools/build.mjs win wasm을 먼저 실행하세요.`);
  }
  files.set(target, await readFile(source));
}

const manifest = JSON.parse(await readFile(path.join(pluginRoot, 'plugin.json'), 'utf8'));
// Authoring modules are optional per build; every other listed module must exist.
manifest.modules = manifest.modules.filter(m => files.has(m.path) || m.role !== 'authoring');
for (const m of manifest.modules) for (const file of [m.path, ...(m.files ?? [])]) if (!files.has(file)) throw new Error(`모듈 파일이 없습니다: ${file}`);
const names = [...files.keys()].sort();
manifest.files = Object.fromEntries(names.map(name => [name, createHash('sha256').update(files.get(name)).digest('hex')]));
const manifestText = JSON.stringify(manifest, null, 2) + '\n';
parseManifest(manifestText);
const archive = writeZip([{ name: 'plugin.json', data: Buffer.from(manifestText) }, ...names.map(name => ({ name, data: files.get(name) }))]);
inspectBytes(archive); // the same verification installPackage performs

await mkdir(outDir, { recursive: true });
const target = path.join(outDir, `${manifest.id}-${manifest.version}.raiseplugin`), temporary = `${target}.${process.pid}.tmp`;
await writeFile(temporary, archive);
await rename(temporary, target);
console.log(JSON.stringify({ package: target, version: manifest.version, files: names.length, authoring: files.has('authoring/windows/cutout-studio.dll'),
  sha256: createHash('sha256').update(archive).digest('hex') }));

import { cp, mkdir, mkdtemp, readFile, readdir, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { execFileSync } from 'node:child_process';
import { resolve, dirname, join } from 'node:path';
import { copyNativeResources } from './copy-native-resources.mjs';

// A pinned, official Node distribution makes the installed MCP independent of PATH/npm.
const version = '22.23.3';
const archive = `node-v${version}-win-x64.zip`;
const cache = resolve('.cache/node-runtime'); await mkdir(cache, { recursive: true });
const base = `https://nodejs.org/dist/v${version}/`;
async function download(name) {
  const response = await fetch(base + name, { signal: AbortSignal.timeout(120000) });
  if (!response.ok) throw new Error(`Node download failed: ${name} ${response.status}`);
  return Buffer.from(await response.arrayBuffer());
}
const sums = await download('SHASUMS256.txt');
const expected = sums.toString('utf8').split('\n').find(line => line.endsWith(`  ${archive}`))?.split(' ')[0];
if (!expected || !/^[a-f0-9]{64}$/.test(expected)) throw new Error('Official Node checksum missing.');
let zip;
try { zip = await readFile(join(cache, archive)); } catch { zip = await download(archive); }
if (createHash('sha256').update(zip).digest('hex') !== expected) throw new Error('Node archive checksum mismatch.');
await writeFile(join(cache, archive), zip);
const unpacked = await mkdtemp(join(cache, 'unpacked-'));
// Paths are fixed descendants of the project cache; no shell-built delete/move commands.
const quote = value => `'${value.replaceAll("'", "''")}'`;
execFileSync('powershell.exe', ['-NoProfile','-NonInteractive','-Command', `Expand-Archive -LiteralPath ${quote(join(cache, archive))} -DestinationPath ${quote(unpacked)}`], { windowsHide: true });
const distribution = join(unpacked, `node-v${version}-win-x64`);
await mkdir('dist/mcp/schema', { recursive: true });
for (const name of await readdir('packages/schema')) if (name.endsWith('.schema.json')) await cp(`packages/schema/${name}`, `dist/mcp/schema/${name}`);
await cp('packages/runtime-service/save.schema.json', 'dist/mcp/main/save.schema.json');
await copyNativeResources('dist/mcp/native');
await cp(join(distribution, 'node.exe'), 'dist/mcp/node.exe');
await cp(join(distribution, 'LICENSE'), 'dist/mcp/NODE-LICENSE.txt');
await writeFile('dist/mcp/package.json', JSON.stringify({ private: true, type: 'module' }));
await writeFile('dist/mcp/runtime.json', JSON.stringify({ nodeVersion: version, architecture: 'win32-x64', source: base + archive, sha256: expected }, null, 2));

// Preserve licenses for the bundled runtime dependency graph (not the build toolchain).
const visited = new Set(); const notices = [];
async function license(name, from = resolve('.')) {
  let at = from; let folder;
  for (;;) {
    const candidate = join(at, 'node_modules', name);
    try { await readFile(join(candidate, 'package.json')); folder = candidate; break; } catch {}
    const up = dirname(at); if (up === at) throw new Error(`Cannot find dependency license: ${name}`); at = up;
  }
  if (visited.has(folder)) return; visited.add(folder);
  const pkg = JSON.parse(await readFile(join(folder, 'package.json'), 'utf8'));
  const files = (await readdir(folder)).filter(file => /^(LICENSE|LICENCE|COPYING|NOTICE)(\.|$)/i.test(file));
  notices.push(`\n===== ${pkg.name}@${pkg.version} (${pkg.license ?? 'see package'}) =====\n`);
  for (const file of files) notices.push(await readFile(join(folder, file), 'utf8'));
  if (!files.length) throw new Error(`Missing license text: ${pkg.name}`);
  for (const dep of Object.keys(pkg.dependencies ?? {})) await license(dep, folder);
}
for (const name of ['@modelcontextprotocol/server','zod','ajv','jsonc-parser']) await license(name);
await writeFile('dist/mcp/THIRD-PARTY-NOTICES.txt', notices.join('\n'));
console.log(`Standalone MCP ready: Node ${version}, official SHA-256 verified, ${visited.size} dependency licenses.`);

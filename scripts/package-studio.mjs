import { cp, mkdir, mkdtemp, writeFile, open } from 'node:fs/promises';
import { resolve, relative } from 'node:path';
import { packager } from '@electron/packager';
const output = process.env.STUDIO_PACKAGE_OUT ?? 'release';
// Windows refuses writes to loaded executables. Check before packager removes
// the old folder, so a running preview cannot leave a partially replaced app.
for (const file of ['Raising Studio.exe', 'dxcompiler.dll', 'resources/app.asar']) {
  try { const handle = await open(resolve(output, 'Raising Studio-win32-x64', file), 'r+'); await handle.close(); }
  catch (error) {
    if (error.code !== 'ENOENT') throw new Error(`Close the existing Raising Studio preview before packaging (${file}), or choose a new STUDIO_PACKAGE_OUT.`, { cause: error });
  }
}
await mkdir('.cache', { recursive: true });
const staging = await mkdtemp(resolve('.cache/studio-package-'));
await cp('dist/studio', `${staging}/dist/studio`, { recursive: true });
await cp('examples/academy-demo', `${staging}/examples/academy-demo`, { recursive: true });
await cp('examples/academy-year', `${staging}/examples/academy-year`, { recursive: true });
await cp('examples/academy-visual', `${staging}/examples/academy-visual`, { recursive: true });
await cp('examples/academy-journey', `${staging}/examples/academy-journey`, { recursive: true, filter: source => !relative('examples/academy-journey', source).split(/[\\/]/).includes('.studio') });
await cp('examples/academy-adventure', `${staging}/examples/academy-adventure`, { recursive: true, filter: source => !relative('examples/academy-adventure', source).split(/[\\/]/).includes('.studio') });
const resourceStaging = await mkdtemp(resolve('.cache/studio-resources-'));
const exportRuntimes = resolve(resourceStaging, 'export-runtimes');
await mkdir(exportRuntimes, { recursive: true });
await cp('dist/web', `${exportRuntimes}/web`, { recursive: true });
await cp(process.env.STUDIO_NATIVE_PLAYER_DIR ?? 'dist/native-player-app', `${exportRuntimes}/native-player-app`, { recursive: true });
await writeFile(`${staging}/package.json`, JSON.stringify({ name: 'raising-studio', productName: 'Raising Studio', version: '0.1.0', type: 'module', main: 'dist/studio/main/index.js' }, null, 2));
const paths = await packager({ dir: staging, out: output, name: 'Raising Studio', platform: 'win32', arch: 'x64', overwrite: true, asar: true, prune: false, electronVersion: '44.7.0', extraResource: [resolve('dist/mcp'),resolve('dist/studio/native'),exportRuntimes] });
console.log(`Portable studio: ${paths.join(', ')}`);

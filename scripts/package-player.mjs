import { cp, mkdir, mkdtemp, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { packager } from '@electron/packager';
await mkdir('.cache', { recursive: true });
const staging = await mkdtemp(resolve('.cache/player-package-'));
await cp('dist/player', `${staging}/dist/player`, { recursive: true });
await cp('examples/academy-demo', `${staging}/examples/academy-demo`, { recursive: true });
await cp('examples/academy-year', `${staging}/examples/academy-year`, { recursive: true });
await writeFile(`${staging}/package.json`, JSON.stringify({ name: 'raising-player', productName: 'Raising Player',
  version: '0.1.0', type: 'module', main: 'dist/player/main/index.js' }, null, 2));
const paths = await packager({ dir: staging, out: process.env.PLAYER_PACKAGE_OUT ?? 'release', name: 'Raising Player', platform: 'win32', arch: 'x64',
  overwrite: true, asar: true, prune: false, electronVersion: '44.7.0',extraResource:[resolve('dist/player/native')] });
console.log(`Portable player: ${paths.join(', ')}`);

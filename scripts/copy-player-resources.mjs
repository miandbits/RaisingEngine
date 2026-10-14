import { cp, mkdir, readdir } from 'node:fs/promises';
import { copyNativeResources } from './copy-native-resources.mjs';
await mkdir('dist/player/schema', { recursive: true });
for (const name of await readdir('packages/schema')) {
  if (name.endsWith('.schema.json')) await cp(`packages/schema/${name}`, `dist/player/schema/${name}`);
}
await cp('packages/runtime-service/save.schema.json', 'dist/player/main/save.schema.json');
await copyNativeResources('dist/player/native');
console.log('Player schemas copied.');

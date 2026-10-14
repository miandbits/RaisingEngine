import { cp, mkdir, readdir } from 'node:fs/promises';
import { copyNativeResources } from './copy-native-resources.mjs';
await mkdir('dist/studio/schema', { recursive: true });
for (const name of await readdir('packages/schema')) if (name.endsWith('.schema.json')) await cp(`packages/schema/${name}`, `dist/studio/schema/${name}`);
await cp('packages/runtime-service/save.schema.json', 'dist/studio/main/save.schema.json');
await copyNativeResources('dist/studio/native');
console.log('Studio schemas copied.');

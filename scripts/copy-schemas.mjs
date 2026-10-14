import { copyFile, mkdir, readdir } from 'node:fs/promises';

const source = new URL('../packages/schema/', import.meta.url);
const destination = new URL('../dist/packages/schema/', import.meta.url);
await mkdir(destination, { recursive: true });
for (const name of (await readdir(source)).filter((name) => name.endsWith('.schema.json'))) {
  await copyFile(new URL(name, source), new URL(name, destination));
}
const runtimeDestination = new URL('../dist/packages/runtime-service/', import.meta.url);
await mkdir(runtimeDestination, { recursive: true });
await copyFile(new URL('../packages/runtime-service/save.schema.json', import.meta.url), new URL('save.schema.json', runtimeDestination));

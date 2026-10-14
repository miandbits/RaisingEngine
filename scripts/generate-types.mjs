import { readFile, readdir, mkdir, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { compile } from 'json-schema-to-typescript';

const directory = fileURLToPath(new URL('../packages/schema/', import.meta.url));
const output = path.join(directory, 'generated');
await mkdir(output, { recursive: true });
for (const file of (await readdir(directory)).filter((name) => name.endsWith('.schema.json') && name !== 'common.schema.json').sort()) {
  const schema = JSON.parse(await readFile(path.join(directory, file), 'utf8'));
  const name = file.replace('.schema.json', '');
  const result = await compile(schema, name, {
    cwd: directory,
    bannerComment: '/* Generated from JSON Schema. Do not edit; run npm run build. */',
    additionalProperties: false,
  });
  await writeFile(path.join(output, `${name}.ts`), result);
}

import { readFile, readdir, mkdir, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { compile } from 'json-schema-to-typescript';

const directory = fileURLToPath(new URL('../packages/schema/', import.meta.url));
const output = path.join(directory, 'generated');
// One conservative update policy drives both browser/export and native validation.
const policy = JSON.parse(await readFile(new URL('../config/presentation-policy.json', import.meta.url), 'utf8'));
await writeFile(new URL('../packages/visual/presentation-policy.generated.ts', import.meta.url), '// Generated from config/presentation-policy.json.\nexport const presentationPolicy = '+JSON.stringify(policy,null,2)+';\n');
await writeFile(new URL('../native/player/presentation-policy.generated.hpp', import.meta.url), '// Generated from config/presentation-policy.json.\n#pragma once\ninline constexpr const char* raising_presentation_policy=R"policy('+JSON.stringify(policy)+')policy";\n');
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

import { readFile, readdir } from 'node:fs/promises';
import { Ajv2020 } from 'ajv/dist/2020.js';
import type { ValidateFunction } from 'ajv';
import { childPointer, diagnostic, schemaFiles, type Diagnostic, type ProjectDocument } from './model.js';

let cached: Promise<Map<string, ValidateFunction>> | undefined;
function getValidators(): Promise<Map<string, ValidateFunction>> {
  return cached ??= (async () => {
    const directory = new URL('../schema/', import.meta.url);
    const ajv = new Ajv2020({ strict: true, allErrors: true, validateSchema: true });
    for (const name of (await readdir(directory)).filter((file) => file.endsWith('.schema.json')).sort()) {
      ajv.addSchema(JSON.parse(await readFile(new URL(name, directory), 'utf8')));
    }
    return new Map(Object.values(schemaFiles).map((file) => [file, ajv.getSchema(`https://raising-engine.example/schemas/0.1.0/${file}`)!]));
  })();
}
export async function validateStructure(doc: ProjectDocument): Promise<Diagnostic[]> {
  const validate = (await getValidators()).get(schemaFiles[doc.kind])!;
  if (validate(doc.data)) return [];
  return (validate.errors ?? []).map((error) => {
    let pointer = error.instancePath;
    if (error.keyword === 'additionalProperties') pointer = childPointer(pointer, String(error.params.additionalProperty));
    if (error.keyword === 'unevaluatedProperties') pointer = childPointer(pointer, String(error.params.unevaluatedProperty));
    if (error.keyword === 'required') pointer = childPointer(pointer, String(error.params.missingProperty));
    const version = pointer === '/formatVersion';
    return diagnostic(doc, pointer, version ? 'FORMAT_VERSION' : 'SCHEMA_INVALID', version
      ? '지원하지 않거나 누락된 formatVersion입니다. 현재 버전은 0.1.0입니다.'
      : `스키마 규칙 위반 (${error.keyword}): ${error.message ?? 'invalid data'}; ${JSON.stringify(error.params)}`);
  });
}

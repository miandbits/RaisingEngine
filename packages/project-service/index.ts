import { realpath, stat } from 'node:fs/promises';
import path from 'node:path';
import { loadProject, isWithin } from './loader.js';
import { validateProject } from '../validation/index.js';
import { diagnostic, sortDiagnostics, type JsonObject } from '../validation/model.js';
import { objectItems } from '../validation/context.js';

export { loadProject } from './loader.js';
export { validateProject } from '../validation/index.js';

export async function checkProject(directory: string) {
  const loaded = await loadProject(directory);
  const validation = await validateProject(loaded);
  for (const doc of validation.documents) {
    if (doc.kind !== 'assets') continue;
    for (const [i, asset] of objectItems((doc.data as JsonObject).assets).entries()) {
      const pointer = `/assets/${i}/path`;
      try {
        const resolved = await realpath(path.resolve(loaded.root, asset.path as string));
        if (!isWithin(loaded.root, resolved)) validation.diagnostics.push(diagnostic(doc, pointer, 'ASSET_OUTSIDE_PROJECT', '에셋이 프로젝트 밖을 가리킵니다.', [asset.id as string]));
        else if (!(await stat(resolved)).isFile()) validation.diagnostics.push(diagnostic(doc, pointer, 'ASSET_NOT_FILE', '에셋 경로는 일반 파일이어야 합니다.', [asset.id as string]));
      } catch (error) {
        const code = (error as NodeJS.ErrnoException).code;
        validation.diagnostics.push(diagnostic(doc, pointer, code === 'ENOENT' ? 'ASSET_MISSING' : 'ASSET_READ', `에셋을 확인할 수 없습니다: ${code ?? 'UNKNOWN'}`, [asset.id as string]));
      }
    }
  }
  sortDiagnostics(validation.diagnostics);
  const errors = validation.diagnostics.filter((d) => d.severity === 'error').length;
  const warnings = validation.diagnostics.length - errors;
  return {
    reportVersion: '1.0.0',
    projectPath: loaded.root,
    valid: errors === 0,
    summary: { parsedDocuments: loaded.documents.length, structuredDocuments: validation.documents.length, errors, warnings },
    diagnostics: validation.diagnostics,
  };
}
export { buildSnapshot } from './snapshot.js';

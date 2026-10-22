import type { ContentByKind } from '../schema/types.js';
import { diagnostic, sortDiagnostics, type LoadedProject, type ProjectDocument, type ValidationResult } from './model.js';
import { validateStructure } from './structure.js';
import { validateSemantics } from './semantics.js';
import { validatePlugins } from './plugins.js';
import { compileRules } from '../rule-extensions/compiler.js';

export type ValidatedDocument = { [K in keyof ContentByKind]: Omit<ProjectDocument, 'kind' | 'data'> & { kind: K; data: ContentByKind[K] } }[keyof ContentByKind];

export async function validateProject(project: LoadedProject): Promise<ValidationResult & { structuredDocuments: ValidatedDocument[]; ruleLock?: import('./model.js').JsonObject }> {
  const compiled = compileRules(project);
  const diagnostics = [...project.diagnostics, ...compiled.diagnostics];
  const documents: ProjectDocument[] = [];
  for (const doc of compiled.documents) {
    const issues = await validateStructure(doc);
    diagnostics.push(...issues);
    if (!issues.length) documents.push(doc);
  }
  // Engine semantics never see plugin documents: their data is plugin-owned (e.g. an
  // `imageAssetId` inside a rig is checked by the plugin service, not the engine walker).
  // An invalid manifest is absent from documents. Do not interpret that as a
  // legacy project with no roster, items or locations and flood the author with
  // false missing-feature errors. All files still receive structural validation;
  // cross-document semantics resume once the manifest can be trusted.
  if (documents.some((d) => d.kind === 'project')) {
    diagnostics.push(...validateSemantics(documents.filter((d) => d.kind !== 'extension' && d.kind !== 'pluginLock')));
  } else {
    const manifest = compiled.documents.find((d) => d.kind === 'project');
    if (manifest) diagnostics.push(diagnostic(manifest, '', 'SEMANTICS_DEFERRED',
      '프로젝트 형식 오류를 먼저 수정하세요. 문서 간 참조·규칙 검사는 수정 후 재검증됩니다.', [], 'warning'));
  }
  const plugins = await validatePlugins(project, documents);
  diagnostics.push(...plugins.diagnostics);
  const structured = plugins.rejected.size ? documents.filter((d) => !plugins.rejected.has(d)) : documents;
  const mappedDiagnostics = diagnostics.map(compiled.remap);
  sortDiagnostics(mappedDiagnostics);
  return {
    valid: !mappedDiagnostics.some((d) => d.severity === 'error'), diagnostics: mappedDiagnostics, documents: structured,
    // Runtime JSON Schema validation above is the boundary for generated types.
    structuredDocuments: structured as unknown as ValidatedDocument[],
    ...(compiled.lock ? { ruleLock: compiled.lock } : {}),
  };
}

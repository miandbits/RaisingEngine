import type { ContentByKind } from '../schema/types.js';
import { sortDiagnostics, type LoadedProject, type ProjectDocument, type ValidationResult } from './model.js';
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
  diagnostics.push(...validateSemantics(documents.filter((d) => d.kind !== 'extension' && d.kind !== 'pluginLock')));
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

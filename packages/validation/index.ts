import type { ContentByKind } from '../schema/types.js';
import { sortDiagnostics, type LoadedProject, type ProjectDocument, type ValidationResult } from './model.js';
import { validateStructure } from './structure.js';
import { validateSemantics } from './semantics.js';

export type ValidatedDocument = { [K in keyof ContentByKind]: Omit<ProjectDocument, 'kind' | 'data'> & { kind: K; data: ContentByKind[K] } }[keyof ContentByKind];

export async function validateProject(project: LoadedProject): Promise<ValidationResult & { structuredDocuments: ValidatedDocument[] }> {
  const diagnostics = [...project.diagnostics];
  const documents: ProjectDocument[] = [];
  for (const doc of project.documents) {
    const issues = await validateStructure(doc);
    diagnostics.push(...issues);
    if (!issues.length) documents.push(doc);
  }
  diagnostics.push(...validateSemantics(documents));
  sortDiagnostics(diagnostics);
  return {
    valid: !diagnostics.some((d) => d.severity === 'error'), diagnostics, documents,
    // Runtime JSON Schema validation above is the boundary for generated types.
    structuredDocuments: documents as unknown as ValidatedDocument[],
  };
}

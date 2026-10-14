export type Json = null | boolean | number | string | Json[] | JsonObject;
export interface JsonObject { [key: string]: Json }
export function isObject(value: unknown): value is JsonObject {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

export const schemaFiles = {
  project: 'project.schema.json', attributes: 'attributes.schema.json', flags: 'flags.schema.json',
  character: 'character.schema.json', activity: 'activity.schema.json', event: 'event.schema.json',
  ending: 'ending.schema.json', assets: 'assets.schema.json', scenario: 'scenario.schema.json',
} as const;
export type DocumentKind = keyof typeof schemaFiles;
export interface SourceLocation { offset: number; line: number; column: number }
export interface RelatedLocation { file: string; pointer: string; line: number; column: number }
export interface Diagnostic extends RelatedLocation {
  code: string;
  severity: 'error' | 'warning';
  message: string;
  relatedIds: string[];
  relatedLocations?: RelatedLocation[];
}
export interface ProjectDocument {
  file: string;
  kind: DocumentKind;
  data: Json;
  version: string;
  locations: ReadonlyMap<string, SourceLocation>;
}
export interface LoadedProject {
  root: string;
  documents: ProjectDocument[];
  diagnostics: Diagnostic[];
}
export interface ValidationResult {
  valid: boolean;
  diagnostics: Diagnostic[];
  documents: ProjectDocument[];
}

export const escapePointer = (key: string): string => key.replace(/~/g, '~0').replace(/\//g, '~1');
export const childPointer = (parent: string, key: string | number): string => `${parent}/${escapePointer(String(key))}`;
export function locationFor(doc: ProjectDocument, pointer: string): SourceLocation {
  let current = pointer;
  while (!doc.locations.has(current) && current !== '') current = current.slice(0, current.lastIndexOf('/'));
  return doc.locations.get(current) ?? { offset: 0, line: 1, column: 1 };
}
export function diagnostic(doc: ProjectDocument, pointer: string, code: string, message: string,
  relatedIds: string[] = [], severity: 'error' | 'warning' = 'error'): Diagnostic {
  const { line, column } = locationFor(doc, pointer);
  return { code, severity, file: doc.file, pointer, line, column, relatedIds, message };
}
export function sortDiagnostics(items: Diagnostic[]): Diagnostic[] {
  const compare = (a: string, b: string) => a < b ? -1 : a > b ? 1 : 0;
  return items.sort((a, b) => compare(a.file, b.file) || a.line - b.line || a.column - b.column || compare(a.code, b.code) || compare(a.pointer, b.pointer));
}

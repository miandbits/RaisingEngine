import { childPointer, diagnostic, isObject, type Diagnostic, type Json, type JsonObject, type ProjectDocument } from './model.js';

export interface Entry { id: string; doc: ProjectDocument; pointer: string; data: JsonObject }
export type Index = Map<string, Entry[]>;
export const objectItems = (value: Json | undefined): JsonObject[] => Array.isArray(value) ? value.filter(isObject) : [];
export const idOf = (data: JsonObject): string => data.id as string;

export class SemanticContext {
  diagnostics: Diagnostic[] = [];
  attributes: Index = new Map();
  flags: Index = new Map();
  characters: Index = new Map();
  activities: Index = new Map();
  events: Index = new Map();
  endings: Index = new Map();
  assets: Index = new Map();
  scenarios: Index = new Map();
  relationships = new Map<Entry, Index>();

  report(doc: ProjectDocument, pointer: string, code: string, message: string, ids: string[] = [], severity: 'error' | 'warning' = 'error'): void {
    this.diagnostics.push(diagnostic(doc, pointer, code, message, ids, severity));
  }

  add(index: Index, entry: Entry, label: string, field = 'id'): void {
    const prior = index.get(entry.id) ?? [];
    if (prior.length) {
      const first = prior[0]!;
      const issue = diagnostic(entry.doc, childPointer(entry.pointer, field), 'ID_DUPLICATE', `${label} ID '${entry.id}'가 중복되었습니다.`, [entry.id]);
      const previous = diagnostic(first.doc, childPointer(first.pointer, field), '', '');
      issue.relatedLocations = [{ file: previous.file, pointer: previous.pointer, line: previous.line, column: previous.column }];
      this.diagnostics.push(issue);
    }
    prior.push(entry);
    index.set(entry.id, prior);
  }

  resolve(index: Index, id: string, doc: ProjectDocument, pointer: string, label: string): Entry | undefined {
    const matches = index.get(id);
    if (!matches?.length) this.report(doc, pointer, 'REFERENCE_MISSING', `${label} '${id}'를 구조 검사에 통과한 문서에서 찾을 수 없습니다.`, [id]);
    else if (matches.length > 1) this.report(doc, pointer, 'REFERENCE_AMBIGUOUS', `${label} '${id}'의 정의가 중복되어 참조가 모호합니다.`, [id]);
    else return matches[0];
    return undefined;
  }

  addArray(index: Index, doc: ProjectDocument, pointer: string, value: Json | undefined, label: string): void {
    objectItems(value).forEach((data, i) => this.add(index, { id: idOf(data), doc, pointer: childPointer(pointer, i), data }, label));
  }

  checkInitial(entry: Entry, value: number, doc: ProjectDocument, pointer: string): void {
    const min = entry.data.min as number, max = entry.data.max as number;
    if (min <= max && (value < min || value > max)) this.report(doc, pointer, 'INITIAL_OUT_OF_RANGE', `초기값 ${value}은 '${entry.id}'의 범위 ${min}..${max} 밖입니다.`, [entry.id]);
  }

  checkRange(entry: Entry): void {
    const min = entry.data.min as number, max = entry.data.max as number;
    if (min > max) this.report(entry.doc, `${entry.pointer}/max`, 'RANGE_ORDER', `최댓값 ${max}은 최솟값 ${min} 이상이어야 합니다.`, [entry.id]);
    this.checkInitial(entry, entry.data.initial as number, entry.doc, `${entry.pointer}/initial`);
  }
}

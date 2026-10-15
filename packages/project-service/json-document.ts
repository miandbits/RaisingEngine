import { parseTree, printParseErrorCode, type Node, type ParseError } from 'jsonc-parser';
import { createHash } from 'node:crypto';
import { childPointer, type Diagnostic, type DocumentKind, type Json, type ProjectDocument, type SourceLocation } from '../validation/model.js';

export interface ParseResult { document?: ProjectDocument; diagnostics: Diagnostic[] }

/**
 * Source locations computed on first use. A document the editor serialized itself (JSON.stringify)
 * cannot have syntax errors or duplicate keys, so only a diagnostic that needs a line/column pays
 * for the full tree walk; on a large plugin document (a 2.7 MiB rig) that walk dominated every edit.
 * A real Map (entries filled in when built) with only #private state: documents stay
 * structuredClone-safe (callers clone loaded projects; a clone is a plain Map of the entries built
 * so far, so callers that need exact locations share this object instead, see buildSnapshot).
 */
class LazyLocations extends Map<string, SourceLocation> {
  #built = false;
  readonly #file: string;
  readonly #kind: DocumentKind;
  readonly #source: string;
  constructor(file: string, kind: DocumentKind, source: string) { super(); this.#file = file; this.#kind = kind; this.#source = source; }
  protected get source(): string { return this.#source; }
  protected get built(): boolean { return this.#built; }
  protected build(): this {
    if (!this.#built) {
      this.#built = true;
      parseDocument(this.#file, this.#kind, this.#source).document?.locations.forEach((location, pointer) => super.set(pointer, location));
    }
    return this;
  }
  override get size(): number { return this.build() && super.size; }
  override get(key: string): SourceLocation | undefined { return this.build() && super.get(key); }
  override has(key: string): boolean { return this.build() && super.has(key); }
  override forEach(callback: (value: SourceLocation, key: string, map: Map<string, SourceLocation>) => void, thisArg?: unknown): void { this.build(); super.forEach(callback, thisArg); }
  override entries(): MapIterator<[string, SourceLocation]> { return this.build() && super.entries(); }
  override keys(): MapIterator<string> { return this.build() && super.keys(); }
  override values(): MapIterator<SourceLocation> { return this.build() && super.values(); }
  override [Symbol.iterator](): MapIterator<[string, SourceLocation]> { return this.build() && super[Symbol.iterator](); }
}
/**
 * Locations in the editor's own serialization (JSON.stringify(value, null, 2) + newline) computed
 * from the value instead of the text: every array element / object member starts its own line,
 * so a node's line is its container's line + 1 + the lines of the siblings before it, and its column
 * is the indent (+ the quoted key and ": " for members). One diagnostic on a 2.7 MiB rig resolves
 * a path in microseconds instead of walking ~100k nodes; iteration falls back to the full map.
 * The per-node caches are #private WeakMaps: never own properties a structured clone could reach.
 */
class PrettyLocations extends LazyLocations {
  #value: Json | undefined;
  #lineStarts: number[] | null = null;
  readonly #lines = new WeakMap<object, number>();
  readonly #before = new WeakMap<object, number[]>();
  readonly #memberIndex = new WeakMap<object, Map<string, number>>();
  #linesOf(v: Json): number {
    if (v === null || typeof v !== 'object') return 1;
    let n = this.#lines.get(v);
    if (n === undefined) {
      const children = Array.isArray(v) ? v : Object.values(v);
      n = children.length ? 2 : 1;
      for (const c of children) n += this.#linesOf(c);
      this.#lines.set(v, n);
    }
    return n;
  }
  #beforeOf(v: object): number[] {
    let prefix = this.#before.get(v);
    if (!prefix) {
      prefix = [];
      let sum = 0;
      for (const c of Array.isArray(v) ? v as Json[] : Object.values(v) as Json[]) { prefix.push(sum); sum += this.#linesOf(c); }
      this.#before.set(v, prefix);
    }
    return prefix;
  }
  #resolve(pointer: string): SourceLocation | undefined {
    // A private parse of the same text: validators may mutate the document's own data.
    this.#value ??= JSON.parse(this.source) as Json;
    let node: Json = this.#value, line = 1, column = 1, depth = 0;
    if (pointer !== '') {
      if (!pointer.startsWith('/')) return undefined;
      for (const raw of pointer.slice(1).split('/')) {
        const part = raw.replace(/~1/g, '/').replace(/~0/g, '~');
        if (Array.isArray(node)) {
          if (!/^(0|[1-9][0-9]*)$/.test(part) || Number(part) >= node.length) return undefined;
          line += 1 + this.#beforeOf(node)[Number(part)]!;
          column = 2 * (depth + 1) + 1;
          node = node[Number(part)]!;
        } else if (node !== null && typeof node === 'object') {
          if (!Object.hasOwn(node, part)) return undefined;
          let index = this.#memberIndex.get(node);
          if (!index) this.#memberIndex.set(node, index = new Map(Object.keys(node).map((k, i) => [k, i])));
          line += 1 + this.#beforeOf(node)[index.get(part)!]!;
          column = 2 * (depth + 1) + JSON.stringify(part).length + 3;
          node = node[part]!;
        } else return undefined;
        depth++;
      }
    }
    if (!this.#lineStarts) {
      this.#lineStarts = [0];
      for (let at = this.source.indexOf('\n'); at >= 0; at = this.source.indexOf('\n', at + 1)) this.#lineStarts.push(at + 1);
    }
    return { offset: (this.#lineStarts[line - 1] ?? 0) + column - 1, line, column };
  }
  // Point lookups never build the full map; once iteration built it, its entries answer directly.
  override get(key: string): SourceLocation | undefined { return this.built ? super.get(key) : this.#resolve(key); }
  override has(key: string): boolean { return this.built ? super.has(key) : this.#resolve(key) !== undefined; }
}
const MAX_DEPTH = 128;
function deeperThan(value: Json, limit: number): boolean {
  const pending: [unknown, number][] = [[value, 0]];
  while (pending.length) {
    const [node, depth] = pending.pop()!;
    if (!node || typeof node !== 'object') continue;
    if (depth >= limit) return true;
    for (const child of Array.isArray(node) ? node : Object.values(node)) if (child && typeof child === 'object') pending.push([child, depth + 1]);
  }
  return false;
}

export interface ParseOptions {
  /** `source` is exactly JSON.stringify(value, null, 2) + newline (the editor's own serialization):
   *  parsed with JSON.parse, locations computed from the value on demand. Falls back to the full
   *  parse for anything unexpected. */
  serialized?: boolean;
}
export function parseDocument(file: string, kind: DocumentKind, source: string, options: ParseOptions = {}): ParseResult {
  if (options.serialized) {
    try {
      const data = JSON.parse(source) as Json;
      // Same depth limit as below; deeper documents take the full path for its exact diagnostics.
      if (data !== null && typeof data === 'object' && !deeperThan(data, MAX_DEPTH))
        return { document: { file, kind, data, version: createHash('sha256').update(source).digest('hex'), locations: new PrettyLocations(file, kind, source) }, diagnostics: [] };
    } catch { /* not plain JSON after all: full parse below reports it */ }
  }
  const diagnostics: Diagnostic[] = [];
  const lineStarts = [0];
  for (let i = 0; i < source.length; i++) if (source[i] === '\n') lineStarts.push(i + 1);
  const locate = (offset: number): SourceLocation => {
    let low = 0, high = lineStarts.length;
    while (low + 1 < high) {
      const mid = (low + high) >>> 1;
      if (lineStarts[mid]! <= offset) low = mid; else high = mid;
    }
    return { offset, line: low + 1, column: offset - lineStarts[low]! + 1 };
  };
  const report = (code: string, pointer: string, offset: number, message: string): void => {
    const { line, column } = locate(offset);
    diagnostics.push({ code, severity: 'error', file, pointer, line, column, relatedIds: [], message });
  };
  const errors: ParseError[] = [];
  let tree: Node | undefined;
  try {
    tree = parseTree(source, errors, { disallowComments: true, allowTrailingComma: false, allowEmptyContent: false });
  } catch {
    report('JSON_PARSE_LIMIT', '', 0, 'JSON을 파싱할 수 없습니다. 중첩 깊이와 문서 크기를 확인하세요.');
    return { diagnostics };
  }
  if (errors.length || !tree) {
    for (const error of errors.slice(0, 20)) report('JSON_SYNTAX', '', error.offset, `JSON 문법 오류: ${printParseErrorCode(error.error)}`);
    if (!errors.length) report('JSON_SYNTAX', '', 0, '빈 JSON 문서입니다.');
    return { diagnostics };
  }
  const locations = new Map<string, SourceLocation>();
  const pending: { node: Node; pointer: string; depth: number }[] = [{ node: tree, pointer: '', depth: 0 }];
  while (pending.length) {
    const { node, pointer, depth } = pending.pop()!;
    locations.set(pointer, locate(node.offset));
    if (depth > MAX_DEPTH) {
      report('JSON_DEPTH_LIMIT', pointer, node.offset, 'JSON 중첩은 128단계를 넘을 수 없습니다.');
      continue;
    }
    if (node.type === 'object') {
      const seen = new Map<string, SourceLocation>();
      for (const property of node.children ?? []) {
        const [key, value] = property.children ?? [];
        if (!key || !value) continue;
        const name = String(key.value);
        const next = childPointer(pointer, name);
        const first = seen.get(name);
        if (first) {
          report('JSON_DUPLICATE_KEY', next, key.offset, `중복 JSON 키 '${name}'입니다. 첫 선언: ${first.line}:${first.column}`);
        } else seen.set(name, locate(key.offset));
        pending.push({ node: value, pointer: next, depth: depth + 1 });
      }
    } else if (node.type === 'array') {
      for (const [i, value] of (node.children ?? []).entries()) pending.push({ node: value, pointer: childPointer(pointer, i), depth: depth + 1 });
    }
  }
  if (diagnostics.length) return { diagnostics };
  return { document: { file, kind, data: JSON.parse(source) as Json, version: createHash('sha256').update(source).digest('hex'), locations }, diagnostics };
}

import { parseTree, printParseErrorCode, type Node, type ParseError } from 'jsonc-parser';
import { createHash } from 'node:crypto';
import { childPointer, type Diagnostic, type DocumentKind, type Json, type ProjectDocument, type SourceLocation } from '../validation/model.js';

export interface ParseResult { document?: ProjectDocument; diagnostics: Diagnostic[] }

export function parseDocument(file: string, kind: DocumentKind, source: string): ParseResult {
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
    if (depth > 128) {
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

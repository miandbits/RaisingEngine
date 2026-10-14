import { open, readdir, realpath, stat } from 'node:fs/promises';
import path from 'node:path';
import { parseDocument } from './json-document.js';
import { sortDiagnostics, type Diagnostic, type DocumentKind, type LoadedProject } from '../validation/model.js';

const MAX_DOCUMENT_BYTES = 2 * 1024 * 1024;
const MAX_DOCUMENTS = 10000;
export function isWithin(root: string, target: string): boolean {
  const relative = path.relative(root, target);
  return relative === '' || (!relative.startsWith(`..${path.sep}`) && relative !== '..' && !path.isAbsolute(relative));
}
function ioDiagnostic(file: string, code: string, message: string): Diagnostic {
  return { code, severity: 'error', file, pointer: '', line: 1, column: 1, message, relatedIds: [] };
}
const errorCode = (error: unknown): string => (error as NodeJS.ErrnoException).code ?? 'UNKNOWN';

export async function loadProject(inputPath: string): Promise<LoadedProject> {
  const result: LoadedProject = { root: path.resolve(inputPath), documents: [], diagnostics: [] };
  try {
    result.root = await realpath(result.root);
    if (!(await stat(result.root)).isDirectory()) throw new Error('not a directory');
  } catch (error) {
    result.diagnostics.push(ioDiagnostic('.', 'PROJECT_DIRECTORY', `프로젝트 폴더가 없거나 읽을 수 없습니다: ${errorCode(error)}`));
    return result;
  }
  const readDocument = async (file: string, kind: DocumentKind): Promise<void> => {
    if (result.documents.length >= MAX_DOCUMENTS) {
      result.diagnostics.push(ioDiagnostic(file, 'PROJECT_SIZE_LIMIT', `프로젝트 문서는 ${MAX_DOCUMENTS}개를 넘을 수 없습니다.`));
      return;
    }
    let handle;
    try {
      const resolved = await realpath(path.join(result.root, file));
      if (!isWithin(result.root, resolved)) {
        result.diagnostics.push(ioDiagnostic(file, 'PATH_OUTSIDE_PROJECT', '문서가 프로젝트 밖을 가리킵니다.'));
        return;
      }
      handle = await open(resolved, 'r');
      const info = await handle.stat();
      if (!info.isFile()) {
        result.diagnostics.push(ioDiagnostic(file, 'DOCUMENT_NOT_FILE', 'JSON 문서 경로가 일반 파일이 아닙니다.'));
        return;
      }
      if (info.size > MAX_DOCUMENT_BYTES) {
        result.diagnostics.push(ioDiagnostic(file, 'DOCUMENT_SIZE_LIMIT', 'JSON 문서는 2 MiB 이하이어야 합니다.'));
        return;
      }
      const bytes = await handle.readFile();
      if (bytes.length > MAX_DOCUMENT_BYTES) {
        result.diagnostics.push(ioDiagnostic(file, 'DOCUMENT_SIZE_LIMIT', 'JSON 문서는 2 MiB 이하이어야 합니다.'));
        return;
      }
      let source: string;
      try { source = new TextDecoder('utf-8', { fatal: true }).decode(bytes); }
      catch {
        result.diagnostics.push(ioDiagnostic(file, 'JSON_ENCODING', 'JSON 문서는 올바른 UTF-8이어야 합니다.'));
        return;
      }
      const parsed = parseDocument(file, kind, source);
      result.diagnostics.push(...parsed.diagnostics);
      if (parsed.document) result.documents.push(parsed.document);
    } catch (error) {
      result.diagnostics.push(ioDiagnostic(file, errorCode(error) === 'ENOENT' ? 'DOCUMENT_MISSING' : 'DOCUMENT_READ', `문서를 읽을 수 없습니다: ${errorCode(error)}`));
    } finally { await handle?.close(); }
  };
  for (const [file, kind] of [['project.json', 'project'], ['attributes.json', 'attributes'], ['flags.json', 'flags'], ['assets/manifest.json', 'assets']] as const) {
    await readDocument(file, kind);
  }
  for (const [folder, kind] of [['characters', 'character'], ['activities', 'activity'], ['events', 'event'], ['endings', 'ending'], ['scenarios', 'scenario']] as const) {
    try {
      const resolved = await realpath(path.join(result.root, folder));
      if (!isWithin(result.root, resolved)) {
        result.diagnostics.push(ioDiagnostic(folder, 'PATH_OUTSIDE_PROJECT', '콘텐츠 폴더가 프로젝트 밖을 가리킵니다.'));
        continue;
      }
      const names = (await readdir(resolved)).filter((name) => name.toLowerCase().endsWith('.json')).sort();
      for (const name of names) await readDocument(`${folder}/${name}`, kind);
    } catch (error) {
      if (errorCode(error) !== 'ENOENT') result.diagnostics.push(ioDiagnostic(folder, 'DIRECTORY_READ', `콘텐츠 폴더를 읽을 수 없습니다: ${errorCode(error)}`));
    }
  }
  sortDiagnostics(result.diagnostics);
  return result;
}

import { open, readdir, realpath, stat } from 'node:fs/promises';
import path from 'node:path';
import { parseDocument } from './json-document.js';
import { sortDiagnostics, type Diagnostic, type DocumentKind, type LoadedProject } from '../validation/model.js';

const MAX_DOCUMENT_BYTES = 2 * 1024 * 1024;
/**
 * Plugin documents (kind `extension`) may be larger: a big animation rig (200 bones, 40 meshes,
 * 9000 keys) is ~0.8 MiB compact but ~2.7 MiB in the editor's indented form. Shared by the loader
 * and the editor's edit checks so a rig that can be edited can always be saved and reloaded.
 */
export const MAX_EXTENSION_DOCUMENT_BYTES = 8 * 1024 * 1024;
export function maxDocumentBytes(kind: DocumentKind | string | null | undefined): number { return kind === 'extension' ? MAX_EXTENSION_DOCUMENT_BYTES : MAX_DOCUMENT_BYTES; }
export function documentSizeMessage(kind: DocumentKind | string | null | undefined): string { return `JSON 문서는 ${maxDocumentBytes(kind) / (1024 * 1024)} MiB 이하이어야 합니다.`; }
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
      if (info.size > maxDocumentBytes(kind)) {
        result.diagnostics.push(ioDiagnostic(file, 'DOCUMENT_SIZE_LIMIT', documentSizeMessage(kind)));
        return;
      }
      const bytes = await handle.readFile();
      if (bytes.length > maxDocumentBytes(kind)) {
        result.diagnostics.push(ioDiagnostic(file, 'DOCUMENT_SIZE_LIMIT', documentSizeMessage(kind)));
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
  await readPlugins(result, readDocument);
  sortDiagnostics(result.diagnostics);
  return result;
}

const PLUGIN_SEGMENT = /^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/;
/**
 * plugins/lock.json and plugins/<plugin>/<type>/<id>.json for ANY plugin id, installed or
 * not: documents of unavailable plugins must still round-trip byte-for-byte. Entries that
 * could never be saved back (bad names, deeper nesting) are reported and left untouched.
 */
async function readPlugins(result: LoadedProject, readDocument: (file: string, kind: DocumentKind) => Promise<void>): Promise<void> {
  const list = async (folder: string) => {
    const resolved = await realpath(path.join(result.root, folder));
    if (!isWithin(result.root, resolved)) {
      result.diagnostics.push(ioDiagnostic(folder, 'PATH_OUTSIDE_PROJECT', '플러그인 폴더가 프로젝트 밖을 가리킵니다.'));
      return null;
    }
    return (await readdir(resolved, { withFileTypes: true })).sort((a, b) => a.name < b.name ? -1 : a.name > b.name ? 1 : 0);
  };
  const skipped = (file: string, message: string) => result.diagnostics.push({ ...ioDiagnostic(file, 'EXTENSION_PATH', message), severity: 'warning' });
  const folderOf = (entry: { isDirectory(): boolean; isSymbolicLink(): boolean }) => entry.isDirectory() || entry.isSymbolicLink();
  let plugins;
  try { plugins = await list('plugins'); }
  catch (error) {
    if (errorCode(error) !== 'ENOENT') result.diagnostics.push(ioDiagnostic('plugins', 'DIRECTORY_READ', `플러그인 폴더를 읽을 수 없습니다: ${errorCode(error)}`));
    return;
  }
  if (!plugins) return;
  if (plugins.some(entry => entry.name === 'lock.json')) await readDocument('plugins/lock.json', 'pluginLock');
  for (const plugin of plugins) {
    if (plugin.name === 'lock.json') continue;
    const folder = `plugins/${plugin.name}`;
    if (!folderOf(plugin)) { if (plugin.name.toLowerCase().endsWith('.json')) skipped(folder, '플러그인 폴더에는 lock.json과 플러그인별 폴더만 둘 수 있습니다. 읽지 않았습니다.'); continue; }
    if (!PLUGIN_SEGMENT.test(plugin.name) || plugin.name.length > 64) { skipped(folder, '플러그인 폴더 이름은 소문자 영문·숫자·하이픈이어야 합니다. 읽지 않았습니다.'); continue; }
    let types;
    try { types = await list(folder); } catch (error) { result.diagnostics.push(ioDiagnostic(folder, 'DIRECTORY_READ', `플러그인 폴더를 읽을 수 없습니다: ${errorCode(error)}`)); continue; }
    for (const type of types ?? []) {
      const typeFolder = `${folder}/${type.name}`;
      if (!folderOf(type)) { if (type.name.toLowerCase().endsWith('.json')) skipped(typeFolder, '플러그인 문서는 plugins/<플러그인>/<종류>/<ID>.json 위치에 있어야 합니다. 읽지 않았습니다.'); continue; }
      if (!PLUGIN_SEGMENT.test(type.name) || type.name.length > 64) { skipped(typeFolder, '문서 종류 폴더 이름은 소문자 영문·숫자·하이픈이어야 합니다. 읽지 않았습니다.'); continue; }
      let files;
      try { files = await list(typeFolder); } catch (error) { result.diagnostics.push(ioDiagnostic(typeFolder, 'DIRECTORY_READ', `플러그인 폴더를 읽을 수 없습니다: ${errorCode(error)}`)); continue; }
      for (const file of files ?? []) {
        if (!file.name.toLowerCase().endsWith('.json')) continue;
        const name = `${typeFolder}/${file.name}`, id = file.name.slice(0, -5);
        if (!file.name.endsWith('.json') || !PLUGIN_SEGMENT.test(id) || id.length > 64) { skipped(name, '플러그인 문서 파일 이름은 <소문자 ID>.json이어야 합니다. 읽지 않았습니다.'); continue; }
        await readDocument(name, 'extension');
      }
    }
  }
}

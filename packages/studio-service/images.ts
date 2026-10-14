import { createHash } from 'node:crypto';
import { readFile, realpath, stat, writeFile } from 'node:fs/promises';
import path from 'node:path';
import type { StudioEditor } from './editor.js';

/** Import immutable PNG bytes, then register them through the shared draft/history. */
export async function importPng(editor: StudioEditor, source: string, revision: number): Promise<string> {
  editor.current(revision);
  if (!editor.root || editor.readOnly) throw new Error('편집 가능한 프로젝트를 열어 주세요.');
  if (path.extname(source).toLowerCase() !== '.png' || (await stat(source)).size > 20 * 1024 * 1024)
    throw new Error('20 MiB 이하의 PNG 이미지를 선택하세요.');
  const bytes = await readFile(source);
  if (bytes.length < 45 || bytes.length > 20 * 1024 * 1024 || !bytes.subarray(0, 8).equals(Buffer.from([137,80,78,71,13,10,26,10])) || bytes.toString('ascii',12,16) !== 'IHDR' || bytes.toString('ascii',bytes.length-8,bytes.length-4) !== 'IEND')
    throw new Error('올바른 PNG 파일이 아닙니다.');
  const width = bytes.readUInt32BE(16), height = bytes.readUInt32BE(20);
  if (!width || !height || width > 8192 || height > 8192 || width * height > 32 * 1024 * 1024)
    throw new Error('PNG 크기는 8192px 및 32메가픽셀 이하여야 합니다.');
  const root = await realpath(editor.root), directory = await realpath(path.join(root, 'assets'));
  const relative = path.relative(root, directory);
  if (relative.startsWith('..') || path.isAbsolute(relative)) throw new Error('프로젝트 밖의 에셋 경로입니다.');
  const id = `image-${createHash('sha256').update(bytes).digest('hex').slice(0, 24)}`;
  const manifest = editor.documents.find(d => d.kind === 'assets');
  if (!manifest) throw new Error('에셋 목록이 없습니다.');
  const data = manifest.data as { assets: { id: string; kind: string; path: string }[] };
  const assetPath = `assets/${id}.png`, existing = data.assets.find(a => a.id === id);
  editor.current(revision);
  if (existing) {
    const actual = await realpath(path.resolve(root, existing.path));
    const rel = path.relative(root, actual);
    if (existing.kind !== 'image' || rel.startsWith('..') || path.isAbsolute(rel) || !(await readFile(actual)).equals(bytes)) throw new Error('기존 에셋 ID가 다른 파일을 가리킵니다.');
    editor.current(revision);
    return id;
  }
  const destination = path.join(directory, `${id}.png`);
  try { await writeFile(destination, bytes, { flag: 'wx' }); }
  catch (error) {
    if ((error as NodeJS.ErrnoException).code !== 'EEXIST') throw error;
    if (path.dirname(await realpath(destination)) !== directory || !(await readFile(destination)).equals(bytes)) throw new Error('같은 이름의 다른 에셋 파일이 있습니다.');
  }
  editor.current(revision);
  editor.edit({type:'replace',file:manifest.file,data:{...data,assets:[...data.assets,{id,kind:'image',path:assetPath}]}},revision);
  return id;
}

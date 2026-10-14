import { createHash, randomUUID } from 'node:crypto';
import { mkdir, open, readFile, realpath, rename, unlink } from 'node:fs/promises';
import path from 'node:path';

export const fileHash = (bytes: Uint8Array | string): string => createHash('sha256').update(bytes).digest('hex');
const allowed = /^(?:project\.json|attributes\.json|flags\.json|assets\/manifest\.json|(?:characters|activities|events|endings|scenarios)\/[a-zA-Z0-9_-]+\.json)$/;
export async function safeFile(root: string, file: string): Promise<string> {
  if (!allowed.test(file)) throw new Error(`허용되지 않은 문서 경로: ${file}`);
  const parent = path.dirname(path.join(root, file));
  await mkdir(parent, { recursive: true });
  const resolved = await realpath(parent);
  const relative = path.relative(root, resolved);
  if (relative.startsWith('..') || path.isAbsolute(relative)) throw new Error(`프로젝트 밖의 경로: ${file}`);
  const target = path.join(resolved, path.basename(file));
  try {
    if ((await realpath(target)).toLowerCase() !== target.toLowerCase()) throw new Error(`링크 문서는 저장할 수 없습니다: ${file}`);
  } catch (error) { if ((error as NodeJS.ErrnoException).code !== 'ENOENT') throw error; }
  return target;
}
async function studioDirectory(root: string): Promise<string> {
  const directory = path.join(root, '.studio');
  await mkdir(directory, { recursive: true });
  if ((await realpath(directory)).toLowerCase() !== directory.toLowerCase()) throw new Error('.studio는 링크 폴더일 수 없습니다.');
  return directory;
}
export async function bytesOrNull(file: string): Promise<Buffer | null> {
  try { return await readFile(file); } catch (error) { if ((error as NodeJS.ErrnoException).code === 'ENOENT') return null; throw error; }
}
async function replace(file: string, bytes: Buffer | null): Promise<void> {
  if (bytes === null) { await unlink(file).catch(error => { if (error.code !== 'ENOENT') throw error; }); return; }
  const temporary = `${file}.${randomUUID()}.tmp`;
  try {
    const handle = await open(temporary, 'wx');
    try { await handle.writeFile(bytes); await handle.sync(); } finally { await handle.close(); }
    await rename(temporary, file);
  } finally { await unlink(temporary).catch(() => {}); }
}
interface Entry { file: string; before: string | null; after: string | null }
interface Journal { version: 1; pid: number; entries: Entry[] }
const decode = (value: string | null) => value === null ? null : Buffer.from(value, 'base64');
const equal = (a: Buffer | null, b: Buffer | null) => a === null || b === null ? a === b : a.equals(b);

/** Roll back an interrupted transaction only when every file still has known bytes. */
export async function recoverProject(root: string, own = false): Promise<boolean> {
  const directory = await studioDirectory(root);
  const file = path.join(directory, 'pending-save.json');
  const bytes = await bytesOrNull(file);
  if (!bytes) return false;
  const journal = JSON.parse(bytes.toString('utf8')) as Journal;
  if (journal.version !== 1 || !Number.isInteger(journal.pid) || journal.pid <= 0 || !Array.isArray(journal.entries) || journal.entries.length > 10000
    || journal.entries.some(e => !e || typeof e.file !== 'string' || !allowed.test(e.file)
      || (e.before !== null && typeof e.before !== 'string') || (e.after !== null && typeof e.after !== 'string'))
    || new Set(journal.entries.map(e => e.file)).size !== journal.entries.length) throw new Error('저장 복구 기록이 올바르지 않습니다. .studio/pending-save.json을 확인하세요.');
  if (!own) {
    try { process.kill(journal.pid, 0); throw new Error('다른 편집기가 저장 중입니다. 작업을 마친 뒤 다시 열어 주세요.'); }
    catch (error) { if ((error as NodeJS.ErrnoException).code !== 'ESRCH') throw error; }
  }
  for (const entry of journal.entries) {
    const current = await bytesOrNull(await safeFile(root, entry.file));
    if (!equal(current, decode(entry.before)) && !equal(current, decode(entry.after))) {
      throw new Error(`복구 충돌: ${entry.file}. 외부 변경을 보존했습니다. .studio/pending-save.json에 이전 내용이 남아 있습니다.`);
    }
  }
  for (const entry of [...journal.entries].reverse()) await replace(await safeFile(root, entry.file), decode(entry.before));
  await unlink(file);
  return true;
}

export async function commitProject(root: string, changes: { file: string; expected: string | null; text: string | null }[],
  verifyReadSet?: () => Promise<void>): Promise<void> {
  const directory = await studioDirectory(root);
  const journalFile = path.join(directory, 'pending-save.json');
  // wx is also the cooperative cross-process save lock.
  const lock = await open(journalFile, 'wx');
  let written = false;
  let closed = false;
  try {
    // Check ALL source documents after acquiring the same lock used by every writer.
    // Checking only changed files would miss concurrent changes to dependencies.
    await verifyReadSet?.();
    const entries: Entry[] = [];
    for (const change of changes) {
      const before = await bytesOrNull(await safeFile(root, change.file));
      if ((before ? fileHash(before) : null) !== change.expected) throw new Error(`외부 변경 충돌: ${change.file}`);
      entries.push({ file: change.file, before: before?.toString('base64') ?? null,
        after: change.text === null ? null : Buffer.from(change.text).toString('base64') });
    }
    await lock.writeFile(JSON.stringify({ version: 1, pid: process.pid, entries } satisfies Journal));
    await lock.sync(); await lock.close(); closed = true; written = true;
    for (const entry of entries) {
      const target = await safeFile(root, entry.file);
      if (!equal(await bytesOrNull(target), decode(entry.before))) throw new Error(`외부 변경 충돌: ${entry.file}`);
      await replace(target, decode(entry.after));
    }
    await unlink(journalFile);
  } catch (error) {
    if (!closed) await lock.close();
    if (written) {
      try { await recoverProject(root, true); }
      catch (recovery) { throw new Error(`${String(error)}\n${String(recovery)}`); }
    } else await unlink(journalFile).catch(() => {});
    throw error;
  }
}

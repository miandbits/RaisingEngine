import { createHash, randomUUID } from 'node:crypto';
import { mkdir, readFile, rename, unlink, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { loadProject } from '../project-service/loader.js';

/*
 * Asset files that an import publishes before the draft that references them is saved (PNG,
 * sprite frames, fonts, plugin pictures). Publishing first keeps a save atomic, but a discarded
 * draft used to leave the files behind. Each new file is recorded in .studio/unsaved-assets.json
 * with its content hash and the owning editor process; when that editor closes the project (or
 * on the next open, after a crash) every recorded file that the saved asset manifest does not
 * list is removed again. A file is only removed when its bytes are still exactly the ones that
 * were published and no live editor process still owns it, so another editor's draft and a file
 * the user replaced by hand are never touched. Losing a record only leaves a file behind.
 */
const JOURNAL = '.studio/unsaved-assets.json';
interface Entry { path: string; sha256: string; owners: number[] }
interface Journal { version: 1; entries: Entry[] }

const sha256 = (bytes: Uint8Array) => createHash('sha256').update(bytes).digest('hex');
const alive = (pid: number) => {
  if (pid === process.pid) return true;
  try { process.kill(pid, 0); return true; } catch (error) { return (error as NodeJS.ErrnoException).code === 'EPERM'; }
};

async function readJournal(root: string): Promise<Journal> {
  try {
    const data = JSON.parse(await readFile(path.join(root, JOURNAL), 'utf8')) as Journal;
    if (data?.version !== 1 || !Array.isArray(data.entries)) return { version: 1, entries: [] };
    const entries = data.entries.filter(e => typeof e?.path === 'string' && /^[0-9a-f]{64}$/.test(String(e.sha256)) && Array.isArray(e.owners))
      .map(e => ({ path: e.path, sha256: e.sha256, owners: e.owners.filter(o => Number.isInteger(o) && o > 0) }));
    return { version: 1, entries };
  } catch { return { version: 1, entries: [] }; }
}
async function writeJournal(root: string, journal: Journal): Promise<void> {
  const file = path.join(root, JOURNAL);
  if (!journal.entries.length) { await unlink(file).catch(() => undefined); return; }
  await mkdir(path.dirname(file), { recursive: true });
  const temporary = `${file}.${randomUUID()}.tmp`;
  await writeFile(temporary, JSON.stringify(journal, null, 2) + '\n');
  await rename(temporary, file);
}
/** Project-relative path with forward slashes, or null outside the project. */
function relativeOf(root: string, file: string): string | null {
  const relative = path.relative(root, path.resolve(root, file));
  return relative.startsWith('..') || path.isAbsolute(relative) ? null : relative.split(path.sep).join('/');
}

/**
 * Records an asset file this process published for a draft. `created` = the file is new (an
 * existing identical file only gains this process as an owner when it is already recorded).
 */
export async function noteUnsavedAsset(root: string, file: string, bytes: Uint8Array, created: boolean): Promise<void> {
  const relative = relativeOf(root, file);
  if (!relative) return;
  try {
    const journal = await readJournal(root), hash = sha256(bytes);
    const entry = journal.entries.find(e => e.path === relative);
    if (entry) { if (entry.sha256 !== hash || entry.owners.includes(process.pid)) return; entry.owners.push(process.pid); }
    else if (created) journal.entries.push({ path: relative, sha256: hash, owners: [process.pid] });
    else return;
    await writeJournal(root, journal);
  } catch { /* The import itself succeeded; without a record the file only stays behind. */ }
}

/**
 * Releases this process's records (`own`) and those of editors that are gone: files the saved
 * asset manifest lists stay; the others are deleted when their bytes are unchanged. Returns the
 * deleted project-relative paths.
 */
export async function releaseUnsavedAssets(root: string): Promise<string[]> {
  const journal = await readJournal(root);
  if (!journal.entries.length) return [];
  let saved: Set<string> | null = null;
  try {
    const loaded = await loadProject(root);
    saved = new Set(loaded.documents.filter(d => d.kind === 'assets')
      .flatMap(d => ((d.data as { assets?: { path?: unknown }[] })?.assets ?? []).map(a => typeof a?.path === 'string' ? relativeOf(root, a.path) : null))
      .filter((p): p is string => !!p));
  } catch { return []; /* An unreadable project keeps every file. */ }
  const removed: string[] = [], kept: Entry[] = [];
  for (const entry of journal.entries) {
    const owners = entry.owners.filter(o => o !== process.pid && alive(o));
    if (owners.length) { kept.push({ ...entry, owners }); continue; }
    if (saved.has(entry.path)) continue;
    const file = path.join(root, entry.path);
    try {
      if (sha256(await readFile(file)) === entry.sha256) { await unlink(file); removed.push(entry.path); }
    } catch { /* Already gone or unreadable: nothing to remove. */ }
  }
  await writeJournal(root, { version: 1, entries: kept });
  return removed;
}

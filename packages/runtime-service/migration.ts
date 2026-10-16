import { createHash, randomUUID } from 'node:crypto';
import { open, link, unlink } from 'node:fs/promises';
import path from 'node:path';
import type { ContentSnapshot, CoreError } from '../core/model.js';
import { canonicalJson } from '../project-service/snapshot.js';
import { createSession, dispatchSession } from './session.js';
import { decodeSave, encodeSave } from './save.js';

export interface SaveChange { path: string; before: unknown; after: unknown }
export interface MigrationReport {
  version: 1; sourceHash: string; sourceFingerprint: string; targetFingerprint: string;
  commands: number; changes: SaveChange[]; truncated: boolean; presentationReset: boolean;
  approval: string;
}
type Failure = { ok: false; error: CoreError };
const fail = (code: string, message: string, location = '/'): Failure => ({ ok: false, error: { code, message, path: location } });
const hash = (text: string) => createHash('sha256').update(text).digest('hex');

/** A bounded report only. Approval hashes the complete resulting save, including omitted differences. */
function differences(before: unknown, after: unknown) {
  const changes: SaveChange[] = []; let truncated = false;
  const walk = (a: any, b: any, pointer: string) => {
    if (canonicalJson(a ?? null) === canonicalJson(b ?? null)) return;
    if (changes.length >= 2000) { truncated = true; return; }
    if (a && b && typeof a === 'object' && typeof b === 'object' && Array.isArray(a) === Array.isArray(b)) {
      for (const key of new Set([...Object.keys(a), ...Object.keys(b)]))
        walk(a[key], b[key], `${pointer}/${key.replace(/~/g, '~0').replace(/\//g, '~1')}`);
    } else changes.push({ path: pointer || '/', before: a ?? null, after: b ?? null });
  };
  walk(before, after, ''); return { changes, truncated };
}

/** Explicit content-update migration: validate against the OLD release, then replay on the new one.
 * Never patches fingerprints or transplants unverified state. Ordinary loading remains strict.
 */
export async function previewSaveMigration(source: ContentSnapshot, target: ContentSnapshot, text: string,
  options: { resetPresentation?: boolean; cancelled?: () => boolean } = {}) {
  if (source.project.gameId !== target.project.gameId) return fail('MIGRATION_GAME', '서로 다른 게임으로 저장을 이전할 수 없습니다.');
  const original = await decodeSave(source, text);
  if (!original.ok) return original;
  if (original.playerPresentation && !options.resetPresentation && source.contentFingerprint !== target.contentFingerprint)
    return fail('MIGRATION_PRESENTATION', '연출·플러그인 상태는 자동 이전하지 않습니다. 연출 초기화를 명시적으로 선택하세요.', '/playerPresentation');
  let replay = createSession(target, original.session.origin.seed, original.session.origin);
  if (!replay.ok) return replay;
  let session = replay.session;
  for (const [index, command] of original.session.commands.entries()) {
    if (index % 32 === 0) { await new Promise<void>(resolve => setImmediate(resolve)); }
    if (options.cancelled?.()) return fail('MIGRATION_CANCELLED', '이전을 취소했습니다.', `/commands/${index}`);
    const result = dispatchSession(target, session, command, session.state.stateRevision);
    if (!result.ok) return fail('MIGRATION_REPLAY', `${result.error.code}: ${result.error.message}`, `/commands/${index}`);
    session = result.session;
  }
  if (options.cancelled?.()) return fail('MIGRATION_CANCELLED', '이전을 취소했습니다.');
  const presentationReset = !!original.playerPresentation && !!options.resetPresentation;
  const encoded = await encodeSave(target, session, presentationReset ? undefined : original.playerPresentation);
  if (!encoded.ok) return encoded;
  const report: MigrationReport = { version: 1, sourceHash: hash(text), sourceFingerprint: source.contentFingerprint,
    targetFingerprint: target.contentFingerprint, commands: session.commands.length,
    ...differences(original.session.state, session.state), presentationReset,
    approval: hash(canonicalJson({ source: hash(text), target: target.contentFingerprint, save: encoded.text, presentationReset })) };
  return { ok: true as const, report, text: encoded.text };
}

/** Publish only to a NEW file; original and existing destinations are never replaced.
 * Hard-link publication is atomic and exclusive on supported local filesystems. Unsupported
 * filesystems fail closed; the temporary file is removed. Keep the old release for recovery.
 */
export async function publishMigratedSave(file: string, source: ContentSnapshot, target: ContentSnapshot,
  originalText: string, approval: string, options: { resetPresentation?: boolean } = {}) {
  const preview = await previewSaveMigration(source, target, originalText, options);
  if (!preview.ok) return preview;
  if (preview.report.approval !== approval) return fail('MIGRATION_STALE', '검토 이후 저장 또는 콘텐츠가 바뀌었습니다. 다시 비교하세요.');
  const destination = path.resolve(file), temporary = `${destination}.${randomUUID()}.tmp`;
  let created = false;
  try {
    const handle = await open(temporary, 'wx'); created = true;
    try { await handle.writeFile(preview.text, 'utf8'); await handle.sync(); } finally { await handle.close(); }
    await link(temporary, destination);
    return { ok: true as const, file: destination, report: preview.report };
  } catch (error) {
    return fail('MIGRATION_IO', `새 저장 파일을 만들지 못했습니다: ${(error as NodeJS.ErrnoException).code ?? 'UNKNOWN'}`, destination);
  } finally { if (created) await unlink(temporary).catch(() => {}); }
}

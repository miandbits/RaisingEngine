import { Ajv2020 } from 'ajv/dist/2020.js';
import { readFile, open, rename, unlink } from 'node:fs/promises';
import { randomUUID } from 'node:crypto';
import path from 'node:path';
import { CORE_VERSION } from '../core/index.js';
import { createRun, dispatchCommand, runtimeBackend } from '../native-bridge/backend.js';
import { nativeReplay } from '../native-bridge/index.js';
import type { ContentSnapshot, CoreError } from '../core/model.js';
import { freeze, object } from '../core/util.js';
import { parseDocument } from '../project-service/json-document.js';
import { canonicalJson } from '../project-service/snapshot.js';
import type { RunSession, SessionResult } from './session.js';
import {PlayerPresentation,type PlayerPresentationEnvelope} from '../player-service/presentation.js';

export const SAVE_VERSION = '1.0.0';
export const MAX_SAVE_BYTES = 16 * 1024 * 1024;
/** Per-plugin opaque presentation state (CONTRACT §8); bounded so animation can never crowd out game state. */
export const MAX_EXTENSION_STATE_BYTES = 1024 * 1024;
export const MAX_EXTENSIONS_BYTES = 4 * 1024 * 1024;
type SaveEnvelope = { saveVersion: typeof SAVE_VERSION; coreVersion: string; contentFingerprint: string; playerPresentation?:PlayerPresentationEnvelope } & RunSession;
export type LoadedSessionResult=SessionResult&{playerPresentation?:PlayerPresentationEnvelope};
type Failure = { ok: false; error: CoreError };
const fail = (code: string, message: string, location = '/'): Failure => ({ ok: false, error: { code, message, path: location } });
let validator: Promise<ReturnType<Ajv2020['compile']>> | undefined;
function getValidator() {
  return validator ??= (async () => {
    const [common, schema] = await Promise.all([
      readFile(new URL('../schema/common.schema.json', import.meta.url), 'utf8'),
      readFile(new URL('./save.schema.json', import.meta.url), 'utf8'),
    ]);
    const ajv = new Ajv2020({ strict: true, allErrors: true });
    ajv.addSchema(JSON.parse(common));
    return ajv.compile(JSON.parse(schema));
  })();
}

/** Replay is validation only: discarded transitions must not be delivered as new presentation cues. */
async function restore(content: ContentSnapshot, value: unknown): Promise<LoadedSessionResult> {
  if (object(value) && typeof value.saveVersion === 'string' && value.saveVersion !== SAVE_VERSION)
    return fail('SAVE_VERSION', `지원하지 않는 세이브 버전: ${value.saveVersion}`, '/saveVersion');
  if (object(value) && typeof value.coreVersion === 'string' && value.coreVersion !== CORE_VERSION)
    return fail('SAVE_CORE_VERSION', `코어 ${CORE_VERSION}과 세이브의 실행 규칙이 다릅니다.`, '/coreVersion');
  const validate = await getValidator();
  if (!validate(value)) {
    const issue = validate.errors?.[0];
    return fail('SAVE_SCHEMA', `세이브 구조 오류: ${issue?.message ?? 'unknown'}`, issue?.instancePath || '/');
  }
  const saved = value as SaveEnvelope;
  if (saved.contentFingerprint !== content.contentFingerprint)
    return fail('CONTENT_MISMATCH', '세이브가 다른 콘텐츠로 만들어졌습니다.', '/contentFingerprint');
  const created = runtimeBackend()==='native' ? nativeReplay(content,saved.origin,saved.commands) : createRun(content, saved.origin.seed, saved.origin);
  if (!created.ok) return created;
  let state = created.state;
  for (const [i, command] of (runtimeBackend()==='native'?[]:saved.commands).entries()) {
    const result = dispatchCommand(content, state, command, state.stateRevision);
    if (!result.ok) return fail('SAVE_REPLAY', `명령 기록을 재현할 수 없습니다: ${result.error.code} — ${result.error.message}`, `/commands/${i}`);
    state = result.state;
  }
  if (canonicalJson(state) !== canonicalJson(saved.state))
    return fail('SAVE_STATE_MISMATCH', '저장 상태가 초기값과 명령 기록의 재현 결과와 다릅니다.', '/state');
  // Plugin state is opaque here: it is size-checked and carried through unchanged, never
  // interpreted, so a missing plugin cannot block loading (unknown ids round-trip).
  const {extensions,...corePresentation}=saved.playerPresentation??{version:1 as const,timeline:null};
  const extensionError=checkExtensions(extensions);
  if(extensionError)return fail('SAVE_PRESENTATION',extensionError,'/playerPresentation/extensions');
  const presentationError=new PlayerPresentation().restore(content,state,saved.playerPresentation?corePresentation:undefined);
  if(presentationError)return fail('SAVE_PRESENTATION',presentationError,'/playerPresentation');
  return { ok: true, session: freeze({ origin: structuredClone(saved.origin), commands: structuredClone(saved.commands), state }),...(saved.playerPresentation?{playerPresentation:structuredClone(saved.playerPresentation)}:{}) };
}

function checkExtensions(extensions: unknown): string | null {
  if (extensions === undefined) return null;
  if (!extensions || typeof extensions !== 'object' || Array.isArray(extensions)) return '플러그인 연출 저장은 객체여야 합니다.';
  let total = 0;
  for (const [id, state] of Object.entries(extensions)) {
    const size = Buffer.byteLength(canonicalJson(state) ?? 'null', 'utf8');
    if (size > MAX_EXTENSION_STATE_BYTES) return `플러그인 '${id}'의 연출 저장이 1 MiB를 넘습니다.`;
    total += size;
  }
  return total > MAX_EXTENSIONS_BYTES ? '플러그인 연출 저장 합계가 4 MiB를 넘습니다.' : null;
}

export async function decodeSave(content: ContentSnapshot, text: string): Promise<LoadedSessionResult> {
  if (Buffer.byteLength(text, 'utf8') > MAX_SAVE_BYTES) return fail('SAVE_TOO_LARGE', '세이브는 16 MiB 이하여야 합니다.');
  // Reuse strict JSON parsing and duplicate-key detection, without applying a content schema.
  const parsed = parseDocument('<save>', 'project', text);
  if (!parsed.document) {
    const issue = parsed.diagnostics[0];
    return fail('SAVE_JSON', `${issue?.code}: ${issue?.message} (${issue?.line}:${issue?.column})`, issue?.pointer || '/');
  }
  return restore(content, parsed.document.data);
}

export async function encodeSave(content: ContentSnapshot, session: RunSession,playerPresentation?:PlayerPresentationEnvelope): Promise<{ ok: true; text: string } | Failure> {
  const envelope: SaveEnvelope = { saveVersion: SAVE_VERSION, coreVersion: CORE_VERSION, contentFingerprint: content.contentFingerprint, ...session,...(playerPresentation?{playerPresentation}:{}) };
  const text = canonicalJson(envelope);
  if (Buffer.byteLength(text, 'utf8') > MAX_SAVE_BYTES) return fail('SAVE_TOO_LARGE', '세이브는 16 MiB 이하여야 합니다.');
  const validated = await restore(content, envelope);
  return validated.ok ? { ok: true, text } : validated;
}

/** Write a sibling temporary file, flush it, then atomically replace the target. Parent must exist. */
export async function saveRun(file: string, content: ContentSnapshot, session: RunSession,playerPresentation?:PlayerPresentationEnvelope): Promise<{ ok: true; file: string } | Failure> {
  const encoded = await encodeSave(content, session,playerPresentation);
  if (!encoded.ok) return encoded;
  const target = path.resolve(file);
  const temporary = path.join(path.dirname(target), `.${path.basename(target)}.${randomUUID()}.tmp`);
  let created = false;
  try {
    const handle = await open(temporary, 'wx'); created = true;
    try { await handle.writeFile(encoded.text, 'utf8'); await handle.sync(); }
    finally { await handle.close(); }
    await rename(temporary, target); created = false;
    return { ok: true, file: target };
  } catch (error) {
    return fail('SAVE_IO', `세이브를 저장하지 못했습니다: ${(error as NodeJS.ErrnoException).code ?? 'UNKNOWN'}`, target);
  } finally {
    if (created) await unlink(temporary).catch(() => {});
  }
}

export async function loadRun(file: string, content: ContentSnapshot): Promise<LoadedSessionResult> {
  let bytes: Buffer;
  try {
    const handle = await open(file, 'r');
    try {
      const info = await handle.stat();
      if (!info.isFile()) return fail('SAVE_IO', '세이브 경로는 일반 파일이어야 합니다.', file);
      if (info.size > MAX_SAVE_BYTES) return fail('SAVE_TOO_LARGE', '세이브는 16 MiB 이하여야 합니다.', file);
      bytes = await handle.readFile();
    } finally { await handle.close(); }
  } catch (error) {
    return fail('SAVE_IO', `세이브를 읽지 못했습니다: ${(error as NodeJS.ErrnoException).code ?? 'UNKNOWN'}`, file);
  }
  let text: string;
  try { text = new TextDecoder('utf-8', { fatal: true }).decode(bytes); }
  catch { return fail('SAVE_UTF8', '세이브가 올바른 UTF-8이 아닙니다.', file); }
  return decodeSave(content, text);
}

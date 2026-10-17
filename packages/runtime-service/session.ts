import type { Overrides } from '../schema/generated/project.js';
import { createRun, dispatchCommand } from '../native-bridge/backend.js';
import type { Command, ContentSnapshot, CoreError, DeepReadonly, RunMode, RunState, Transition } from '../core/model.js';
import { freeze } from '../core/util.js';
import { trackRelationshipChanges, type RelationshipChanges } from './relationship-history.js';

export const MAX_COMMANDS = 100000;
export interface RunOrigin { seed: number; mode: RunMode; initialOverrides: Overrides; userValues?: Record<string, string> }
export type RunSession = DeepReadonly<{ origin: RunOrigin; commands: Command[]; state: RunState; relationshipChanges?: RelationshipChanges }>;
export type SessionResult = { ok: true; session: RunSession } | { ok: false; error: CoreError };
export type SessionDispatch = { ok: true; session: RunSession; transition: DeepReadonly<Transition> }
  | { ok: false; session: RunSession; error: CoreError };

export function createSession(content: ContentSnapshot, seed: number,
  options: { mode?: RunMode; initialOverrides?: Overrides; userValues?: Record<string, string> } = {}): SessionResult {
  const origin: RunOrigin = { seed, mode: options.mode ?? 'full',
    initialOverrides: structuredClone(options.initialOverrides ?? { attributes: {}, flags: {}, relationships: {} }) };
  const created = createRun(content, seed, { ...origin, ...(options.userValues !== undefined ? { userValues: options.userValues } : {}) });
  if (!created.ok) return created;
  if (content.project.userFields?.length) origin.userValues = structuredClone(options.userValues ?? {});
  return { ok: true, session: freeze({ origin, commands: [], state: created.state, relationshipChanges:{} }) };
}

export function dispatchSession(content: ContentSnapshot, session: RunSession, command: unknown, expectedRevision: number): SessionDispatch {
  if (session.commands.length >= MAX_COMMANDS) return { ok: false, session,
    error: { code: 'COMMAND_LIMIT', message: `명령 기록은 ${MAX_COMMANDS}개를 넘을 수 없습니다.`, path: '/commands' } };
  const result = dispatchCommand(content, session.state, command, expectedRevision);
  if (!result.ok) return { ok: false, session, error: result.error };
  return { ok: true, session: freeze({ origin: session.origin, commands: [...session.commands, result.transition.command], state: result.state,
    relationshipChanges: trackRelationshipChanges(session.relationshipChanges ?? {}, session.state, result.state) }), transition: result.transition };
}

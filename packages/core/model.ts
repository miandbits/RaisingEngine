import type { ContentByKind } from '../schema/types.js';
import type { Effects } from '../schema/generated/activity.js';
import type { Narrative, Scene } from '../schema/generated/event.js';
import type { GameplayCommand, GameplayState } from '../gameplay/model.js';
import type { SystemsCommand, SystemsState } from '../systems/model.js';

export const CORE_VERSION = '0.2.0';
export type DeepReadonly<T> = T extends object ? { readonly [K in keyof T]: DeepReadonly<T[K]> } : T;
export interface ContentData {
  project: ContentByKind['project'];
  attributes: ContentByKind['attributes']['attributes'];
  flags: ContentByKind['flags']['flags'];
  characters: ContentByKind['character'][];
  activities: ContentByKind['activity'][];
  events: ContentByKind['event'][];
  endings: ContentByKind['ending'][];
  assets: ContentByKind['assets']['assets'];
}
export type ContentSnapshot = DeepReadonly<ContentData & {
  snapshotVersion: '1';
  contentFingerprint: string;
  assetHashes: Record<string, string>;
}>;
export type NumericTarget = Extract<Effects[number], { op: 'add' | 'set' }>['target'];
export interface Values {
  attributes: Record<string, number>;
  flags: Record<string, boolean>;
  relationships: Record<string, Record<string, number>>;
}
export interface RandomState {
  algorithm: 'pcg32-xsh-rr-v1';
  seed: number;
  state: string;
  draws: number;
}
export type Phase = 'planning' | 'ready' | 'activity-result' | 'period-end' | 'narrative' | 'finished' | 'expedition' | 'battle';
export type RunMode = 'activities-only' | 'full';
export type NarrativeNode = Narrative['nodes'][number];
export interface NarrativeCursor {
  actorId?: string;
  kind: 'event' | 'ending'; id: string; nodeId: string;
  returnPhase?: 'planning' | 'ready' | 'period-end'; returnPresentation?: PresentationState;
  finishPeriod?: boolean;
}
export interface PresentationState {
  backgroundAssetId: string | null;
  musicAssetId: string | null;
  portraits: NonNullable<Scene['portraits']>[number][];
}
export interface EventDecision {
  eventId: string; priority: number;
  reason: 'selected' | 'completed' | 'before-window' | 'expired' | 'requirements' | 'priority' | 'interaction-only';
}
export interface NarrativeStep {
  cursor: NarrativeCursor; nodeType: NarrativeNode['type']; choiceId: string | null; changes: Change[];
}
export interface CalendarTime { periodIndex: number; year: number; month: number; periodInMonth: number }
export interface NumericChange {
  kind: 'numeric'; source: string; target: NumericTarget;
  before: number; operand: number; requested: number; after: number; delta: number;
  clamped: false | 'min' | 'max';
}
export interface FlagChange { kind: 'flag'; source: string; flagId: string; before: boolean; after: boolean }
export type Change = NumericChange | FlagChange;
export interface ActivityResult {
  actorResults?: { characterId: string; result: ActivityResult }[];
  activityId: string; time: CalendarTime; changes: Change[];
  success: null | { requestedChance: number; chance: number; roll: number; passed: boolean; rawDraws: number };
}
/** Internal transaction draft. Published states and restored sessions are deeply frozen. */
export interface RunData extends Values {
  /** Explicit view navigation, cleared when the simulation changes phase. */
  uiScreenId?: string;
  periodPreparation?: { changes: ActivityResult['changes'] };
  activityIntermission?: boolean;
  progression?: import('./progression.js').ProgressionState;
  roster?: import('./roster.js').RosterState;
  userValues?: Record<string, string>;
  coreVersion: typeof CORE_VERSION;
  contentFingerprint: string;
  mode: RunMode;
  stateRevision: number;
  periodIndex: number;
  phase: Phase;
  schedule: string[] | null;
  rng: RandomState;
  completedEvents: string[];
  lastActivityResult: ActivityResult | null;
  activeNarrative: NarrativeCursor | null;
  endingId: string | null;
  presentation: PresentationState;
  gameplay?: GameplayState;
  systems?: SystemsState;
}
export type RunState = DeepReadonly<RunData>;
export type Command = GameplayCommand | SystemsCommand
  | { type: 'advanceSession' | 'cancelSession' }
  | { type: 'cancelActorSession'; actorId: string }
  | { type: 'waitTime'; ticks: number }
  | { type: 'assignActivities'; assignments: Record<string, string> }
  | { type: 'executeActorAction'; actorId: string; actionId: string; quantity: number }
  | { type: 'setSchedule'; activityIds: string[] }
  | { type: 'executeActivity' }
  | { type: 'acknowledgeResult' }
  | { type: 'completePeriodWithoutEvents' }
  | { type: 'resolvePeriodEnd' }
  | { type: 'advanceNarrative' }
  | { type: 'choose'; choiceId: string };
export interface CoreError { code: string; message: string; path: string }
export interface Transition {
  command: Command; fromRevision: number; toRevision: number;
  fromPhase: Phase; toPhase: Phase; time: CalendarTime;
  activity: ActivityResult | null;
  narrative: NarrativeStep | null;
  eventDecisions: EventDecision[];
  endingSelected: string | null;
  soundCues: string[];
}
export type DispatchResult =
  | { ok: true; state: RunState; transition: DeepReadonly<Transition> }
  | { ok: false; state: RunState; error: CoreError };
export type CreateResult = { ok: true; state: RunState } | { ok: false; error: CoreError };

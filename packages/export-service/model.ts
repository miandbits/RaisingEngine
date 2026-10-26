import type { ContentSnapshot } from '../core/model.js';
import type { GameExtensions } from '../plugin-host/bundle.js';
export interface PresentationRevision {
  formatVersion: '1';
  sourceContentFingerprint: string;
  /** Actual packaged image bytes; content remains the unchanged save/replay baseline. */
  images: Record<string, { baselineSha256: string; sha256: string }>;
  /** Render-only overlay. Commands, saves and replays always use unchanged content. */
  visual?: NonNullable<ContentSnapshot['project']['visual']>;
  policy?: 'paint-layout-v1';
  changedPaths?: string[];
}
/** `extensions` exists only when a screen uses a plugin element (plugin-host CONTRACT §6). */
export interface GameBundle { formatVersion: '1'; content: ContentSnapshot; assets: Record<string, string>; /** Host window metadata; never part of gameplay or save identity. */ application?: {displayName:string}; extensions?: GameExtensions; presentationRevision?: PresentationRevision }
export type ExportTarget = 'web' | 'windows' | 'macos' | 'ios';

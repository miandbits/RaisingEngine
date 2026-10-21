import type { ContentSnapshot } from '../core/model.js';
import type { GameExtensions } from '../plugin-host/bundle.js';
export interface PresentationRevision {
  formatVersion: '1';
  sourceContentFingerprint: string;
  /** Actual packaged image bytes; content remains the unchanged save/replay baseline. */
  images: Record<string, { baselineSha256: string; sha256: string }>;
}
/** `extensions` exists only when a screen uses a plugin element (plugin-host CONTRACT §6). */
export interface GameBundle { formatVersion: '1'; content: ContentSnapshot; assets: Record<string, string>; extensions?: GameExtensions; presentationRevision?: PresentationRevision }
export type ExportTarget = 'web' | 'windows';

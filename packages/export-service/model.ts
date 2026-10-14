import type { ContentSnapshot } from '../core/model.js';
import type { GameExtensions } from '../plugin-host/bundle.js';
/** `extensions` exists only when a screen uses a plugin element (plugin-host CONTRACT §6). */
export interface GameBundle { formatVersion: '1'; content: ContentSnapshot; assets: Record<string, string>; extensions?: GameExtensions }
export type ExportTarget = 'web' | 'windows';

import type { ContentSnapshot } from '../core/model.js';
export interface GameBundle { formatVersion: '1'; content: ContentSnapshot; assets: Record<string, string> }
export type ExportTarget = 'web' | 'windows';

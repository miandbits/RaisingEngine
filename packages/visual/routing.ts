import type { ReadonlyDesign } from './model.js';
import type { RunState } from '../core/model.js';

/** Screen names have no semantic meaning when routing is authored. */
export function resolveScreen(design: ReadonlyDesign, state: RunState | null, narrativeType?: string): string {
  const phase = state?.phase ?? 'title';
  const legacy = phase === 'title' ? 'title' : phase === 'finished' ? 'ending' : phase === 'narrative' ? 'story'
    : phase === 'battle' || phase === 'expedition' ? phase
    : phase === 'planning' || phase === 'ready' ? state?.gameplay?.screenId ?? 'schedule' : 'schedule';
  const routing = design.routing;
  if (!routing) return legacy;
  if (state?.uiScreenId && design.screens.some(s => s.id === state.uiScreenId)) return state.uiScreenId;
  if (routing.useLocationScreens && (phase === 'planning' || phase === 'ready') && state?.gameplay?.screenId && design.screens.some(s => s.id === state.gameplay!.screenId)) return state.gameplay.screenId;
  const key = phase === 'narrative' ? narrativeType === 'choice' ? 'choice' : 'dialogue' : phase;
  return routing.phases.find(r => r.phase === key)?.screenId ?? routing.defaultScreenId;
}

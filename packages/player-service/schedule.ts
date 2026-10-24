import { calendarLabels, type Calendar } from '../core/calendar.js';
import type { RunState } from '../core/model.js';

/** A presentation driver, never a shortcut around canonical simulation commands. */
export function scheduleStep(state: RunState | null, playback: string | undefined,
  timeline: { cursor: { completed: boolean } } | null | undefined): 'execute' | 'continue' | null {
  if (playback !== 'continuous' || !state?.schedule) return null;
  if (state.phase === 'ready') return 'execute';
  if (state.phase === 'activity-result' && (!timeline || timeline.cursor.completed)) return 'continue';
  return null;
}

export function periodLabel(index: number, count: number, calendar?:Calendar,locale='en'): string {
  if(calendar)return `${calendarLabels(calendar,locale).period} ${index+1}`;
  return count === 3 ? ['상순', '중순', '하순'][index] ?? '' : `${index + 1}주차`;
}

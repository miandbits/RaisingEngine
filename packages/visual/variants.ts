import type { Element } from './model.js';
import type { RunState } from '../core/model.js';

export function usesPresentationVariants(value: unknown): boolean {
  return !!value && typeof value === 'object' &&
    ('variants' in value && Array.isArray(value.variants) && value.variants.length > 0 || Object.values(value).some(usesPresentationVariants));
}

/** Activity filters describe the displayed result, never the next queued activity. */
export function presentationActivity(state: RunState | null | undefined): string | undefined {
  return state?.phase === 'activity-result' ? state.lastActivityResult?.activityId : undefined;
}

/** First matching rule wins. Always derive from the authored base; never mutate it. */
export function resolvePresentation<T extends Element>(element: T, activityId: string | undefined,
  evaluate: (condition: NonNullable<Element['variants']>[number]['when']) => boolean): T {
  const rule = element.variants?.find(rule =>
    (!rule.activityIds?.length || !!activityId && rule.activityIds.includes(activityId)) && evaluate(rule.when));
  if (!rule) return element;
  const { extensionRef, ...values } = rule.values;
  const result = { ...element, ...values };
  if (values.text !== undefined) result.binding = 'none';
  if (values.imageAssetId !== undefined) result.imageSource = 'asset';
  if (extensionRef !== undefined && element.extension) result.extension = { ...element.extension, ref: extensionRef };
  return result;
}

import type { Element } from './model.js';
import type { RunState } from '../core/model.js';

export function usesPresentationVariants(value: unknown): boolean {
  return !!value && typeof value === 'object' &&
    ('variants' in value && Array.isArray(value.variants) && value.variants.length > 0 || Object.values(value).some(usesPresentationVariants));
}

/** Activity filters describe the displayed result, never the next queued activity. */
export function presentationActivity(state: RunState | null | undefined, element?:Element): string | undefined {
  if(element?.uiContext?.activityId)return element.uiContext.activityId;
  if(state?.phase!=='activity-result')return undefined;
  const actor=element?.uiContext?.actorId;
  return actor ? state.lastActivityResult?.actorResults?.find(row=>row.characterId===actor)?.result.activityId : state.lastActivityResult?.activityId;
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

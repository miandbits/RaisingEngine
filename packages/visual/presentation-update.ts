import { presentationPolicy as policy } from './presentation-policy.generated.js';

type ObjectValue = Record<string, unknown>;
const object = (value: unknown): value is ObjectValue => !!value && typeof value === 'object' && !Array.isArray(value);
function erase(value: unknown, keys: readonly string[]): void {
  if (object(value)) for (const key of keys) delete value[key];
}
function elements(value: unknown): void {
  if (!Array.isArray(value)) return;
  for (const element of value) if (object(element)) {
    erase(element, policy.element);
    // Bound text, dialogue and user input remain part of the simulation baseline.
    if (policy.staticTextKinds.includes(String(element.kind)) && !Object.hasOwn(element, 'binding')) delete element.text;
  }
}
/** Conservative allowlist: unknown fields, conditions, actions, IDs, order, bindings,
 * variants and component overrides must remain byte-for-byte equivalent as JSON.
 */
export function presentationSemantics(visual: unknown): unknown {
  const result = structuredClone(visual);
  if (!object(result)) return result;
  erase(result, policy.root);
  if (object(result.defaultStyle)) {
    erase(result.defaultStyle, policy.style);
    if (!Object.keys(result.defaultStyle).length) delete result.defaultStyle;
  }
  if (Array.isArray(result.styles)) for (const style of result.styles) erase(style, [...policy.style, 'label']);
  if (Array.isArray(result.screens)) for (const screen of result.screens) if (object(screen)) {
    delete screen.label; elements(screen.elements);
  }
  if (Array.isArray(result.components)) for (const component of result.components) if (object(component)) {
    erase(component, ['label', 'width', 'height']); elements(component.elements);
  }
  return result;
}
function canonical(value: unknown): string {
  if (Array.isArray(value)) return '[' + value.map(canonical).join(',') + ']';
  if (object(value)) return '{' + Object.keys(value).sort().map(key => JSON.stringify(key) + ':' + canonical(value[key])).join(',') + '}';
  return JSON.stringify(value) ?? 'undefined';
}
export function verifyPresentationVisual(baseline: unknown, next: unknown): void {
  if (!baseline || !next || canonical(presentationSemantics(baseline)) !== canonical(presentationSemantics(next)))
    throw new Error('PRESENTATION_CONTENT_CHANGED: Only verified paint, geometry and unbound interface text may change. Keep rules, story, actions, conditions, screen structure and bindings unchanged.');
}
export function presentationVisualChanges(baseline: unknown, next: unknown): string[] {
  const changes: string[] = [];
  const visit = (a: unknown, b: unknown, pointer: string): void => {
    if (canonical(a) === canonical(b)) return;
    if (object(a) && object(b)) for (const key of [...new Set([...Object.keys(a), ...Object.keys(b)])].sort())
      visit(a[key], b[key], pointer + '/' + key.replaceAll('~', '~0').replaceAll('/', '~1'));
    else if (Array.isArray(a) && Array.isArray(b) && a.length === b.length) a.forEach((v, i) => visit(v, b[i], pointer + '/' + i));
    else changes.push(pointer);
  };
  visit(baseline, next, '/project/visual'); return changes;
}

import type { Element, Screen } from './model.js';

export type ScreenTemplate = 'blank' | 'home' | 'city' | 'shop' | 'social';

/** Layout templates contain no simulation rules. All actions stay in project data. */
export function createScreen(id: string, label: string, template: ScreenTemplate = 'blank', width = 1280, height = 720): Screen {
  const sx = width / 1280, sy = height / 720;
  const el = (id: string, kind: Element['kind'], x: number, y: number, w: number, h: number, extra: Partial<Element> = {}): Element => ({
    id, kind, x: Math.round(x * sx), y: Math.round(y * sy), width: Math.max(16, Math.round(w * sx)), height: Math.max(16, Math.round(h * sy)), ...extra,
  });
  const button = (id: string, text: string, action: NonNullable<Element['action']>, x: number, extra: Partial<Element> = {}) =>
    el(id, 'button', x, 30, 135, 48, { text, action, fill: '#355c4c', color: '#ffffff', radius: 10, fontSize: 19, ...extra });
  const elements: Element[] = [
    el('background', 'image', 0, 0, 1280, 720, { imageSource: 'background', fit: 'cover', locked: true, motion: 'fade' }),
    el('header-panel', 'panel', 24, 18, 1232, 78, { fill: '#fff9e8ee', radius: 16 }),
    el('screen-title', 'text', 46, 30, 410, 50, { text: label, fontSize: 28 }),
    el('calendar', 'text', 460, 38, 210, 36, { binding: 'calendar', fontSize: 18 }),
    button('schedule-link', '일정', 'navigate', 678, { screenId: 'schedule' }),
    button('save', '저장', 'save', 824), button('load', '불러오기', 'load', 970),
  ];
  if (template !== 'blank') {
    elements.push(el('content-panel', 'panel', 30, 120, 760, 570, { fill: '#fff9e8ee', radius: 20 }));
    elements.push(el('side-panel', 'panel', 820, 120, 430, 570, { fill: '#fff9e8ee', radius: 20 }));
    const kinds: Record<Exclude<ScreenTemplate, 'blank'>, Element['kind']> = { home: 'stats', city: 'locations', shop: 'shop', social: 'interactions' };
    elements.push(el('main-widget', kinds[template], 56, 144, 708, 520, { fontSize: 22 }));
    const sideKind = template === 'shop' ? 'inventory' : template === 'social' ? 'relationships' : template === 'city' ? 'interactions' : 'locations';
    const split = template === 'shop' || template === 'social';
    elements.push(el('side-widget', sideKind, 846, 144, 378, split ? 238 : 520, { fontSize: 20 }));
    if (split) elements.push(el('exit-locations', 'locations', 846, 410, 378, 250, { fontSize: 18 }));
  }
  return { id, label, elements };
}

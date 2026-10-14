import type { Element, VisualDesign, Screen } from './model.js';

/** Starting layouts are ordinary project screens that designers can edit. */
export function systemsScreens(width: number, height: number): Screen[] {
  const e = (id: string, kind: Element['kind'], x: number, y: number, w: number, h: number, extra: Partial<Element> = {}): Element => ({
    id, kind, x: Math.round(x * width / 1280), y: Math.round(y * height / 720), width: Math.round(w * width / 1280), height: Math.round(h * height / 720), ...(kind === 'button' ? { fill: '#304d46', color: '#ffffff', radius: 8 } : {}), ...extra,
  });
  const shell = () => [e('background', 'image', 0, 0, 1280, 720, { imageSource: 'background', fit: 'cover', locked: true }), e('panel', 'panel', 40, 100, 1200, 580, { fill: '#fff9e8ed', radius: 16 }), e('save', 'button', 970, 25, 120, 50, { text: '저장', action: 'save' }), e('load', 'button', 1100, 25, 140, 50, { text: '불러오기', action: 'load' })];
  return [
    { id: 'expedition', label: '탐험', elements: [...shell(), e('expedition-actions', 'expedition', 70, 125, 730, 520, { fontSize: 22 }), e('expedition-stages', 'relationship-stages', 830, 125, 370, 520, { fontSize: 20 })] },
    { id: 'battle', label: '전투', elements: [...shell(), e('battle-actions', 'battle', 100, 125, 1080, 520, { fontSize: 24 })] },
    { id: 'adventure-hub', label: '모험 준비', elements: [...shell(), e('quests', 'quests', 65, 125, 550, 500, { fontSize: 20 }), e('equipment', 'equipment', 645, 125, 550, 500, { fontSize: 20 }), e('schedule', 'button', 55, 25, 260, 50, { text: '일정으로 돌아가기', action: 'navigate', screenId: 'schedule' })] },
  ];
}
export function ensureSystemsScreens(design: VisualDesign): VisualDesign {
  return { ...design, screens: [...design.screens, ...systemsScreens(design.width, design.height).filter(screen => !design.screens.some(existing => existing.id === screen.id))] };
}

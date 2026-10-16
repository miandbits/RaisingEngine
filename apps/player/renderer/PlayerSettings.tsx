import React, { useRef } from 'react';
export interface PlayerPreferences { music: number; effects: number; reducedMotion: boolean; highContrast: boolean }
export const preferencesKey = 'raising-player-preferences-v1';
export function readPreferences(): PlayerPreferences {
  const defaults = { music: 0.4, effects: 1, reducedMotion: window.matchMedia?.('(prefers-reduced-motion: reduce)').matches ?? false, highContrast: false };
  try {
    const saved = JSON.parse(localStorage.getItem(preferencesKey) ?? 'null');
    if (!saved || typeof saved !== 'object') return defaults;
    return { music: Number.isFinite(saved.music) ? Math.max(0, Math.min(1, saved.music)) : defaults.music,
      effects: Number.isFinite(saved.effects) ? Math.max(0, Math.min(1, saved.effects)) : defaults.effects,
      reducedMotion: typeof saved.reducedMotion === 'boolean' ? saved.reducedMotion : defaults.reducedMotion,
      highContrast: typeof saved.highContrast === 'boolean' ? saved.highContrast : defaults.highContrast };
  } catch { return defaults; }
}
export function PlayerSettings({ value, onChange, locale, onOpenChange, muted, onMuted }: { value: PlayerPreferences; onChange: (value: PlayerPreferences) => void; locale: string | undefined; onOpenChange: (open: boolean) => void; muted: boolean; onMuted: (muted: boolean) => void }) {
  const dialog = useRef<HTMLDialogElement>(null), en = locale?.startsWith('en');
  return <div className="player-quality">
    <button className="player-settings-open" onClick={() => { dialog.current?.showModal(); onOpenChange(true); }} aria-haspopup="dialog">{en ? 'Settings' : '환경 설정'}</button>
    <dialog ref={dialog} onClose={() => onOpenChange(false)} aria-labelledby="player-settings-title">
      <h2 id="player-settings-title">{en ? 'Sound & accessibility' : '소리·접근성'}</h2>
      <label><input type="checkbox" checked={!muted} onChange={e => onMuted(!e.target.checked)}/>{en ? 'Enable sound' : '소리 켜기'}</label>
      <label>{en ? 'Music volume' : '배경음 음량'} <input type="range" min="0" max="1" step="0.05" value={value.music} onChange={e => onChange({ ...value, music: Number(e.target.value) })}/><output>{Math.round(value.music * 100)}%</output></label>
      <label>{en ? 'Effects volume' : '효과음 음량'} <input type="range" min="0" max="1" step="0.05" value={value.effects} onChange={e => onChange({ ...value, effects: Number(e.target.value) })}/><output>{Math.round(value.effects * 100)}%</output></label>
      <label><input type="checkbox" checked={value.reducedMotion} onChange={e => onChange({ ...value, reducedMotion: e.target.checked })}/>{en ? 'Reduce decorative motion' : '장식 움직임 줄이기'}</label>
      <label><input type="checkbox" checked={value.highContrast} onChange={e => onChange({ ...value, highContrast: e.target.checked })}/>{en ? 'High contrast controls' : '조작 버튼 고대비'}</label>
      <p>{en ? 'Settings stay on this device. Story timing and results are unchanged.' : '설정은 이 기기에 보관됩니다. 이야기의 시간과 결과는 바뀌지 않습니다.'}</p>
      <button onClick={() => dialog.current?.close()}>{en ? 'Close' : '닫기'}</button>
    </dialog>
  </div>;
}

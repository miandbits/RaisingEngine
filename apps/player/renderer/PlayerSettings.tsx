import {normalizeReadablePreferences} from '../../../packages/visual/readable-layout.js';
import React, { createContext, useContext, useEffect, useRef } from 'react';
import {slotWords} from '../../../packages/player-service/save-slots.js';
export interface PlayerPreferences { music: number; effects: number; reducedMotion: boolean; highContrast: boolean; readingLayout: 'auto'|'on'; textScale: 1|1.25|1.5|2 }
export const preferencesKey = 'raising-player-preferences-v1';
export function readPreferences(): PlayerPreferences {
  const defaults:PlayerPreferences = { music: 0.4, effects: 1, reducedMotion: window.matchMedia?.('(prefers-reduced-motion: reduce)').matches ?? false, highContrast: false, readingLayout:'auto', textScale:1 };
  try {
    const saved = JSON.parse(localStorage.getItem(preferencesKey) ?? 'null');
    if (!saved || typeof saved !== 'object') return defaults;
    return { music: Number.isFinite(saved.music) ? Math.max(0, Math.min(1, saved.music)) : defaults.music,
      effects: Number.isFinite(saved.effects) ? Math.max(0, Math.min(1, saved.effects)) : defaults.effects,
      reducedMotion: typeof saved.reducedMotion === 'boolean' ? saved.reducedMotion : defaults.reducedMotion,
      highContrast: typeof saved.highContrast === 'boolean' ? saved.highContrast : defaults.highContrast,
      ...normalizeReadablePreferences(saved) };
  } catch { return defaults; }
}
export interface PlayerSettingsState { value:PlayerPreferences; onChange:(value:PlayerPreferences)=>void; muted:boolean; onMuted:(value:boolean)=>void; readableEnabled?:boolean;readingOverlayActive?:boolean; onReadingOverlay?:(id:string,open:boolean)=>void }
export const PlayerPreferencesContext=createContext<PlayerSettingsState|null>(null);
export const usePlayerPreferences=()=>useContext(PlayerPreferencesContext);
export function PlayerSettingsControls({value,onChange,muted,onMuted,readableEnabled,locale}:{locale?:string|undefined}&PlayerSettingsState) {
  const words=(ko:string,en:string,ja:string)=>slotWords(locale,ko,en,ja);
  return <div className="player-settings-controls">
    <label><input type="checkbox" checked={!muted} onChange={e=>onMuted(!e.target.checked)}/>{words('소리 켜기','Enable sound','音声を有効にする')}</label>
    <label>{words('배경음 음량','Music volume','音楽の音量')}<input aria-label={words('배경음 음량','Music volume','音楽の音量')} type="range" min="0" max="1" step="0.05" value={value.music} onChange={e=>onChange({...value,music:Number(e.target.value)})}/><output>{Math.round(value.music*100)}%</output></label>
    <label>{words('효과음 음량','Effects volume','効果音の音量')}<input aria-label={words('효과음 음량','Effects volume','効果音の音量')} type="range" min="0" max="1" step="0.05" value={value.effects} onChange={e=>onChange({...value,effects:Number(e.target.value)})}/><output>{Math.round(value.effects*100)}%</output></label>
    {readableEnabled&&<><label>{words('읽기 레이아웃','Reading layout','読みやすいレイアウト')}<select aria-label={words('읽기 레이아웃','Reading layout','読みやすいレイアウト')} value={value.readingLayout} onChange={e=>onChange({...value,readingLayout:e.target.value==='on'?'on':'auto'})}><option value="auto">{words('자동','Auto','自動')}</option><option value="on">{words('항상 사용','Always on','常に使用')}</option></select></label>
    <label>{words('글자 크기','Text size','文字サイズ')}<select aria-label={words('글자 크기','Text size','文字サイズ')} value={value.textScale} onChange={e=>onChange({...value,textScale:Number(e.target.value) as PlayerPreferences['textScale']})}>{[1,1.25,1.5,2].map(scale=><option value={scale} key={scale}>{scale*100}%</option>)}</select></label></>}
    <label><input type="checkbox" checked={value.reducedMotion} onChange={e=>onChange({...value,reducedMotion:e.target.checked})}/>{words('장식 움직임 줄이기','Reduce decorative motion','装飾の動きを減らす')}</label>
    <label><input type="checkbox" checked={value.highContrast} onChange={e=>onChange({...value,highContrast:e.target.checked})}/>{words('조작 버튼 고대비','High contrast controls','操作ボタンの高コントラスト')}</label>
  </div>;
}
export function PlayerSettings({locale,locales,onLocale,localeBusy,onOpenChange,...settings}:{locale:string|undefined;locales?:readonly {id:string;label:string}[]|undefined;onLocale?:((locale:string)=>void)|undefined;localeBusy?:boolean;onOpenChange:(open:boolean)=>void}&PlayerSettingsState) {
  const dialog=useRef<HTMLDialogElement>(null),words=(ko:string,en:string,ja:string)=>slotWords(locale,ko,en,ja);
  useEffect(()=>{const open=()=>{dialog.current?.showModal();onOpenChange(true);};window.addEventListener('raising-open-reading-settings',open);return()=>window.removeEventListener('raising-open-reading-settings',open);},[onOpenChange]);
  return <div className="player-quality">
    <button className="player-settings-open" onClick={()=>{dialog.current?.showModal();onOpenChange(true);}} aria-haspopup="dialog">{words('환경 설정','Settings','設定')}</button>
    <dialog ref={dialog} onClose={()=>onOpenChange(false)} aria-labelledby="player-settings-title" style={settings.readableEnabled?{fontSize:18*settings.value.textScale}:undefined}>
      <h2 id="player-settings-title">{words('소리·접근성','Sound & accessibility','音声とアクセシビリティ')}</h2>
      {settings.readableEnabled&&onLocale&&locales&&locales.length>1&&<div className="player-settings-controls"><label>{words('언어','Language','言語')}<select aria-label={words('언어','Language','言語')} value={locale} disabled={localeBusy} onChange={e=>onLocale(e.target.value)}>{locales.map(item=><option key={item.id} value={item.id}>{item.label}</option>)}</select></label></div>}
      <PlayerSettingsControls {...settings} locale={locale}/>
      <p>{words('설정은 이 기기에 보관됩니다. 이야기의 시간과 결과는 바뀌지 않습니다.','Settings stay on this device. Story timing and results are unchanged.','設定はこの端末に保存されます。物語の時間と結果は変わりません。')}</p>
      <button data-reading-id="engine-reading-close" onClick={()=>dialog.current?.close()}>{words('닫기','Close','閉じる')}</button>
    </dialog>
  </div>;
}

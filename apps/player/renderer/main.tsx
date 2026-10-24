import type {SaveSlotAction} from '../../../packages/player-service/save-slots.js';
import {calendarText,calendarLabels} from '../../../packages/core/calendar.js';
import React, { useEffect, useRef, useState } from 'react';
import type { PlayerAction, PlayerBridge, PlayerReply, PlayerView, Version } from '../../../packages/player-service/controller.js';
import './style.css';
import './player-quality.css';
import { PlayerSettings, readPreferences, preferencesKey } from './PlayerSettings.js';
import { VisualPlayer } from './VisualPlayer.js';
import {TimelineOverlay} from './TimelineOverlay.js';
import { activityPreview } from '../../../packages/visual/activity-summary.js';

declare global { interface Window { player: PlayerBridge } }
import { periodLabel, scheduleStep } from '../../../packages/player-service/schedule.js';
import { assetUrl } from './assets.js';
let eventAudio: AudioContext | undefined;

export function Player({ preview = false }: { preview?: boolean }) {
  const [view, setView] = useState<PlayerView | null>(null);
  const [busy, setBusy] = useState(true);
  const locked = useRef(false);
  const [error, setError] = useState<string | null>(null);
  const [notice, setNotice] = useState<string | null>(null);
  const [plan, setPlan] = useState<string[]>(['', '', '']);
  const [muted, setMuted] = useState(true);
  const [preferences, setPreferences] = useState(readPreferences);
  const [settingsOpen, setSettingsOpen] = useState(false);
  const [hidden, setHidden] = useState(document.hidden);
  useEffect(() => {
    const changed = () => setHidden(document.hidden);
    document.addEventListener('visibilitychange', changed);
    return () => document.removeEventListener('visibilitychange', changed);
  }, []);
  useEffect(() => {
    try { localStorage.setItem(preferencesKey, JSON.stringify(preferences)); } catch { /* Storage may be disabled; session settings still work. */ }
    document.documentElement.dataset.playerReducedMotion = String(preferences.reducedMotion);
    document.documentElement.dataset.playerHighContrast = String(preferences.highContrast);
  }, [preferences]);
  const music = useRef<HTMLAudioElement | null>(null);
  const sounds = useRef<HTMLAudioElement[]>([]);
  const state = view?.state;
  const count=view?.periodsPerMonth??3;
  const periods=Array.from({length:count},(_,i)=>periodLabel(i,count,view?.calendarProject?.calendar,view?.locale));
  const version: Version | null = state && view ? { generation: view.generation, revision: state.stateRevision } : null;

  function accept(reply: PlayerReply) {
    setView(reply.view); setError(reply.error); setNotice(reply.notice);
    if (!muted && !document.hidden && reply.view) for (const id of reply.soundCues) {
      const audio = new Audio(assetUrl(id, reply.view.generation));
      audio.dataset.baseVolume ??= "1"; audio.volume = Number(audio.dataset.baseVolume) * preferences.effects;
      sounds.current.push(audio);
      audio.onended = () => { sounds.current = sounds.current.filter(x => x !== audio); };
      audio.play().catch(() => setNotice('소리를 재생하지 못했습니다. 에셋 파일을 확인해 주세요.'));
    }
  }
  async function request(operation: () => Promise<PlayerReply>) {
    if (locked.current) return;
    locked.current = true; setBusy(true);
    try { accept(await operation()); }
    catch { setError('요청을 처리하지 못했습니다. 다시 시도해 주세요.'); }
    finally { locked.current = false; setBusy(false); }
  }
  useEffect(() => { void request(() => window.player.view()); }, []);
  const scheduleKey = state?.schedule?.join('|') ?? '';
  const monthIndex = state ? Math.floor(state.periodIndex / count) : -1;
  useEffect(() => {
    const first = view?.activities[0]?.id ?? '';
    setPlan(state?.schedule ? [...state.schedule] : Array<string>(count).fill(first));
  }, [view?.generation, scheduleKey, monthIndex, count]);
  useEffect(() => {
    sounds.current.forEach(audio => audio.pause()); sounds.current = [];
    return () => { sounds.current.forEach(audio => audio.pause()); sounds.current = []; };
  }, [view?.generation, muted]);
  // Plugin event sounds (presentation only; CONTRACT.md §7): played like sound cues while unmuted.
  useEffect(() => {
    const host = window.raisingExtensions, generation = view?.generation;
    if (!host || muted || generation === undefined) return;
    host.setSoundListener(sound => {
      if (document.hidden) return;
      const audio = new Audio(assetUrl(sound.asset, generation)); audio.dataset.baseVolume = String(sound.volume); audio.volume = sound.volume * preferences.effects;
      if (sound.balance !== 0 && typeof AudioContext !== 'undefined') {
        // Stereo balance through Web Audio; without it the sound still plays centred.
        try { eventAudio ??= new AudioContext(); const pan = eventAudio.createStereoPanner(); pan.pan.value = sound.balance; eventAudio.createMediaElementSource(audio).connect(pan).connect(eventAudio.destination); } catch { /* centred */ }
      }
      audio.dataset.baseVolume ??= "1"; audio.volume = Number(audio.dataset.baseVolume) * preferences.effects;
      sounds.current.push(audio);
      audio.onended = () => { sounds.current = sounds.current.filter(x => x !== audio); };
      audio.play().catch(() => {});
    });
    return () => host.setSoundListener(null);
  }, [view?.generation, muted, preferences.effects]);
  useEffect(() => {
    music.current?.pause(); music.current = null;
    const id = state?.presentation.musicAssetId;
    if (!muted && id && view) {
      const audio = new Audio(assetUrl(id, view.generation)); audio.loop = true; audio.volume = preferences.music;
      music.current = audio;
      if (!document.hidden) audio.play().catch(() => setNotice('배경음을 재생하지 못했습니다. 에셋 파일을 확인해 주세요.'));
    }
    return () => { music.current?.pause(); };
  }, [state?.presentation.musicAssetId, view?.generation, muted]);

  useEffect(() => {
    if (music.current) music.current.volume = preferences.music;
    sounds.current.forEach(audio => { audio.volume = Number(audio.dataset.baseVolume ?? 1) * preferences.effects; });
  }, [preferences.music, preferences.effects]);
  useEffect(() => {
    if (hidden) { music.current?.pause(); sounds.current.forEach(audio => audio.pause()); sounds.current = []; }
    else if (!muted) void music.current?.play().catch(() => {});
  }, [hidden, muted]);

  const slots=window.player.slots?(action:SaveSlotAction)=>void request(()=>window.player.slots!(action,version)):undefined;
  useEffect(()=>{if(!window.player.checkpoint||!version||!view?.saveSlots?.checkpointPending||view.saveSlots.screenId||busy||hidden||settingsOpen)return;const timer=setTimeout(()=>void request(()=>window.player.checkpoint!(version)),40);return()=>clearTimeout(timer);},[view?.generation,state?.stateRevision,view?.saveSlots?.checkpointPending,view?.saveSlots?.screenId,busy,hidden,settingsOpen]);
  const act = (action: PlayerAction) => { if ((document.hidden || settingsOpen) && (action.type === 'scheduleStep' || action.type === 'timelineTick')) return; if (version) void request(() => window.player.act(action, version)); };
  const autoStep=scheduleStep(state??null,view?.schedulePlayback,view?.timeline);
  useEffect(()=>{if(settingsOpen||hidden||busy||error||view?.saveSlots?.screenId||view?.saveSlots?.checkpointPending||!autoStep||!version)return;const timer=setTimeout(()=>act({type:'scheduleStep'}),autoStep==='execute'?150:700);return()=>clearTimeout(timer);},[view?.generation,state?.stateRevision,autoStep,busy,error,hidden,settingsOpen,view?.saveSlots?.screenId,view?.saveSlots?.checkpointPending]);
  const canPlan = state?.phase === 'planning' || state?.phase === 'ready';
  const dirty = !state?.schedule || plan.some((id, index) => id !== state.schedule?.[index]);
  const attributeLabel = (id: string) => view?.attributes.find(x => x.id === id)?.label ?? id;
  const itemLabel = (id: string) => view?.gameplayDefinition?.items.find(x => x.id === id)?.name ?? id;
  const characterName = (id: string) => view?.characters.find(x => x.id === id)?.name ?? id;
  const relationshipLabel = (characterId: string, relationshipId: string) => `${characterName(characterId)} · ${view?.characters.find(c => c.id === characterId)?.relationships.find(r => r.id === relationshipId)?.label ?? relationshipId}`;
  const result = state?.lastActivityResult;
  const node = view?.narrative?.node;

  const settings = <PlayerSettings value={preferences} onChange={setPreferences} locale={view?.locale} onOpenChange={setSettingsOpen} muted={muted} onMuted={setMuted}/>;
  if (view?.visual) return <div className="visual-shell">{settings}{window.player.setLocale&&view.localization&&<label className="player-language">Language <select aria-label="Language" disabled={busy} value={view.locale} onChange={e=>void request(()=>window.player.setLocale!(e.target.value))}>{view.localization.locales.map(l=><option key={l.id} value={l.id}>{l.label}</option>)}</select></label>}<VisualPlayer {...(slots?{slots}:{})} view={view} design={view.visual} busy={busy||!!view.saveSlots?.checkpointPending&&!view.saveSlots.screenId} plan={plan} onPlan={setPlan} act={act} newRun={userValues => void request(() => window.player.newRun(version, userValues))} save={() => version && void request(() => window.player.save(version))} load={() => void request(() => window.player.load(version))} sound={() => setMuted(x=>!x)} muted={muted} preview={preview} error={error} notice={notice}/></div>;
  return <div className="app-shell">{settings}
    <header className="topbar">
      <div className="brand"><span className="brand-mark" aria-hidden="true">✦</span><div><strong>육성 일지</strong><span>RAISING PLAYER</span></div></div>
      <nav aria-label="게임 메뉴">
        {!preview && <button disabled={busy} onClick={() => void request(() => window.player.openProject(version))}>프로젝트 열기</button>}
        {!preview && <button disabled={busy} onClick={() => void request(() => window.player.openYearSample(version))}>12개월 샘플</button>}
        {!preview && <button disabled={busy || !view} onClick={() => void request(() => window.player.load(version))}>불러오기</button>}
        {!preview && <button disabled={busy || !state} onClick={() => version && void request(() => window.player.save(version))}>저장하기</button>}
        <button aria-pressed={!muted} disabled={busy} onClick={() => setMuted(x => !x)}>{muted ? '소리 켜기' : '소리 끄기'}</button>
      </nav>
    </header>
    {error && <div role="alert" className="alert"><div><strong>진행하지 못했습니다</strong><pre>{error}</pre></div><button onClick={() => setError(null)}>닫기</button></div>}
    {notice && <p role="status" className="notice">{notice}</p>}
    {!state ? <main className="welcome">
      <div className="welcome-emblem" aria-hidden="true">✦</div>
      <p className="eyebrow">당신의 선택으로 채우는 시간</p>
      <h1>{view?.title ?? '이야기를 불러오세요'}</h1>
      <p className="intro">{view?.description || '일정을 계획하고, 만남을 이어가며 나만의 이야기를 완성하세요.'}</p>
      {view && <div className="welcome-meta"><span>{view.calendarProject?.calendar?`${view.durationMonths} ${calendarLabels(view.calendarProject.calendar,view.locale).month}`:`${view.durationMonths}개월의 여정`}</span><span>{view.activities.length}가지 활동</span><span>선택에 따라 달라지는 결말</span></div>}
      <button className="primary large" disabled={busy || !view} onClick={() => void request(() => window.player.newRun(version))}>새 게임 시작 <span aria-hidden="true">→</span></button>
      <p className="muted small">한 달은 {periods.join(" · ")}, {count}번의 일정으로 흘러갑니다.</p>
    </main> : <main className="game-layout">
      <aside className="sidebar">
        <p className="eyebrow">성장의 기록</p><h1>{view!.title}</h1>
        <div className="calendar"><span>{view!.calendarProject?.calendar?`${calendarLabels(view!.calendarProject.calendar,view!.locale).year} ${view!.time!.year}`:`${view!.time!.year}년`}</span><strong>{view!.time!.month}<small>{view!.calendarProject?.calendar?calendarLabels(view!.calendarProject.calendar,view!.locale).month:'월'}</small></strong><span>{periods[view!.time!.periodInMonth]}</span></div>
        <div className="journey"><span>여정</span><span>{Math.min(state.periodIndex + 1, view!.durationMonths * count)} / {view!.durationMonths * count}</span></div>
        <progress aria-label="여정 진행" value={state.phase === 'finished' ? view!.durationMonths * count : state.periodIndex} max={view!.durationMonths * count}/>
        <section className="stats" aria-label="능력과 자원">
          {view!.attributes.map((attribute, i) => <div className="stat" key={attribute.id}>
            <div><span><i className={`stat-dot tone-${i % 4}`} />{attribute.label}</span><strong data-testid={`stat-${attribute.id}`}>{state.attributes[attribute.id]}</strong></div>
            <progress aria-label={attribute.label} value={(state.attributes[attribute.id] ?? attribute.min) - attribute.min} max={attribute.max - attribute.min || 1}/>
          </div>)}
        </section>
        {view!.characters.some(c => c.relationships.length) && <section className="relationships"><h2>함께하는 사람들</h2>
          {view!.characters.flatMap(c => c.relationships.map(r => <div key={`${c.id}.${r.id}`}><span>{c.name} · {r.label}</span><strong>{state.relationships[c.id]?.[r.id]}</strong></div>))}
        </section>}
        <button className="restart" disabled={busy} onClick={() => void request(() => window.player.newRun(version))}>새 게임</button>
      </aside>
      <div className="main-content">
        <div className="chapter-heading"><div><p className="eyebrow">{state.phase === 'finished' ? '여정의 끝' : '하루하루 쌓이는 변화'}</p><h2>{state.phase === 'finished' ? '우리의 여정을 기억하며' : (view!.calendarProject?.calendar?calendarText(view!.time!,view!.calendarProject.calendar,view!.locale,false):`${view!.time!.month}월의 일지`)}</h2></div><span className="chapter-badge">{state.phase === 'finished' ? '완료' : `${periods[view!.time!.periodInMonth]} 진행 중`}</span></div>
        <div className="schedule" aria-label="월간 일정">
          {periods.map((label, index) => {
            const passed = index < view!.time!.periodInMonth || state.phase === 'finished';
            const current = index === view!.time!.periodInMonth && state.phase !== 'finished';
            const activity = view!.activities.find(a => a.id === (canPlan ? plan[index] : state.schedule?.[index]));
            return <div className={`schedule-slot ${current ? 'current' : ''} ${passed ? 'passed' : ''}`} key={label}>
              <div className="slot-heading"><span>{label}</span><span>{passed ? '✓' : current ? '지금' : `0${index + 1}`}</span></div>
              {canPlan ? <select aria-label={`${label} 활동`} disabled={busy || passed} value={plan[index]} onChange={e => setPlan(p => { const next: string[] = [...p]; next[index] = e.target.value; return next; })}>
                {view!.activities.map(a => <option value={a.id} key={a.id}>{a.label}</option>)}
              </select> : <strong>{activity?.label ?? '일정 대기'}</strong>}
              <p>{activity?.costs.length ? activity.costs.map(c => `${attributeLabel(c.attributeId)} −${c.amount}`).join(' · ') : '고정 비용 없음 · 효과·규칙의 변동은 별도'}</p>
              {activity?.previewAttributes?.length ? <p className="activity-preview">{activityPreview(activity,view!.attributes.map(a=>({...a,value:view!.state?.attributes[a.id]??view!.initialState.attributes?.[a.id]??a.initial})))}</p> : null}
              {activity?.description && <p>{activity.description}</p>}
            </div>;
          })}
        </div>
        <section className={`story-card ${state.phase === 'finished' ? 'ending-card' : ''}`} aria-label="이야기">
          {state.presentation.backgroundAssetId && <img className="scene-background" src={assetUrl(state.presentation.backgroundAssetId, view!.generation)} alt="장면 배경" onError={() => setError('배경 이미지를 읽지 못했습니다. 프로젝트 에셋을 확인해 주세요.')} />}
          {state.presentation.portraits.length > 0 && <div className="portraits">{state.presentation.portraits.map(p => {
            const id = view!.characters.find(c => c.id === p.characterId)?.portraits[p.portraitId];
            return id ? <img key={p.position} className={`portrait ${p.position}`} src={assetUrl(id, view!.generation)} alt={characterName(p.characterId)} /> : null;
          })}</div>}
          <div className="story-content" key={`${view!.generation}-${state.phase}-${state.activeNarrative?.nodeId ?? ''}`}>
            {state.phase === 'period-end' && <><h3>다음 이야기를 준비하고 있습니다</h3><p className="muted">오류 내용을 확인한 뒤 다시 시도하거나 게임을 저장할 수 있습니다.</p><button className="primary" disabled={busy} onClick={() => act({ type: 'continue' })}>진행 다시 시도</button></>}
            {canPlan && <>
              <span className="story-symbol" aria-hidden="true">✧</span><p className="eyebrow">다음 걸음을 정해 보세요</p><h3>어떤 시간을 보낼까요?</h3>
              <p className="story-text">{view!.activities.find(a => a.id === plan[view!.time!.periodInMonth])?.description ?? '작은 선택들이 모여 새로운 내일을 만듭니다.'}</p>
              <p className="muted">이번 달의 일정을 고르세요. 아직 지나지 않은 일정은 바꿀 수 있습니다.</p>
              <button className="primary" disabled={busy || plan.some(x => !x)} onClick={() => dirty ? act({ type: 'schedule', activityIds: plan }) : act({ type: 'execute' })}>{dirty ? '일정 확정' : `${periods[view!.time!.periodInMonth]} 활동 시작`} <span aria-hidden="true">→</span></button>
              <p className="small muted">활동 조건과 비용은 시작할 때 확인합니다.</p>
            </>}
            {state.phase === 'activity-result' && result && <>
              <p className="eyebrow">{periods[view!.time!.periodInMonth]}의 기록</p><h3>{view!.activities.find(a => a.id === result.activityId)?.label} 완료</h3>
              {result.success && <p className="result-tag">{result.success.passed ? '성공' : '실패'} · 성공 확률 {result.success.chance / 100}%</p>}
              <div className="changes">{result.changes.some(change=>change.kind==='numeric'&&change.delta!==0)?result.changes.map((change, index) => change.kind === 'numeric'&&change.delta!==0 ? <div key={index}><span>{change.target.kind === 'attribute' ? attributeLabel(change.target.attributeId) : change.target.kind === 'inventory' ? itemLabel(change.target.itemId) : relationshipLabel(change.target.characterId, change.target.relationshipId)}</span><strong>{change.delta > 0 ? '+' : ''}{change.delta}</strong><small>{change.before} → {change.after}</small></div> : null):<p>능력 변화 없음</p>}</div>
              <button className="primary" disabled={busy} onClick={() => act({ type: 'continue' })}>다음으로 <span aria-hidden="true">→</span></button>
            </>}
            {state.phase === 'narrative' && node && <>
              <p className="eyebrow">{view!.storyLabel}</p>
              {node.type === 'dialogue' && <><h3 className="speaker">{node.speakerId ? characterName(node.speakerId) : '이야기'}</h3><p className="dialogue" data-testid="dialogue">{node.text}</p><button className="primary" disabled={busy} onClick={() => act({ type: 'continue' })}>계속 <span aria-hidden="true">→</span></button></>}
              {node.type === 'choice' && <><h3>{node.prompt}</h3><div className="choices">{view!.narrative!.choices.map(choice => <button disabled={busy || !choice.enabled} key={choice.id} onClick={() => act({ type: 'choose', choiceId: choice.id })}><span>{choice.text}{!choice.enabled && <small>조건을 충족하지 못했습니다</small>}</span><span aria-hidden="true">→</span></button>)}</div></>}
              {node.type !== 'dialogue' && node.type !== 'choice' && <button className="primary" disabled={busy} onClick={() => act({ type: 'continue' })}>진행 다시 시도</button>}
            </>}
            {state.phase === 'finished' && <><span className="story-symbol" aria-hidden="true">✦</span><p className="eyebrow">이야기를 완성했습니다</p><h3 data-testid="ending">{view!.endingLabel}</h3><p className="story-text">{view!.calendarProject?.calendar?`${view!.durationMonths} ${calendarLabels(view!.calendarProject.calendar,view!.locale).month}`:`${view!.durationMonths}개월`} 동안의 선택이 만든 결말입니다.<br/>새로운 일정으로 또 다른 이야기를 만나 보세요.</p><button className="primary" disabled={busy} onClick={() => void request(() => window.player.newRun(version))}>새로운 이야기 시작 <span aria-hidden="true">→</span></button></>}
          </div>
        </section>
        <footer className="game-footer"><span>선택은 당신의 몫, 성장은 우리의 이야기.</span><span>{busy ? '처리 중…' : '언제든 저장하고 이어갈 수 있습니다.'}</span></footer>
      </div>
    </main>}
    {view&&<TimelineOverlay timeline={view.timeline??null} generation={view.generation} busy={busy} act={act} save={()=>version&&void request(()=>window.player.save(version))} load={()=>void request(()=>window.player.load(version))}/>}
  </div>;
}


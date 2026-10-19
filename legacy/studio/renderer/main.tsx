import { ExportDialog } from './ExportDialog.js';
import React, { useEffect, useMemo, useRef, useState } from 'react';
import { createRoot } from 'react-dom/client';
import type { DraftDocument, StudioBridge, StudioReply, StudioView } from '../../../packages/studio-service/editor.js';
import type { Json } from '../../../packages/validation/model.js';
import { Player } from '../../../apps/player/renderer/main.js';
import { SchemaForm, references } from './SchemaForm.js';
import { buildCatalog } from '../../../packages/analysis-service/catalog.js';
import type { AnalysisJob } from '../../../packages/analysis-service/model.js';
import { Explorer } from './Explorer.js';
import { Flow } from './Flow.js';
import { Tests } from './Tests.js';
import { VisualEditor, createVisualNavigation } from './VisualEditor.js';
import { GameplayEditor, type RelationEventDraft } from './GameplayEditor.js';
import { SystemsEditor, type SystemsFocus } from './SystemsEditor.js';
import './style.css';
declare global { interface Window { studio: StudioBridge } }
const kindNames: Record<string, string> = { project: '프로젝트', attributes: '능력·자원', flags: '플래그', character: '인물', activity: '활동', event: '사건', ending: '엔딩', assets: '에셋', scenario: '시나리오' };
const object = (value: Json): Record<string, Json> => value && typeof value === 'object' && !Array.isArray(value) ? value : {};
const title = (doc: DraftDocument) => String(object(doc.data).label ?? object(doc.data).name ?? object(doc.data).title ?? kindNames[doc.kind]);
// Apply only fields changed by this input, to the latest acknowledged draft.
function mergeDraft(before: Json | undefined, next: Json | undefined, live: Json | undefined): Json | undefined {
  if (JSON.stringify(before) === JSON.stringify(next)) return live;
  if (before && next && live && typeof before === 'object' && typeof next === 'object' && typeof live === 'object') {
    if (Array.isArray(before) && Array.isArray(next) && Array.isArray(live) && before.length === next.length && live.length === before.length) return next.map((v, i) => mergeDraft(before[i], v, live[i])!);
    if (!Array.isArray(before) && !Array.isArray(next) && !Array.isArray(live)) {
      const result = { ...live };
      for (const key of new Set([...Object.keys(before), ...Object.keys(next)])) {
        const value = mergeDraft(before[key], next[key], live[key]);
        if (value === undefined) delete result[key]; else result[key] = value;
      }
      return result;
    }
  }
  if (JSON.stringify(live) !== JSON.stringify(before) && JSON.stringify(live) !== JSON.stringify(next)) throw new Error('동시에 바뀐 필드가 있습니다. 현재 값을 확인하고 다시 입력해 주세요.');
  return next;
}
function Studio() {
  const [view, setView] = useState<StudioView | null>(null);
  const latest = useRef<StudioView | null>(null); const pending = useRef(Promise.resolve());
  const [busy, setBusy] = useState(false); const count = useRef(0);
  const [error, setError] = useState<string | null>(null); const [notice, setNotice] = useState<string | null>(null);
  const [selected, setSelected] = useState('activities/study.json'); const [filter, setFilter] = useState('');
  const [exporting, setExporting] = useState(false);
  const [gameplayFocus, setGameplayFocus] = useState<{category:'locations'|'items'|'shops'|'interactions';id:string;filter?:string}>();
  useEffect(() => setGameplayFocus(undefined), [view?.root]);
  const [mode, setMode] = useState<'form' | 'table'>('form'); const [preview, setPreview] = useState(false);
  const [adding, setAdding] = useState(false); const [newKind, setNewKind] = useState<'activity' | 'event' | 'ending' | 'character'>('activity'); const [newId, setNewId] = useState('');
  const [deleting, setDeleting] = useState(false); const [focusPath, setFocusPath] = useState('');
  const [referenceId, setReferenceId] = useState('');
  const [workspace, setWorkspace] = useState<'content' | 'flow' | 'tests' | 'visual' | 'gameplay' | 'systems'>('content');
  const [systemsFocus, setSystemsFocus] = useState<SystemsFocus>({ category: 'hero', id: '', filter: '' });
  useEffect(() => setSystemsFocus({ category: 'hero', id: '', filter: '' }), [view?.root]);
  const [visualNavigation,setVisualNavigation]=useState(createVisualNavigation);
  const setVisualFocus=(screenId:string)=>setVisualNavigation(previous=>({...previous,screenId,selected:'',tab:'layout'}));
  useEffect(()=>setVisualNavigation(createVisualNavigation()),[view?.root]);
  const [tabs, setTabs] = useState<string[]>([]); const [itemPointer, setItemPointer] = useState('');
  const [history, setHistory] = useState<string[]>([]); const [analysisJob, setAnalysisJob] = useState<AnalysisJob | null>(null);
  const [launchedAnalysis, setLaunchedAnalysis] = useState<AnalysisJob | null>(null);
  const [startingAnalysis, setStartingAnalysis] = useState(false);
  const [autoValidate, setAutoValidate] = useState(true);
  const catalog = useMemo(() => buildCatalog(view?.documents ?? [], view?.diagnostics ?? []), [view?.documents, view?.diagnostics]);
  function request(fn: (v: StudioView) => Promise<StudioReply>, after?: (reply: StudioReply) => void, quiet = false) {
    count.current++; setBusy(true);
    pending.current = pending.current.then(async () => {
      try {
        const reply = await fn(latest.current!); latest.current = reply.view; setView(reply.view);
        if (!quiet) { setError(reply.error); setNotice(reply.notice); } else if (reply.error) setError(reply.error);
        after?.(reply);
      } catch (e) { setError(e instanceof Error ? e.message : String(e)); }
      finally { count.current--; setBusy(count.current > 0); }
    });
  }
  useEffect(() => request(() => window.studio.view()), []);
  useEffect(() => { setTabs([]); setHistory([]); setItemPointer(''); setAnalysisJob(null); setLaunchedAnalysis(null); }, [view?.root]);
  useEffect(() => {
    if (!view || !autoValidate) return;
    const revision = view.revision;
    const timer = setTimeout(() => { if (latest.current?.revision === revision) request(async v => ({ ...await window.studio.validate(v.revision), notice: null }), undefined, true); }, 650);
    return () => clearTimeout(timer);
  }, [view?.revision, autoValidate]);
  useEffect(() => {
    if (view && !view.documents.some(d => d.file === selected)) setSelected(view.documents[0]?.file ?? '');
  }, [view?.root, view?.documents.length]);
  useEffect(() => {
    if (focusPath) document.getElementById(`field-${focusPath}`)?.scrollIntoView({ behavior: 'smooth', block: 'center' });
  }, [selected, focusPath, mode]);
  const doc = view?.documents.find(d => d.file === selected);
  const documentRefs = doc && view ? references(view.documents, doc) : [];
  const refs = doc && view ? references(view.documents, doc, referenceId) : [];
  const jump = (file: string, pointer = '') => {
    if (selected !== file) setHistory(h => [...h, selected].slice(-50));
    setTabs(t => [...new Set([...t, file])].slice(-12)); setSelected(file); setFocusPath(pointer); setMode('form'); setWorkspace('content');
    const item = catalog.find(i => i.file === file && i.pointer && (pointer === i.pointer || pointer.startsWith(`${i.pointer}/`)));
    setItemPointer(item?.pointer ?? '');
  };
  const replace = (document: DraftDocument, next: Json) => request(v => {
    const live = v.documents.find(d => d.file === document.file);
    if (!live) throw new Error('문서가 삭제되었습니다.');
    return window.studio.edit({ type: 'replace', file: document.file, data: mergeDraft(document.data, next, live.data)! }, v.revision);
  });
  const createRelationEvent = (draft:RelationEventDraft) => request(async v => {
    const project=v.documents.find(d=>d.kind==='project');
    if(!project)throw new Error('프로젝트 문서를 찾을 수 없습니다.');
    const root=object(project.data);const gameplay=object(root.gameplay??{});
    const currentInteractions=gameplay.interactions as Json[]??[];
    if(!currentInteractions.some(i=>object(i).id===draft.interactionId))throw new Error('연결할 교류가 변경되었거나 삭제되었습니다. 다시 선택해 주세요.');
    const data={formatVersion:'0.1.0',id:draft.id,label:draft.label,trigger:'interaction',window:{fromPeriod:0,throughPeriod:Number(root.durationMonths)*3-1},priority:50,
      requirements:{op:'compare',comparator:'gte',left:{op:'read',target:{kind:'relationship',characterId:draft.characterId,relationshipId:draft.relationshipId}},right:{op:'literal',value:draft.threshold}},
      entryNodeId:'greeting',nodes:[{id:'greeting',type:'dialogue',speakerId:draft.characterId,text:draft.text,next:'done'},{id:'done',type:'end'}]};
    const interactions=currentInteractions.map(i=>object(i).id===draft.interactionId?{...object(i),eventId:draft.id}:i);
    const linked=await window.studio.edit({type:'batch',edits:[
      {type:'add',kind:'event',id:draft.id},
      {type:'replace',file:`events/${draft.id}.json`,data},
      {type:'replace',file:project.file,data:{...root,gameplay:{...gameplay,interactions}}},
    ]},v.revision);
    return {...linked,notice:linked.error?null:'관계 사건을 만들고 교류에 연결했습니다.'};
  });
  const analyzeDocument = (document: DraftDocument) => {
    if (startingAnalysis || (document.kind !== 'event' && document.kind !== 'ending')) return;
    setStartingAnalysis(true);
    const file = document.file;
    request(async v => {
      try {
      // Wait for queued field edits, then resolve the current ID and revision.
      const live = v.documents.find(d => d.file === file);
      if (!live || (live.kind !== 'event' && live.kind !== 'ending')) throw new Error('분석할 문서가 없습니다.');
      const result = await window.analysis.request({ type: 'start', revision: v.revision, options: {
        runs: 3, seed: 42, strategy: 'random', maxCommands: 2000, timeoutMs: 60000,
        beamWidth: 24, maxStates: 20000, target: { kind: live.kind, id: String(object(live.data).id) },
      } });
      if (result.job) {
        setLaunchedAnalysis(result.job); setAnalysisJob(result.job); setWorkspace('tests');
      }
      return { ...await window.studio.view(), error: result.error ?? null, notice: null };
      } finally { setStartingAnalysis(false); }
    });
  };
  return <div className="studio">
    <header className="studio-header"><div className="studio-brand"><span>✦</span><div><strong>Raising Studio</strong><small>이야기를 만드는 작업실</small></div></div>
      <nav aria-label="편집 메뉴">
        <button onClick={() => request(() => window.studio.create())}>새 프로젝트</button>
        <button onClick={() => request(() => window.studio.create('academy-year'))}>12개월 새 프로젝트</button>
        <button onClick={() => request(() => window.studio.create('academy-visual'))}>비주얼 샘플 새 프로젝트</button>
        <button onClick={() => request(() => window.studio.create('academy-journey'))}>도시·선물 샘플 새 프로젝트</button>
        <button onClick={() => request(() => window.studio.create('academy-adventure'))}>탐험·전투 샘플 새 프로젝트</button>
        <button onClick={() => request(() => window.studio.open())}>열기</button>
        <button disabled={!view} onClick={() => request(v => window.studio.reload(v.revision))}>다시 불러오기</button>
        <button disabled={!view?.undo} onClick={() => request(v => window.studio.edit({ type: 'undo' }, v.revision))}>실행 취소</button>
        <button disabled={!view?.redo} onClick={() => request(v => window.studio.edit({ type: 'redo' }, v.revision))}>다시 실행</button>
        <button disabled={!view} onClick={() => request(v => window.studio.validate(v.revision))}>검증</button>
        <button disabled={!view || view.readOnly} onClick={() => request(v => window.studio.save(v.revision))}>초안 저장</button>
        <button disabled={!view || busy} onClick={() => {setError(null);setExporting(true);}}>게임 내보내기</button>
        <button className="studio-primary" disabled={!view} onClick={() => preview ? setPreview(false) : request(v => window.studio.preview(v.revision), r => { if (!r.error) setPreview(true); })}>{preview ? '편집기로 돌아가기' : '플레이 미리보기'}</button>
      </nav>
    </header>
    {exporting && <ExportDialog busy={busy} error={error} onClose={() => setExporting(false)} onExport={target => request(v => window.studio.exportGame(target, v.revision), r => { if (!r.error && r.notice) setExporting(false); })}/>}
    {error && <div className="studio-alert" role="alert"><span>{error}</span><button onClick={() => setError(null)}>오류 닫기</button></div>}
    {notice && <div className="studio-notice" role="status">{notice}</div>}
    {preview ? <div className="preview"><p className="preview-note">미리보기 시작 시점의 초안을 실행합니다. 돌아온 뒤 다시 시작하면 새 편집 내용이 반영됩니다.</p><Player preview/></div> : view && <>
      <div className="workspace-status"><span>{view.readOnly ? '기본 샘플 · 새 프로젝트로 복사해 저장하세요' : view.root}</span><span>{busy ? '처리 중…' : view.dirty ? '저장하지 않은 변경 있음' : '저장된 상태'}</span></div>
      <nav className="work-tabs" aria-label="작업 공간"><button aria-pressed={workspace === 'content'} onClick={() => setWorkspace('content')}>콘텐츠</button><button aria-pressed={workspace === 'flow'} onClick={() => setWorkspace('flow')}>흐름</button><button aria-pressed={workspace === 'tests'} onClick={() => setWorkspace('tests')}>테스트</button><button aria-pressed={workspace === 'visual'} onClick={() => setWorkspace('visual')}>화면·연출</button><button aria-pressed={workspace === 'gameplay'} onClick={() => setWorkspace('gameplay')}>육성 작업실</button><button aria-pressed={workspace === 'systems'} onClick={() => setWorkspace('systems')}>탐험·성장</button><label className="inline-check"><input type="checkbox" checked={autoValidate} onChange={e => setAutoValidate(e.target.checked)}/>자동 검증</label></nav>
      <div className={`studio-workspace ${workspace !== 'content' ? 'analysis-workspace' : ''} ${['visual','gameplay'].includes(workspace) ? 'visual-workspace' : ''}`}>
        {!['visual','gameplay','systems'].includes(workspace) && <Explorer items={catalog} root={view.root ?? ''} selected={`${selected}#${itemPointer}`} onSelect={i => { jump(i.file, i.pointer); if(i.kind==='screen'){setVisualFocus(i.id);setWorkspace('visual');} else {const category=({location:'locations',item:'items',shop:'shops',interaction:'interactions'} as const)[i.kind as 'location'|'item'|'shop'|'interaction'];if(category){setGameplayFocus({category,id:i.id});setWorkspace('gameplay');}} }} onAdd={() => setAdding(true)} onTags={(files, tag) => { for (const file of files) request(v => { const d = v.documents.find(x => x.file === file); if (!d) return window.studio.view(); const data = object(d.data); return window.studio.edit({ type: 'replace', file, data: { ...data, tags: [...new Set([...(Array.isArray(data.tags) ? data.tags : []), tag])] } }, v.revision); }); }}/>}
        <main className="editor-pane"><div className="document-tabs"><button aria-label="이전 문서" disabled={!history.length} onClick={() => { const file = history[history.length - 1]!; setHistory(h => h.slice(0,-1)); setSelected(file); setItemPointer(''); setWorkspace('content'); }}>←</button>{tabs.map(file => <button key={file} aria-pressed={selected===file} onClick={() => jump(file)}>{view.documents.find(d=>d.file===file) ? title(view.documents.find(d=>d.file===file)!) : file}</button>)}</div>
          {workspace !== 'tests' && workspace !== 'visual' && workspace !== 'gameplay' && doc && ['event', 'ending'].includes(doc.kind) && <div className="target-analysis-actions"><button className="studio-primary" disabled={startingAnalysis || view.readOnly} onClick={() => analyzeDocument(doc)}>{doc.kind === 'event' ? '이 사건 도달 분석' : '이 엔딩 도달 분석'}</button><small>{title(doc)} · 현재 초안으로 즉시 실행 · seed 42 · 최대 3회 / 60초 · 탐색 폭 24 / 상태 20,000개{view.readOnly ? ' · 새 프로젝트로 복사 후 실행하세요.' : ' · 결과와 설정은 테스트 탭에서 확인하세요.'}</small></div>}
          {workspace === 'systems' ? <SystemsEditor view={view} focus={systemsFocus} onFocus={setSystemsFocus} onChange={replace} onWorkshop={() => setWorkspace('gameplay')} onPreview={() => request(v => window.studio.preview(v.revision), r => { if (!r.error) setPreview(true); })}/> : workspace === 'gameplay' ? <GameplayEditor view={view} focus={gameplayFocus} onFocus={setGameplayFocus} onChange={replace} onJump={jump} onScreen={id=>{setVisualFocus(id);setWorkspace('visual');}} onCreateEvent={createRelationEvent} onPreview={() => request(v => window.studio.preview(v.revision), r => { if (!r.error) setPreview(true); })}/> : workspace === 'visual' ? <VisualEditor view={view} navigation={visualNavigation} onNavigation={setVisualNavigation} onChange={replace} onImport={() => request(v => window.studio.importImage(v.revision))} onPreview={() => request(v => window.studio.preview(v.revision), r => { if (!r.error) setPreview(true); })}/> : workspace === 'flow' ? <Flow items={catalog} selected={`${selected}#${itemPointer}`} onJump={jump} onFocus={(file, pointer) => { jump(file, pointer); setWorkspace('flow'); }} job={analysisJob}/> : workspace === 'tests' ? <Tests key={`${view.root}-${launchedAnalysis?.id ?? ''}`} initialJob={launchedAnalysis} view={view} items={catalog} onJob={setAnalysisJob} onReplay={() => setPreview(true)} onReload={() => request(() => window.studio.view())} onJump={jump}/> : <><div className="editor-tabs"><button aria-pressed={mode === 'form'} onClick={() => setMode('form')}>문서 편집</button><button aria-pressed={mode === 'table'} onClick={() => setMode('table')}>활동 비교표</button></div>
          {mode === 'table' ? <><div className="document-heading"><p>활동을 비교하고 행을 눌러 상세 값을 편집하세요.</p><h1>활동 비교표</h1></div><table><thead><tr><th>활동</th><th>비용</th><th>기본 효과</th><th>성공 판정</th></tr></thead><tbody>{view.documents.filter(d => d.kind === 'activity').map(d => {
            const data = object(d.data);
            return <tr key={d.file}><td><button onClick={() => jump(d.file)}>{title(d)}</button><small>{String(data.id)}</small></td><td>{(data.costs as Json[] ?? []).map((c, i) => <div key={i}>{String(object(c).attributeId)} −{String(object(c).amount)}</div>)}</td><td>{(data.baseEffects as Json[] ?? []).map((e, i) => <div key={i}>{String(object(object(e).target ?? {}).attributeId ?? object(e).flagId ?? '관계')} · {String(object(e).op)} {String(object(object(e).value ?? {}).value ?? '')}</div>)}</td><td>{data.successCheck ? '사용' : '없음'}</td></tr>;
          })}</tbody></table></> : doc && <>
            <div className="document-heading"><div><p>{kindNames[doc.kind]} / {doc.file}</p><h1>{title(doc)}</h1></div>{['activity', 'event', 'ending', 'character', 'scenario'].includes(doc.kind) && <button className="delete-document" onClick={() => setDeleting(true)}>문서 삭제</button>}</div>
            {['event', 'ending'].includes(doc.kind) && <div className="node-map"><strong>대본 흐름</strong><div>{(object(doc.data).nodes as Json[] ?? []).map((n, i) => <button key={i} onClick={() => setFocusPath(`/nodes/${i}`)}>{String(object(n).id)}<small>{String(object(n).type)}{object(n).next ? ` → ${String(object(n).next)}` : object(n).type === 'branch' ? ` → ${String(object(n).then)} / ${String(object(n).else)}` : ''}</small></button>)}</div></div>}
            <div className="form-note">필드를 입력하고 다른 곳을 누르면 초안에 반영됩니다. * 필수 항목 · 기간은 0부터 셉니다.</div>
            {itemPointer && <button onClick={() => setItemPointer('')}>문서 전체 보기</button>}
            <SchemaForm key={`${view.root}-${doc.file}-${itemPointer}`} document={doc} documents={view.documents} itemPointer={itemPointer} onChange={next => replace(doc, next)}/>
          </>}
          </>}
        </main>
        {!['visual','gameplay','systems'].includes(workspace) && <aside className="inspector"><section><h2>진단 <span>{view.diagnostics.length}</span></h2><p>검증을 누르면 초안 전체를 검사합니다. 항목을 누르면 해당 문서로 이동합니다.</p>
          {!view.diagnostics.length && <div className="empty-inspector">표시할 진단이 없습니다.</div>}
          {view.diagnostics.map((d, i) => <button className={`diagnostic ${d.severity}`} key={i} onClick={() => jump(d.file, d.pointer)}><strong>{d.code}</strong><span>{d.message}</span><small>{d.file}:{d.line}:{d.column}<br/>{d.pointer}</small></button>)}
        </section><section><h2>참조 찾기 <span>{refs.length}</span></h2><input aria-label="참조 ID 검색" placeholder="현재 문서 또는 ID 입력" value={referenceId} onChange={e => setReferenceId(e.target.value)}/>{refs.length ? refs.map((r, i) => <button className="reference" key={i} onClick={() => jump(r.file, r.pointer)}>{r.file}<small>{r.pointer}</small></button>) : <p>현재 문서의 사용 위치 또는 입력한 ID의 정확한 참조를 찾습니다.</p>}</section><section className="help"><h2>초안과 미리보기</h2><p>구조가 올바른 초안은 의미 오류가 있어도 저장할 수 있습니다. 플레이 미리보기는 모든 오류를 해결한 뒤 시작합니다.</p><p>외부 파일 변경을 발견하면 저장을 멈춥니다. 초안을 확인한 뒤 다시 불러오세요.</p></section></aside>}
      </div>
    </>}
    {adding && <div className="modal-backdrop"><section role="dialog" aria-modal="true" aria-label="콘텐츠 추가" className="studio-modal"><h2>새 콘텐츠</h2><label>종류<select aria-label="새 콘텐츠 종류" value={newKind} onChange={e => setNewKind(e.target.value as typeof newKind)}>{['activity', 'event', 'ending', 'character'].map(kind => <option key={kind} value={kind}>{kindNames[kind]}</option>)}</select></label><label>ID<input aria-label="새 콘텐츠 ID" placeholder="예: sword-practice" value={newId} onChange={e => setNewId(e.target.value)}/></label><p>소문자 영문으로 시작하고 숫자·하이픈을 사용할 수 있습니다.</p><footer><button onClick={() => setAdding(false)}>취소</button><button className="studio-primary" disabled={!newId || busy} onClick={() => request(v => window.studio.edit({ type: 'add', kind: newKind, id: newId }, v.revision), r => { if (!r.error) { const added = r.view.documents.find(d => d.kind === newKind && object(d.data).id === newId); if (added) jump(added.file); setAdding(false); setNewId(''); } })}>만들기</button></footer></section></div>}
    {deleting && doc && <div className="modal-backdrop"><section role="dialog" aria-modal="true" aria-label="문서 삭제 확인" className="studio-modal"><h2>{title(doc)} 삭제</h2><p>참조 {documentRefs.length}곳을 확인하세요. 삭제는 초안에 반영되며 실행 취소할 수 있습니다.</p><footer><button onClick={() => setDeleting(false)}>취소</button><button onClick={() => request(v => window.studio.edit({ type: 'delete', file: doc.file }, v.revision), r => { if (!r.error) setDeleting(false); })}>삭제 확인</button></footer></section></div>}
  </div>;
}
createRoot(document.getElementById('root')!).render(<Studio/>);

import {deriveLastCommittedPlan,hasPlanReuse} from '../player-service/plan-reuse.js';
import {calendarDate,calendarText} from '../core/calendar.js';
import {resolveScreen} from '../visual/routing.js';
import {slotsEnabled,slotCount,slotScreens,slotWords,type SaveSlotAction,type SaveSlotsView,type SlotRow} from '../player-service/save-slots.js';
import {validSaveRecord,type SaveRecord} from '../player-service/save-metadata.js';
import type {SlotStorage,StoredSlot} from './slot-storage.js';
import { scheduleStep } from '../player-service/schedule.js';
import {trackChoiceFeedback,choiceFeedbackText,choiceHints,type ChoiceFeedback} from '../visual/choice-feedback.js';
import { resolveUserValues } from '../core/user-fields.js';
import { CoreFault, freeze } from '../core/util.js';
import {battleUiTypes,selectBattle,type BattleSelection,type BattleUiAction} from '../player-service/battle.js';
import type { ContentSnapshot, RunState, Transition, Command } from '../core/model.js';
import { CORE_VERSION } from '../core/model.js';
import type { PlayerAction, PlayerBridge, PlayerReply, PlayerView, Version } from '../player-service/controller.js';
import type { RunOrigin, RunSession } from '../runtime-service/session.js';
import {trackRelationshipChanges, type RelationshipChanges} from '../runtime-service/relationship-history.js';
import { parseTree, type ParseError, type Node } from 'jsonc-parser';
import {localizeContent,localeId} from '../localization/catalog.js';
import {projectDataUi} from '../visual/data-ui-projection.js';
import {PlayerPresentation,type PlayerPresentationEnvelope,type TimelineAction} from '../player-service/presentation.js';
import type {ExtensionHost} from './extensions.js';
type PresentationEnvelope=PlayerPresentationEnvelope&{extensions?:Record<string,unknown>};

export interface RuntimeRequest { request(input: unknown): unknown }
export interface WebFiles { read(): Promise<string | null>; write(text: string): Promise<void> }
export interface SaveValidator { (value: unknown): boolean; errors?: unknown }
type Failure = { ok: false; error: { code: string; message: string; path?: string } };
type StateReply = { ok: true; state: RunState; transition?: Transition; relationshipChanges?: RelationshipChanges;choiceFeedback?:ChoiceFeedback|null } | Failure;
type Projection = { ok: true; time: PlayerView['time']; narrative: PlayerView['narrative']; gameplay: PlayerView['gameplay']; systems: PlayerView['systems'] } | Failure;
const MAX_BYTES = 16 * 1024 * 1024;
const MAX_COMMANDS = 100000;

/** Stable comparison is independent of JSON object insertion order and host platform. */
function canonical(value: unknown): string {
  if (Array.isArray(value)) return `[${value.map(canonical).join(',')}]`;
  if (value && typeof value === 'object') return `{${Object.keys(value).sort().map(k => `${JSON.stringify(k)}:${canonical((value as Record<string, unknown>)[k])}`).join(',')}}`;
  return JSON.stringify(value);
}
function parseSave(text: string,locale:string): unknown {
  if (new TextEncoder().encode(text).length > MAX_BYTES) throw new Error(slotWords(locale,'세이브는 16 MiB 이하여야 합니다.','Saves must be no larger than 16 MiB.','セーブは16MiB以下にしてください。'));
  const errors: ParseError[] = [];
  const tree = parseTree(text, errors, { disallowComments: true, allowTrailingComma: false, allowEmptyContent: false });
  if (!tree || errors.length) throw new Error(slotWords(locale,'올바른 JSON 세이브가 아닙니다.','This is not a valid JSON save.','正しいJSONセーブではありません。'));
  const pending: { node: Node; depth: number }[] = [{ node: tree, depth: 0 }];
  while (pending.length) {
    const { node, depth } = pending.pop()!;
    if (depth > 128) throw new Error(slotWords(locale,'세이브 구조가 너무 깊습니다.','The save structure is too deeply nested.','セーブの構造が深すぎます。'));
    if (node.type === 'object') {
      const keys = new Set<string>();
      for (const p of node.children ?? []) {
        const key = p.children?.[0]?.value as string;
        if (keys.has(key)) throw new Error(slotWords(locale,'세이브에 중복된 키가 있습니다: ','The save contains a duplicate key: ','セーブに重複したキーがあります: ')+key);
        keys.add(key);
      }
    }
    for (const child of node.children ?? []) pending.push({ node: child, depth: depth + 1 });
  }
  return JSON.parse(text) as unknown;
}

/** Browser shell only. All rules, transitions and view availability execute in the C++ Wasm module. */
export class WebPlayerController implements PlayerBridge {
  locale='';
  session: RunSession | null = null;
  choiceFeedback:ChoiceFeedback|null=null;
  generation = 1;
  presentation = new PlayerPresentation();
  constructor(readonly content: ContentSnapshot, private readonly runtime: RuntimeRequest,
    private readonly files: WebFiles, private readonly validateSave: SaveValidator, readonly extensions: ExtensionHost | null = null, private readonly slotStorage:SlotStorage|null=null) {
    // A running game owns a fixed snapshot; registration must never mask mutations.
    this.content=freeze(structuredClone(content));
  }
  private localized:{locale:string;content:ContentSnapshot}|null=null;
  private readonly projections=new WeakMap<ContentSnapshot,{state:RunState|null;result:Exclude<Projection,Failure>}>();
  private localizedContent():ContentSnapshot {
    const locale=localeId(this.content.project.localization,this.locale);
    if(this.localized?.locale===locale)return this.localized.content;
    const content=freeze(localizeContent(this.content,this.content.project.localization,locale));
    this.localized={locale,content};return content;
  }

  private slotView:SaveSlotsView={rows:[],screenId:null,automatic:false,selected:0,page:0,confirmation:null,available:false};
  private slotExpected:string|null=null;
  private slotBusy=false;
  private slotAutoGeneration=-1;
  private slotAutoRevision=-1;
  private slotAutoPhase='';
  private slotLastCommand='start';
  private get hasSlots(){return slotsEnabled(this.content.project.visual);}
  private words(ko:string,en:string,ja:string){return slotWords(localeId(this.content.project.localization,this.locale),ko,en,ja);}
  private projection(state = this.session?.state ?? null,content=this.content): Exclude<Projection, Failure> {
    const cached=this.projections.get(content);if(cached?.state===state)return cached.result;
    const result = this.runtime.request({ operation: 'view', content, state }) as Projection;
    if (!result.ok) throw new Error(`[${result.error.code}] ${result.error.message}`);
    this.projections.set(content,{state,result});return result;
  }
  private playerView(): PlayerView {
    this.locale=localeId(this.content.project.localization,this.locale);
    const c = this.localizedContent(), state = this.session?.state ?? null, cursor = state?.activeNarrative;
    const projection = this.projection(state,c);
    return { ...(hasPlanReuse(c)?{lastCommittedPlan:deriveLastCommittedPlan(this.session?.commands??[],c.project.periodsPerMonth,c.activities.map(a=>a.id))}:{}),gameId:c.project.gameId, ...(this.hasSlots?{saveSlots:{...this.slotView,rows:this.slotView.rows.map(row=>({...row,...(row.periodIndex!==undefined&&c.project.calendar?{date:calendarText(calendarDate(c.project,row.periodIndex),c.project.calendar,this.locale)}:{})})),...(state?{current:this.currentRecord()}:{}),checkpointPending:!!state&&(this.slotAutoGeneration!==this.generation||this.slotAutoRevision!==state.stateRevision)}}:{}), generation: this.generation, title: c.project.title, description: c.project.description ?? '', initialState: c.project.initialState,
      rosterCharacterIds: c.project.roster?.members.map(member=>member.characterId)??[],
      ...(c.project.userFields?.length ? { userFields: c.project.userFields, userValues: state?.userValues ?? Object.fromEntries(c.project.userFields.map(f => [f.id, f.initial])) } : {}),
      ...(c.project.localization?{localization:c.project.localization,locale:this.locale}:{}),
      choiceFeedback:choiceFeedbackText(c,state,this.choiceFeedback,this.locale),choiceHints:choiceHints(c,state,this.locale),
      ...(c.project.calendar?{calendarProject:{startYear:c.project.startYear,startMonth:c.project.startMonth,periodsPerMonth:c.project.periodsPerMonth,calendar:c.project.calendar}}:{}),
      durationMonths: c.project.durationMonths, periodsPerMonth: c.project.periodsPerMonth, schedulePlayback: c.project.schedulePlayback ?? 'manual', attributes: c.attributes, characters: c.characters,
      activities: c.activities, assets: c.assets, state, relationshipChanges:this.session?.relationshipChanges, visual: c.project.visual ?? null,
      timeline: this.presentation.view(c,state),
      battleSelection:this.presentation.selectedBattle(c,state),
      dataUi: projectDataUi(c,state,projection.gameplay,input=>this.runtime.request(input), {systems:projection.systems,battleSelection:this.presentation.selectedBattle(c,state)}),
      gameplayDefinition: c.project.gameplay ?? null, runtimeBackend: 'wasm',
      time: projection.time, narrative: projection.narrative, gameplay: projection.gameplay, systems: projection.systems ?? null,
      storyLabel: cursor ? (cursor.kind === 'event' ? c.events : c.endings).find(x => x.id === cursor.id)?.label ?? null : null,
      endingLabel: state?.endingId ? c.endings.find(x => x.id === state.endingId)?.label ?? null : null };
  }
  private reply(error: string | null = null, notice: string | null = null, soundCues: readonly string[] = []): PlayerReply {
    return { view: this.playerView(), error, notice, soundCues };
  }
  private current(version: Version | null): boolean {
    return this.session ? version?.generation === this.generation && version.revision === this.session.state.stateRevision : version === null;
  }
  async view() { return this.reply(); }
  async setLocale(locale:string){if(!this.content.project.localization?.locales.some(l=>l.id===locale))return this.reply('등록되지 않은 언어입니다.');this.locale=locale;return this.reply();}
  async openProject() { return this.reply(null, this.words('이 플레이어는 내보낸 게임 전용입니다.','This player opens exported games only.','このプレイヤーは書き出されたゲーム専用です。')); }
  async openYearSample() { return this.openProject(); }
  async newRun(version: Version | null, userValues?: Record<string, string>): Promise<PlayerReply> {
    if (this.slotBusy||this.slotView.screenId) return this.reply('Close the records window before starting a new game.');
    if (!this.current(version)) return this.reply(this.words('화면이 갱신되었습니다. 다시 선택해 주세요.','The screen changed. Please select again.','画面が更新されました。もう一度選んでください。'));
    // Validate before the WASM JSON boundary can omit undefined values or reject lone surrogates.
    try { resolveUserValues(this.content.project.userFields, userValues); }
    catch (error) { if (error instanceof CoreFault) { const issue = error.diagnostic(); return this.reply(`${issue.message} (${issue.path})`); } throw error; }
    const origin: RunOrigin = { seed: 42, mode: 'full', initialOverrides: { attributes: {}, flags: {}, relationships: {} } };
    const r = this.runtime.request({ operation: 'create', content: this.content, seed: origin.seed, options: { ...origin, ...(userValues !== undefined ? { userValues } : {}) } }) as StateReply;
    if (!r.ok) return this.reply(`[${r.error.code}] ${r.error.message} (${r.error.path ?? '/userValues'})`);
    if (this.content.project.userFields?.length) origin.userValues = structuredClone(userValues ?? {});
    this.session = { origin, commands: [], state: r.state }; this.choiceFeedback=null; this.presentation.clear(); this.extensions?.reset();this.slotLastCommand='start'; this.generation++;
    const cues:string[]=[];return this.reply(this.settle(cues),null,cues);
  }
  private dispatch(command: unknown, cues: string[]): string | null {
    const session = this.session!;
    if (session.commands.length >= MAX_COMMANDS) return '명령 기록 한도를 초과했습니다.';
    // Single choke point for submitted commands: extensions observe every reply exactly once.
    const observing = this.extensions?.active ? this.extensions : null;
    const before = observing ? { state: session.state, narrative: this.projection(session.state).narrative } : null;
    const r = this.runtime.request({ operation: 'command', content: this.content, state: session.state,
      command, expectedRevision: session.state.stateRevision }) as StateReply;
    if (!r.ok) { const error = `[${r.error.code}] ${r.error.message} (${r.error.path ?? '/'})`; observing?.observePresentation(before, before, command, session.state, error); return error; }
    if (!r.transition) throw new Error('런타임 명령 응답에 전이가 없습니다.');
    this.choiceFeedback=trackChoiceFeedback(this.choiceFeedback,this.content,session.state,r.transition.command,{state:r.state,transition:r.transition});
    this.session = { origin: session.origin, commands: [...session.commands, r.transition.command], state: r.state,
      relationshipChanges:trackRelationshipChanges(session.relationshipChanges??{},session.state,r.state) };
    observing?.observePresentation(before, { state: r.state, narrative: this.projection(r.state).narrative }, command, r.state);
    cues.push(...r.transition.soundCues); return null;
  }
  private settle(cues: string[]): string | null {
    for (let step = 0; step < 10000; step++) {
      const state = this.session!.state, node = this.projection().narrative?.node;
      const command = state.phase === 'period-end' ? { type: 'resolvePeriodEnd' }
        : node && node.type !== 'dialogue' && node.type !== 'choice' ? { type: 'advanceNarrative' } : null;
      if (!command) return null;
      const error = this.dispatch(command, cues); if (error) return error;
    }
    return '자동 진행 한도를 초과했습니다. 대본을 확인해 주세요.';
  }
  async act(action: PlayerAction, version: Version): Promise<PlayerReply> {
    if (!this.current(version)) return this.reply(this.words('화면이 갱신되었습니다. 다시 선택해 주세요.','The screen changed. Please select again.','画面が更新されました。もう一度選んでください。'));
    if (this.slotBusy||this.slotView.screenId) return this.reply();
    if (!this.session) return this.reply(this.words('새 게임을 시작해 주세요.','Start a new game first.','先に新しいゲームを始めてください。'));
    if (this.extensions?.restorationPending) return this.reply(this.words('화면 복원 후 다시 선택해 주세요.','Wait for the screen to restore, then select again.','画面の復元後にもう一度選んでください。'));
    if (this.extensions?.presentationPending) return this.reply(this.words('화면 갱신 후 다시 선택해 주세요.','Wait for the screen to update, then select again.','画面の更新後にもう一度選んでください。'));
    if(action.type==='scheduleStep'){
      const completionFlag=this.content.project.periodFlow?.intermission?.completionFlagId;
      if(this.session.state.activityIntermission&&completionFlag&&!this.session.state.flags[completionFlag])return this.reply();
      const next=scheduleStep(this.session.state,this.content.project.schedulePlayback,this.presentation.view(this.content,this.session.state)); if(!next)return this.reply(); action={type:next};
    }
    if(['timelineTick','timelinePause','timelineSpeed','timelineSkip'].includes(action.type))return this.reply(this.presentation.act(this.content,this.session.state,action as TimelineAction));
    const timeline=this.presentation.view(this.content,this.session.state);
    if(timeline&&(action.type!=='continue'||!timeline.cursor.completed))return this.reply(this.words('활동 연출을 마치거나 건너뛴 뒤 다음으로 진행해 주세요.','Finish or skip the activity presentation before continuing.','活動の演出を終了するかスキップしてから進んでください。'));
    let battleCommand:unknown;
    if(battleUiTypes.includes(action.type??'')){
      const selected=selectBattle(this.content,this.session.state,this.presentation.selectedBattle(this.content,this.session.state),action as BattleUiAction,input=>this.runtime.request(input));
      if(selected.error)return this.reply(selected.error);
      this.presentation.battleSelection=selected.selection;
      if(!selected.command)return this.reply();
      battleCommand=selected.command;
    }
    let command: unknown;
    switch (action.type) {
      case 'confirmBattleAction': command=battleCommand; break;
      case 'executeActorAction': case 'assignActivities':
      case 'navigate': case 'visit': case 'buy': case 'sell': case 'useService': case 'setDiet': case 'executeAction': case 'selectPolicy': case 'interact': command = action; break;
      case 'acceptQuest': case 'claimQuest': case 'equip': case 'unequip': case 'moveExpedition': case 'returnFromExpedition': case 'startNodeBattle': case 'battleAction': command = action; break;
      case 'schedule': case 'commitPlan': command = { type: 'setSchedule', activityIds: action.activityIds }; break;
      case 'execute': command = { type: this.session.state.phase === 'ready' && this.session.state.activityIntermission ? 'acknowledgeResult' : 'executeActivity' }; break;
      case 'choose': command = action; break;
      case 'continue': command = { type: (this.session.state.phase === 'ready' && this.session.state.activityIntermission) || this.session.state.phase === 'activity-result' ? 'acknowledgeResult'
        : this.session.state.phase === 'period-end' ? 'resolvePeriodEnd' : 'advanceNarrative' }; break;
      default: return this.reply('알 수 없는 동작입니다.');
    }
    const cues: string[] = [];
    const previousRevision=this.session.state.stateRevision;
    const error = this.dispatch(command, cues) ?? (action.type==='commitPlan'?this.dispatch({type:'executeActivity'},cues):null) ?? this.settle(cues);
    if(this.session.state.stateRevision!==previousRevision)this.slotLastCommand=(command as {type:string}).type;
    return this.reply(error, null, cues);
  }
  private encode(record?:SaveRecord):string {
    if(!this.session)throw new Error('No game to save.');
    if(this.extensions?.presentationPending)throw new Error(this.words('화면 갱신 후 다시 저장해 주세요.','Wait for the screen to update, then save again.','画面の更新後にもう一度保存してください。'));
    const extensions=this.extensions?.save(true);
    const presentation:PresentationEnvelope|undefined=this.presentation.view(this.content,this.session.state)||this.presentation.selectedBattle(this.content,this.session.state)||extensions||record?{...this.presentation.envelope(this.content,this.session.state),...(extensions?{extensions}:{}),...(record?{saveRecord:record}:{})}:undefined;
    const text=canonical({saveVersion:'1.0.0',coreVersion:CORE_VERSION,contentFingerprint:this.content.contentFingerprint,origin:this.session.origin,commands:this.session.commands,state:this.session.state,...(presentation?{playerPresentation:presentation}:{})});
    if(new TextEncoder().encode(text).length>MAX_BYTES)throw new Error('Save exceeds 16 MiB. Export an earlier checkpoint.');
    return text;
  }
  async save(version:Version):Promise<PlayerReply>{
    if(this.hasSlots)return this.slots({type:'open',kind:'save'},version);
    if(!this.session||!this.current(version))return this.reply(this.words('저장할 현재 게임이 없습니다.','There is no current game to save.','保存するゲームがありません。'));
    try{await this.files.write(this.encode());return this.reply(null,this.words('게임을 저장했습니다.','Game saved.','ゲームを保存しました。'));}catch(error){return this.reply(String(error));}
  }
  async load(version:Version|null):Promise<PlayerReply>{
    if(this.hasSlots)return this.slots({type:'open',kind:'load'},version);
    if(!this.current(version))return this.reply(this.words('화면이 갱신되었습니다. 다시 선택해 주세요.','The screen changed. Please select again.','画面が更新されました。もう一度選んでください。'));
    try{const text=await this.files.read();if(text===null)return this.reply();if(!this.current(version))return this.reply('Game changed while importing.');return this.restore(text);}catch(error){return this.reply(String(error));}
  }
  private restore(text:string):PlayerReply {
    try {
      const value = parseSave(text,localeId(this.content.project.localization,this.locale));
      // Plugin states are opaque to the save schema; validate them separately as a plain object.
      const {extensions:savedExtensions,...presentationRest}=((value as {playerPresentation?:PresentationEnvelope}).playerPresentation??{}) as PresentationEnvelope;
      if (savedExtensions !== undefined && (!savedExtensions || typeof savedExtensions !== 'object' || Array.isArray(savedExtensions))) throw new Error(this.words('세이브 데이터 구조가 올바르지 않습니다.','The save data has an invalid structure.','セーブデータの形式が正しくありません。'));
      const checked = savedExtensions === undefined ? value : { ...(value as object), playerPresentation: presentationRest };
      if (!this.validateSave(checked)) throw new Error(this.words('세이브 데이터 구조가 올바르지 않습니다.','The save data has an invalid structure.','セーブデータの形式が正しくありません。'));
      const saved = checked as RunSession & { saveVersion: string; coreVersion: string; contentFingerprint: string;playerPresentation?:PlayerPresentationEnvelope };
      if (saved.saveVersion !== '1.0.0' || saved.coreVersion !== CORE_VERSION) throw new Error(this.words('지원하지 않는 세이브 또는 코어 버전입니다.','This save or rules version is not supported.','このセーブまたはルールのバージョンには対応していません。'));
      if (saved.contentFingerprint !== this.content.contentFingerprint) throw new Error(this.words('다른 콘텐츠로 만든 세이브입니다.','This save belongs to different game content.','このセーブは異なるゲーム内容で作成されています。'));
      if (saved.origin.mode !== 'full') throw new Error(this.words('활동 전용 세이브는 플레이어에서 열 수 없습니다.','Activity-only saves cannot be opened in this player.','活動専用のセーブはこのプレイヤーでは開けません。'));
      const replayed = this.runtime.request({ operation: 'replay', content: this.content, origin: saved.origin, commands: saved.commands, includeRelationshipChanges:true,includeChoiceFeedback:true }) as StateReply;
      if (!replayed.ok) throw new Error(`${replayed.error.message} (${replayed.error.path ?? '/'})`);
      const state = replayed.state;
      if (canonical(state) !== canonical(saved.state)) throw new Error(this.words('저장 상태가 명령 기록의 재현 결과와 다릅니다.','The saved state does not match its replayed command history.','保存された状態と操作履歴の再現結果が一致しません。'));
      const previous = this.session;
      const previousChoice=this.choiceFeedback;
      const previousPresentation=structuredClone(this.presentation.cursor);
    const previousScheduleSpeed=this.presentation.scheduleSpeed;
    const previousBattleSelection=structuredClone(this.presentation.battleSelection);
      const presentationError=this.presentation.restore(this.content,state,saved.playerPresentation);
      if(presentationError)throw new Error(presentationError);
      this.session = { origin: structuredClone(saved.origin), commands: structuredClone(saved.commands) as Command[], state, relationshipChanges:replayed.relationshipChanges??{} };
      this.choiceFeedback=replayed.choiceFeedback??null;
      try {
        const error = this.settle([]);
        if (error) { this.session = previous;this.choiceFeedback=previousChoice; this.presentation.cursor=previousPresentation;this.presentation.scheduleSpeed=previousScheduleSpeed; this.presentation.battleSelection=previousBattleSelection; return this.reply(error); }
      } catch (error) { this.session = previous;this.choiceFeedback=previousChoice; this.presentation.cursor=previousPresentation;this.presentation.scheduleSpeed=previousScheduleSpeed; this.presentation.battleSelection=previousBattleSelection; throw error; }
      this.extensions?.reset(); this.extensions?.scheduleRestore(savedExtensions, true, this.session.state);
      this.generation++;this.slotView.screenId=null;this.slotView.confirmation=null;this.slotAutoGeneration=this.generation;this.slotAutoRevision=this.session.state.stateRevision;this.slotAutoPhase=this.session.state.phase; return this.reply(null, this.words('저장한 게임을 불러왔습니다.','Continued from your saved game.','保存したところから再開しました。'));
    } catch (e) { return this.reply(e instanceof Error ? e.message : String(e)); }
  }
  private currentRecord():Pick<SaveRecord,'date'|'place'|'screenId'|'phase'>{
    const c=this.localizedContent(),state=this.session?.state;
    if(!state)return {date:'',place:'',screenId:'',phase:''};
    const screenId=c.project.visual?resolveScreen(c.project.visual,state,this.projection(state,c).narrative?.node.type):'';
    const time=calendarDate(c.project,state.periodIndex);
    return {date:c.project.calendar?calendarText(time,c.project.calendar,this.locale):`${time.year} / ${time.month} · ${time.periodInMonth+1}`,place:c.project.visual?.screens.find(s=>s.id===screenId)?.label??screenId,screenId,phase:state.phase};
  }
  private preview(row:StoredSlot):SlotRow {
    const key={automatic:row.automatic,index:row.index};
    try{
      const value=parseSave(row.text,localeId(this.content.project.localization,this.locale)) as {state?:RunState;contentFingerprint?:string;saveVersion?:string;coreVersion?:string;commands?:unknown;playerPresentation?:PresentationEnvelope};
      const state=value.state;
      if(!state||!Number.isInteger(state.periodIndex)||state.periodIndex<0||state.periodIndex>1000000||typeof state.phase!=='string'||!state.attributes||Object.values(state.attributes).some(n=>!Number.isInteger(n))||!Array.isArray(value.commands))throw new Error('Invalid record');
      const record=value.playerPresentation?.saveRecord;
      if(record!==undefined&&!validSaveRecord(record))throw new Error('Invalid record metadata');
      const compatible=value.contentFingerprint===this.content.contentFingerprint&&value.saveVersion==='1.0.0'&&value.coreVersion===CORE_VERSION;
      return {...key,status:compatible?'ready':'incompatible',periodIndex:state.periodIndex,...(record?{record}:{})};
    }catch{return {...key,status:'invalid'};}
  }
  private async refreshSlots(){
    if(!this.slotStorage)throw new Error('Browser save storage is unavailable. Export a backup file to keep your progress.');
    try{const stored=await this.slotStorage.read(),rows:SlotRow[]=[];
      for(const automatic of [false,true])for(let index=0;index<slotCount(this.content.project.visual!,automatic);index++){
        const row=stored.find(r=>r.automatic===automatic&&r.index===index);rows.push(row?this.preview(row):{automatic,index,status:'empty'});
      }
      this.slotView.rows=rows;this.slotView.available=true;
    }catch(error){this.slotView.available=false;throw error;}
  }
  private async writeSlot(automatic:boolean,index:number,expected:string|null){
    if(!this.slotStorage)throw new Error('Browser save storage is unavailable.');
    // Capture the presentation after React committed; the transaction itself stays synchronous.
    const base=JSON.parse(this.encode()) as {playerPresentation?:PresentationEnvelope};
    const detail=this.currentRecord(),savedAt=new Date().toISOString();
    await this.slotStorage.update(rows=>{
      const previews=rows.map(row=>this.preview(row));let sequence=1;
      for(const row of previews)sequence=Math.max(sequence,(row.record?.sequence??0)+1);
      if(!Number.isSafeInteger(sequence))throw new Error('Save sequence is too large. Export a backup.');
      if(automatic){let oldest=Infinity;for(let i=0;i<slotCount(this.content.project.visual!,true);i++){const row=previews.find(r=>r.automatic&&r.index===i),seq=row?.record?.sequence??0;if(seq<oldest){oldest=seq;index=i;}}}
      else if((rows.find(r=>!r.automatic&&r.index===index)?.text??null)!==expected)throw new Error(this.words('확인하는 동안 기록이 바뀌었습니다. 다시 선택해 주세요.','The record changed during confirmation. Select it again.','確認中に記録が変わりました。選び直してください。'));
      const record:SaveRecord={version:1,...detail,thumbnail:'',savedAt,sequence,automatic};
      const presentation={...this.presentation.envelope(this.content,this.session!.state),...base.playerPresentation,saveRecord:record};
      const text=canonical({...base,playerPresentation:presentation});
      if(new TextEncoder().encode(text).length>MAX_BYTES)throw new Error('Save exceeds 16 MiB.');
      return {automatic,index,text};
    });
    await this.refreshSlots();
  }
  async checkpoint(version:Version):Promise<PlayerReply>{
    if(!this.hasSlots||!this.session||!this.current(version)||this.slotBusy||this.slotView.screenId||this.extensions?.presentationPending||this.extensions?.restorationPending)return this.reply();
    const state=this.session.state,previous=this.slotAutoGeneration===this.generation?this.slotAutoRevision:-1,previousPhase=this.slotAutoPhase;
    if(previous===state.stateRevision)return this.reply();
    this.slotAutoGeneration=this.generation;this.slotAutoRevision=state.stateRevision;this.slotAutoPhase=state.phase;
    const command=this.slotLastCommand;
    if(!slotCount(this.content.project.visual!,true)||previous>=0&&(command==='navigate'||command==='visit'||command==='advanceNarrative'&&previousPhase===state.phase))return this.reply();
    this.slotBusy=true;
    try{await this.writeSlot(true,0,null);return this.reply();}
    catch(error){return this.reply(this.words('자동 저장 실패: ','Automatic save failed: ','自動保存に失敗: ')+String(error));}
    finally{this.slotBusy=false;}
  }
  async slots(action:SaveSlotAction,version:Version|null):Promise<PlayerReply>{
    if(!this.hasSlots||!this.current(version))return this.reply('Game changed. Select again.');
    if(this.slotBusy)return this.reply();
    const design=this.content.project.visual!;
    if(action.type==='close'||action.type==='cancel'){this.slotView.confirmation=null;if(action.type==='close')this.slotView.screenId=null;return this.reply();}
    this.slotBusy=true;
    try{
      if(action.type==='export'){if(!this.session)throw new Error('No game to export.');await this.files.write(this.encode());return this.reply(null,this.words('백업 파일을 내려받았습니다.','Backup file downloaded.','バックアップをダウンロードしました。'));}
      if(action.type==='import'&&this.session){this.slotView.confirmation='import';return this.reply();}
      if(action.type==='import'||action.type==='confirm'&&this.slotView.confirmation==='import'){const text=await this.files.read();if(text===null)return this.reply();if(!this.current(version))throw new Error('Game changed while importing.');return this.restore(text);}
      if(action.type==='open'){
        const ids=slotScreens(design,action.kind),screenId=action.screenId??ids[0];
        if(!screenId||!ids.includes(screenId))throw new Error('Connect an authored save/load records modal to this project.');
        this.slotView={...this.slotView,screenId,automatic:false,selected:0,page:0,confirmation:null};
        await this.refreshSlots();return this.reply();
      }
      if(!this.slotView.screenId)throw new Error('Open the records window first.');
      if(action.type==='group'){this.slotView.automatic=action.automatic;this.slotView.selected=0;this.slotView.page=0;this.slotView.confirmation=null;await this.refreshSlots();return this.reply();}
      if(action.type==='page'){this.slotView.page=Math.max(0,Math.min(Math.ceil(slotCount(design,this.slotView.automatic)/6)-1,this.slotView.page+Math.sign(action.delta)));this.slotView.confirmation=null;return this.reply();}
      if(action.type==='select'){if(!Number.isInteger(action.index)||action.index<0||action.index>=slotCount(design,this.slotView.automatic))throw new Error('Invalid save slot.');this.slotView.selected=action.index;this.slotView.confirmation=null;return this.reply();}
      const saving=action.type==='save'||action.type==='confirm'&&this.slotView.confirmation==='save';
      if(action.type==='confirm'&&!this.slotView.confirmation)return this.reply();
      if(saving&&(!this.session||this.slotView.automatic))throw new Error('Automatic records are protected. Start a game to save a manual record.');
      if(!this.slotStorage)throw new Error('Browser save storage unavailable. Export a backup file.');
      let text:string|null=null;
      await this.slotStorage.update(rows=>{text=rows.find(r=>r.automatic===this.slotView.automatic&&r.index===this.slotView.selected)?.text??null;
        if(action.type==='confirm'&&text!==this.slotExpected)throw new Error('The record changed during confirmation. Select it again.');return null;});
      if(!saving&&(!text||this.preview({automatic:this.slotView.automatic,index:this.slotView.selected,text}).status!=='ready'))throw new Error('This record cannot be loaded.');
      if(action.type!=='confirm'&&(!saving||text!==null)){this.slotExpected=text;this.slotView.confirmation=saving?'save':'load';return this.reply();}
      this.slotView.confirmation=null;
      if(saving){await this.writeSlot(false,this.slotView.selected,action.type==='confirm'?this.slotExpected:null);return this.reply(null,this.words('이야기를 기록했습니다.','Record saved.','物語を保存しました。'));}
      return this.restore(text!);
    }catch(error){this.slotView.confirmation=null;return this.reply(error instanceof Error?error.message:String(error));}
    finally{this.slotBusy=false;}
  }

}

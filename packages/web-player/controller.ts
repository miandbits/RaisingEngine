import {usesPresentationVariants} from '../visual/variants.js';
import { scheduleStep } from '../player-service/schedule.js';
import { resolveUserValues } from '../core/user-fields.js';
import { CoreFault } from '../core/util.js';
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
type StateReply = { ok: true; state: RunState; transition?: Transition; relationshipChanges?: RelationshipChanges } | Failure;
type Projection = { ok: true; time: PlayerView['time']; narrative: PlayerView['narrative']; gameplay: PlayerView['gameplay']; systems: PlayerView['systems'] } | Failure;
const MAX_BYTES = 16 * 1024 * 1024;
const MAX_COMMANDS = 100000;

/** Stable comparison is independent of JSON object insertion order and host platform. */
function canonical(value: unknown): string {
  if (Array.isArray(value)) return `[${value.map(canonical).join(',')}]`;
  if (value && typeof value === 'object') return `{${Object.keys(value).sort().map(k => `${JSON.stringify(k)}:${canonical((value as Record<string, unknown>)[k])}`).join(',')}}`;
  return JSON.stringify(value);
}
function parseSave(text: string): unknown {
  if (new TextEncoder().encode(text).length > MAX_BYTES) throw new Error('세이브는 16 MiB 이하여야 합니다.');
  const errors: ParseError[] = [];
  const tree = parseTree(text, errors, { disallowComments: true, allowTrailingComma: false, allowEmptyContent: false });
  if (!tree || errors.length) throw new Error('올바른 JSON 세이브가 아닙니다.');
  const pending: { node: Node; depth: number }[] = [{ node: tree, depth: 0 }];
  while (pending.length) {
    const { node, depth } = pending.pop()!;
    if (depth > 128) throw new Error('세이브 구조가 너무 깊습니다.');
    if (node.type === 'object') {
      const keys = new Set<string>();
      for (const p of node.children ?? []) {
        const key = p.children?.[0]?.value as string;
        if (keys.has(key)) throw new Error(`세이브에 중복된 키가 있습니다: ${key}`);
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
  generation = 1;
  presentation = new PlayerPresentation();
  constructor(readonly content: ContentSnapshot, private readonly runtime: RuntimeRequest,
    private readonly files: WebFiles, private readonly validateSave: SaveValidator, readonly extensions: ExtensionHost | null = null) {}

  private projection(state = this.session?.state ?? null,content=this.content): Exclude<Projection, Failure> {
    const result = this.runtime.request({ operation: 'view', content, state }) as Projection;
    if (!result.ok) throw new Error(`[${result.error.code}] ${result.error.message}`);
    return result;
  }
  private playerView(): PlayerView {
    this.locale=localeId(this.content.project.localization,this.locale);
    const c = localizeContent(this.content,this.content.project.localization,this.locale), state = this.session?.state ?? null, cursor = state?.activeNarrative;
    const projection = this.projection(state,c);
    return { generation: this.generation, title: c.project.title, description: c.project.description ?? '', initialState: c.project.initialState,
      ...(c.project.userFields?.length ? { userFields: c.project.userFields, userValues: state?.userValues ?? Object.fromEntries(c.project.userFields.map(f => [f.id, f.initial])) } : {}),
      ...(c.project.localization?{localization:c.project.localization,locale:this.locale}:{}),
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
  async openProject() { return this.reply(null, '이 플레이어는 내보낸 게임 전용입니다.'); }
  async openYearSample() { return this.openProject(); }
  async newRun(version: Version | null, userValues?: Record<string, string>): Promise<PlayerReply> {
    if (!this.current(version)) return this.reply('화면이 갱신되었습니다. 다시 선택해 주세요.');
    // Validate before the WASM JSON boundary can omit undefined values or reject lone surrogates.
    try { resolveUserValues(this.content.project.userFields, userValues); }
    catch (error) { if (error instanceof CoreFault) { const issue = error.diagnostic(); return this.reply(`${issue.message} (${issue.path})`); } throw error; }
    const origin: RunOrigin = { seed: 42, mode: 'full', initialOverrides: { attributes: {}, flags: {}, relationships: {} } };
    const r = this.runtime.request({ operation: 'create', content: this.content, seed: origin.seed, options: { ...origin, ...(userValues !== undefined ? { userValues } : {}) } }) as StateReply;
    if (!r.ok) return this.reply(`[${r.error.code}] ${r.error.message} (${r.error.path ?? '/userValues'})`);
    if (this.content.project.userFields?.length) origin.userValues = structuredClone(userValues ?? {});
    this.session = { origin, commands: [], state: r.state }; this.presentation.clear(); this.extensions?.reset(); this.generation++;
    return this.reply();
  }
  private dispatch(command: unknown, cues: string[]): string | null {
    const session = this.session!;
    if (session.commands.length >= MAX_COMMANDS) return '명령 기록 한도를 초과했습니다.';
    // Single choke point for submitted commands: extensions observe every reply exactly once.
    const observing = this.extensions?.active ? this.extensions : null;
    const before = observing ? { state: session.state, narrative: this.projection(session.state).narrative } : null;
    const r = this.runtime.request({ operation: 'command', content: this.content, state: session.state,
      command, expectedRevision: session.state.stateRevision }) as StateReply;
    if (!r.ok) { const error = `[${r.error.code}] ${r.error.message} (${r.error.path ?? '/'})`; observing?.observe(before, before, command, error); return error; }
    if (!r.transition) throw new Error('런타임 명령 응답에 전이가 없습니다.');
    this.session = { origin: session.origin, commands: [...session.commands, r.transition.command], state: r.state,
      relationshipChanges:trackRelationshipChanges(session.relationshipChanges??{},session.state,r.state) };
    observing?.observe(before, { state: r.state, narrative: this.projection(r.state).narrative }, command, null);
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
    if (!this.current(version)) return this.reply('화면이 갱신되었습니다. 다시 선택해 주세요.');
    if (!this.session) return this.reply('새 게임을 시작해 주세요.');
    if(action.type==='scheduleStep'){const next=scheduleStep(this.session.state,this.content.project.schedulePlayback,this.presentation.view(this.content,this.session.state)); if(!next)return this.reply(); action={type:next};}
    if(['timelineTick','timelinePause','timelineSpeed','timelineSkip'].includes(action.type))return this.reply(this.presentation.act(this.content,this.session.state,action as TimelineAction));
    const timeline=this.presentation.view(this.content,this.session.state);
    if(timeline&&(action.type!=='continue'||!timeline.cursor.completed))return this.reply('활동 연출을 마치거나 건너뛴 뒤 다음으로 진행해 주세요.');
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
      case 'execute': command = { type: 'executeActivity' }; break;
      case 'choose': command = action; break;
      case 'continue': command = { type: this.session.state.phase === 'activity-result' ? 'acknowledgeResult'
        : this.session.state.phase === 'period-end' ? 'resolvePeriodEnd' : 'advanceNarrative' }; break;
      default: return this.reply('알 수 없는 동작입니다.');
    }
    const cues: string[] = [];
    const error = this.dispatch(command, cues) ?? (action.type==='commitPlan'?this.dispatch({type:'executeActivity'},cues):null) ?? this.settle(cues);
    return this.reply(error, null, cues);
  }
  async save(version: Version): Promise<PlayerReply> {
    if (!this.session || !this.current(version)) return this.reply('저장할 현재 게임이 없습니다.');
    const extensions=this.extensions?.save(usesPresentationVariants(this.content.project.visual));
    const presentation:PresentationEnvelope|undefined=this.presentation.view(this.content,this.session.state)||this.presentation.selectedBattle(this.content,this.session.state)||extensions?{...this.presentation.envelope(this.content,this.session.state),...(extensions?{extensions}:{})}:undefined;
    const text = canonical({ saveVersion: '1.0.0', coreVersion: CORE_VERSION,
      contentFingerprint: this.content.contentFingerprint, origin:this.session.origin,commands:this.session.commands,state:this.session.state,...(presentation?{playerPresentation:presentation}:{}) });
    if (new TextEncoder().encode(text).length > MAX_BYTES) return this.reply('세이브는 16 MiB 이하여야 합니다.');
    try { await this.files.write(text); return this.reply(null, '게임을 저장했습니다.'); }
    catch (e) { return this.reply(String(e)); }
  }
  async load(version: Version | null): Promise<PlayerReply> {
    if (!this.current(version)) return this.reply('화면이 갱신되었습니다. 다시 선택해 주세요.');
    try {
      const text = await this.files.read(); if (text === null) return this.reply();
      if (!this.current(version)) return this.reply('불러오는 동안 게임이 변경되었습니다. 다시 선택해 주세요.');
      const value = parseSave(text);
      // Plugin states are opaque to the save schema; validate them separately as a plain object.
      const {extensions:savedExtensions,...presentationRest}=((value as {playerPresentation?:PresentationEnvelope}).playerPresentation??{}) as PresentationEnvelope;
      if (savedExtensions !== undefined && (!savedExtensions || typeof savedExtensions !== 'object' || Array.isArray(savedExtensions))) throw new Error('세이브 데이터 구조가 올바르지 않습니다.');
      const checked = savedExtensions === undefined ? value : { ...(value as object), playerPresentation: presentationRest };
      if (!this.validateSave(checked)) throw new Error('세이브 데이터 구조가 올바르지 않습니다.');
      const saved = checked as RunSession & { saveVersion: string; coreVersion: string; contentFingerprint: string;playerPresentation?:PlayerPresentationEnvelope };
      if (saved.saveVersion !== '1.0.0' || saved.coreVersion !== CORE_VERSION) throw new Error('지원하지 않는 세이브 또는 코어 버전입니다.');
      if (saved.contentFingerprint !== this.content.contentFingerprint) throw new Error('다른 콘텐츠로 만든 세이브입니다.');
      if (saved.origin.mode !== 'full') throw new Error('활동 전용 세이브는 플레이어에서 열 수 없습니다.');
      const replayed = this.runtime.request({ operation: 'replay', content: this.content, origin: saved.origin, commands: saved.commands, includeRelationshipChanges:true }) as StateReply;
      if (!replayed.ok) throw new Error(`${replayed.error.message} (${replayed.error.path ?? '/'})`);
      const state = replayed.state;
      if (canonical(state) !== canonical(saved.state)) throw new Error('저장 상태가 명령 기록의 재현 결과와 다릅니다.');
      const previous = this.session;
      const previousPresentation=structuredClone(this.presentation.cursor);
    const previousScheduleSpeed=this.presentation.scheduleSpeed;
    const previousBattleSelection=structuredClone(this.presentation.battleSelection);
      const presentationError=this.presentation.restore(this.content,state,saved.playerPresentation);
      if(presentationError)throw new Error(presentationError);
      this.session = { origin: structuredClone(saved.origin), commands: structuredClone(saved.commands) as Command[], state, relationshipChanges:replayed.relationshipChanges??{} };
      try {
        const error = this.settle([]);
        if (error) { this.session = previous; this.presentation.cursor=previousPresentation;this.presentation.scheduleSpeed=previousScheduleSpeed; this.presentation.battleSelection=previousBattleSelection; return this.reply(error); }
      } catch (error) { this.session = previous; this.presentation.cursor=previousPresentation;this.presentation.scheduleSpeed=previousScheduleSpeed; this.presentation.battleSelection=previousBattleSelection; throw error; }
      this.extensions?.reset(); this.extensions?.scheduleRestore(savedExtensions, true);
      this.generation++; return this.reply(null, '저장한 게임을 불러왔습니다.');
    } catch (e) { return this.reply(e instanceof Error ? e.message : String(e)); }
  }
}

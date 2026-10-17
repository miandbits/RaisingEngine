import { scheduleStep } from '../player-service/schedule.js';
import {battleUiTypes,selectBattle,type BattleSelection,type BattleUiAction} from '../player-service/battle.js';
import { loadProject } from '../project-service/loader.js';
import { buildSnapshot } from '../project-service/snapshot.js';
import { calendarTime, getNarrativeView } from '../core/index.js';
import type { ContentSnapshot, RunState } from '../core/model.js';
import { createSession, dispatchSession, type RunSession } from '../runtime-service/session.js';
import { loadRun, saveRun } from '../runtime-service/save.js';
import type { GameplayCommand, GameplayView } from '../gameplay/model.js';
import type { SystemsCommand, SystemsView } from '../systems/model.js';
import { projectRuntime, runtimeBackend } from '../native-bridge/backend.js';
import {localizeContent,localeId,type Localization} from '../localization/catalog.js';
import {nativeRequest} from '../native-bridge/index.js';
import {projectDataUi,type DataUiProjection} from '../visual/data-ui-projection.js';
import {PlayerPresentation,type TimelineView,type TimelineAction} from './presentation.js';

export interface PlayerView {
  relationshipChanges?: RunSession['relationshipChanges'];
  userFields?: ContentSnapshot['project']['userFields'];
  userValues?: Readonly<Record<string, string>>;
  battleSelection?: BattleSelection|null;
  timeline?: TimelineView|null;
  dataUi?: DataUiProjection;
  localization?: Localization;
  locale?: string;
  initialState: ContentSnapshot['project']['initialState'];
  generation: number;
  title: string;
  description: string;
  durationMonths: number;
  periodsPerMonth?: number;
  schedulePlayback?: 'manual' | 'continuous';
  attributes: ContentSnapshot['attributes'];
  characters: ContentSnapshot['characters'];
  activities: ContentSnapshot['activities'];
  assets: ContentSnapshot['assets'];
  state: RunState | null;
  time: ReturnType<typeof calendarTime> | null;
  narrative: ReturnType<typeof getNarrativeView>;
  storyLabel: string | null;
  endingLabel: string | null;
  visual: ContentSnapshot['project']['visual'] | null;
  gameplayDefinition: ContentSnapshot['project']['gameplay'] | null;
  gameplay: GameplayView | null;
  systems: SystemsView | null;
  runtimeBackend: 'native' | 'typescript' | 'wasm';
}
export interface PlayerReply {
  view: PlayerView | null;
  error: string | null;
  notice: string | null;
  soundCues: readonly string[];
}
export interface Version { generation: number; revision: number }
export type PlayerAction = GameplayCommand | SystemsCommand | TimelineAction | BattleUiAction | { type: 'schedule'; activityIds: string[] }
  | {type:'executeActorAction';actorId:string;actionId:string;quantity:number}
  | {type:'assignActivities';assignments:Record<string,string>}
  | { type: 'commitPlan'; activityIds: string[] }
  | { type: 'scheduleStep' } | { type: 'execute' } | { type: 'continue' } | { type: 'choose'; choiceId: string };
export interface PlayerBridge {
  setLocale?(locale:string):Promise<PlayerReply>;
  view(): Promise<PlayerReply>;
  newRun(version: Version | null, userValues?: Record<string, string>): Promise<PlayerReply>;
  act(action: PlayerAction, version: Version): Promise<PlayerReply>;
  save(version: Version): Promise<PlayerReply>;
  load(version: Version | null): Promise<PlayerReply>;
  openProject(version: Version | null): Promise<PlayerReply>;
  openYearSample(version: Version | null): Promise<PlayerReply>;
}

/** The main process owns this controller. UI actions never mutate game state directly. */
export class PlayerController {
  content: ContentSnapshot | null = null;
  root: string | null = null;
  session: RunSession | null = null;
  generation = 0;
  presentation = new PlayerPresentation();

  view(): PlayerView | null {
    if (!this.content) return null;
    const c = localizeContent(this.content,this.content.project.localization);
    const state = this.session?.state ?? null;
    const cursor = state?.activeNarrative;
    const projection = projectRuntime(c,state);
    return {
      ...(c.project.localization?{localization:c.project.localization,locale:localeId(c.project.localization)}:{}),
      ...(c.project.userFields?.length ? { userFields: c.project.userFields, userValues: state?.userValues ?? Object.fromEntries(c.project.userFields.map(f => [f.id, f.initial])) } : {}),
      generation: this.generation, title: c.project.title, description: c.project.description ?? '', initialState: c.project.initialState,
      durationMonths: c.project.durationMonths, periodsPerMonth: c.project.periodsPerMonth, schedulePlayback: c.project.schedulePlayback ?? 'manual', attributes: c.attributes, characters: c.characters,
      activities: c.activities, assets: c.assets, state, relationshipChanges:this.session?.relationshipChanges,
      visual: c.project.visual ?? null,
      timeline: this.presentation.view(c,state),
      battleSelection:this.presentation.selectedBattle(c,state),
      dataUi: projectDataUi(c,state,projection.gameplay,nativeRequest, {systems:projection.systems,battleSelection:this.presentation.selectedBattle(c,state)}),
      gameplayDefinition: c.project.gameplay ?? null, gameplay: projection.gameplay,
      systems: projection.systems,
      runtimeBackend: runtimeBackend(),
      time: projection.time,
      narrative: projection.narrative,
      storyLabel: cursor ? (cursor.kind === 'event' ? c.events : c.endings).find(x => x.id === cursor.id)?.label ?? null : null,
      endingLabel: state?.endingId ? c.endings.find(x => x.id === state.endingId)?.label ?? null : null,
    };
  }

  reply(error: string | null = null, notice: string | null = null, soundCues: readonly string[] = []): PlayerReply {
    return { view: this.view(), error, notice, soundCues };
  }

  isCurrent(version: unknown): boolean {
    if (!this.session) return version === null;
    if (!version || typeof version !== 'object') return false;
    const v = version as Partial<Version>;
    return v.generation === this.generation && v.revision === this.session.state.stateRevision;
  }

  async openProject(root: string): Promise<PlayerReply> {
    const loaded = await loadProject(root);
    const built = await buildSnapshot(loaded);
    if (!built.ok) return this.reply(built.diagnostics.filter(d => d.severity === 'error')
      .map(d => `${d.file}:${d.line}:${d.column} ${d.pointer} [${d.code}] ${d.message}`).join('\n'));
    // Commit only after successful validation; failed open keeps the current game intact.
    this.content = built.snapshot;
    this.root = loaded.root;
    this.session = null;
    this.generation++;
    return this.reply();
  }

  newRun(version: unknown, userValues?: Record<string, string>): PlayerReply {
    if (!this.isCurrent(version)) return this.reply('화면이 갱신되었습니다. 다시 선택해 주세요.');
    if (!this.content) return this.reply('먼저 프로젝트를 열어 주세요.');
    const result = createSession(this.content, 42, userValues === undefined ? {} : { userValues });
    if (!result.ok) return this.reply(`${result.error.message} (${result.error.path})`);
    this.session = result.session;
    this.presentation.clear();
    this.generation++;
    return this.reply();
  }

  act(action: unknown, version: unknown): PlayerReply {
    if (!this.isCurrent(version)) return this.reply('화면이 갱신되었습니다. 다시 선택해 주세요.');
    if (!this.content || !this.session) return this.reply('새 게임을 시작해 주세요.');
    if (!action || typeof action !== 'object') return this.reply('알 수 없는 동작입니다.');
    let a = action as Partial<PlayerAction>;
    if(a.type==='scheduleStep'){const next=scheduleStep(this.session.state,this.content.project.schedulePlayback,this.presentation.view(this.content,this.session.state));if(!next)return this.reply();a={type:next};}
    if(['timelineTick','timelinePause','timelineSpeed','timelineSkip'].includes(a.type??''))return this.reply(this.presentation.act(this.content,this.session.state,a as TimelineAction));
    const timeline=this.presentation.view(this.content,this.session.state);
    if(timeline&&(a.type!=='continue'||!timeline.cursor.completed))return this.reply('활동 연출을 마치거나 건너뛴 뒤 다음으로 진행해 주세요.');
    let battleCommand:unknown;
    if(battleUiTypes.includes(a.type??'')){
      const selected=selectBattle(this.content,this.session.state,this.presentation.selectedBattle(this.content,this.session.state),a as BattleUiAction,nativeRequest);
      if(selected.error)return this.reply(selected.error);
      this.presentation.battleSelection=selected.selection;
      if(!selected.command)return this.reply();
      battleCommand=selected.command;
    }
    let command: unknown;
    switch (a.type) {
      case 'executeActorAction': case 'assignActivities': command=action; break;
      case 'confirmBattleAction': command=battleCommand; break;
      case 'navigate': case 'visit': case 'buy': case 'sell': case 'useService': case 'setDiet': case 'executeAction': case 'selectPolicy': case 'interact': command = action; break;
      case 'acceptQuest': case 'claimQuest': case 'equip': case 'unequip': case 'moveExpedition': case 'returnFromExpedition': case 'startNodeBattle': case 'battleAction': command = action; break;
      case 'schedule': case 'commitPlan': command = { type: 'setSchedule', activityIds: a.activityIds }; break;
      case 'execute': command = { type: 'executeActivity' }; break;
      case 'choose': command = { type: 'choose', choiceId: a.choiceId }; break;
      case 'continue': command = { type: this.session.state.phase === 'activity-result' ? 'acknowledgeResult'
        : this.session.state.phase === 'period-end' ? 'resolvePeriodEnd' : 'advanceNarrative' }; break;
      default: return this.reply('알 수 없는 동작입니다.');
    }
    const cues: string[] = [];
    const send = (next: unknown): string | null => {
      const r = dispatchSession(this.content!, this.session!, next, this.session!.state.stateRevision);
      if (!r.ok) return `[${r.error.code}] ${r.error.message} (${r.error.path})`;
      this.session = r.session;
      cues.push(...r.transition.soundCues);
      return null;
    };
    const firstError = send(command);
    if (firstError) return this.reply(firstError);
    if(a.type==='commitPlan'){
      const executionError=send({type:'executeActivity'});
      if(executionError)return this.reply(executionError,null,cues);
    }
    return this.settle(send, cues);
  }

  private settle(send: (command: unknown) => string | null, cues: string[]): PlayerReply {
    // Each script is acyclic; the bound additionally protects the host from excessive content.
    for (let step = 0; step < 10000; step++) {
      const state = this.session!.state;
      const node = projectRuntime(this.content!, state).narrative?.node;
      const command = state.phase === 'period-end' ? { type: 'resolvePeriodEnd' }
        : node && node.type !== 'dialogue' && node.type !== 'choice' ? { type: 'advanceNarrative' } : null;
      if (!command) return this.reply(null, null, cues);
      const error = send(command);
      if (error) return this.reply(error, null, cues);
    }
    return this.reply('자동 진행 한도를 초과했습니다. 대본을 확인해 주세요.', null, cues);
  }

  async save(file: string, version: unknown): Promise<PlayerReply> {
    if (!this.session || !this.content || !this.isCurrent(version)) return this.reply('저장할 현재 게임이 없습니다.');
    const presentation=this.presentation.view(this.content,this.session.state)||this.presentation.selectedBattle(this.content,this.session.state)?this.presentation.envelope(this.content,this.session.state):undefined;
    const saved = await saveRun(file, this.content, this.session,presentation);
    return saved.ok ? this.reply(null, '게임을 저장했습니다.') : this.reply(`[${saved.error.code}] ${saved.error.message}`);
  }

  async load(file: string, version: unknown): Promise<PlayerReply> {
    if (!this.isCurrent(version)) return this.reply('화면이 갱신되었습니다. 다시 선택해 주세요.');
    if (!this.content) return this.reply('먼저 프로젝트를 열어 주세요.');
    const loaded = await loadRun(file, this.content);
    if(!this.isCurrent(version))return this.reply('불러오는 동안 게임이 변경되었습니다. 다시 선택해 주세요.');
    if (!loaded.ok) return this.reply(`[${loaded.error.code}] ${loaded.error.message}`);
    if (loaded.session.state.mode !== 'full') return this.reply('활동 전용 세이브는 플레이어에서 열 수 없습니다.');
    // Normalize headless checkpoints too. Restoring itself emits no previous sound cues.
    let candidate = loaded.session;
    const previous = this.session;
    const previousPresentation=structuredClone(this.presentation.cursor);
    const previousScheduleSpeed=this.presentation.scheduleSpeed;
    const previousBattleSelection=structuredClone(this.presentation.battleSelection);
    const presentationError=this.presentation.restore(this.content,loaded.session.state,loaded.playerPresentation);
    if(presentationError)return this.reply(presentationError);
    this.session = candidate;
    const reply = this.settle(command => {
      const r = dispatchSession(this.content!, candidate, command, candidate.state.stateRevision);
      if (!r.ok) return r.error.message;
      candidate = r.session;
      this.session = candidate;
      return null;
    }, []);
    if (reply.error) { this.session = previous; this.presentation.cursor=previousPresentation;this.presentation.scheduleSpeed=previousScheduleSpeed; this.presentation.battleSelection=previousBattleSelection; return this.reply(reply.error); }
    this.generation++;
    return this.reply(null, '저장한 지점에서 이어갑니다.');
  }
}

import {transitionRoute,addRoute} from './routes.js';
import { createRun, dispatchCommand } from '../native-bridge/backend.js';
import { evaluateCondition, evaluateNumber } from '../core/evaluation.js';
import type { Command, ContentSnapshot, RunState, Transition } from '../core/model.js';
import { emptyResult, validateOptions, type AnalysisOptions, type AnalysisResult, type Trace } from './model.js';
import { object } from './catalog.js';

type Candidate = { state: RunState; commands: Command[]; transitions: Readonly<Transition>[] };
const add = (map: Record<string, number>, key: string) => { map[key] = (map[key] ?? 0) + 1; };
export function applyCommands(content: ContentSnapshot, state: RunState, commands: Command[]): Candidate | null {
  const transitions: Readonly<Transition>[] = [];
  for (const command of commands) {
    const next = dispatchCommand(content, state, command, state.stateRevision);
    if (!next.ok) return null;
    state = next.state; transitions.push(next.transition as Readonly<Transition>);
  }
  return { state, commands, transitions };
}
function candidates(content: ContentSnapshot, state: RunState, reasons: Record<string, number>,random?:()=>number,budget=Infinity): Candidate[] {
  const result: Candidate[] = [];
  const pending:Command[][]=[];
  const attempt = (commands: Command[]) => {pending.push(commands);};
  const execute = (commands: Command[]) => {
    let next = state; const transitions: Readonly<Transition>[] = [];
    for (const command of commands) {
      const r = dispatchCommand(content, next, command, next.stateRevision);
      if (!r.ok) { add(reasons, r.error.code); return; }
      next = r.state; transitions.push(r.transition as Readonly<Transition>);
    }
    result.push({ state: next, commands, transitions });
  };
  if (state.phase === 'planning' || state.phase === 'ready') {
    const systems = content.project.systems;
    if (systems && state.systems) {
      for (const quest of systems.quests) {
        attempt([{ type: 'acceptQuest', questId: quest.id }]);
        attempt([{ type: 'claimQuest', questId: quest.id }]);
      }
      for (const equipment of systems.equipment) attempt([{ type: 'equip', equipmentId: equipment.id }]);
      for (const slot of ['weapon', 'armor', 'accessory'] as const) if (state.systems.equipment[slot]) attempt([{ type: 'unequip', slot }]);
    }
    const gameplay = content.project.gameplay;
    if (gameplay && state.gameplay) {
      const location = gameplay.locations.find(x => x.id === state.gameplay!.locationId)!;
      // Visits and single-unit purchases compose into every reachable gameplay transaction.
      // Pure screen navigation is omitted: no rule reads screenId and visit selects its screen.
      for (const id of [...location.connections].sort()) if (id !== location.id) attempt([{ type: 'visit', locationId: id }]);
      for (const shop of gameplay.shops.filter(x => x.locationId === location.id)) for (const offer of shop.offers)
        attempt([{ type: 'buy', shopId: shop.id, itemId: offer.itemId, quantity: 1 }]);
      for(const shop of gameplay.shops.filter(x=>x.locationId===location.id))for(const offer of shop.sellOffers??[])attempt([{type:'sell',shopId:shop.id,itemId:offer.itemId,quantity:1}]);
      for(const service of gameplay.services??[])attempt([{type:'useService',serviceId:service.id,quantity:1}]);
      for(const action of gameplay.actions??[])attempt([{type:'executeAction',actionId:action.id,quantity:1}]);
      for(const group of gameplay.policyGroups??[]){for(const option of group.options)attempt([{type:'selectPolicy',groupId:group.id,optionId:option.id}]);if(state.gameplay.policySelections?.[group.id])attempt([{type:'selectPolicy',groupId:group.id,optionId:null}]);}
      for(const diet of gameplay.diets??[])attempt([{type:'setDiet',dietId:diet.id}]);
      for (const interaction of gameplay.interactions.filter(x => x.locationId === location.id))
        attempt([{ type: 'interact', interactionId: interaction.id }]);
    }
    if (content.project.roster && state.roster) {
      if(content.project.roster.stateModel==='character-v1')for(const member of content.project.roster.members)for(const actionId of member.actionIds??[])attempt([{type:'executeActorAction',actorId:member.characterId,actionId,quantity:1}]);
      const activities = [...content.activities].filter(a => !a.expeditionId).sort((a,b)=>a.id.localeCompare(b.id));
      const members = content.project.roster.members;
      // Bound the branching factor; this is sampled search, not an impossibility proof.
      const count = Math.min(256, Math.pow(activities.length, members.length));
      for(let n=0;n<count;n++) {
        let code=n;
        const assignments: Record<string,string> = {};
        for(const member of members) {
          const i=random ? Math.floor(random()*activities.length) : code%activities.length;
          assignments[member.characterId]=activities[i]!.id;code=Math.floor(code/activities.length);
        }
        attempt([{type:'assignActivities',assignments},{type:'executeActivity'}]);
      }
    } else for (const activity of [...content.activities].sort((a, b) => a.id.localeCompare(b.id))) {
      const ids = [...(state.schedule ?? Array<string>(content.project.periodsPerMonth).fill(activity.id))] as string[];
      ids[state.periodIndex % content.project.periodsPerMonth] = activity.id;
      attempt([{ type: 'setSchedule', activityIds: ids }, { type: 'executeActivity' }]);
    }
  } else if (state.phase === 'expedition' && state.systems?.expedition) {
    const trip = state.systems.expedition;
    const expedition = content.project.systems!.expeditions.find(x => x.id === trip.expeditionId)!;
    const node = expedition.nodes.find(n => n.id === trip.nodeId)!;
    for (const nodeId of [...node.connections].sort()) attempt([{ type: 'moveExpedition', nodeId }]);
    attempt([{ type: 'returnFromExpedition' }]);
  } else if (state.phase === 'battle') {
    for (const action of ['attack', 'guard', 'flee'] as const) attempt([{ type: 'battleAction', action }]);
    for (const item of content.project.systems!.consumables) attempt([{ type: 'battleAction', action: 'useItem', itemId: item.itemId }]);
  } else if (state.phase === 'narrative') {
    const cursor = state.activeNarrative!;
    const story = (cursor.kind === 'event' ? content.events : content.endings).find(e => e.id === cursor.id)!;
    const node = story.nodes.find(n => n.id === cursor.nodeId)!;
    if (node.type === 'choice') for (const choice of node.choices) attempt([{ type: 'choose', choiceId: choice.id }]);
    else attempt([{ type: 'advanceNarrative' }]);
  } else if (state.phase === 'activity-result') attempt([{ type: 'acknowledgeResult' }]);
  else if (state.phase === 'period-end') attempt([{ type: 'resolvePeriodEnd' }]);
  if(random)for(let i=pending.length-1;i>0;i--){const j=Math.floor(random()*(i+1));[pending[i],pending[j]]=[pending[j]!,pending[i]!];}
  for(const commands of pending){if(commands.length>budget)continue;execute(commands);if(random&&result.length)break;}
  return result;
}
function advance(content: ContentSnapshot, trace: Trace, candidate: Candidate): Trace {
  const events = new Set(trace.events), choices = new Set(trace.choices), nodes = new Set(trace.nodes); const eventOrder = [...trace.eventOrder];
  for (const t of candidate.transitions) {
    for (const e of t.eventDecisions) if (e.reason === 'selected') { events.add(e.eventId); eventOrder.push(e.eventId); }
    if (t.narrative) {
      const n = t.narrative; const prefix = `${n.cursor.kind}:${n.cursor.id}/${n.cursor.nodeId}`;
      nodes.add(prefix); if (n.choiceId) choices.add(`${prefix}/${n.choiceId}`);
    }
  }
  const cursor = candidate.state.activeNarrative;
  if (cursor) {
    nodes.add(`${cursor.kind}:${cursor.id}/${cursor.nodeId}`);
    if (cursor.kind === 'event') { if (!events.has(cursor.id)) eventOrder.push(cursor.id); events.add(cursor.id); }
  }
  return { ...trace, route:[...(trace.route??[]),...candidate.transitions.flatMap((t,i)=>transitionRoute(t,trace.commands.length+i+1,content))], state: candidate.state, commands: [...trace.commands, ...candidate.commands], events: [...events], choices: [...choices], nodes: [...nodes], eventOrder };
}
function initial(content: ContentSnapshot, seed: number, run: number): Trace {
  const r = createRun(content, seed, { mode: 'full' });
  if (!r.ok) throw new Error(r.error.message);
  return { route:[{nodeId:'start',label:'시작',kind:'start',step:0,periodIndex:0,chance:null,roll:null,passed:null}], run, seed, commands: [], state: r.state, outcome: 'limit', reason: '', events: [], choices: [], nodes: [], eventOrder: [] };
}
function reached(trace: Trace, target: AnalysisOptions['target']): boolean {
  if (!target) return false;
  if (target.kind === 'ending') return trace.state.phase === 'finished' && trace.state.endingId === target.id;
  if (target.kind === 'event') return trace.events.includes(target.id);
  return trace.choices.includes(`event:${target.id}/${target.nodeId}/${target.choiceId}`);
}
function distance(content: ContentSnapshot, state: RunState, raw: unknown, depth = 0): number {
  const c = object(raw);
  try { if (evaluateCondition(content, state, c as any)) return 0; } catch { return 1; }
  if (depth > 8) return 100;
  if (c.op === 'all') return c.conditions.reduce((n: number, v: unknown) => n + distance(content, state, v, depth + 1), 0);
  if (c.op === 'any') return Math.min(...c.conditions.map((v: unknown) => distance(content, state, v, depth + 1)));
  if (c.op === 'flag') {
    const scores: number[] = [];
    for (const event of content.events) {
      const visit = (value: unknown, prerequisites: unknown[]) => {
        if (!value || typeof value !== 'object') return;
        const obj = object(value); const conditions = obj.requirements ? [...prerequisites, obj.requirements] : prerequisites;
        if (obj.op === 'setFlag' && obj.flagId === c.flagId && obj.value === c.equals) scores.push(1 + (state.periodIndex > event.window.throughPeriod ? 10000 : 0) + conditions.reduce<number>((s, x) => s + distance(content, state, x, depth + 1), 0));
        for (const child of Object.values(value)) visit(child, conditions);
      };
      visit(event, []);
    }
    if (scores.length) return Math.min(...scores);
  }
  if (c.op === 'eventCompleted') { const event = content.events.find(e => e.id === c.eventId); if (event) return 1 + distance(content, state, event.requirements, depth + 1); }
  if (c.op === 'compare') {
    const a = evaluateNumber(content, state, c.left), b = evaluateNumber(content, state, c.right);
    return Math.min(1000, Math.abs(a - b) + 1);
  }
  return 100;
}

/** A search hint only: ignores locked paths and never serves as an impossibility proof. */
function gameplayDistance(content: ContentSnapshot, state: RunState, eventId: string): number {
  const definition = content.project.gameplay, gameplay = state.gameplay;
  if (!definition || !gameplay || state.activeNarrative?.id === eventId || state.completedEvents.includes(eventId)) return 0;
  const linked = definition.interactions.filter(x => x.eventId === eventId);
  if (!linked.length) return 0;
  const hops = (from: string, to: string): number => {
    const seen = new Set([from]); let frontier = [from], count = 0;
    while (frontier.length) {
      if (frontier.includes(to)) return count;
      const next: string[] = [];
      for (const id of frontier) for (const id2 of definition.locations.find(x => x.id === id)?.connections ?? [])
        if (!seen.has(id2)) { seen.add(id2); next.push(id2); }
      frontier = next; count++;
    }
    return 1000;
  };
  return Math.min(...linked.map(interaction => {
    let score = hops(gameplay.locationId, interaction.locationId);
    if (interaction.itemId && (gameplay.inventory[interaction.itemId] ?? 0) < (interaction.quantity ?? 1)) {
      const missing = (interaction.quantity ?? 1) - (gameplay.inventory[interaction.itemId] ?? 0);
      const suppliers = definition.shops.filter(s => s.offers.some(o => o.itemId === interaction.itemId));
      score = suppliers.length ? Math.min(...suppliers.map(shop => hops(gameplay.locationId, shop.locationId) + missing + hops(shop.locationId, interaction.locationId))) : 1000;
    }
    if (interaction.requirements) score += distance(content, state, interaction.requirements);
    if (interaction.oncePerPeriod && gameplay.interactionPeriods[interaction.id] === state.periodIndex) score += 10;
    return score;
  }));
}
/** Bounded-search ordering only, not an impossibility proof. */
function systemsDistance(content: ContentSnapshot, state: RunState, eventId: string): number {
  const definition = content.project.systems, sys = state.systems;
  if (!definition || !sys || state.activeNarrative?.id === eventId || state.completedEvents.includes(eventId)) return 0;
  const stages = definition.relationshipStages.filter(s => s.eventId === eventId);
  if (!stages.length) return 0;
  return Math.min(...stages.map(stage => {
    if (sys.unlockedStages.includes(stage.id)) return 0;
    const gap = Math.max(0, stage.threshold - state.relationships[stage.characterId]![stage.relationshipId]!);
    const quests = definition.quests.filter(q => q.rewards.effects.some(e => e.op === 'add' && e.target.kind === 'relationship' && e.target.characterId === stage.characterId && e.target.relationshipId === stage.relationshipId));
    if (!quests.length) return gap;
    return gap + Math.min(...quests.map(q => {
      const saved = Object.hasOwn(sys.quests, q.id) ? sys.quests[q.id] : undefined;
      if (!saved) return 30;
      if (saved.status === 'claimed') return 40;
      if (saved.status === 'completed') return sys.expedition ? 2 : 1;
      let score = 20 + (q.targetCount - saved.progress) * 5;
      if (sys.expedition) {
        if (sys.battle?.enemyId === q.enemyId) score = 4 + sys.battle.enemyHp / Math.max(1, sys.expedition.stats.attack);
        else {
          const expedition = definition.expeditions.find(e => e.id === sys.expedition!.expeditionId)!;
          const targets = expedition.nodes.filter(n => n.enemyId === q.enemyId && !sys.expedition!.clearedNodes.includes(n.id)).map(n => n.id);
          const seen = new Set<string>(), pending: [string, number][] = [[sys.expedition.nodeId, 0]];
          while (pending.length) { const [id, hops] = pending.shift()!; if (seen.has(id)) continue; seen.add(id); if (targets.includes(id)) { score = 6 + hops; break; } for (const next of expedition.nodes.find(n => n.id === id)!.connections) pending.push([next, hops + 1]); }
        }
      }
      // Nonnegative equipment bonuses make owned equipment a useful first step.
      const equipped = Object.values(sys.equipment).filter(Boolean).length;
      return score - equipped * 0.25;
    }));
  }));
}
// A deliberately narrow proof: only comparisons between declared attribute bounds and literals.
export function impossibilityProof(content: ContentSnapshot, target: NonNullable<AnalysisOptions['target']>): string | null {
  if (target.kind === 'ending' && target.id === content.project.defaultEndingId) return null;
  const story = (target.kind === 'ending' ? content.endings : content.events).find(x => x.id === target.id)!;
  function impossible(raw: unknown): boolean {
    const c = object(raw);
    if (c.op === 'all') return c.conditions.some(impossible);
    if (c.op === 'any') return c.conditions.every(impossible);
    if (c.op !== 'compare' || c.left?.op !== 'read' || c.left.target?.kind !== 'attribute' || c.right?.op !== 'literal') return false;
    const a = content.attributes.find(x => x.id === c.left.target.attributeId); if (!a) return false;
    const n = c.right.value;
    return c.comparator === 'gt' ? a.max <= n : c.comparator === 'gte' ? a.max < n : c.comparator === 'lt' ? a.min >= n : c.comparator === 'lte' ? a.min > n : c.comparator === 'eq' ? n < a.min || n > a.max : c.comparator === 'ne' ? a.min === n && a.max === n : false;
  }
  if (impossible(story.requirements)) return `${target.id}: 선언된 능력·자원 범위에서 요구 조건을 만족할 수 없습니다. ${JSON.stringify(story.requirements)}`;
  if (target.kind === 'choice') {
    const node = story.nodes.find(n => n.type === 'choice' && n.id === target.nodeId);
    if (node?.type === 'choice') { const choice = node.choices.find(c => c.id === target.choiceId); if (choice && impossible(choice.requirements)) return `${target.id}/${target.nodeId}/${target.choiceId}: 선택 조건이 선언 범위를 벗어납니다.`; }
  }
  return null;
}
export function record(result: AnalysisResult, trace: Trace, target: AnalysisOptions['target']) {
  result.completed++; add(result.outcomes, trace.outcome);if(result.routeGraph)addRoute(result.routeGraph,trace);
  const novel = trace.events.some(e => !result.events[e]) || trace.choices.some(c => !result.choices[c]) || !!trace.state.endingId && !result.endings[trace.state.endingId];
  trace.events.forEach(e => add(result.events, e)); trace.choices.forEach(c => add(result.choices, c)); trace.nodes.forEach(n => add(result.nodes, n));
  if (trace.state.phase === 'finished' && trace.state.endingId) add(result.endings, trace.state.endingId);
  for (const [id, value] of Object.entries(trace.state.attributes)) {
    const a = result.attributes[id] ?? { min: value, max: value, sum: 0 }; a.min = Math.min(a.min, value); a.max = Math.max(a.max, value); a.sum += value; result.attributes[id] = a;
  }
  if (reached(trace, target)) { result.targetStatus = 'found'; result.traces.unshift(trace); result.traces = result.traces.slice(0, 30); }
  else if (result.traces.length < 30 && (novel || trace.outcome !== 'finished' || result.traces.length < 3)) result.traces.push(trace);
}
export interface RunHooks { trace?:(trace:Trace)=>void|Promise<void>; cancelled?: () => boolean; progress?: (result: AnalysisResult) => void|Promise<void>; now?: () => number }
export async function runAnalysis(content: ContentSnapshot, input: AnalysisOptions, hooks: RunHooks = {}): Promise<AnalysisResult> {
  const options = validateOptions(input, content); const result = emptyResult(options.runs); const now = hooks.now ?? Date.now; const started = now();
  const stopped = () => { if (hooks.cancelled?.()) { result.stopReason = 'cancelled'; return true; } if (now() - started >= options.timeoutMs) { result.stopReason = 'time-limit'; return true; } return false; };
  const finish=async(trace:Trace)=>{await hooks.trace?.(trace);record(result,trace,options.target);};
  const pulse = async () => { await hooks.progress?.(result); await new Promise<void>(r => setImmediate(r)); };
  if (options.target) { result.targetStatus = 'not-found'; result.proof = impossibilityProof(content, options.target); if (result.proof) { result.targetStatus = 'impossible'; return result; } }
  for (let run = 0; run < options.runs && !stopped(); run++) {
    const seed = (options.seed + run) >>> 0; let rng = (seed ^ 0x9e3779b9) >>> 0;
    const random = () => { rng = (Math.imul(rng, 1664525) + 1013904223) >>> 0; return rng / 4294967296; };
    let beam = [initial(content, seed, run)]; let last = beam[0]!;
    const seen = new Set<string>(); let iterations = 0;
    while (beam.length && !stopped()) {
      const expanded: Trace[] = [];
      for (const trace of beam) {
        last = trace;
        if (reached(trace, options.target)) { trace.outcome = 'target'; await finish(trace); await hooks.progress?.(result); return result; }
        if (trace.state.phase === 'finished') { trace.outcome = 'finished'; if (!options.target) { await finish(trace); beam = []; break; } continue; }
        if (trace.commands.length >= options.maxCommands) { trace.reason = '명령 수 제한'; continue; }
        if (options.target && result.exploredStates >= (options.maxStates ?? 5000)) { result.stopReason = 'state-limit'; await finish(trace); return result; }
        result.exploredStates++;
        const next = candidates(content, trace.state, result.blocked,!options.target&&options.strategy==='random'?random:undefined,options.maxCommands-trace.commands.length).filter(c => trace.commands.length + c.commands.length <= options.maxCommands);
        if (!next.length) { trace.outcome = 'blocked'; trace.reason = '실행 가능한 활동/선택 없음 또는 명령 예산 부족'; if (!options.target) { await finish(trace); beam = []; break; } continue; }
        if (options.target) {
          for (const candidate of next) {
            const t = advance(content, trace, candidate);
            // Keep RNG, flags, relationships, schedule and presentation in the identity. Revision does not change gameplay.
            const key = JSON.stringify({ ...t.state, stateRevision: 0 });
            if (!seen.has(key)) { seen.add(key); expanded.push(t); }
          }
        } else {
          const scored = next.map(c => {
            let score = random();
            if (options.strategy === 'focus') score += (c.state.attributes[options.attributeId!]! - trace.state.attributes[options.attributeId!]!) * 100;
            if (options.strategy === 'conserve') score += content.attributes.filter(a => a.kind === 'resource').reduce((s, a) => s + (c.state.attributes[a.id]! - trace.state.attributes[a.id]!) / Math.max(1, a.max - a.min), 0) * 100;
            if (options.strategy === 'explore') {
              const t = advance(content, trace, c);
              score += t.choices.filter(k => !result.choices[k]).length * 100 + t.events.filter(k => !result.events[k]).length * 20;
              score += (c.state.gameplay?.visited.filter(id => !trace.state.gameplay?.visited.includes(id)).length ?? 0) * 10;
              if (c.commands.some(command => command.type === 'interact')) score += 2;
              if (c.state.systems && trace.state.systems) {
                score += (c.state.systems.unlockedStages.length - trace.state.systems.unlockedStages.length) * 30;
                for (const [id, quest] of Object.entries(c.state.systems.quests)) {
                  const previous = Object.hasOwn(trace.state.systems.quests, id) ? trace.state.systems.quests[id] : undefined;
                  if (!previous) score += 8;
                  else score += (quest.progress - previous.progress) * 10 + (quest.status === 'claimed' && previous.status !== 'claimed' ? 20 : 0);
                }
                if (c.commands.some(command => command.type === 'battleAction' && command.action === 'attack')) score += 1;
              }
            }
            return { c, score };
          }).sort((a, b) => b.score - a.score);
          expanded.push(advance(content, trace, scored[0]!.c));
        }
      }
      if (!beam.length) break;
      if (options.target) {
        const target = (options.target.kind === 'ending' ? content.endings : content.events).find(x => x.id === options.target!.id)!;
        const competitors = options.target.kind === 'ending' ? content.endings.filter(e => e.id !== target.id && (e.priority > target.priority || e.priority === target.priority && e.id < target.id)) : [];
        const score = (t: Trace) => distance(content, t.state, target.requirements) * 100
          + (options.target!.kind !== 'ending' ? gameplayDistance(content, t.state, target.id) * 10 : 0)
          + (options.target!.kind !== 'ending' ? systemsDistance(content, t.state, target.id) * 10 : 0)
          + (options.target!.kind !== 'ending' && t.state.periodIndex > content.events.find(e => e.id === target.id)!.window.throughPeriod ? 1000000 : 0)
          + competitors.reduce((sum,e) => sum + 10 / (1 + distance(content,t.state,e.requirements)), 0);
        const ranked = expanded.map(t => ({ t, score: score(t) }));
        ranked.sort((a, b) => Number(reached(b.t, options.target)) - Number(reached(a.t, options.target)) || a.score - b.score || b.t.state.periodIndex - a.t.state.periodIndex);
        expanded.splice(0, expanded.length, ...ranked.map(r => r.t));
      }
      beam = expanded.slice(0, options.target ? options.beamWidth ?? 12 : 1);
      if (++iterations % 16 === 0) await pulse();
    }
    if(result.stopReason!=='completed'&&last.outcome==='limit')last.reason=result.stopReason==='cancelled'?'사용자가 중단했습니다.':'분석 시간 제한';
    if (options.target || result.completed <= run) await finish(last);
    await pulse();
  }
  return result;
}

export function replayTrace(content: ContentSnapshot, trace: Trace, steps = trace.commands.length) {
  if (!Number.isSafeInteger(steps) || steps < 0 || steps > trace.commands.length) throw new Error('재생 명령 위치가 범위를 벗어납니다.');
  const first = createRun(content, trace.seed, { mode: 'full' }); if (!first.ok) throw new Error(first.error.message);
  const replay = applyCommands(content, first.state, trace.commands.slice(0, steps)); if (!replay) throw new Error('재현 명령 실행에 실패했습니다.');
  if (steps === trace.commands.length && JSON.stringify(replay.state) !== JSON.stringify(trace.state)) throw new Error('기록된 최종 상태와 재현 결과가 다릅니다.');
  return replay;
}

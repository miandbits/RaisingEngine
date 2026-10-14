import { fileURLToPath } from 'node:url';
import { loadProject, buildSnapshot } from '../dist/packages/project-service/index.js';
import { createRun, dispatchCommand, calendarTime } from '../dist/packages/core/index.js';

const loaded = await loadProject(fileURLToPath(new URL('../examples/academy-demo', import.meta.url)));
const built = await buildSnapshot(loaded);
if (!built.ok) throw new Error(JSON.stringify(built.diagnostics));
const created = createRun(built.snapshot, 42, { mode: 'activities-only' });
if (!created.ok) throw new Error(JSON.stringify(created.error));
let state = created.state;
const activities = [];
function command(input) {
  const result = dispatchCommand(built.snapshot, state, input, state.stateRevision);
  if (!result.ok) throw new Error(JSON.stringify(result.error));
  state = result.state;
  if (result.transition.activity) activities.push({ periodIndex: state.periodIndex, activityId: result.transition.activity.activityId, attributes: state.attributes });
}
for (const activityIds of [['study', 'work', 'rest'], ['study', 'rest', 'study']]) {
  command({ type: 'setSchedule', activityIds });
  for (let period = 0; period < 3; period++) {
    command({ type: 'executeActivity' });
    command({ type: 'acknowledgeResult' });
    command({ type: 'completePeriodWithoutEvents' });
  }
}
console.log(JSON.stringify({ mode: state.mode, note: '활동만 실행합니다. 사건·대본·엔딩·시나리오 해석은 4단계 범위입니다.',
  contentFingerprint: state.contentFingerprint, seed: state.rng.seed, activities,
  final: { phase: state.phase, time: calendarTime(built.snapshot, state), revision: state.stateRevision, attributes: state.attributes, randomDraws: state.rng.draws } }, null, 2));

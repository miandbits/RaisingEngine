import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {runGoalRoute} from '../../dist/packages/analysis-service/goal-routes.js';
import {checkGoalImpacts, routeDigest} from './goal-impact.mjs';
import {listGoalRoutes} from './goal-routes.mjs';
import {goalBaselineSelection} from './goal-approvals.mjs';
import {listGoalBaselines, readGoalBaseline, runtimeIdentity} from './goal-baselines.mjs';

async function snapshot(root) {
  const built = await buildSnapshot(await loadProject(root));
  if (!built.ok) throw Error('프로젝트 검증 실패: ' + JSON.stringify(built.diagnostics));
  return built.snapshot;
}

// Same comparison and approval service as the editor. This never creates a
// baseline or accepts a change; CI must use the reviewed files committed to Git.
export async function runGoalRegression(root, {timeoutMs = 120000, runtimePolicy = 'strict'} = {}) {
  if (!Number.isSafeInteger(timeoutMs) || timeoutMs < 1 || timeoutMs > 3600000) throw Error('timeout-ms 범위는 1~3600000입니다.');
  const started = Date.now(), content = await snapshot(root);
  const remaining = () => Math.max(1, timeoutMs - (Date.now() - started));
  const impact = await checkGoalImpacts(root, content, content.contentFingerprint, {timeoutMs: remaining(), runtimePolicy});
  const entries = await listGoalRoutes(root);
  const report = {...impact, kind: 'goal-regression', project: root, runtimePolicy, errors: [], durationMs: 0};
  if (!report.rows.length) report.errors.push('등록된 경로가 없습니다. 검사를 생략하여 통과할 수 없습니다.');
  for (const row of report.rows) {
    const route = entries.find(e => e.route?.id === row.routeId)?.route;
    if (!route || ['error', 'incomplete', 'missing-baseline'].includes(row.status)) continue;
    if (Date.now() - started >= timeoutMs) {
      row.status = 'incomplete'; row.error = '중간 저장·불러오기 검사 시간 제한'; continue;
    }
    try {
      row.replay = await runGoalRoute(content, route, {timeoutMs: remaining()});
      if (!row.replay.ok) row.status = row.replay.failure?.code === 'TIMEOUT' ? 'incomplete' : 'failed';
    } catch (error) { row.status = 'error'; row.error = error.message; }
  }
  // Detect edits during a local/CI run instead of publishing a mixed result.
  const latest = await listGoalRoutes(root);
  const digest = rows => routeDigest(rows.map(e => e.route ?? {file: e.file.split(/[\\/]/).at(-1), error: e.error}));
  if (digest(latest) !== report.routesDigest) report.errors.push('검사 중 등록 경로가 변경되었습니다.');
  const baselines = await listGoalBaselines(root);
  if (baselines.some(b => b.error)) report.errors.push('손상된 기준 기록이 있습니다.');
  if ((await goalBaselineSelection(root, baselines)).revision !== report.selectionRevision) report.errors.push('검사 중 승인 기준이 변경되었습니다.');
  for (const row of report.rows.filter(r => r.baselineChecksum)) {
    if ((await readGoalBaseline(root, row.baselineId)).checksum !== row.baselineChecksum) report.errors.push(`검사 중 기준 기록이 변경되었습니다: ${row.routeId}`);
  }
  if ((await snapshot(root)).contentFingerprint !== report.fingerprint || await runtimeIdentity() !== report.runtime) report.errors.push('검사 중 콘텐츠 또는 런타임이 변경되었습니다.');
  report.counts = Object.fromEntries(['failed', 'changed', 'unchanged', 'missing-baseline', 'error', 'incomplete'].map(s => [s, report.rows.filter(r => r.status === s).length]));
  report.complete = !report.errors.length && !report.baselineErrors.length && !report.counts.error && !report.counts.incomplete && !report.counts['missing-baseline'];
  report.ok = report.complete && !report.counts.failed && !report.counts.changed;
  report.exitCode = report.ok ? 0 : report.complete ? 1 : 2;
  report.durationMs = Date.now() - started;
  return report;
}

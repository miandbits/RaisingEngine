import {readFile, stat, realpath} from 'node:fs/promises';
import path from 'node:path';
import {randomUUID, createHash} from 'node:crypto';
import {runGoalRegression} from './goal-regression.mjs';
import {publishGoalImpacts} from './goal-impact.mjs';

const statuses = new Set(['failed', 'changed', 'unchanged', 'missing-baseline', 'error', 'incomplete']);
const plain = v => v && typeof v === 'object' && !Array.isArray(v);
export class GoalCiReports {
  current = null;
  async open(editor, file, revision) {
    editor.current(revision);
    if (typeof file !== 'string' || !path.isAbsolute(file)) throw Error('CI report.json의 절대 경로를 지정하세요.');
    const sourceFile = await realpath(file), info = await stat(sourceFile);
    if (!info.isFile() || info.size > 16 * 1024 * 1024) throw Error('CI 보고서는 16 MiB 이하의 JSON 파일이어야 합니다.');
    return this.openBytes(editor, await readFile(sourceFile), revision, sourceFile);
  }
  openBytes(editor, bytes, revision, sourceFile, origin) {
    editor.current(revision);
    if (!Buffer.isBuffer(bytes) || bytes.length > 16 * 1024 * 1024) throw Error('CI 보고서는 16 MiB 이하이어야 합니다.');
    const report = JSON.parse(bytes.toString('utf8'));
    if (!plain(report) || report.version !== 1 || report.kind !== 'goal-regression' || typeof report.ok !== 'boolean' || !Array.isArray(report.rows) || report.rows.length > 128 || !Array.isArray(report.errors)) throw Error('지원하지 않는 CI 경로 보고서입니다. CLI report.json을 선택하세요.');
    const ids = new Set();
    for (const row of report.rows) {
      if (!plain(row) || !statuses.has(row.status) || (row.routeId !== undefined && (typeof row.routeId !== 'string' || !/^[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}$/.test(row.routeId) || ids.has(row.routeId)))) throw Error('CI 보고서의 경로 행이 손상되었거나 중복되었습니다.');
      if (row.routeId) ids.add(row.routeId);
    }
    editor.current(revision);
    this.current = {id: randomUUID(), root: editor.root, sourceFile, origin, checksum: createHash('sha256').update(bytes).digest('hex'), report};
    return this.describe();
  }
  describe(local) {
    const c = this.current, r = c.report, reasons = [];
    if (local) {
      if (r.fingerprint !== local.fingerprint) reasons.push('콘텐츠가 CI 실행 시점과 다릅니다.');
      if (r.routesDigest !== local.routesDigest) reasons.push('등록 경로가 CI 실행 시점과 다릅니다.');
      if (r.selectionRevision !== local.selectionRevision) reasons.push('활성 기준 또는 부분 승인 기록이 CI 실행 시점과 다릅니다.');
      for (const row of r.rows) if (row.baselineChecksum && !local.rows.some(x => x.routeId === row.routeId && x.baselineId === row.baselineId && x.baselineChecksum === row.baselineChecksum)) reasons.push(`기준 기록이 다르거나 없습니다: ${row.routeId}`);
    }
    // Imported file paths, report IDs and change-item IDs are never used for
    // execution, graph authorization or approvals. Only local replays may do so.
    return {id: c.id, sourceFile: c.sourceFile, origin: c.origin, checksum: c.checksum, source: r, matches: !!local && !reasons.length, reasons};
  }
  async recheck(editor, content, revision, id) {
    if (!this.current || this.current.id !== id || this.current.root !== editor.root) throw Error('CI 보고서 세션이 만료되었습니다. 다시 여세요.');
    editor.current(revision); await editor.assertDiskUnchanged();
    if (editor.view({shared: true}).dirty) throw Error('먼저 초안을 저장한 뒤 CI 경로를 재검증하세요.');
    const report = await runGoalRegression(editor.root, {runtimePolicy: 'verify-replay'});
    report.testedRevision = revision;
    const impact = await publishGoalImpacts(editor, content, revision, report);
    return {goalCi: this.describe(impact), goalImpact: impact};
  }
}

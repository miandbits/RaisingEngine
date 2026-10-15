import path from 'node:path';
import {parseArgs} from 'node:util';
import {writeGoalReports} from './goal-regression-report.mjs';

let directory = path.resolve('.cache/goal-regression');
try {
  const {values, positionals} = parseArgs({allowPositionals: true, options: {
    'report-dir': {type: 'string'}, 'timeout-ms': {type: 'string'},
    'runtime-policy': {type: 'string'}, help: {type: 'boolean', short: 'h'},
  }});
  if (values.help) {
    console.log('사용법: npm run check:regression -- 프로젝트폴더 [--report-dir 폴더] [--timeout-ms 120000] [--runtime-policy strict|verify-replay]\n종료 코드: 0 통과 / 1 회귀·도달 실패 / 2 설정·기준·검사 오류. 승인 기록은 변경하지 않습니다.');
  } else {
    if (values['report-dir']) directory = path.resolve(values['report-dir']);
    if (positionals.length !== 1) throw Error('프로젝트 폴더 하나를 지정하세요. --help 참조');
    // Dynamic import lets missing build artifacts produce a machine-readable error report too.
    const {runGoalRegression} = await import('../apps/qt-studio/goal-regression.mjs');
    const report = await runGoalRegression(path.resolve(positionals[0]), {
      timeoutMs: Number(values['timeout-ms'] ?? 120000), runtimePolicy: values['runtime-policy'] ?? 'strict',
    });
    await writeGoalReports(directory, report);
    for (const row of report.rows) console.log(`${row.status === 'unchanged' ? 'PASS' : 'FAIL'} ${row.routeId ?? row.name}: ${row.status} · 승인 ${row.approvedChangeCount ?? 0} · 남은 변화 ${row.pendingChangeCount ?? 0} · 최초 단계 ${row.firstDifference ?? '—'}`);
    for (const error of report.errors) console.error(error);
    console.log(`보고서: ${directory}`); process.exitCode = report.exitCode;
  }
} catch (error) {
  const report = {version: 1, kind: 'goal-regression', ok: false, complete: false, exitCode: 2, rows: [], errors: [error.message], createdAt: new Date().toISOString()};
  await writeGoalReports(directory, report).catch(e => console.error('보고서 저장 실패: ' + e.message));
  console.error(error.message); process.exitCode = 2;
}

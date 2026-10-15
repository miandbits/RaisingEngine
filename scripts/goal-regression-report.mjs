import {mkdir, writeFile, rename, unlink} from 'node:fs/promises';
import {randomUUID} from 'node:crypto';
import path from 'node:path';

const cell = value => String(value ?? '').replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('|', '&#124;').replace(/[\r\n]+/g, ' ');
const xml = value => String(value ?? '').replaceAll('&', '&amp;').replaceAll('<', '&lt;').replaceAll('>', '&gt;').replaceAll('"', '&quot;').replaceAll("'", '&apos;').replace(/[\x00-\x08\x0B\x0C\x0E-\x1F]/g, '');
const detail = row => row.error ?? row.replay?.failure?.message ?? row.failure?.message ?? row.message ?? '';

export function markdownReport(report) {
  const lines = ['# 경로 회귀 검증', '', `결과: **${report.ok ? 'PASS' : 'FAIL'}** · 종료 코드 ${report.exitCode}`, '',
    `런타임 정책: ${cell(report.runtimePolicy)} · 검사 시간: ${report.durationMs ?? 0} ms`, '',
    '| 경로 | 결과 | 승인 | 남은 변화 | 최초 차이 단계 | 저장 복원 |', '| --- | --- | ---: | ---: | ---: | --- |'];
  for (const row of report.rows ?? []) lines.push(`| ${cell(row.name ?? row.routeId)} | ${cell(row.status)} | ${row.approvedChangeCount ?? 0} | ${row.pendingChangeCount ?? 0} | ${row.firstDifference ?? '—'} | ${row.replay ? `${row.replay.ok ? 'PASS' : 'FAIL'} (${row.replay.saves}회)` : '미실행'} |`);
  for (const error of [...(report.errors ?? []), ...(report.baselineErrors ?? []).map(e => e.error)]) lines.push('', `- 오류: ${cell(error)}`);
  for (const row of report.rows ?? []) {
    lines.push('', `## ${cell(row.name ?? row.routeId)}`, '');
    if (detail(row)) lines.push(cell(detail(row)), '');
    if (row.runtimeVerification) lines.push(`원본 런타임 재현: ${cell(row.runtimeVerification.status)}`, '');
    const pending = (row.changeItems ?? []).filter(i => !i.approved);
    for (const item of pending.slice(0, 50)) {
      const observation = item.observations[0];
      lines.push(`- #${item.firstStep} ${cell(item.kind)} ${cell(item.path)}: ${cell(JSON.stringify(observation?.before))} → ${cell(JSON.stringify(observation?.after))}`);
    }
    if (pending.length > 50) lines.push('- 나머지 항목과 모든 발생 값은 JSON 보고서를 확인하세요.');
    for (const flag of ['goalFailed', 'executionFailed', 'routeChanged', 'runtimeChanged']) if (row.flags?.[flag]) lines.push(`- 보호 검사: ${flag}`);
  }
  lines.push('', '등록된 seed·명령의 회귀 검사이며, 모든 경로의 도달 가능성 또는 화면·입력 품질을 보장하지 않습니다.', '');
  return lines.join('\n');
}

export function junitReport(report) {
  const cases = (report.rows ?? []).map(row => {
    const error = ['error', 'incomplete', 'missing-baseline'].includes(row.status);
    const fail = row.status !== 'unchanged';
    const message = detail(row) || `${row.status}: ${row.pendingChangeCount ?? 0} remaining changes; first step ${row.firstDifference ?? 'unknown'}`;
    return {name: row.routeId ?? row.name, error, fail, message, output: JSON.stringify({approved: row.approvedChangeCount, pending: row.pendingChangeCount, replay: row.replay, runtimeVerification: row.runtimeVerification})};
  });
  for (const message of [...(report.errors ?? []), ...(report.baselineErrors ?? []).map(e => e.error)]) cases.push({name: 'validation', error: true, fail: true, message});
  return '<?xml version="1.0" encoding="UTF-8"?>\n' +
    `<testsuite name="goal-regression" tests="${cases.length}" failures="${cases.filter(c => c.fail && !c.error).length}" errors="${cases.filter(c => c.error).length}" time="${(report.durationMs ?? 0) / 1000}">\n` +
    cases.map(c => `  <testcase classname="goal-regression" name="${xml(c.name)}">${c.fail ? `<${c.error ? 'error' : 'failure'} message="${xml(c.message)}">${xml(c.message)}</${c.error ? 'error' : 'failure'}>` : ''}<system-out>${xml(c.output ?? '')}</system-out></testcase>`).join('\n') + '\n</testsuite>\n';
}

export async function writeGoalReports(directory, report) {
  await mkdir(directory, {recursive: true});
  const files = {'report.json': JSON.stringify(report, null, 2) + '\n', 'summary.md': markdownReport(report), 'junit.xml': junitReport(report)};
  for (const [name, text] of Object.entries(files)) {
    const temp = path.join(directory, '.' + randomUUID() + '.tmp');
    try { await writeFile(temp, text, {flag: 'wx'}); await rename(temp, path.join(directory, name)); }
    finally { await unlink(temp).catch(() => {}); }
  }
}

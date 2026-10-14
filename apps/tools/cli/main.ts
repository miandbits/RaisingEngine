import { checkProject } from '../../../packages/project-service/index.js';

const usage = '사용법: node dist/apps/tools/cli/main.js validate <프로젝트 폴더> [--json]\n종료 코드: 0 정상(경고 포함), 1 검증 오류, 2 사용법/내부 오류';

async function main(): Promise<void> {
  const args = process.argv.slice(2);
  if (args.length === 1 && (args[0] === '--help' || args[0] === '-h')) {
    process.stdout.write(`${usage}\n`);
    return;
  }
  const json = args.includes('--json');
  const rest = args.filter((arg) => arg !== '--json');
  if (rest[0] !== 'validate' || rest.length !== 2 || rest[1]!.startsWith('--') || args.filter((arg) => arg === '--json').length > 1) {
    process.stderr.write(`${usage}\n`);
    process.exitCode = 2;
    return;
  }
  try {
    const result = await checkProject(rest[1]!);
    if (json) process.stdout.write(`${JSON.stringify(result, null, 2)}\n`);
    else {
      for (const issue of result.diagnostics) {
        process.stdout.write(`${issue.file}:${issue.line}:${issue.column} ${issue.severity} ${issue.code} ${issue.pointer || '/'}\n  ${issue.message}\n`);
        for (const related of issue.relatedLocations ?? []) process.stdout.write(`  첫 선언: ${related.file}:${related.line}:${related.column} ${related.pointer || '/'}\n`);
      }
      process.stdout.write(`${result.valid ? 'PASS' : 'FAIL'}: ${result.summary.parsedDocuments}개 문서, 오류 ${result.summary.errors}개, 경고 ${result.summary.warnings}개\n`);
    }
    process.exitCode = result.valid ? 0 : 1;
  } catch (error) {
    const message = error instanceof Error ? error.message : String(error);
    if (json) process.stdout.write(`${JSON.stringify({ reportVersion: '1.0.0', valid: false, internalError: { code: 'INTERNAL_ERROR', message } })}\n`);
    else process.stderr.write(`INTERNAL_ERROR: ${message}\n`);
    process.exitCode = 2;
  }
}

await main();

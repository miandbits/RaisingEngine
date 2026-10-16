import { readFile, writeFile, stat } from 'node:fs/promises';
import { loadProject, buildSnapshot } from '../dist/packages/project-service/index.js';
import { previewSaveMigration, publishMigratedSave } from '../dist/packages/runtime-service/migration.js';
import { MAX_SAVE_BYTES } from '../dist/packages/runtime-service/save.js';

// Preview first; apply recomputes both validation and the approval digest.
const args = process.argv.slice(2), options = {};
try {
  for (let i = 0; i < args.length; i++) {
    if (args[i] === '--reset-presentation') options.reset = true;
    else if (['--from','--to','--save','--report','--output','--approve'].includes(args[i]) && args[i+1] && !args[i+1].startsWith('--')) options[args[i].slice(2)] = args[++i];
    else throw Error(`알 수 없거나 값이 없는 옵션: ${args[i]}`);
  }
  if (!options.from || !options.to || !options.save || (!!options.output !== !!options.approve))
    throw Error('사용법: node scripts/migrate-save.mjs --from OLD_PROJECT --to NEW_PROJECT --save SAVE [--report NEW_REPORT] [--reset-presentation] [--output NEW_SAVE --approve PREVIEW_DIGEST]');
  const snapshot = async root => { const result = await buildSnapshot(await loadProject(root)); if (!result.ok) throw Error(JSON.stringify(result.diagnostics)); return result.snapshot; };
  const [source, target] = await Promise.all([snapshot(options.from), snapshot(options.to)]);
  const info = await stat(options.save); if (!info.isFile() || info.size > MAX_SAVE_BYTES) throw Error('저장 파일이 없거나 16 MiB를 초과했습니다.');
  const text = new TextDecoder('utf-8', { fatal: true }).decode(await readFile(options.save));
  const settings = { resetPresentation: !!options.reset };
  const result = options.output ? await publishMigratedSave(options.output, source, target, text, options.approve, settings) : await previewSaveMigration(source, target, text, settings);
  const report = result.ok ? { ok: true, report: result.report, ...(result.file ? { file: result.file } : {}) } : result;
  if (options.report) await writeFile(options.report, JSON.stringify(report, null, 2), { flag: 'wx' });
  console.log(JSON.stringify(report, null, 2));
  if (!result.ok) process.exitCode = 1;
} catch (error) { console.error(error.message); process.exitCode = 1; }

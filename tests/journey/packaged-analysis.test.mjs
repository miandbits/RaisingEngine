import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdir, readFile, readdir, writeFile } from 'node:fs/promises';
import { resolve, join } from 'node:path';
import { performance } from 'node:perf_hooks';
import { openJourneyStudio, click } from './gui-helpers.mjs';

test('packaged analysis worker completes 1000 plays started through the real editor UI', { timeout: 600000 }, async t => {
  assert.ok(process.env.STUDIO_EXECUTABLE, 'requires the packaged Studio');
  const projectRoot = resolve('projects/journey-playtest-2026-10-11');
  const { app, page, errors } = await openJourneyStudio(t);
  await app.evaluate(({ dialog }, root) => { dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [root] }); }, projectRoot);
  await click(page, '열기'); await page.getByText(projectRoot, { exact: true }).waitFor();
  await click(page, '테스트');
  await page.getByLabel('실행 횟수', { exact: true }).fill('1000');
  await page.getByLabel('기준 seed', { exact: true }).fill('42000');
  await page.getByLabel('시간 제한', { exact: true }).fill('600');
  await page.getByLabel('플레이 전략', { exact: true }).selectOption('random');
  const started = performance.now();
  await click(page, '분석 시작');
  await page.waitForFunction(() => {
    const text = document.querySelector('[data-testid="analysis-summary"]')?.textContent || '';
    return text.includes('completed · 1000/1000회') || /failed|cancelled/.test(text);
  }, undefined, { timeout: 590000 });
  const elapsedMs = performance.now() - started;
  const summary = await page.getByTestId('analysis-summary').textContent();
  assert.match(summary, /completed · 1000\/1000회/);
  const jobId = await page.getByLabel('분석 작업', { exact: true }).inputValue();
  const files = await readdir(resolve(projectRoot, '.studio'), { recursive: true });
  let persistedJob;
  for (const relative of files.filter(name => name.endsWith('.json') && name.includes(jobId))) {
    const value = JSON.parse(await readFile(join(projectRoot, '.studio', relative), 'utf8'));
    if (value.id === jobId && value.result) { persistedJob = value; break; }
  }
  assert.ok(persistedJob, 'completed job must remain available in the handoff project');
  assert.equal(persistedJob.result.completed, 1000);
  assert.equal(persistedJob.result.outcomes.finished, 1000);
  const output = resolve('.cache/journey-packaged-analysis'); await mkdir(output, { recursive: true });
  await page.screenshot({ path: resolve(output, 'completed-1000.png'), fullPage: true });
  await writeFile(resolve(output, 'evidence.json'), JSON.stringify({ projectRoot, jobId, elapsedMs, summary, result: persistedJob.result }, null, 2));
  assert.deepEqual(errors, []);
});

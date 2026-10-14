import test from 'node:test';
import assert from 'node:assert/strict';
import { _electron } from 'playwright';
import { mkdir, mkdtemp, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
const env = { ...process.env }; delete env.NODE_OPTIONS; delete env.ELECTRON_RUN_AS_NODE;
const output = resolve('.cache/studio-qa');
const view = page => page.evaluate(() => window.studio.view()).then(r => r.view);
async function idle(page) { await page.waitForFunction(() => !!document.querySelector('.workspace-status') && !document.querySelector('.workspace-status').textContent.includes('처리 중')); }
async function click(page, name) { await page.getByRole('button', { name, exact: typeof name === 'string' }).click(); if (name !== '플레이 미리보기') await idle(page); }
async function fill(page, field, text) { const input = page.getByLabel(field, { exact: true }); await input.fill(String(text)); await input.press('Tab'); await idle(page); }
async function add(page, kind, id) { await click(page, '+ 추가'); await page.getByLabel('새 콘텐츠 종류', { exact: true }).selectOption(kind); await page.getByLabel('새 콘텐츠 ID', { exact: true }).fill(id); await click(page, '만들기'); }
async function screenshot(page, name) { await page.evaluate(() => window.scrollTo(0, 0)); await page.screenshot({ path: `${output}/${name}.png`, animations: 'disabled' }); }
test('Studio UI authors activity/event/ending without code, previews ending, diagnoses errors and protects external edits', { timeout: 120000 }, async t => {
  await mkdir(output, { recursive: true }); const project = await mkdtemp(`${output}/project-`);
  const electron = await _electron.launch({ ...(process.env.STUDIO_EXECUTABLE ? { executablePath: resolve(process.env.STUDIO_EXECUTABLE), args: [] } : { args: ['dist/studio/main/index.js'] }), cwd: resolve('.'), env, timeout: 30000 });
  t.after(async () => { await electron.evaluate(({ app }) => app.exit(0)).catch(() => {}); });
  const page = await electron.firstWindow(); const errors = []; page.on('pageerror', e => errors.push(e.message));
  await idle(page); assert.equal((await view(page)).readOnly, true);
  await electron.evaluate(({ dialog }, root) => {
    dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [root] });
    dialog.showMessageBox = async () => ({ response: 1, checkboxChecked: false });
  }, project);
  await click(page, '새 프로젝트'); assert.equal((await view(page)).readOnly, false, await page.locator('.studio-alert').allTextContents().then(x => x.join('\n')));
  await page.getByLabel('참조 ID 검색').fill('skill');
  assert.ok(await page.locator('.reference').count() >= 3);
  await page.getByLabel('참조 ID 검색').fill('');
  await screenshot(page, 'workspace');
  await click(page, '활동 비교표'); assert.equal(await page.getByRole('row').count(), 4);
  await screenshot(page, 'activity-table');
  await add(page, 'activity', 'practice'); await fill(page, '/label', '자유 연습');
  await click(page, '기본 효과 추가');
  await fill(page, '/baseEffects/0/target/attributeId', 'skill'); await fill(page, '/baseEffects/0/value/value', '2');
  await click(page, '실행 취소'); assert.equal((await view(page)).documents.find(d => d.file === 'activities/practice.json').data.baseEffects[0].value.value, 0);
  await click(page, '다시 실행');
  await add(page, 'event', 'greeting'); await fill(page, '/label', '첫 수업의 인사');
  await fill(page, '/nodes/0/text', '새로운 연습을 시작했구나.');
  await page.getByLabel('/nodes/0 형식', { exact: true }).selectOption({ label: '선택' }); await idle(page);
  await fill(page, '/nodes/0/prompt', '어떻게 인사할까요?');
  await fill(page, '/nodes/0/choices/0/id', 'hello');
  await fill(page, '/nodes/0/choices/0/text', '반갑게 인사한다');
  await fill(page, '/nodes/0/choices/0/next', 'end');
  await click(page, '검증'); assert.equal((await view(page)).diagnostics.filter(d => d.severity === 'error').length, 0);
  await screenshot(page, 'narrative-form');
  await add(page, 'ending', 'quiet-end'); await fill(page, '/label', '꾸준한 수련자'); await fill(page, '/priority', 50);
  await fill(page, '/nodes/0/text', '작은 연습이 쌓여 새로운 나를 만났습니다.');
  await click(page, '검증'); assert.equal((await view(page)).diagnostics.filter(d => d.severity === 'error').length, 0);
  // A dangling node is located from diagnostics and can be undone without losing other edits.
  await fill(page, '/nodes/0/next', 'missing-node'); await click(page, '검증');
  assert.ok((await view(page)).diagnostics.some(d => d.pointer.includes('next')));
  await page.locator('.diagnostic').first().click(); await screenshot(page, 'diagnostics');
  await click(page, '실행 취소'); await click(page, '검증');
  await click(page, '초안 저장'); assert.equal((await view(page)).dirty, false);
  assert.equal(JSON.parse(await readFile(`${project}/activities/practice.json`, 'utf8')).baseEffects[0].value.value, 2);
  await click(page, '플레이 미리보기'); await page.getByRole('button', { name: '새 게임 시작', exact: true }).click();
  const playerState = () => page.evaluate(() => window.player.view()).then(r => r.view.state);
  for (let month = 0; month < 2; month++) {
    for (const [i, period] of ['상순', '중순', '하순'].entries()) await page.getByLabel(`${period} 활동`, { exact: true }).selectOption(month === 0 && i === 0 ? 'practice' : 'rest');
    await page.getByRole('button', { name: '일정 확정', exact: true }).click();
    for (const period of ['상순', '중순', '하순']) {
      await page.getByRole('button', { name: `${period} 활동 시작`, exact: true }).click();
      await page.getByRole('button', { name: '다음으로', exact: true }).click();
      for (let n = 0; n < 10 && (await playerState()).phase === 'narrative'; n++) {
        const choice = page.getByRole('button', { name: '반갑게 인사한다', exact: true });
        if (await choice.count()) await choice.click(); else await page.getByRole('button', { name: '계속', exact: true }).click();
      }
    }
  }
  assert.equal(await page.getByTestId('ending').innerText(), '꾸준한 수련자'); assert.equal((await playerState()).attributes.skill, 2);
  await screenshot(page, 'preview-ending');
  await click(page, '편집기로 돌아가기'); assert.equal((await view(page)).dirty, false);
  const disk = JSON.parse(await readFile(`${project}/activities/work.json`, 'utf8')); disk.description = '외부에서 수정한 내용';
  await writeFile(`${project}/activities/work.json`, JSON.stringify(disk));
  await fill(page, '/label', '저장 충돌 테스트'); await click(page, '초안 저장');
  assert.match(await page.getByRole('alert').innerText(), /외부 변경 충돌/); assert.equal((await view(page)).dirty, true);
  assert.equal(JSON.parse(await readFile(`${project}/activities/work.json`, 'utf8')).description, disk.description);
  await screenshot(page, 'external-conflict');
  await click(page, '오류 닫기');
  await electron.evaluate(({ dialog }) => { dialog.showMessageBox = async () => ({ response: 0, checkboxChecked: false }); });
  await click(page, '다시 불러오기'); assert.equal((await view(page)).dirty, true);
  await electron.evaluate(({ dialog }) => { dialog.showMessageBox = async () => ({ response: 1, checkboxChecked: false }); });
  await click(page, '다시 불러오기'); assert.equal((await view(page)).dirty, false);
  const incomplete = JSON.parse(await readFile(`${project}/activities/practice.json`, 'utf8')); delete incomplete.costs;
  await writeFile(`${project}/activities/practice.json`, JSON.stringify(incomplete)); await click(page, '다시 불러오기');
  await page.locator('.content-explorer').getByRole('button', { name: /자유 연습/ }).click();
  await click(page, '/costs 사용'); await click(page, '검증');
  assert.equal((await view(page)).diagnostics.filter(d => d.severity === 'error').length, 0);
  await click(page, '초안 저장'); assert.deepEqual(JSON.parse(await readFile(`${project}/activities/practice.json`, 'utf8')).costs, []);
  await electron.evaluate(({ BrowserWindow }) => BrowserWindow.getAllWindows()[0].setSize(1000, 700));
  await screenshot(page, 'minimum-window');
  assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > window.innerWidth), false);
  assert.deepEqual(await page.evaluate(() => ({ require: typeof window.require, process: typeof window.process })), { require: 'undefined', process: 'undefined' });
  assert.deepEqual(errors, []);
});

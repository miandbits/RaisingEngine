import test from 'node:test';
import assert from 'node:assert/strict';
import { cp, mkdir, mkdtemp, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { openJourneyStudio, click, select } from './gui-helpers.mjs';

async function navigation(page) {
  return {
    screen: await page.getByLabel('편집 화면', { exact: true }).inputValue(),
    selected: await page.locator('.visual-layers button[aria-pressed="true"] small').allTextContents(),
    snap: await page.getByLabel('8px 격자 맞춤', { exact: true }).isChecked(),
    sceneTab: await page.getByRole('button', { name: '장면·연출', exact: true }).getAttribute('aria-pressed'),
    newKind: await page.getByLabel('추가할 UI 종류', { exact: true }).inputValue(),
    sceneTarget: await page.getByLabel('연출 대상', { exact: true }).count() ? await page.getByLabel('연출 대상', { exact: true }).inputValue() : null,
  };
}

test('visual editor retains screen, element and tools across preview and workspace changes, and resets for another project', { timeout: 60000 }, async t => {
  const { app, page, root, errors } = await openJourneyStudio(t);
  const host = await app.evaluate(({ app }) => ({ packaged: app.isPackaged, path: app.getAppPath(), executable: app.getPath('exe') }));
  if (process.env.STUDIO_EXECUTABLE) {
    assert.equal(host.packaged, true);
    assert.equal(host.executable, resolve(process.env.STUDIO_EXECUTABLE));
  }
  await cp(resolve('examples/academy-journey'), root, { recursive: true });
  const otherRoot = resolve(await mkdtemp(`${resolve('.cache/journey-ui')}/visual-other-`));
  await cp(resolve('examples/academy-journey'), otherRoot, { recursive: true });
  await click(page, '열기'); await page.getByText(resolve(root), { exact: true }).waitFor();
  await click(page, '화면·연출');
  const choose = async () => {
    await select(page, '편집 화면', 'journey-notice');
    await page.locator('.visual-layers button').filter({ has: page.getByText('button-new', { exact: true }) }).click();
    await page.getByLabel('8px 격자 맞춤', { exact: true }).uncheck();
    await click(page, '장면·연출');
    await select(page, '연출 대상', 'activities/read.json');
    await select(page, '추가할 UI 종류', 'image');
  };
  await choose(); const before = await navigation(page);
  await click(page, '실제 플레이로 확인'); await click(page, '새 게임 시작'); await click(page, '편집기로 돌아가기');
  const afterPreview = await navigation(page);
  await choose(); await click(page, '콘텐츠'); await click(page, '화면·연출');
  const afterWorkspace = await navigation(page);
  await choose();
  await app.evaluate(({ dialog }, root) => { dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [root] }); }, otherRoot);
  await click(page, '열기'); await page.getByText(otherRoot, { exact: true }).waitFor();
  if (!await page.getByLabel('편집 화면', { exact: true }).count()) await click(page, '화면·연출');
  const afterOtherProject = await navigation(page);
  const output = resolve('.cache/journey-visual-navigation'); await mkdir(output, { recursive: true });
  await writeFile(resolve(output, 'evidence.json'), JSON.stringify({ host, before, afterPreview, afterWorkspace, afterOtherProject }, null, 2));
  await page.screenshot({ path: resolve(output, 'other-project.png') });
  assert.deepEqual(afterPreview, before, 'preview/back must retain visual editing context');
  assert.deepEqual(afterWorkspace, before, 'workspace switching must retain visual editing context');
  assert.equal(afterOtherProject.screen, 'schedule', 'another project resets stale visual selection');
  assert.equal(afterOtherProject.snap, true);
  assert.equal(afterOtherProject.sceneTab, 'false');
  assert.equal(afterOtherProject.newKind, 'text');
  assert.deepEqual(errors, []);
});

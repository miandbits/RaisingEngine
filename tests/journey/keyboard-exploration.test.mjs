import test from 'node:test';
import assert from 'node:assert/strict';
import { cp, mkdir, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { openJourneyStudio, click, select, state } from './gui-helpers.mjs';

async function keyboardActivate(page, button) {
  for (let i = 0; i < 80; i++) {
    if (await button.evaluate(element => document.activeElement === element)) {
      await page.keyboard.press('Enter'); return i;
    }
    await page.keyboard.press('Tab');
  }
  assert.fail('Visible button is not reachable with Tab');
}

test('packaged exploratory keyboard, cancellation and editor selection review', { timeout: 120000 }, async t => {
  const { app, page, root, errors } = await openJourneyStudio(t);
  await cp(resolve('examples/academy-journey'), root, { recursive: true });
  await click(page, '열기'); await page.getByText(resolve(root), { exact: true }).waitFor();
  await click(page, '육성 작업실');
  await page.locator('.journey-steps button').filter({ hasText: '아이템' }).click();
  await page.getByPlaceholder('이름 또는 ID 검색', { exact: true }).fill('감사의');
  const selectedImage = await page.getByLabel('아이템 이미지', { exact: true }).inputValue();
  await click(page, '플레이 미리보기'); await click(page, '새 게임 시작');
  await app.evaluate(({ dialog }) => {
    dialog.showOpenDialog = async () => ({ canceled: true, filePaths: [] });
    dialog.showSaveDialog = async () => ({ canceled: true });
  });
  const beforeCancel = await state(page);
  await click(page, '저장'); assert.deepEqual(await state(page), beforeCancel);
  await click(page, '불러오기'); assert.deepEqual(await state(page), beforeCancel);
  const observations = { cancelPreservedState: true, keyboard: [], selectionPreserved: false };
  await click(page, '일정');
  for (const period of ['상순', '중순', '하순']) await select(page, `${period} 활동`, 'rest');
  await click(page, '일정 확정'); await click(page, '활동 시작'); await click(page, '결과 확인 · 다음');
  for (let i = 0; i < 40; i++) {
    const view = (await page.evaluate(() => window.player.view())).view;
    if (view.state.phase !== 'narrative') {
      if (observations.keyboard.some(step => step.choice)) break;
      if (await page.getByRole('button', { name: '일정', exact: true }).count()) await click(page, '일정');
      await click(page, '활동 시작'); await click(page, '결과 확인 · 다음');
      continue;
    }
    const choice = view.narrative.node.type === 'choice';
    const target = choice ? page.locator('.visual-choices button:enabled').first() : page.getByRole('button', { name: '이야기 계속', exact: true });
    const label = await target.textContent();
    const tabs = await keyboardActivate(page, target);
    observations.keyboard.push({ choice, label, tabs });
  }
  assert.ok(observations.keyboard.some(step => step.choice), 'existing first-month story must exercise an actual dialogue choice');
  await click(page, '도시로 외출');
  assert.equal(await page.getByRole('button', { name: '유나에게 꽃 선물', exact: true }).isDisabled(), true);
  await click(page, '꽃집으로 이동'); await click(page, '감사의 꽃 구매');
  assert.equal(await page.getByRole('button', { name: '감사의 꽃 구매', exact: true }).isDisabled(), true);
  await click(page, '편집기로 돌아가기');
  assert.equal(await page.getByLabel('아이템 이미지', { exact: true }).inputValue(), selectedImage);
  assert.equal(await page.getByPlaceholder('이름 또는 ID 검색', { exact: true }).inputValue(), '감사의');
  await click(page, '콘텐츠'); await click(page, '육성 작업실');
  assert.equal(await page.getByLabel('아이템 이미지', { exact: true }).inputValue(), selectedImage);
  assert.equal(await page.getByPlaceholder('이름 또는 ID 검색', { exact: true }).inputValue(), '감사의');
  observations.selectionPreserved = true;
  const output = resolve('.cache/journey-keyboard-exploration'); await mkdir(output, { recursive: true });
  await page.screenshot({ path: resolve(output, 'returned-editor.png') });
  await writeFile(resolve(output, 'evidence.json'), JSON.stringify(observations, null, 2));
  assert.deepEqual(errors, []);
});

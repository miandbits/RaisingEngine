import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile, mkdtemp, mkdir, access, cp } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { resolve } from 'node:path';
import { openJourneyStudio, click, select, projectData } from './gui-helpers.mjs';

async function sceneReady(page, assetId) {
  const image = page.locator(`.visual-stage img[src*="${assetId}"]`).first();
  await image.waitFor();
  await page.waitForFunction(id => [...document.querySelectorAll('.visual-stage img')].some(image => image.src.includes(id) && image.complete && image.naturalWidth > 0), assetId);
  await page.evaluate(async () => {
    await new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve)));
    await Promise.all(document.getAnimations().map(animation => animation.finished.catch(() => {})));
  });
}

test('designer imports flower art, assigns item portrait and exports Windows and web from editor controls', { timeout: 120000 }, async t => {
  const { app, page, errors } = await openJourneyStudio(t);
  const image = resolve('assets/journey/thank-you-flowers.png');
  const imageBytes = await readFile(image), imageId = `image-${createHash('sha256').update(imageBytes).digest('hex').slice(0, 16)}`;
  const output = resolve('.cache/journey-polish-ui'); await mkdir(output, { recursive: true });
  const root = process.env.AUTHOR_SAMPLE === '1' ? resolve('examples/academy-journey') : resolve(await mkdtemp(`${output}/project-`));
  if (process.env.AUTHOR_SAMPLE !== '1') await cp(resolve('examples/academy-journey'), root, { recursive: true });
  const exports = await mkdtemp(`${output}/exports-`);
  await app.evaluate(({ dialog }, { root, image, exports }) => {
    dialog.showOpenDialog = async (_window, options) => ({ canceled: false, filePaths: [options.properties?.includes('openFile') ? image : root] });
    dialog.showSaveDialog = async (_window, options) => ({ canceled: false, filePath: `${exports}/${String(options.defaultPath).includes('web') ? 'web' : 'windows'}` });
  }, { root, image, exports });
  await click(page, '열기'); await page.getByText(root, { exact: true }).waitFor();
  await click(page, '화면·연출'); await click(page, '이미지 가져오기');
  await click(page, '육성 작업실');
  await page.locator('.journey-steps button').filter({ hasText: '아이템' }).click();
  await select(page, '아이템 이미지', imageId);
  const backgrounds = {};
  for (const [locationName, filename] of [['우리 집', 'apprentice-home.png'], ['작은 상점', 'town-florist.png']]) {
    const source = resolve('assets/journey', filename), bytes = await readFile(source);
    const assetId = `image-${createHash('sha256').update(bytes).digest('hex').slice(0, 16)}`; backgrounds[locationName] = assetId;
    await app.evaluate(({ dialog }, source) => { dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [source] }); }, source);
    await click(page, '화면·연출'); await click(page, '이미지 가져오기');
    await click(page, '육성 작업실'); await page.locator('.journey-steps button').filter({ hasText: '장소' }).click();
    await page.locator('.gameplay-browser').getByRole('button').filter({ hasText: locationName }).click();
    await select(page, '장소 배경', assetId);
  }
  await click(page, '초안 저장');
  await page.getByText('저장된 상태', { exact: true }).waitFor();
  const project = await projectData(root); assert.equal(project.gameplay.items.find(item => item.id === 'flower').imageAssetId, imageId);
  for (const [name, assetId] of Object.entries(backgrounds)) assert.equal(project.gameplay.locations.find(location => location.name === name).presentation.backgroundAssetId, assetId);
  const manifest = JSON.parse(await readFile(resolve(root, 'assets/manifest.json'), 'utf8'));
  assert.deepEqual(await readFile(resolve(root, manifest.assets.find(asset => asset.id === imageId).path)), imageBytes);
  await page.screenshot({ path: resolve(output, 'flower-item-editor.png') });
  await click(page, '이 흐름 플레이'); await click(page, '새 게임 시작');
  await sceneReady(page, backgrounds['우리 집']);
  await page.screenshot({ path: resolve(output, 'authored-home.png') });
  await click(page, '작은 상점 방문');
  await sceneReady(page, backgrounds['작은 상점']);
  const visibleImage = page.locator(`.visual-stage img[src*="${imageId}"]`).first();
  await visibleImage.waitFor(); assert.equal(await visibleImage.evaluate(img => img.complete && img.naturalWidth > 0), true);
  await page.screenshot({ path: resolve(output, 'flower-shop.png') });
  await click(page, '편집기로 돌아가기');
  await page.getByLabel('자동 검증', { exact: true }).uncheck();
  for (const target of ['windows', 'web']) {
    await click(page, '게임 내보내기'); await select(page, '내보내기 대상', target);
    await app.evaluate(({ dialog }, path) => { dialog.showSaveDialog = async () => ({ canceled: false, filePath: path }); }, resolve(exports, target));
    await click(page, '새 폴더로 내보내기');
    await page.getByRole('dialog', { name: '게임 내보내기', exact: true }).waitFor({ state: 'hidden', timeout: 10000 }).catch(async error => { throw new Error(`${error.message}\nPAGE ERRORS ${JSON.stringify(errors)}\n${await page.locator('body').innerText()}`); });
    await access(resolve(exports, target, 'game.json'));
    await access(resolve(exports, target, target === 'windows' ? 'RaisingPlayer.exe' : 'index.html'));
  }
  assert.deepEqual(errors, []);
});

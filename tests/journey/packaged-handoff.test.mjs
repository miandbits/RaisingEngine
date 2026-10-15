import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdir, mkdtemp, readdir, readFile, writeFile, access } from 'node:fs/promises';
import { resolve } from 'node:path';
import { spawnSync } from 'node:child_process';
import { openJourneyStudio, click, select, state } from './gui-helpers.mjs';
import { browserSmoke } from './browser-smoke.mjs';
import { sceneReady } from './scene-ready.mjs';

test('packaged Studio creates the handoff project, plays and saves the journey, and exports bundled native/web players', { timeout: 180000 }, async t => {
  assert.ok(process.env.STUDIO_EXECUTABLE, 'must use a packaged executable');
  const projectRoot = resolve('tests/fixtures/projects/journey-playtest-2026-10-11'), output = resolve('.cache/journey-packaged-ui');
  await mkdir(projectRoot, { recursive: true }); await mkdir(output, { recursive: true });
  const empty = (await readdir(projectRoot)).length === 0;
  const save = resolve(projectRoot, 'saves/first-flower.json');
  const exportRoot = resolve(await mkdtemp(`${resolve('release/native-journey-preview')}/game-exports-`));
  const { app, page, errors } = await openJourneyStudio(t);
  const host = await app.evaluate(({ app }) => ({ packaged: app.isPackaged, path: app.getAppPath(), exe: app.getPath('exe') }));
  assert.equal(host.packaged, true); assert.match(host.path, /resources[\\/]app/);
  await app.evaluate(({ dialog }, { projectRoot, save }) => {
    dialog.showOpenDialog = async (_window, options) => ({ canceled: false, filePaths: [options.properties?.includes('openFile') ? save : projectRoot] });
    dialog.showSaveDialog = async () => ({ canceled: false, filePath: save });
  }, { projectRoot, save });
  if (empty) {
    // The create dialog requires an empty destination; the save directory is made after copying.
    await app.evaluate(({ dialog }, projectRoot) => { dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [projectRoot] }); }, projectRoot);
    await click(page, '도시·선물 샘플 새 프로젝트');
  } else await click(page, '열기');
  await page.getByText(projectRoot, { exact: true }).waitFor();
  await app.evaluate(({ dialog }, { projectRoot, save }) => { dialog.showOpenDialog = async (_window, options) => ({ canceled: false, filePaths: [options.properties?.includes('openFile') ? save : projectRoot] }); }, { projectRoot, save });
  await mkdir(resolve(projectRoot, 'saves'), { recursive: true });
  await click(page, '플레이 미리보기'); await click(page, '새 게임 시작');
  assert.equal((await page.evaluate(() => window.player.view())).view.runtimeBackend, 'native');
  const addonProof = await app.evaluate(() => {
    const Module = process.getBuiltinModule('module');
    return { resourcesPath: process.resourcesPath, loaded: Object.keys(Module._cache).filter(file => file.endsWith('.node')) };
  });
  const addonManifest = JSON.parse(await readFile(resolve(addonProof.resourcesPath, 'native/addon.json'), 'utf8'));
  const runtimeManifest = JSON.parse(await readFile(resolve(addonProof.resourcesPath, 'native/runtime.json'), 'utf8'));
  assert.equal(addonManifest.executable, runtimeManifest.executable);
  assert.equal(addonManifest.filename, `raising-addon-${addonManifest.sourceRevision}.node`);
  assert.ok(addonProof.loaded.includes(resolve(addonProof.resourcesPath, 'native', addonManifest.filename)), JSON.stringify(addonProof));
  await page.locator('.visual-stage').getByText('소지금 60 G', { exact: true }).waitFor();
  await click(page, '일정'); for (const period of ['상순', '중순', '하순']) await select(page, `${period} 활동`, 'rest');
  await click(page, '일정 확정 / 활동 시작'); await click(page, '일정 확정 / 활동 시작'); await click(page, '다음으로');
  for (let i = 0; i < 20 && (await state(page)).phase === 'narrative'; i++) {
    const next = page.getByRole('button', { name: '계속', exact: true });
    if (await next.count()) await next.click();
    else await page.locator('.visual-stage button:not([disabled])').last().click();
  }
  assert.equal((await state(page)).periodIndex, 1);
  await click(page, '도시로 외출'); await click(page, '꽃집으로 이동'); await click(page, '감사의 꽃 구매');
  await click(page, '광장으로 돌아가기'); await click(page, '저장'); const beforeGift = await state(page);
  await access(save);
  await click(page, '유나에게 꽃 선물'); await click(page, '불러오기');
  assert.deepEqual(await state(page), beforeGift);
  await click(page, '유나에게 꽃 선물');
  await page.waitForFunction(() => [...document.querySelectorAll('.visual-stage img')].every(image => image.complete && image.naturalWidth > 0));
  await page.evaluate(async () => { await Promise.all(document.getAnimations().map(animation => animation.finished.catch(() => {}))); });
  await page.screenshot({ path: resolve(output, 'packaged-relationship.png') });
  await click(page, '계속'); await click(page, '우리 집 방문');
  await page.locator('.visual-stage').getByText('유나 신뢰 15', { exact: true }).waitFor();
  await page.screenshot({ path: resolve(output, 'packaged-hud.png') });
  await click(page, '편집기로 돌아가기');
  for (const target of ['windows', 'web']) {
    const destination = resolve(exportRoot, target);
    await app.evaluate(({ dialog }, destination) => { dialog.showSaveDialog = async () => ({ canceled: false, filePath: destination }); }, destination);
    await click(page, '게임 내보내기'); await select(page, '내보내기 대상', target); await click(page, '새 폴더로 내보내기');
    await page.getByRole('dialog', { name: '게임 내보내기', exact: true }).waitFor({ state: 'hidden' });
    await access(resolve(destination, target === 'windows' ? 'RaisingPlayer.exe' : 'index.html'));
    const bundle = JSON.parse(await readFile(resolve(destination, 'game.json'), 'utf8'));
    assert.equal(bundle.content.project.gameplay.items[0].id, 'flower');
  }
  const automation = resolve(output, 'native-export.steps.json'), reportFile = resolve(output, 'native-export.report.json');
  await writeFile(automation, JSON.stringify([{ clickWindow: 'start' }, { clickWindow: 'visit-market' }, { clickWindow: 'buy-florist-flower' }, { clickWindow: 'visit-city' }, { clickWindow: 'interact-gift-yuna' }, { expectScreen: 'story' }, { waitFrames: 120 }, { screenshot: resolve(output, 'packaged-native-export.png') }]));
  const native = spawnSync(resolve(exportRoot, 'windows/RaisingPlayer.exe'), ['--bundle', resolve(exportRoot, 'windows'), '--save', resolve(output, 'native-save.json'), '--hidden', '--automation', automation, '--report', reportFile], { timeout: 30000, windowsHide: true, encoding: 'utf8' });
  assert.equal(native.status, 0, native.stderr || native.stdout);
  assert.equal(JSON.parse(await readFile(reportFile, 'utf8')).final.state.relationships.mentor.trust, 15);
  await browserSmoke(t, resolve(exportRoot, 'web'), resolve(output, 'packaged-web-export.png'));
  // Leave the handoff project with a fresh home preview rather than the completed test run.
  await click(page, '플레이 미리보기'); await click(page, '새 게임 시작');
  await page.locator('.visual-stage').getByText('소지금 60 G', { exact: true }).waitFor();
  await sceneReady(page, (await state(page)).presentation.backgroundAssetId);
  await page.screenshot({ path: resolve(output, 'handoff-home.png') });
  await writeFile(resolve(output, 'evidence.json'), JSON.stringify({ host, addonProof, addonManifest, runtimeManifest, projectRoot, save, exportRoot, createdNew: empty, finalState: await state(page) }, null, 2));
  assert.deepEqual(errors, []);
});

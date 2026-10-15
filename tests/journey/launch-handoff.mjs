import { _electron } from 'playwright';
import { resolve } from 'node:path';
import { mkdir } from 'node:fs/promises';
import { sceneReady } from './scene-ready.mjs';

const executablePath = resolve(process.env.STUDIO_EXECUTABLE || 'release/native-journey-preview/Raising Studio-win32-x64/Raising Studio.exe');
const projectRoot = resolve(process.env.HANDOFF_PROJECT || 'tests/fixtures/projects/journey-playtest-2026-10-11');
const env = { ...process.env };
delete env.NODE_OPTIONS;
delete env.ELECTRON_RUN_AS_NODE;
const app = await _electron.launch({ executablePath, args: [], cwd: resolve('.'), env });
try {
  const page = await app.firstWindow();
  await app.evaluate(({ dialog }, projectRoot) => {
    globalThis.__handoffOriginalOpenDialog = dialog.showOpenDialog;
    dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [projectRoot] });
  }, projectRoot);
  try {
    await page.getByRole('button', { name: '열기', exact: true }).click();
    await page.getByText(projectRoot, { exact: true }).waitFor();
  } finally {
    await app.evaluate(({ dialog }) => {
      dialog.showOpenDialog = globalThis.__handoffOriginalOpenDialog;
      delete globalThis.__handoffOriginalOpenDialog;
    });
  }
  await page.getByRole('button', { name: '플레이 미리보기', exact: true }).click();
  await page.getByRole('button', { name: '새 게임 시작', exact: true }).click();
  await page.locator('.visual-stage').getByText('소지금 60 G', { exact: true }).waitFor();
  const result = await page.evaluate(() => window.player.view());
  if (result.error || result.view.runtimeBackend !== 'native') throw new Error(JSON.stringify(result));
  await sceneReady(page, result.view.state.presentation.backgroundAssetId);
  await mkdir(resolve('.cache/journey-packaged-ui'), { recursive: true });
  await page.screenshot({ path: resolve('.cache/journey-packaged-ui/handoff-live.png') });
  console.log(JSON.stringify({ ready: true, executablePath, projectRoot, dialogsRestored: true, pid: app.process().pid }));
  await new Promise(resolve => app.once('close', resolve));
} catch (error) {
  await app.close().catch(() => {});
  throw error;
}

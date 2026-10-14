import assert from 'node:assert/strict';
import { _electron } from 'playwright';
import { mkdir, mkdtemp, readFile } from 'node:fs/promises';
import { resolve } from 'node:path';

/** Only native dialogs are stubbed. All authoring/game actions use actual controls. */
export async function openJourneyStudio(t) {
  const output = resolve('.cache/journey-ui');
  await mkdir(output, { recursive: true });
  const root = await mkdtemp(`${output}/project-`);
  const save = resolve(root, 'journey-save.json');
  const env = { ...process.env };
  delete env.NODE_OPTIONS;
  delete env.ELECTRON_RUN_AS_NODE;
  const app = await _electron.launch({
    ...(process.env.STUDIO_EXECUTABLE
      ? { executablePath: resolve(process.env.STUDIO_EXECUTABLE), args: [] }
      : { args: ['dist/studio/main/index.js'] }),
    cwd: resolve('.'), env,
  });
  t.after(async () => {
    const windows = app.windows();
    if (windows[0]) await windows[0].screenshot({ path: `${output}/last-screen.png` }).catch(() => {});
    await app.evaluate(({ app }) => app.exit(0)).catch(() => {});
  });
  const page = await app.firstWindow();
  const errors = [];
  page.on('pageerror', error => errors.push(error.message));
  await page.waitForSelector('.workspace-status');
  await app.evaluate(({ dialog }, { root, save }) => {
    dialog.showOpenDialog = async (_window, options) => ({
      canceled: false, filePaths: [options.properties?.includes('openFile') ? save : root],
    });
    dialog.showSaveDialog = async () => ({ canceled: false, filePath: save });
    dialog.showMessageBox = async () => ({ response: 1 });
  }, { root, save });
  return { app, page, errors, output, root, save };
}

export const click = (page, name) => page.getByRole('button', { name, exact: true }).click();
export async function fill(page, label, value) {
  const field = page.getByLabel(label, { exact: true });
  await field.fill(String(value));
  await field.press('Tab');
}
export const select = (page, label, value) => page.getByLabel(label, { exact: true }).selectOption(value);
export async function projectData(root) {
  return JSON.parse(await readFile(resolve(root, 'project.json'), 'utf8'));
}
export async function state(page) {
  const result = await page.evaluate(async () => window.player.view());
  assert.equal(result.error, null, JSON.stringify(result));
  return result.view.state;
}
export async function saveAndRestore(page, change) {
  await click(page, '저장');
  const before = await state(page);
  await change();
  await click(page, '불러오기');
  await page.waitForFunction(async expected =>
    JSON.stringify((await window.player.view()).view.state) === expected, JSON.stringify(before));
  assert.deepEqual(await state(page), before);
  return before;
}

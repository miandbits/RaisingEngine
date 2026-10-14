import test from 'node:test';
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { mkdir, mkdtemp, readFile, writeFile, cp } from 'node:fs/promises';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { exportGame } from '../../dist/packages/export-service/export.js';
import { PlayerController } from '../../dist/packages/player-service/controller.js';

const click = (page, name) => page.getByRole('button', { name, exact: true }).click();
const state = page => page.evaluate(async () => (await window.player.view()).view.state);
const version = controller => ({ generation: controller.generation, revision: controller.session.state.stateRevision });
async function upload(page, file) {
  const chooser = page.waitForEvent('filechooser');
  await click(page, '불러오기');
  await (await chooser).setFiles(file);
}

test('exported browser executes C++ Wasm and exchanges real download/upload saves with native desktop', { timeout: 120000 }, async t => {
  const output = resolve('.cache/journey-web-ui'); await mkdir(output, { recursive: true });
  const workspace = await mkdtemp(`${output}/run-`), destination = resolve(workspace, 'web');
  const projectRoot = resolve(workspace, 'project');
  await cp(resolve(process.env.JOURNEY_PROJECT_ROOT ?? 'examples/academy-journey'), projectRoot, { recursive: true });
  const built = await buildSnapshot(await loadProject(projectRoot));
  assert.equal(built.ok, true, JSON.stringify(built.diagnostics));
  await exportGame({ content: built.snapshot, projectRoot, destination, target: 'web', runtimes: { web: resolve('dist/web'), windows: resolve('dist/native-player-app') } });
  const mime = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.wasm': 'application/wasm', '.json': 'application/json', '.css': 'text/css', '.png': 'image/png', '.svg': 'image/svg+xml' };
  const server = createServer(async (req, res) => {
    try {
      const pathname = decodeURIComponent(new URL(req.url, 'http://127.0.0.1').pathname);
      const file = resolve(destination, `.${pathname === '/' ? '/index.html' : pathname}`);
      if (!file.startsWith(`${destination}${sep}`)) { res.writeHead(403); res.end(); return; }
      const bytes = await readFile(file);
      res.writeHead(200, { 'Content-Type': mime[extname(file)] ?? 'application/octet-stream', 'Cache-Control': 'no-store' }); res.end(bytes);
    } catch { res.writeHead(404); res.end(); }
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  t.after(() => { server.closeAllConnections(); server.close(); });
  const browser = await chromium.launch({ channel: 'chrome', headless: true });
  t.after(() => browser.close());
  const page = await browser.newPage({ viewport: { width: 1280, height: 800 }, acceptDownloads: true });
  const errors = []; page.on('pageerror', error => errors.push(error.message));
  const browserErrors = []; page.on('console', message => { if (message.type() === 'error') browserErrors.push(message.text()); });
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  await page.waitForFunction(() => !!document.querySelector('.visual-stage') || !!document.querySelector('[role=alert]'), null, { timeout: 10000 }).catch(async error => {
    console.error(JSON.stringify({ errors, browserErrors, body: await page.locator('body').innerText() }));
    await page.screenshot({ path: resolve(output, 'startup-failure.png') });
    throw error;
  });
  assert.equal(await page.getByRole('alert').count(), 0, await page.locator('body').innerText());
  await click(page, '새 게임 시작');
  await page.locator('.visual-stage[data-screen=home]').waitFor();
  assert.equal(await page.evaluate(async () => (await window.player.view()).view.runtimeBackend), 'wasm');
  await click(page, '도시 광장 방문');
  assert.equal(await page.getByRole('button', { name: '유나에게 꽃 선물', exact: true }).isDisabled(), true);
  await click(page, '꽃집으로 이동');
  await click(page, '감사의 꽃 구매');
  await click(page, '광장으로 돌아가기');
  const savedState = await state(page);
  const download = page.waitForEvent('download'); await click(page, '저장');
  const saveFile = resolve(workspace, 'web-download.json'); await (await download).saveAs(saveFile);
  const saved = JSON.parse(await readFile(saveFile, 'utf8'));
  assert.deepEqual(saved.state, savedState);
  const controller = new PlayerController();
  assert.equal((await controller.openProject(projectRoot)).error, null);
  assert.equal(controller.newRun(null).error, null);
  assert.equal((await controller.load(saveFile, version(controller))).error, null);
  assert.deepEqual(controller.session.state, savedState);
  assert.equal(controller.view().runtimeBackend, 'native');
  assert.equal(controller.act({ type: 'interact', interactionId: 'gift-yuna' }, version(controller)).error, null);
  const desktopState = controller.session.state;
  assert.equal(desktopState.phase, 'narrative');
  const desktopSave = resolve(workspace, 'desktop-save.json');
  assert.equal((await controller.save(desktopSave, version(controller))).error, null);
  await upload(page, desktopSave);
  await page.waitForFunction(async expected => JSON.stringify((await window.player.view()).view.state) === expected, JSON.stringify(desktopState));
  assert.ok((await page.getByTestId('dialogue').innerText()).includes('광장에서 만나요'));
  await page.waitForFunction(() => [...document.querySelectorAll('.visual-stage img')].every(image => image.complete && image.naturalWidth > 0));
  await page.waitForTimeout(450);
  await page.screenshot({ path: resolve(output, 'wasm-relationship-event.png') });
  await click(page, '계속');
  await page.locator('.visual-stage[data-screen=city]').waitFor();
  const settled = await state(page);
  assert.equal(settled.relationships.mentor.trust, 15);
  assert.equal(settled.gameplay.inventory.flower, 0);
  const tampered = structuredClone(saved); tampered.state.attributes.money += 1;
  const badFile = resolve(workspace, 'tampered.json'); await writeFile(badFile, JSON.stringify(tampered));
  await upload(page, badFile);
  await page.getByRole('alert').waitFor();
  assert.match(await page.getByRole('alert').innerText(), /저장 상태.*재현/);
  assert.deepEqual(await state(page), settled);
  await page.setViewportSize({ width: 844, height: 390 });
  await page.waitForTimeout(200);
  const stage = await page.locator('.visual-stage').boundingBox();
  assert.ok(stage.x >= -1 && stage.y >= -1 && stage.x + stage.width <= 845 && stage.y + stage.height <= 391);
  await page.screenshot({ path: resolve(output, 'landscape-small.png') });
  assert.deepEqual(errors, []);
});

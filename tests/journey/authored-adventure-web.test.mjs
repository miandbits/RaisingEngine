import test from 'node:test';
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { mkdir, mkdtemp, readFile, writeFile, cp } from 'node:fs/promises';
import { resolve, extname, sep } from 'node:path';
import { chromium } from 'playwright';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { exportGame } from '../../dist/packages/export-service/export.js';

const state = page => page.evaluate(async () => (await window.player.view()).view.state);
const click = (page, name) => page.getByRole('button', { name, exact: true }).click();
const authored = (page, id) => page.locator(`[data-element-id="${id}"] button`).click();
const phase = (page, expected) => page.waitForFunction(async value => (await window.player.view()).view.state?.phase === value, expected);
async function winBattle(page) {
  for (let i = 0; i < 8; i++) {
    const before = await state(page); if (before.phase !== 'battle') return;
    await click(page, '공격');
    await page.waitForFunction(async revision => (await window.player.view()).view.state.stateRevision > revision, before.stateRevision);
  }
  assert.fail('Battle did not end within eight actual UI attacks');
}

test('GUI-authored adventure plays complete route in exported C++ Wasm browser with unequip and downloaded save replay', { timeout: 120000 }, async t => {
  const output = resolve('.cache/journey-adventure-web'); await mkdir(output, { recursive: true });
  const root = await mkdtemp(`${output}/run-`), projectRoot = resolve(root, 'project'), destination = resolve(root, 'web');
  await cp(resolve('examples/academy-adventure'), projectRoot, { recursive: true });
  const built = await buildSnapshot(await loadProject(projectRoot));
  assert.equal(built.ok, true, JSON.stringify(built.diagnostics));
  await exportGame({ content: built.snapshot, projectRoot, destination, target: 'web', runtimes: { web: resolve('dist/web'), windows: resolve('dist/native-player-mobile-check-app') } });
  const mime = { '.html': 'text/html', '.js': 'text/javascript', '.mjs': 'text/javascript', '.wasm': 'application/wasm', '.json': 'application/json', '.css': 'text/css', '.png': 'image/png' };
  const server = createServer(async (req, res) => {
    try {
      const pathname = decodeURIComponent(new URL(req.url, 'http://127.0.0.1').pathname);
      const file = resolve(destination, `.${pathname === '/' ? '/index.html' : pathname}`);
      if (!file.startsWith(`${destination}${sep}`)) { res.writeHead(403); res.end(); return; }
      const bytes = await readFile(file); res.writeHead(200, { 'Content-Type': mime[extname(file)] ?? 'application/octet-stream' }); res.end(bytes);
    } catch { res.writeHead(404); res.end(); }
  });
  await new Promise(done => server.listen(0, '127.0.0.1', done));
  t.after(() => { server.closeAllConnections(); server.close(); });
  const browser = await chromium.launch({ channel: 'chrome', headless: true }); t.after(() => browser.close());
  const page = await browser.newPage({ viewport: { width: 1280, height: 800 }, acceptDownloads: true });
  const errors = []; page.on('pageerror', error => errors.push(error.message));
  await page.goto(`http://127.0.0.1:${server.address().port}`);
  await page.waitForFunction(() => !!document.querySelector('.visual-stage') || !!document.querySelector('[role="alert"]'), null, { timeout: 10000 }).catch(async error => {
    console.error(JSON.stringify({ errors, body: await page.locator('body').innerText() })); throw error;
  });
  assert.equal(await page.getByRole('alert').count(), 0, await page.locator('body').innerText());
  await authored(page, 'start'); await phase(page, 'planning');
  assert.equal(await page.evaluate(async () => (await window.player.view()).view.runtimeBackend), 'wasm');
  await authored(page, 'adventure-link'); await click(page, '수락 · 첫 탐험 의뢰');
  await authored(page, 'schedule'); await authored(page, 'button-new'); await authored(page, 'button-2');
  await click(page, '연습용 검 구매'); await click(page, '우리 집 방문'); await authored(page, 'adventure-link');
  await click(page, '장착 · 연습용 검');
  assert.equal(await page.getByRole('button', { name: '장착 해제 · 연습용 검', exact: true }).isEnabled(), true);
  await click(page, '장착 해제 · 연습용 검'); await click(page, '장착 · 연습용 검');
  await authored(page, 'schedule');
  await page.getByLabel('상순 활동', { exact: true }).selectOption('forest-trip');
  await page.getByLabel('중순 활동', { exact: true }).selectOption('rest'); await page.getByLabel('하순 활동', { exact: true }).selectOption('rest');
  await authored(page, 'advance'); await phase(page, 'ready'); await authored(page, 'advance'); await phase(page, 'expedition');
  await click(page, '이동 · 슬라임 공터'); await phase(page, 'battle');
  const saved = await state(page), saveFile = resolve(root, 'battle-save.json');
  const download = page.waitForEvent('download'); await authored(page, 'save'); await (await download).saveAs(saveFile);
  assert.deepEqual(JSON.parse(await readFile(saveFile, 'utf8')).state, saved);
  await winBattle(page); await phase(page, 'expedition'); const first = await state(page);
  const chooser = page.waitForEvent('filechooser'); await authored(page, 'load'); await (await chooser).setFiles(saveFile); await phase(page, 'battle');
  assert.deepEqual(await state(page), saved);
  await winBattle(page); await phase(page, 'expedition'); assert.deepEqual(await state(page), first);
  assert.equal(first.gameplay.inventory.flower, 1);
  await page.screenshot({ path: resolve(output, 'victory.png') });
  await click(page, '이동 · 입구'); await click(page, '탐험에서 귀환'); await phase(page, 'activity-result');
  await authored(page, 'advance'); await authored(page, 'button-new'); await click(page, '우리 집 방문'); await authored(page, 'adventure-link');
  await click(page, '보상 받기 · 첫 탐험 의뢰'); await phase(page, 'narrative');
  assert.match(await page.getByTestId('dialogue').innerText(), /첫 탐험을 무사히/);
  await page.waitForTimeout(450); await page.screenshot({ path: resolve(output, 'relationship-event.png') });
  await authored(page, 'advance'); await page.locator('.visual-stage[data-screen="adventure-hub"]').waitFor();
  const final = await state(page); assert.equal(final.relationships.mentor.trust, 15); assert.equal(final.systems.quests['first-expedition'].status, 'claimed');
  assert.ok(final.systems.unlockedStages.includes('mentor-friend')); assert.deepEqual(errors, []);
  await writeFile(resolve(output, 'evidence.json'), JSON.stringify({ checkedAt: new Date().toISOString(), destination, contentFingerprint: built.snapshot.contentFingerprint, runtime: 'wasm', saveFile, checks: ['purchase', 'equip', 'unequip', 'quest', 'expedition', 'combat', 'loot', 'download-upload-save', 'exact-replay', 'relationship-event'], finalState: final }, null, 2));
});

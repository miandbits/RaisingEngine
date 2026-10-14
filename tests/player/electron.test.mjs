import test from 'node:test';
import assert from 'node:assert/strict';
import { _electron } from 'playwright';
import { resolve } from 'node:path';
import { cp, mkdir, mkdtemp, readFile, writeFile } from 'node:fs/promises';
import { loadProject } from '../../dist/packages/project-service/loader.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { runScenario } from '../../dist/packages/runtime-service/scenario.js';
import { createSession } from '../../dist/packages/runtime-service/session.js';
import { saveRun } from '../../dist/packages/runtime-service/save.js';

const artifacts = resolve('.cache/player-qa');
const saveFile = resolve(artifacts, 'ui-save.json');
const env = { ...process.env }; delete env.NODE_OPTIONS; delete env.ELECTRON_RUN_AS_NODE;
const sample = resolve('examples/academy-demo');
const launch = () => _electron.launch({ ...(process.env.PLAYER_EXECUTABLE ? { executablePath: resolve(process.env.PLAYER_EXECUTABLE), args: [] } : { args: ['.'] }), cwd: resolve('.'), env, timeout: 30000 });
const view = page => page.evaluate(() => window.player.view()).then(reply => reply.view);
async function idle(page) { await page.getByRole('button', { name: '프로젝트 열기', exact: true }).waitFor(); await page.waitForFunction(() => ![...document.querySelectorAll('button')].find(x => x.textContent === '프로젝트 열기')?.disabled); }
async function click(page, name) { await page.getByRole('button', { name, exact: typeof name === 'string' }).click(); await idle(page); }
async function screenshot(page, name) { await page.evaluate(() => window.scrollTo(0, 0)); await page.screenshot({ path: `${artifacts}/${name}.png`, fullPage: true, animations: 'disabled' }); }
async function roundtrip(page) {
  const before = (await view(page)).state;
  await click(page, '저장하기');
  assert.match(await page.getByRole('status').innerText(), /저장했습니다/);
  await click(page, '불러오기');
  assert.match(await page.getByRole('status').innerText(), /이어갑니다/);
  assert.deepEqual((await view(page)).state, before);
}
async function schedule(page, activities) {
  for (const [i, period] of ['상순', '중순', '하순'].entries()) await page.getByLabel(`${period} 활동`, { exact: true }).selectOption(activities[i]);
  await click(page, '일정 확정');
}

test('Electron UI: both sample endings, full state parity, save/load, errors and window layout', { timeout: 120000 }, async t => {
  await mkdir(artifacts, { recursive: true });
  const electron = await launch();
  t.after(async () => { await electron.evaluate(({ app }) => app.exit(0)).catch(() => {}); });
  electron.process().stderr.on('data', chunk => { if (process.env.PLAYER_DEBUG) process.stderr.write(chunk); });
  const page = await electron.firstWindow();
  const errors = []; page.on('pageerror', error => errors.push(error.message));
  await idle(page);
  assert.equal((await view(page))?.title, '견습생의 두 달');
  await screenshot(page, 'welcome');
  await electron.evaluate(({ dialog }, saveFile) => {
    dialog.showSaveDialog = async () => ({ canceled: false, filePath: saveFile });
    dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [saveFile] });
    dialog.showMessageBox = async () => ({ response: 1, checkboxChecked: false });
  }, saveFile);
  const built = await buildSnapshot(await loadProject(sample)); assert.equal(built.ok, true);
  for (const certified of [true, false]) {
    await click(page, certified ? '새 게임 시작' : '새로운 이야기 시작');
    await schedule(page, ['study', 'work', 'rest']);
    await screenshot(page, 'schedule');
    for (const period of ['상순', '중순', '하순']) {
      await click(page, `${period} 활동 시작`);
      assert.equal((await view(page)).state.phase, 'activity-result');
      if (period === '상순') await roundtrip(page);
      await click(page, '다음으로');
    }
    assert.match(await page.getByTestId('dialogue').innerText(), /수료 시험/);
    await click(page, '계속');
    await roundtrip(page);
    await screenshot(page, 'choice');
    await click(page, certified ? '접수비 5를 내고 참가한다.' : '이번에는 참가하지 않는다.');
    await roundtrip(page);
    await schedule(page, certified ? ['study', 'rest', 'study'] : ['work', 'rest', 'work']);
    for (const period of ['상순', '중순', '하순']) {
      await click(page, `${period} 활동 시작`); await click(page, '다음으로');
    }
    for (let i = 0; i < 10 && (await view(page)).state.phase !== 'finished'; i++) await click(page, '계속');
    assert.equal(await page.getByTestId('ending').innerText(), certified ? '정식 견습 기사' : '새로운 출발');
    await roundtrip(page);
    const scenario = JSON.parse(await readFile(`${sample}/scenarios/${certified ? 'certified' : 'ordinary'}-route.json`, 'utf8'));
    const expected = await runScenario(built.snapshot, scenario); assert.equal(expected.ok, true);
    assert.deepEqual((await view(page)).state, expected.session.state);
    await screenshot(page, `${certified ? 'certified' : 'ordinary'}-ending`);
  }
  // Cancellation and malformed files preserve the live run.
  const final = (await view(page)).state;
  await electron.evaluate(({ dialog }) => { dialog.showMessageBox = async () => ({ response: 0, checkboxChecked: false }); });
  await click(page, '새로운 이야기 시작'); assert.deepEqual((await view(page)).state, final);
  await electron.evaluate(({ dialog }) => { dialog.showMessageBox = async () => ({ response: 1, checkboxChecked: false }); });
  await electron.evaluate(({ dialog }) => { dialog.showOpenDialog = async () => ({ canceled: true, filePaths: [] }); });
  await click(page, '불러오기'); assert.deepEqual((await view(page)).state, final);
  await writeFile(`${artifacts}/bad.json`, '{bad');
  await electron.evaluate(({ dialog }, file) => { dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [file] }); }, `${artifacts}/bad.json`);
  await click(page, '불러오기'); assert.ok(await page.getByRole('alert').isVisible());
  assert.deepEqual((await view(page)).state, final);
  await click(page, '닫기');
  const invalidProject = await mkdtemp(resolve(artifacts, 'invalid-'));
  await electron.evaluate(({ dialog }, file) => { dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [file] }); }, invalidProject);
  await click(page, '프로젝트 열기'); assert.ok(await page.getByRole('alert').isVisible());
  assert.deepEqual((await view(page)).state, final); await click(page, '닫기');
  // The same core rejects an unaffordable activity; the UI allows a corrected schedule.
  const poor = createSession(built.snapshot, 42, { initialOverrides: { attributes: { money: 0, skill: 10 }, flags: {}, relationships: {} } });
  assert.equal(poor.ok, true); assert.equal((await saveRun(saveFile, built.snapshot, poor.session)).ok, true);
  await electron.evaluate(({ dialog }, file) => { dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [file] }); }, saveFile);
  await click(page, '불러오기'); await schedule(page, ['study', 'work', 'rest']);
  const before = (await view(page)).state;
  await click(page, '상순 활동 시작'); assert.ok(await page.getByRole('alert').isVisible());
  assert.deepEqual((await view(page)).state, before);
  await page.getByLabel('상순 활동', { exact: true }).selectOption('rest');
  await page.getByLabel('중순 활동', { exact: true }).selectOption('rest'); await click(page, '일정 확정');
  await click(page, '상순 활동 시작'); assert.equal((await view(page)).state.phase, 'activity-result');
  await electron.evaluate(({ BrowserWindow }) => BrowserWindow.getAllWindows()[0].setSize(900, 680));
  await screenshot(page, 'minimum-window');
  assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > window.innerWidth), false);
  await click(page, '다음으로');
  for (const period of ['중순', '하순']) { await click(page, `${period} 활동 시작`); await click(page, '다음으로'); }
  await click(page, '계속');
  assert.equal(await page.getByRole('button', { name: /접수비 5를 내고 참가한다/ }).isDisabled(), true);
  assert.equal(await page.getByRole('button', { name: '이번에는 참가하지 않는다.', exact: true }).isEnabled(), true);
  await screenshot(page, 'disabled-choice');
  assert.deepEqual(await page.evaluate(() => ({ require: typeof window.require, process: typeof window.process })), { require: 'undefined', process: 'undefined' });
  const preferences = await electron.evaluate(({ BrowserWindow }) => {
    const p = BrowserWindow.getAllWindows()[0].webContents.getLastWebPreferences();
    return { sandbox: p.sandbox, contextIsolation: p.contextIsolation, nodeIntegration: p.nodeIntegration };
  });
  assert.deepEqual(preferences, { sandbox: true, contextIsolation: true, nodeIntegration: false });
  assert.deepEqual(errors, []);
});

test('Electron media: manifest assets, isolation, frozen hashes and changed project', { timeout: 60000 }, async t => {
  await mkdir(artifacts, { recursive: true });
  const fixture = await mkdtemp(resolve(artifacts, 'assets-'));
  await cp(sample, fixture, { recursive: true });
  const png = Buffer.from('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+aAt8AAAAASUVORK5CYII=', 'base64');
  await writeFile(`${fixture}/assets/test.png`, png);
  const wave = Buffer.alloc(8044); wave.write('RIFF'); wave.writeUInt32LE(8036, 4); wave.write('WAVEfmt ', 8); wave.writeUInt32LE(16, 16);
  wave.writeUInt16LE(1, 20); wave.writeUInt16LE(1, 22); wave.writeUInt32LE(8000, 24); wave.writeUInt32LE(8000, 28); wave.writeUInt16LE(1, 32); wave.writeUInt16LE(8, 34); wave.write('data', 36); wave.writeUInt32LE(8000, 40); wave.fill(128, 44);
  await writeFile(`${fixture}/assets/test.wav`, wave);
  const manifest = { formatVersion: '0.1.0', assets: [{ id: 'test-image', kind: 'image', path: 'assets/test.png' }, { id: 'test-audio', kind: 'audio', path: 'assets/test.wav' }] };
  await writeFile(`${fixture}/assets/manifest.json`, JSON.stringify(manifest));
  const activity = JSON.parse(await readFile(`${fixture}/activities/study.json`, 'utf8'));
  activity.presentation = { backgroundAssetId: 'test-image', musicAssetId: 'test-audio', soundAssetId: 'test-audio', portraits: [{ characterId: 'mentor', portraitId: 'default', position: 'center' }] };
  await writeFile(`${fixture}/activities/study.json`, JSON.stringify(activity));
  const mentor = JSON.parse(await readFile(`${fixture}/characters/mentor.json`, 'utf8')); mentor.portraits = { default: 'test-image' };
  await writeFile(`${fixture}/characters/mentor.json`, JSON.stringify(mentor));
  const electron = await launch();
  t.after(async () => { await electron.evaluate(({ app }) => app.exit(0)).catch(() => {}); });
  const page = await electron.firstWindow(); await idle(page);
  await electron.evaluate(({ dialog }, fixture) => { dialog.showOpenDialog = async () => ({ canceled: false, filePaths: [fixture] }); }, fixture);
  await click(page, '프로젝트 열기'); assert.equal(await page.getByRole('alert').count(), 0);
  await click(page, '새 게임 시작'); await schedule(page, ['study', 'rest', 'rest']); await click(page, '소리 켜기');
  await click(page, '상순 활동 시작');
  await page.waitForFunction(() => [...document.images].length === 2 && [...document.images].every(image => image.complete && image.naturalWidth === 1));
  assert.equal(await page.getByRole('alert').count(), 0);
  const generation = (await view(page)).generation;
  const statuses = await electron.evaluate(async ({ net }, generation) => {
    const statuses = [];
    for (const tail of [`test-image?g=${generation}`, `test-audio?g=${generation}`, `missing?g=${generation}`, `test-image?g=${generation - 1}`, `..%2Fproject.json?g=${generation}`]) {
      const response = await net.fetch(`raising-asset://project/${tail}`); statuses.push(response.status);
    }
    return statuses;
  }, generation);
  assert.deepEqual(statuses, [200, 200, 404, 404, 404]);
  await writeFile(`${fixture}/assets/test.png`, Buffer.from('changed'));
  const changed = await electron.evaluate(async ({ net }, generation) => (await net.fetch(`raising-asset://project/test-image?g=${generation}`)).status, generation);
  assert.equal(changed, 409);
  await click(page, '소리 끄기');
});

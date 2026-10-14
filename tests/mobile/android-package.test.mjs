import test from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdir, mkdtemp, readFile, readdir, writeFile, access } from 'node:fs/promises';
import { resolve, join } from 'node:path';

async function fixture() {
  await mkdir('.cache/android-package-tests', { recursive: true });
  const root = await mkdtemp(resolve('.cache/android-package-tests/case-'));
  const bundle = join(root, 'bundle'), sdk = join(root, 'fake-sdk'), java = join(root, 'fake-java');
  for (const dir of [sdk, join(java, 'bin'), join(bundle, 'assets'), join(bundle, 'fonts'), join(bundle, 'licenses', 'nested')]) await mkdir(dir, { recursive: true });
  // Presence satisfies prepare-only preflight. These are deliberately not executable SDK tools.
  await writeFile(join(java, 'bin', process.platform === 'win32' ? 'java.exe' : 'java'), 'MUST NOT EXECUTE');
  const game = { formatVersion: '1', content: { contentFingerprint: 'sha256:fixture-snapshot', project: { gameId: 'android-test', title: 'Android fixture' } }, assets: { flower: 'assets/flower.png', music: 'assets/theme.ogg' } };
  const files = new Map([
    ['game.json', Buffer.from(JSON.stringify(game))], ['assets/flower.png', Buffer.from([0, 1, 2, 255])],
    ['assets/theme.ogg', Buffer.from('fixture audio bytes')], ['fonts/NotoSansKR.ttf', Buffer.from('fixture font bytes')],
    ['fonts/OFL.txt', Buffer.from('font license')], ['licenses/SDL.txt', Buffer.from('SDL license')], ['licenses/nested/RmlUi.txt', Buffer.from('RmlUi license')],
  ]);
  for (const [name, bytes] of files) await writeFile(join(bundle, name), bytes);
  await writeFile(join(bundle, 'RaisingPlayer.exe'), 'DESKTOP EXE MUST NOT SHIP');
  await writeFile(join(bundle, 'unlisted-secret.txt'), 'UNLISTED FILE MUST NOT SHIP');
  await writeFile(join(bundle, 'assets', 'unlisted.png'), 'UNLISTED IMAGE MUST NOT SHIP');
  const run = (out, extra = []) => spawnSync(process.execPath, ['scripts/build-android.mjs', '--prepare-only', '--bundle', bundle, '--out', out,
    '--sdk', sdk, '--java-home', java, '--gradle', join(root, 'DO-NOT-RUN-GRADLE'), '--application-id', 'org.example.preparation', ...extra],
    { cwd: resolve('.'), encoding: 'utf8', env: { ...process.env, NODE_OPTIONS: '' }, windowsHide: true, timeout: 30000 });
  return { root, bundle, game, files, run };
}
async function names(root, prefix = '') {
  const result = [];
  for (const entry of await readdir(root, { withFileTypes: true })) {
    const name = prefix + entry.name;
    if (entry.isDirectory()) result.push(...await names(join(root, entry.name), name + '/'));
    else result.push(name);
  }
  return result.sort();
}
function succeeded(result) { assert.equal(result.status, 0, `${result.error ?? ''}\n${result.stdout}\n${result.stderr}`); }

test('Android prepare-only stages exact declared assets, fonts, licenses and launcher without desktop binaries', async () => {
  const f = await fixture(), out = join(f.root, 'prepared'); succeeded(f.run(out));
  const app = join(out, 'project/app/src/main'), staged = join(app, 'assets/game');
  assert.deepEqual(await names(staged), [...f.files.keys()].sort());
  for (const [name, bytes] of f.files) assert.deepEqual(await readFile(join(staged, name)), bytes, name);
  assert.deepEqual(await readFile(join(app, 'AndroidManifest.xml')), await readFile('native/mobile/android/AndroidManifest.xml'));
  assert.deepEqual(await readFile(join(app, 'java/org/raisingengine/player/RaisingActivity.java')), await readFile('native/mobile/android/RaisingActivity.java'));
  await access(join(app, 'java/org/libsdl/app/SDLActivity.java'));
  const report = JSON.parse(await readFile(join(out, 'build.json'), 'utf8'));
  assert.equal(report.status, 'source-prepared'); assert.equal(report.distribution, 'debug-preview');
  assert.equal(report.applicationId, 'org.example.preparation'); assert.equal(report.contentFingerprint, f.game.content.contentFingerprint);
  assert.match(report.buildId, /^[a-f0-9]{64}$/);
  assert.equal(await readFile(join(app, 'assets/raising-build-id.txt'), 'utf8'), report.buildId);
  assert.ok(!(await names(out)).some(name => /\.(apk|exe)$/i.test(name)), 'prepare-only cannot claim or include compiled applications');
});

test('Android content build ID is reproducible and changes when an asset byte changes', async () => {
  const f = await fixture(); const ids = [];
  for (let i = 0; i < 3; i++) {
    if (i === 2) await writeFile(join(f.bundle, 'assets/flower.png'), Buffer.from([0, 1, 3, 255]));
    const out = join(f.root, `prepared-${i}`); succeeded(f.run(out));
    ids.push(JSON.parse(await readFile(join(out, 'build.json'), 'utf8')).buildId);
  }
  assert.equal(ids[0], ids[1]); assert.notEqual(ids[1], ids[2]);
});

test('Android preparation refuses an existing output and preserves its contents', async () => {
  const f = await fixture(), out = join(f.root, 'existing'); await mkdir(out); await writeFile(join(out, 'keep.txt'), 'original');
  const result = f.run(out); assert.notEqual(result.status, 0); assert.match(result.stderr, /Output already exists/);
  assert.deepEqual(await names(out), ['keep.txt']); assert.equal(await readFile(join(out, 'keep.txt'), 'utf8'), 'original');
});

test('Android preparation rejects traversal, absolute, and missing declared asset paths before creating output', async () => {
  const f = await fixture();
  const invalid = ['../outside.png', '/outside.png', 'assets/../outside.png', 'assets/missing.png'];
  for (let i = 0; i < invalid.length; i++) {
    await writeFile(join(f.bundle, 'game.json'), JSON.stringify({ ...f.game, assets: { flower: invalid[i] } }));
    const out = join(f.root, `rejected-${i}`), result = f.run(out);
    assert.notEqual(result.status, 0, invalid[i]); assert.match(result.stderr, /Invalid exported asset path|ENOENT/);
    await assert.rejects(access(out), /ENOENT/);
  }
});

import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile, writeFile, mkdtemp, mkdir, cp, copyFile, appendFile, readdir} from 'node:fs/promises';
import {spawn} from 'node:child_process';
import {createHash} from 'node:crypto';
import path from 'node:path';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {captureGoalBaseline} from '../../dist/packages/analysis-service/goal-history.js';
import {nativeExecutable} from '../../dist/packages/native-bridge/index.js';
import {runtimeIdentity, saveGoalBaseline} from '../../apps/qt-studio/goal-baselines.mjs';
import {checkGoalImpacts} from '../../apps/qt-studio/goal-impact.mjs';
import {changeGoalBaseline, goalBaselineSelection} from '../../apps/qt-studio/goal-approvals.mjs';

const json = async file => JSON.parse(await readFile(file, 'utf8'));
async function content(root) {
  const result = await buildSnapshot(await loadProject(root));
  assert.equal(result.ok, true, JSON.stringify(result.diagnostics)); return result.snapshot;
}
async function cli(root, directory, args = [], env = {}) {
  const output = path.join(directory, 'reports', String(cli.sequence++));
  const child = spawn(process.execPath, ['scripts/check-goal-regression.mjs', root, '--report-dir', output, ...args], {
    cwd: path.resolve('.'), windowsHide: true, env: {...process.env, RAISING_NATIVE_TRANSPORT: 'process', ...env}, stdio: ['ignore', 'pipe', 'pipe'],
  });
  let log = ''; child.stdout.on('data', b => log += b); child.stderr.on('data', b => log += b);
  const exit = await new Promise((resolve, reject) => { child.on('error', reject); child.on('exit', resolve); });
  const report = await json(path.join(output, 'report.json'));
  assert.equal(exit, report.exitCode, log);
  assert.match(await readFile(path.join(output, 'summary.md'), 'utf8'), /경로 회귀 검증/);
  assert.match(await readFile(path.join(output, 'junit.xml'), 'utf8'), /<testsuite/);
  return {exit, report, output};
}
cli.sequence = 0;

test('CLI honors exact partial approvals, rejects remaining regressions and ending failures, and writes reports', {timeout: 240000}, async () => {
  await mkdir('.cache', {recursive: true});
  const root = await mkdtemp(path.resolve('.cache/goal-regression-cli-'));
  await cp(path.resolve('projects/starlight-three-months'), root, {recursive: true, filter: s => !s.split(path.sep).some(part => ['.studio', 'playtests'].includes(part))});
  await mkdir(path.join(root, 'playtests/routes'), {recursive: true});
  const route = await json('projects/starlight-three-months/playtests/routes/ordinary.json');
  await writeFile(path.join(root, 'playtests/routes/ordinary.json'), JSON.stringify(route));
  const initial = await content(root), baseline = await captureGoalBaseline(initial, route, 'CLI initial', await runtimeIdentity());
  const saved = await saveGoalBaseline(root, baseline), originalBytes = await readFile(saved.file);
  let result = await cli(root, root); assert.equal(result.exit, 0);
  assert.ok(result.report.rows[0].replay.saves > 1); assert.equal(result.report.rows[0].replay.ok, true);
  const projectFile = path.join(root, 'project.json'), project = await json(projectFile);
  const initialMoney = project.initialState.attributes.money;
  project.initialState.attributes.money = initialMoney + 1;
  await writeFile(projectFile, JSON.stringify(project));
  result = await cli(root, root); assert.equal(result.exit, 1);
  const changed = await content(root), impact = await checkGoalImpacts(root, changed, 'test');
  const row = impact.rows[0], selectable = row.changeItems.filter(i => i.approvable);
  assert.ok(selectable.some(i => i.path === '/attributes/money'));
  await changeGoalBaseline(root, {
    expectedRevision: (await goalBaselineSelection(root)).revision, route, baseline, content: changed,
    reportId: impact.id, itemIds: selectable.map(i => i.id), note: 'Intended numeric change', validate: async () => {},
  });
  result = await cli(root, root); assert.equal(result.exit, 0, JSON.stringify(result.report.rows[0].changeItems.filter(i => !i.approved)));
  assert.ok(result.report.rows[0].approvedChangeCount > 0);
  const ledgerBytes = await readFile(path.join(root, 'playtests/baselines/.approvals'));
  // Actual alternate binary hash: same executable with an inert trailer, not a
  // mocked identity. The portable policy must replay the original before reuse.
  const alternate = path.join(root, process.platform === 'win32' ? 'alternate.exe' : 'alternate');
  await copyFile(nativeExecutable(), alternate); await appendFile(alternate, '\nregression-test-trailer\n');
  result = await cli(root, root, [], {RAISING_NATIVE_EXECUTABLE: alternate}); assert.equal(result.exit, 1);
  result = await cli(root, root, ['--runtime-policy', 'verify-replay'], {RAISING_NATIVE_EXECUTABLE: alternate});
  assert.equal(result.exit, 0); assert.equal(result.report.rows[0].runtimeVerification.status, 'verified');
  // No blanket ignore: changing an already approved value fails again.
  project.initialState.attributes.money = initialMoney + 2;
  await writeFile(projectFile, JSON.stringify(project));
  result = await cli(root, root); assert.equal(result.exit, 1); assert.ok(result.report.rows[0].pendingChangeCount > 0);
  project.initialState.attributes.money = initialMoney;
  await writeFile(projectFile, JSON.stringify(project));
  result = await cli(root, root); assert.equal(result.exit, 1, 'Returning to original is a regression against the approved expectation');
  // Keep the approved money change, introduce an independent unapproved value.
  project.initialState.attributes.money = initialMoney + 1;
  project.initialState.attributes.stamina -= 1;
  await writeFile(projectFile, JSON.stringify(project));
  result = await cli(root, root); assert.equal(result.exit, 1);
  assert.ok(result.report.rows[0].approvedChangeCount > 0); assert.ok(result.report.rows[0].pendingChangeCount > 0);
  const endingFile = path.join(root, 'endings/knight.json'), endingBytes = await readFile(endingFile);
  await writeFile(endingFile, JSON.stringify({...JSON.parse(endingBytes), requirements: {op: 'always'}}));
  result = await cli(root, root); assert.equal(result.exit, 1); assert.equal(result.report.rows[0].status, 'failed');
  assert.equal(result.report.rows[0].replay.ok, false);
  assert.match(await readFile(path.join(result.output, 'junit.xml'), 'utf8'), /<failure/);
  await writeFile(endingFile, endingBytes);
  assert.deepEqual(await readFile(saved.file), originalBytes); assert.deepEqual(await readFile(path.join(root, 'playtests/baselines/.approvals')), ledgerBytes);
  // A valid checksum alone cannot authorize a mismatching recorded execution.
  const incompatible = structuredClone(baseline); incompatible.trace.states[0].attributes.money += 10;
  const {checksum, ...payload} = incompatible; incompatible.checksum = createHash('sha256').update(JSON.stringify(payload)).digest('hex');
  await writeFile(saved.file, JSON.stringify(incompatible));
  result = await cli(root, root, ['--runtime-policy', 'verify-replay'], {RAISING_NATIVE_EXECUTABLE: alternate});
  assert.equal(result.exit, 2); assert.equal(result.report.rows[0].runtimeVerification.status, 'mismatch');
  await writeFile(saved.file, originalBytes);
  await writeFile(path.join(root, 'playtests/baselines/.approvals'), 'corrupt');
  result = await cli(root, root); assert.equal(result.exit, 2);
  await writeFile(path.join(root, 'playtests/baselines/.approvals'), ledgerBytes);
  result = await cli(root, root, ['--timeout-ms', '1']); assert.equal(result.exit, 2);
  assert.ok(result.report.counts.incomplete > 0);
  await writeFile(path.resolve('.cache/goal-regression-cli-fixture.txt'), root);
});

test('CLI fails closed for empty/missing baselines, invalid project, malformed route and arguments', {timeout: 120000}, async () => {
  const root = await mkdtemp(path.resolve('.cache/goal-regression-cli-'));
  await cp(path.resolve('projects/starlight-three-months'), root, {recursive: true, filter: s => !s.split(path.sep).some(part => ['.studio', 'baselines'].includes(part))});
  let result = await cli(root, root); assert.equal(result.exit, 2); assert.ok(result.report.counts['missing-baseline'] > 0);
  const routes = path.join(root, 'playtests/routes');
  for (const file of await readdir(routes)) await writeFile(path.join(routes, file), 'invalid-json');
  result = await cli(root, root); assert.equal(result.exit, 2); assert.ok(result.report.counts.error > 0);
  const empty = await mkdtemp(path.resolve('.cache/goal-regression-cli-'));
  await cp(path.resolve('projects/starlight-three-months'), empty, {recursive: true, filter: s => !s.split(path.sep).some(part => ['.studio', 'playtests'].includes(part))});
  result = await cli(empty, root); assert.equal(result.exit, 2); assert.match(result.report.errors[0], /경로가 없습니다/);
  result = await cli(path.join(root, 'missing'), root); assert.equal(result.exit, 2);
  result = await cli(root, root, ['--runtime-policy', 'ignore']); assert.equal(result.exit, 2);
  result = await cli(root, root, ['--timeout-ms=-1']); assert.equal(result.exit, 2);
});

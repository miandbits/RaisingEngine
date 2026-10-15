import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdtemp, cp, mkdir, readFile, writeFile, copyFile, appendFile} from 'node:fs/promises';
import path from 'node:path';
import {QtStudioService} from '../../apps/qt-studio/service.mjs';
import {runGoalRegression} from '../../apps/qt-studio/goal-regression.mjs';
import {writeGoalReports} from '../../scripts/goal-regression-report.mjs';
import {nativeExecutable} from '../../dist/packages/native-bridge/index.js';

test('CI report stays immutable while local graph approval, restore and revalidation use fresh trusted results', {timeout: 180000}, async () => {
  await mkdir('.cache', {recursive: true});
  const root = await mkdtemp(path.resolve('.cache/goal-ci-'));
  await cp(path.resolve('examples/starlight-three-months'), root, {recursive: true, filter: s => !s.split(path.sep).includes('.studio')});
  const projectFile = path.join(root, 'project.json'), project = JSON.parse(await readFile(projectFile, 'utf8'));
  project.initialState.attributes.money += 1;
  await writeFile(projectFile, JSON.stringify(project, null, 2));
  const ci = await runGoalRegression(root, {runtimePolicy: 'verify-replay'});
  assert.equal(ci.exitCode, 1);
  await writeGoalReports(path.join(root, 'reports'), ci);
  const file = path.join(root, 'reports/report.json'), bytes = await readFile(file);
  const service = new QtStudioService(); await service.handle({method: 'open', params: {root}});
  const call = (method, params = {}) => service.handle({method, params: {revision: service.editor.revision, ...params}});
  const imported = (await call('goalCiOpen', {file})).goalCi;
  assert.equal(imported.source.id, ci.id);
  let response = await call('goalCiRecheck', {id: imported.id}), report = response.goalImpact;
  assert.equal(response.goalCi.matches, true); assert.notEqual(report.id, ci.id);
  const row = () => report.rows.find(r => r.routeId === 'ordinary');
  const params = () => ({impactReportId: report.id, impactRow: report.rows.findIndex(r => r.routeId === 'ordinary')});
  await assert.rejects(call('goalCompareOpen', {impactReportId: ci.id, impactRow: 1}));
  let graph = (await call('goalCompareOpen', params())).goalComparison;
  assert.equal(graph.jumpStep, 0);
  let step = (await call('goalCompareStep', {comparisonId: graph.id, leftStep: 0, rightStep: 0})).goalComparisonStep;
  assert.equal(step.stale, false);
  const money = step.review.rows.find(r => r.path === '/attributes/money'); assert.equal(money.status, 'regression');
  assert.equal(money.current, money.original + 1);
  const originalBaseline = row().baselineId;
  report = (await call('goalBaselineApprovePartial', {...params(), itemIds: [money.id], note: 'CI review: intentional initial money'})).goalImpact;
  assert.equal(row().status, 'unchanged'); assert.ok(row().replay.ok); assert.ok(row().approvedChangeCount > 0);
  assert.equal((await call('goalCompareStep', {comparisonId: graph.id, leftStep: 0, rightStep: 0})).goalComparisonStep.stale, true);
  response = await call('goalCiRecheck', {id: imported.id}); report = response.goalImpact;
  assert.equal(response.goalCi.matches, false); assert.ok(response.goalCi.reasons.some(s => s.includes('승인')));
  assert.equal(response.goalCi.source.rows.find(r => r.routeId === 'ordinary').status, 'changed');
  report = (await call('goalBaselineRestore', {...params(), baselineId: originalBaseline, note: 'restore original'})).goalImpact;
  assert.equal(row().status, 'changed'); assert.equal(row().approvedChangeCount, 0); assert.ok(row().replay.ok);
  graph = (await call('goalCompareOpen', params())).goalComparison;
  assert.equal(graph.jumpStep, 0);
  assert.deepEqual(await readFile(file), bytes);
  // Dirty content cannot silently validate the saved project instead.
  const doc = service.editor.documents.find(d => d.kind === 'project');
  await call('edit', {edit: {type: 'replace', file: doc.file, data: {...doc.data, title: doc.data.title + ' draft'}}});
  await assert.rejects(call('goalCiRecheck', {id: imported.id}), /저장/);
  await call('edit', {edit: {type: 'replace', file: doc.file, data: doc.data}});
  // An imported report cannot choose a local file, route, baseline or item ID.
  const hostile = {...ci, project: '../do-not-follow', rows: ci.rows.map(r => ({...r, baselineId: '../outside', changeItems: []}))};
  await writeFile(file, JSON.stringify(hostile));
  const next = (await call('goalCiOpen', {file})).goalCi;
  await assert.rejects(call('goalCiRecheck', {id: imported.id}), /만료/);
  response = await call('goalCiRecheck', {id: next.id});
  assert.ok(response.goalImpact.rows.find(r => r.routeId === 'ordinary').changeItems.length > 0);
  const invalid = path.join(root, 'invalid.json');
  await writeFile(invalid, JSON.stringify({...ci, rows: [ci.rows[0], ci.rows[0]]}));
  await assert.rejects(call('goalCiOpen', {file: invalid}), /중복/);
  await writeFile(invalid, JSON.stringify({version: 1, kind: 'wrong', rows: []}));
  await assert.rejects(call('goalCiOpen', {file: invalid}), /지원하지/);
  await writeFile(file, bytes);
  // Return a clean fixture with original baseline active for real Qt interactions.
  await call('goalCiOpen', {file});
  await writeFile(path.resolve('.cache/goal-ci-fixture.txt'), root);
});

test('portable CI graph approvals use the verified baseline context across runtime hashes', {timeout: 120000}, async () => {
  const root = await mkdtemp(path.resolve('.cache/goal-ci-portable-'));
  await cp(path.resolve('examples/starlight-three-months'), root, {recursive: true, filter: s => !s.split(path.sep).includes('.studio')});
  const projectFile = path.join(root, 'project.json'), project = JSON.parse(await readFile(projectFile, 'utf8'));
  project.initialState.attributes.money += 1; await writeFile(projectFile, JSON.stringify(project));
  const binary = path.join(root, process.platform === 'win32' ? 'alternate.exe' : 'alternate');
  await copyFile(nativeExecutable(), binary); await appendFile(binary, '\nCI editor portability test\n');
  const previous = process.env.RAISING_NATIVE_EXECUTABLE;
  try {
    process.env.RAISING_NATIVE_EXECUTABLE = binary;
    const ci = await runGoalRegression(root, {runtimePolicy: 'verify-replay'});
    await writeGoalReports(path.join(root, 'reports'), ci);
    const service = new QtStudioService(); await service.handle({method: 'open', params: {root}});
    const call = (method, params = {}) => service.handle({method, params: {revision: service.editor.revision, ...params}});
    const imported = (await call('goalCiOpen', {file: path.join(root, 'reports/report.json')})).goalCi;
    let report = (await call('goalCiRecheck', {id: imported.id})).goalImpact;
    const params = () => ({impactReportId: report.id, impactRow: report.rows.findIndex(r => r.routeId === 'ordinary')});
    assert.equal(report.rows[params().impactRow].runtimeVerification.status, 'verified');
    let graph = (await call('goalCompareOpen', params())).goalComparison;
    let step = (await call('goalCompareStep', {comparisonId: graph.id, leftStep: 0, rightStep: 0})).goalComparisonStep;
    assert.equal(step.stale, false); assert.equal(step.review.canApprove, true);
    const money = step.review.rows.find(r => r.path === '/attributes/money');
    report = (await call('goalBaselineApprovePartial', {...params(), itemIds: [money.id]})).goalImpact;
    assert.equal(report.rows[params().impactRow].status, 'unchanged');
    graph = (await call('goalCompareOpen', params())).goalComparison;
    step = (await call('goalCompareStep', {comparisonId: graph.id, leftStep: 0, rightStep: 0})).goalComparisonStep;
    assert.equal(step.stale, false); assert.equal(step.review.rows.find(r => r.path === '/attributes/money').status, 'approved');
    assert.equal((await runGoalRegression(root, {runtimePolicy: 'verify-replay'})).rows.find(r => r.routeId === 'ordinary').status, 'unchanged');
  } finally {
    if (previous === undefined) delete process.env.RAISING_NATIVE_EXECUTABLE; else process.env.RAISING_NATIVE_EXECUTABLE = previous;
  }
});

import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdir, readFile, writeFile, access } from 'node:fs/promises';
import path from 'node:path';
import { content, doc, loaded, temporary } from './helpers.mjs';
import { validateProject } from '../../dist/packages/validation/index.js';
import { createSession, dispatchSession } from '../../dist/packages/runtime-service/session.js';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';
import { nativeCreateRun, nativeDispatchCommand } from '../../dist/packages/native-bridge/index.js';
import { exportGame } from '../../dist/packages/export-service/export.js';

const details = [{ attributeId: 'money', label: '현재 비용 기준', valueFormat: '<b>{value}</b> $& {value}' }, { attributeId: 'skill' }];
const must = r => { assert.equal(r.ok, true, JSON.stringify(r.error ?? r.diagnostics)?.slice(0,1200)); return r; };
const comparable = state => { const { contentFingerprint, ...same } = state; return same; };

test('activity preview metadata is optional, bounded by Unicode code points, and preserved literally', async () => {
  const legacy = await content();
  assert.ok(legacy.activities.every(a => !Object.hasOwn(a, 'previewAttributes')));
  const valid = await content(p => { doc(p, 'activity', 'rest').previewAttributes = [
    { attributeId: 'money', label: '😀'.repeat(80), valueFormat: '😀'.repeat(160) },
    ...Array.from({ length: 7 }, () => ({ attributeId: 'skill', valueFormat: '' })),
  ]; });
  assert.equal(valid.activities.find(a => a.id === 'rest').previewAttributes.length, 8);
  for (const [value, pointer] of [
    [[], '/previewAttributes'],
    [Array.from({ length: 9 }, () => ({ attributeId: 'skill' })), '/previewAttributes'],
    [[{ attributeId: 'skill', label: '' }], '/previewAttributes/0/label'],
    [[{ attributeId: 'skill', label: '😀'.repeat(81) }], '/previewAttributes/0/label'],
    [[{ attributeId: 'skill', valueFormat: '😀'.repeat(161) }], '/previewAttributes/0/valueFormat'],
  ]) {
    const p = structuredClone(loaded); doc(p, 'activity', 'rest').previewAttributes = value;
    const result = await validateProject(p); assert.equal(result.valid, false);
    assert.ok(result.diagnostics.some(d => d.code === 'SCHEMA_INVALID' && d.file === 'activities/rest.json' && d.pointer === pointer), JSON.stringify(result.diagnostics).slice(0,1200));
  }
  const exact = await content(p => { doc(p, 'activity', 'rest').previewAttributes = structuredClone(details); });
  assert.deepEqual(exact.activities.find(a => a.id === 'rest').previewAttributes, details);
});

test('existing semantic walker resolves preview attributes once at the exact activity-document location', async () => {
  const p = structuredClone(loaded); doc(p, 'activity', 'rest').previewAttributes = [{ attributeId: 'missing-preview' }];
  const result = await validateProject(p); assert.equal(result.valid, false);
  const matches = result.diagnostics.filter(d => d.code === 'REFERENCE_MISSING' && d.file === 'activities/rest.json' && d.pointer === '/previewAttributes/0/attributeId');
  assert.equal(matches.length, 1, JSON.stringify(result.diagnostics).slice(0,1200));
  assert.deepEqual(matches[0].relatedIds, ['missing-preview']);
});

test('preview presentation does not change core transitions, RNG, commands or save semantics in TS and C++', async () => {
  const plain = await content(), decorated = await content(p => { doc(p, 'activity', 'rest').previewAttributes = structuredClone(details); });
  assert.notEqual(decorated.contentFingerprint, plain.contentFingerprint);
  let a = must(createSession(plain, 711)).session, b = must(createSession(decorated, 711)).session;
  assert.deepEqual(comparable(a.state), comparable(b.state));
  assert.deepEqual(must(nativeCreateRun(decorated, 711, { mode: 'full' })).state, b.state);
  for (const command of [{ type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] }, { type: 'executeActivity' }, { type: 'acknowledgeResult' }, { type: 'resolvePeriodEnd' }]) {
    const cpp = must(nativeDispatchCommand(decorated, b.state, command, b.state.stateRevision));
    const ar = must(dispatchSession(plain, a, command, a.state.stateRevision));
    const br = must(dispatchSession(decorated, b, command, b.state.stateRevision));
    assert.deepEqual(cpp.state, br.session.state); assert.deepEqual(cpp.transition, br.transition);
    assert.deepEqual(ar.transition, br.transition); assert.deepEqual(comparable(ar.session.state), comparable(br.session.state));
    a = ar.session; b = br.session;
  }
  assert.deepEqual(a.commands, b.commands); assert.deepEqual(a.origin, b.origin);
  const saved = must(await encodeSave(decorated, b));
  assert.deepEqual(must(await decodeSave(decorated, saved.text)).session, b);
  assert.equal(must(await encodeSave(decorated, must(await decodeSave(decorated, saved.text)).session)).text, saved.text);
  assert.equal((await decodeSave(plain, saved.text)).error.code, 'CONTENT_MISMATCH');
});

test('preview capability gates web and native export while legacy export and exact metadata remain intact', async t => {
  const plain = await content(), decorated = await content(p => { doc(p, 'activity', 'rest').previewAttributes = structuredClone(details); });
  const root = await temporary(t);
  const base = { formatVersion: '1', imageExtensions: ['.png','.jpg','.jpeg','.webp'], audioExtensions: ['.wav','.mp3','.ogg'], fontExtensions: ['.ttf','.otf'] };
  for (const target of ['web', 'windows']) {
    // Packaging-only stubs: this verifies the gate, not executable player behavior.
    const runtime = path.join(root, `${target}-runtime`); await mkdir(runtime);
    await writeFile(path.join(runtime, target === 'web' ? 'index.html' : 'RaisingPlayer.exe'), 'capability contract fixture');
    const capabilities = path.join(runtime, 'capabilities.json'); await writeFile(capabilities, JSON.stringify(base));
    const options = { target, projectRoot: loaded.root, runtimes: { web: runtime, windows: runtime } };
    await exportGame({ ...options, content: plain, destination: path.join(root, `${target}-legacy`) });
    const old = path.join(root, `${target}-old`);
    await assert.rejects(exportGame({ ...options, content: decorated, destination: old }), /activity-summary-v1/);
    await assert.rejects(access(old), { code: 'ENOENT' });
    await writeFile(capabilities, JSON.stringify({ ...base, visualFeatures: ['activity-summary-v1'] }));
    const destination = path.join(root, `${target}-current`);
    await exportGame({ ...options, content: decorated, destination });
    const bundle = JSON.parse(await readFile(path.join(destination, 'game.json'), 'utf8'));
    assert.deepEqual(bundle.content.activities.find(a => a.id === 'rest').previewAttributes, details);
  }
});

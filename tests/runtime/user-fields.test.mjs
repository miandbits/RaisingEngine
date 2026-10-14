import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile, mkdir, writeFile, access } from 'node:fs/promises';
import path from 'node:path';
import Ajv2020 from 'ajv/dist/2020.js';
import { content, doc, loaded, temporary } from './helpers.mjs';
import { createRun, dispatchCommand } from '../../dist/packages/core/run.js';
import { createSession, dispatchSession } from '../../dist/packages/runtime-service/session.js';
import { encodeSave, decodeSave } from '../../dist/packages/runtime-service/save.js';
import { buildSnapshot } from '../../dist/packages/project-service/snapshot.js';
import { validateProject } from '../../dist/packages/validation/index.js';
import { PlayerController } from '../../dist/packages/player-service/controller.js';
import { WebPlayerController } from '../../dist/packages/web-player/controller.js';
import { calendarTime, getNarrativeView } from '../../dist/packages/core/index.js';
import { getGameplayView } from '../../dist/packages/gameplay/service.js';
import { exportGame } from '../../dist/packages/export-service/export.js';
import { createVisual } from '../../dist/packages/visual/model.js';
import { getSystemsView } from '../../dist/packages/systems/service.js';

const fields = [
  { id: 'daughter-name', type: 'text', label: '이름', initial: '올리브', minLength: 1, maxLength: 4 },
  { id: 'family-name', type: 'text', label: '가족 이름', initial: '가족', minLength: 0, maxLength: 40 },
];
const named = () => content(p => { doc(p, 'project').userFields = structuredClone(fields); });
const must = r => { assert.equal(r.ok, true, JSON.stringify(r)); return r; };

test('creation merges defaults and counts Korean/non-BMP Unicode without trimming or truncation', async () => {
  const c = await named();
  const plain = must(createRun(c, 42, { mode: 'full' })).state;
  assert.deepEqual(plain.userValues, { 'daughter-name': '올리브', 'family-name': '가족' });
  const input = { 'daughter-name': '가😀나다', 'family-name': '  김 가족  ' };
  const a = must(createSession(c, 42, { userValues: input })).session;
  assert.deepEqual(a.state.userValues, input);
  assert.deepEqual(a.origin.userValues, input);
  input['daughter-name'] = '변경';
  assert.equal(a.state.userValues['daughter-name'], '가😀나다');
  assert.equal(a.origin.userValues['daughter-name'], '가😀나다');
  assert.ok(Object.isFrozen(a.state.userValues));
  const b = must(createSession(c, 42, { userValues: { 'daughter-name': '가😀나다', 'family-name': '  김 가족  ' } })).session;
  assert.deepEqual(a, b);
  assert.equal(must(createSession(c, 42, { userValues: { 'family-name': '' } })).session.state.userValues['family-name'], '');
});

test('invalid types, fields, controls, lengths and isolated surrogates are rejected with field locations', async () => {
  const c = await named();
  for (const value of ['', '가나다라마', 123, null, '\n', '이름\r', 'a\t', 'a\0', 'a\x7f', 'a\u0085', 'a\u2028', 'a\u2029', '\ud800', '\udfff']) {
    const r = createSession(c, 42, { userValues: { 'daughter-name': value } });
    assert.equal(r.ok, false, JSON.stringify(value));
    assert.equal(r.error.code, 'INVALID_USER_VALUE');
    assert.equal(r.error.path, '/userValues/daughter-name');
  }
  for (const userValues of [null, [], '이름', 5]) assert.equal(createSession(c, 42, { userValues }).ok, false);
  const unknown = createSession(c, 42, { userValues: { 'not/a~field': '이름' } });
  assert.equal(unknown.error.code, 'UNKNOWN_USER_FIELD');
  assert.equal(unknown.error.path, '/userValues/not~1a~0field');
});

test('named save replays through commands, rejects state/origin tampering and keeps old saves exact', async () => {
  const c = await named();
  let session = must(createSession(c, 123, { userValues: { 'daughter-name': '김😀' } })).session;
  for (const command of [{ type: 'setSchedule', activityIds: ['rest', 'rest', 'rest'] }, { type: 'executeActivity' }]) {
    session = must(dispatchSession(c, session, command, session.state.stateRevision)).session;
  }
  const encoded = must(await encodeSave(c, session));
  assert.deepEqual(must(await decodeSave(c, encoded.text)).session, session);
  for (const where of ['state', 'origin']) {
    const save = JSON.parse(encoded.text); save[where].userValues['daughter-name'] = '다른이름';
    const result = await decodeSave(c, JSON.stringify(save));
    assert.equal(result.ok, false); assert.equal(result.error.code, 'SAVE_STATE_MISMATCH');
  }
  const bad = JSON.parse(encoded.text); bad.origin.userValues.unknown = '이름';
  assert.equal((await decodeSave(c, JSON.stringify(bad))).ok, false);
  const old = await content(), legacy = must(createSession(old, 42)).session;
  assert.equal(Object.hasOwn(legacy.state, 'userValues'), false);
  assert.equal(Object.hasOwn(legacy.origin, 'userValues'), false);
  assert.deepEqual(must(createSession(old, 42, { userValues: {} })).session, legacy);
  assert.equal(createSession(old, 42, { userValues: { name: '이름' } }).ok, false);
  const oldSave = must(await encodeSave(old, legacy));
  assert.equal(must(await encodeSave(old, must(await decodeSave(old, oldSave.text)).session)).text, oldSave.text);
  assert.equal((await decodeSave(c, oldSave.text)).error.code, 'CONTENT_MISMATCH');
});

test('shared validation reports duplicate IDs, default/range problems and input/text references', async () => {
  const base = structuredClone(loaded); doc(base, 'project').userFields = structuredClone(fields);
  const valid = await validateProject(base); assert.equal(valid.valid, true, JSON.stringify(valid.diagnostics));
  for (const [mutate, code, pointer] of [
    [p => p.userFields.push(p.userFields[0]), 'USER_FIELD_DUPLICATE', '/userFields/2/id'],
    [p => p.userFields[0].minLength = 5, 'USER_FIELD_RANGE', '/userFields/0/minLength'],
    [p => p.userFields[0].initial = '너무긴이름', 'USER_FIELD_INITIAL', '/userFields/0/initial'],
  ]) {
    const copy = structuredClone(base); mutate(doc(copy, 'project')); const checked = await validateProject(copy);
    assert.ok(checked.diagnostics.some(d => d.code === code && d.pointer === pointer), JSON.stringify(checked.diagnostics));
  }
  const visualBase = await content(p => {
    const project = doc(p, 'project'); project.userFields = structuredClone(fields);
    project.visual = createVisual();
    project.visual.screens[0].elements.push(
      { id: 'name-input', kind: 'input', userFieldId: 'daughter-name', x: 10, y: 10, width: 200, height: 40 },
      { id: 'name-label', kind: 'text', binding: 'user-field', userFieldId: 'daughter-name', x: 10, y: 60, width: 200, height: 40 },
    );
  });
  const inputIndex = visualBase.project.visual.screens[0].elements.findIndex(e => e.id === 'name-input');
  assert.equal(visualBase.project.visual.screens[0].elements[inputIndex].kind, 'input');
  const broken = structuredClone(base), project = doc(broken, 'project'); project.visual = structuredClone(visualBase.project.visual);
  project.visual.screens[0].elements[inputIndex].userFieldId = 'missing';
  assert.ok((await validateProject(broken)).diagnostics.some(d => d.code === 'UI_USER_FIELD_REFERENCE' && d.pointer.endsWith(`/${inputIndex}/userFieldId`)));
  delete project.visual.screens[0].elements[inputIndex].userFieldId;
  assert.equal((await buildSnapshot(broken)).ok, false);
});

test('desktop and web create atomically, preserve drafts/session on errors, and expose loaded user values', async () => {
  const c = await named(), desktop = new PlayerController(); desktop.content = c;
  assert.equal(desktop.view().userValues['daughter-name'], '올리브');
  assert.equal(desktop.newRun(null, { 'daughter-name': '미😀' }).error, null);
  const original = desktop.session, generation = desktop.generation;
  const version = { generation, revision: original.state.stateRevision };
  assert.match(desktop.newRun(version, { 'daughter-name': '' }).error, /daughter-name/);
  assert.equal(desktop.session, original); assert.equal(desktop.generation, generation);
  assert.equal(desktop.view().userValues['daughter-name'], '미😀');
  assert.ok(desktop.newRun({ ...version, generation: generation - 1 }, { 'daughter-name': '다른' }).error);
  const ajv = new Ajv2020({ strict: true });
  ajv.addSchema(JSON.parse(await readFile(new URL('../../packages/schema/common.schema.json', import.meta.url), 'utf8')));
  const validate = ajv.compile(JSON.parse(await readFile(new URL('../../packages/runtime-service/save.schema.json', import.meta.url), 'utf8')));
  const runtime = { request(input) {
    input = JSON.parse(JSON.stringify(input)); // Match the real WASM transport boundary.
    if (input.operation === 'create') return createRun(c, input.seed, input.options);
    if (input.operation === 'replay') {
      let result = createRun(c, input.origin.seed, input.origin); if (!result.ok) return result;
      for (const command of input.commands) { result = dispatchCommand(c, result.state, command, result.state.stateRevision); if (!result.ok) return result; }
      return result;
    }
    if (input.operation === 'view') return { ok: true, time: input.state ? calendarTime(c, input.state) : null, narrative: input.state ? getNarrativeView(c, input.state) : null, gameplay: getGameplayView(c, input.state), systems: getSystemsView(c, input.state) };
    throw Error(input.operation);
  } };
  let text = null; const web = new WebPlayerController(c, runtime, { read: async () => text, write: async value => { text = value; } }, validate);
  assert.equal((await web.newRun(null, { 'daughter-name': '미😀' })).error, null);
  const previous = web.session, v = { generation: web.generation, revision: previous.state.stateRevision };
  assert.ok((await web.newRun(v, { 'daughter-name': 10 })).error); assert.equal(web.session, previous);
  for (const bad of [{ 'daughter-name': undefined }, { unknown: undefined }, { 'daughter-name': '\ud800' }]) { assert.ok((await web.newRun(v, bad)).error); assert.equal(web.session, previous); }
  assert.equal((await web.save(v)).error, null); assert.ok(text);
  assert.equal((await web.newRun(v, { 'daughter-name': '다른' })).error, null);
  assert.equal((await web.load({ generation: web.generation, revision: 0 })).error, null);
  assert.equal((await web.view()).view.userValues['daughter-name'], '미😀');
});


test('export refuses old runtimes before publishing user-field projects', async t => {
  const c = await named(), directory = await temporary(t), runtime = path.join(directory, 'old-runtime');
  await mkdir(runtime); await writeFile(path.join(runtime, 'index.html'), '<!-- capability gate fixture; never executed -->');
  await writeFile(path.join(runtime, 'capabilities.json'), JSON.stringify({ formatVersion: '1', imageExtensions: ['.png'], audioExtensions: ['.ogg'], visualFeatures: [] }));
  const destination = path.join(directory, 'export');
  await assert.rejects(() => exportGame({ content: c, projectRoot: loaded.root, destination, target: 'web', runtimes: { web: runtime, windows: runtime } }), /user-fields-v1/);
  await assert.rejects(() => access(destination), { code: 'ENOENT' });
});

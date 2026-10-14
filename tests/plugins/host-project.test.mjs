import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile, rm, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { installPackage } from '../../dist/packages/plugin-host/index.js';
import { StudioEditor } from '../../dist/packages/studio-service/editor.js';
import { addCutout, addElement, controllerData, cutoutPackage, envelope, lock, newProject, png, rigData, serialize, temp, CUTOUT_VERSION } from './host-helpers.mjs';

let installedStore;
const store = () => installedStore ??= (async () => { const dir = await temp('store'); await installPackage(dir, await cutoutPackage()); return dir; })();
const open = async (root, pluginStore) => { const editor = new StudioEditor({ pluginStore }); await editor.open(root); return editor; };
const codes = (editor, file) => editor.diagnostics.filter(d => !file || d.file === file).map(d => `${d.severity}:${d.code}:${d.file}${d.pointer}`);
const doc = (editor, file) => editor.documents.find(d => d.file === file);
const RIG = 'plugins/cutout-animation/rig/hero.json', CONTROLLER = 'plugins/cutout-animation/controller/hero-main.json';

test('missing plugin: warnings only, documents kept byte-for-byte across open -> edit another document -> save', async () => {
  const root = await newProject();
  await addCutout(root); await addElement(root);
  // Deliberately non-canonical formatting: an untouched document must never be re-serialized.
  const odd = `{"formatVersion":"0.1.0","plugin":"cutout-animation","type":"rig","id":"hero","data":${JSON.stringify(rigData())}}`;
  await writeFile(path.join(root, RIG), odd);
  const before = { rig: await readFile(path.join(root, RIG)), controller: await readFile(path.join(root, CONTROLLER)), lock: await readFile(path.join(root, 'plugins/lock.json')) };
  const editor = await open(root, await temp('empty-store'));
  assert.deepEqual(editor.diagnostics.filter(d => d.severity === 'error'), []);
  assert.ok(codes(editor).includes('warning:PLUGIN_MISSING:plugins/lock.json/plugins/0/version'));
  assert.ok(codes(editor).includes('warning:PLUGIN_MISSING:project.json/visual/screens/0/elements/' + (doc(editor, 'project.json').data.visual.screens[0].elements.length - 1) + '/extension/plugin'));
  assert.equal(doc(editor, RIG).kind, 'extension');
  assert.equal(doc(editor, 'plugins/lock.json').kind, 'pluginLock');
  const project = structuredClone(doc(editor, 'project.json').data); project.title = '플러그인 없는 저장';
  editor.edit({ type: 'replace', file: 'project.json', data: project }, editor.revision);
  await editor.save(editor.revision);
  assert.equal(JSON.parse(await readFile(path.join(root, 'project.json'), 'utf8')).title, '플러그인 없는 저장');
  assert.deepEqual(await readFile(path.join(root, RIG)), before.rig);
  assert.deepEqual(await readFile(path.join(root, CONTROLLER)), before.controller);
  assert.deepEqual(await readFile(path.join(root, 'plugins/lock.json')), before.lock);
  const snapshot = await editor.snapshot(editor.revision);
  assert.equal(snapshot.ok, true);
  assert.equal(JSON.stringify(snapshot.snapshot).includes('"rig"'), false, 'extension documents are not game content');
});

test('not locked and disabled plugins are reported as warnings and never validated', async () => {
  const root = await newProject();
  await addCutout(root, { lockData: null, rig: rigData({ bones: [{ id: 'a', parent: 'a' }] }) });
  let editor = await open(root, await store());
  assert.deepEqual(codes(editor).filter(c => c.includes('PLUGIN')).sort(), [`warning:PLUGIN_NOT_LOCKED:${CONTROLLER}/plugin`, `warning:PLUGIN_NOT_LOCKED:${RIG}/plugin`]);
  assert.equal(codes(editor).some(c => c.includes('ANIM_')), false);
  await writeFile(path.join(root, 'plugins/lock.json'), serialize(lock([{ id: 'cutout-animation', version: CUTOUT_VERSION, enabled: false }])));
  editor = await open(root, await store());
  assert.deepEqual(codes(editor).filter(c => c.includes('PLUGIN')), ['warning:PLUGIN_DISABLED:plugins/lock.json/plugins/0/enabled']);
  assert.equal(editor.diagnostics.some(d => d.severity === 'error'), false);
  assert.deepEqual((await editor.plugins()).project.map(p => [p.id, p.state, p.enabled]), [['cutout-animation', 'disabled', false]]);
});

test('cutout service diagnostics surface with plugin file and /data pointers', async () => {
  const root = await newProject();
  await addCutout(root, {
    rig: rigData({ attachments: [{ slot: 'torso', name: 'torso', imageAssetId: 'nope', width: 40, height: 60 }] }),
    controller: controllerData({ defaultClip: 'missing-clip' }), image: png(48, 60),
  });
  await addElement(root, { plugin: 'cutout-animation', type: 'character', ref: 'ghost' });
  const broken = rigData({
    bones: [{ id: 'root', parent: null }, { id: 'a', parent: 'b' }, { id: 'b', parent: 'a' }, { id: 'c', parent: 'ghost' }],
    slots: [], attachments: [{ slot: 'torso', name: 'torso', imageAssetId: 'hero-torso', width: 40, height: 60 }],
    clips: [{ id: 'idle', durationMs: 1000, loop: true, tracks: [{ target: 'bone:root', property: 'y', keys: [{ timeMs: 500, value: 0 }, { timeMs: 100, value: 1 }, { timeMs: 2000, value: 2 }] }] }],
  });
  await writeFile(path.join(root, 'plugins/cutout-animation/rig/broken.json'), serialize(envelope('rig', 'broken', broken)));
  await writeFile(path.join(root, 'plugins/cutout-animation/controller/orphan.json'), serialize(envelope('controller', 'orphan', controllerData({ rig: 'nobody' }))));
  const editor = await open(root, await store());
  const all = codes(editor), BROKEN = 'plugins/cutout-animation/rig/broken.json';
  for (const expected of [
    `error:ANIM_PARENT_CYCLE:${BROKEN}/data/bones/1/parent`, `error:ANIM_PARENT_CYCLE:${BROKEN}/data/bones/2/parent`,
    `error:ANIM_PARENT_MISSING:${BROKEN}/data/bones/3/parent`, `error:ANIM_SLOT_MISSING:${BROKEN}/data/attachments/0/slot`,
    `error:ANIM_KEY_UNSORTED:${BROKEN}/data/clips/0/tracks/0/keys/1/timeMs`, `error:ANIM_KEY_TIME_RANGE:${BROKEN}/data/clips/0/tracks/0/keys/2/timeMs`,
    `error:ANIM_IMAGE_MISSING:${RIG}/data/attachments/0/imageAssetId`, `error:ANIM_CLIP_UNKNOWN:${CONTROLLER}/data/defaultClip`,
    `error:ANIM_RIG_MISSING:plugins/cutout-animation/controller/orphan.json/data/rig`,
  ]) assert.ok(all.includes(expected), `${expected}\n${all.join('\n')}`);
  const ref = editor.diagnostics.find(d => d.code === 'PLUGIN_ELEMENT_REF');
  assert.equal(ref.file, 'project.json'); assert.match(ref.pointer, /^\/visual\/screens\/0\/elements\/\d+\/extension\/ref$/);
  const cycle = editor.diagnostics.find(d => d.code === 'ANIM_PARENT_CYCLE');
  assert.ok(cycle.line > 1, 'pointer resolves to a source line');
  assert.equal(all.filter(c => c.includes('REFERENCE_MISSING')).length, 0, 'engine walker never checks plugin data');

  // PNG size mismatch is a warning (the asset exists with 48x60).
  const fixed = structuredClone(doc(editor, RIG).data); fixed.data.attachments[0].imageAssetId = 'hero-torso';
  editor.edit({ type: 'replace', file: RIG, data: fixed }, editor.revision); await editor.validate(editor.revision);
  assert.ok(codes(editor).includes(`warning:ANIM_IMAGE_SIZE_MISMATCH:${RIG}/data/attachments/0/width`));
  assert.equal(codes(editor).some(c => c.startsWith(`error:ANIM_IMAGE_MISSING:${RIG}`)), false);

  // Plugin JSON schema failures are structural: reported under /data and they block save.
  const invalid = structuredClone(fixed); invalid.data.unexpected = true;
  editor.edit({ type: 'replace', file: RIG, data: invalid }, editor.revision); await editor.validate(editor.revision);
  assert.ok(codes(editor).includes(`error:SCHEMA_INVALID:${RIG}/data/unexpected`));
  await assert.rejects(editor.save(editor.revision), /구조 오류/);
});

test('extension path must match the envelope; engine-side element type check', async () => {
  const root = await newProject();
  await addCutout(root);
  await writeFile(path.join(root, RIG), serialize(envelope('rig', 'other', rigData())));
  await addElement(root, { plugin: 'cutout-animation', type: 'unknown-kind', ref: 'hero-main' });
  const editor = await open(root, await store());
  assert.ok(codes(editor).includes(`error:EXTENSION_PATH:${RIG}/id`));
  assert.ok(editor.diagnostics.some(d => d.code === 'PLUGIN_ELEMENT_TYPE' && d.pointer.endsWith('/extension/type')));
});

test('create / undo / redo / delete of extension documents through StudioEditor', async () => {
  const root = await newProject();
  await addCutout(root, { rig: null, controller: null });
  const editor = await open(root, await store());
  const file = 'plugins/cutout-animation/rig/fresh.json', data = envelope('rig', 'fresh', rigData());
  assert.throws(() => editor.edit({ type: 'create', file: 'characters/x.json', data }, editor.revision), /create는/);
  assert.throws(() => editor.edit({ type: 'create', file: 'plugins/cutout-animation/rig/../x.json', data }, editor.revision), /create는/);
  editor.edit({ type: 'create', file, data }, editor.revision);
  assert.throws(() => editor.edit({ type: 'create', file, data }, editor.revision), /이미 있는 문서/);
  assert.equal(doc(editor, file).kind, 'extension');
  editor.edit({ type: 'undo' }, editor.revision);
  assert.equal(doc(editor, file), undefined);
  editor.edit({ type: 'redo' }, editor.revision);
  assert.equal(doc(editor, file).kind, 'extension');
  await editor.validate(editor.revision);
  assert.equal(editor.diagnostics.some(d => d.severity === 'error'), false, codes(editor).join('\n'));
  await editor.save(editor.revision);
  assert.equal(await readFile(path.join(root, file), 'utf8'), serialize(data));
  // Reopen sees the saved document; delete is allowed for extension documents and removes the file.
  const reopened = await open(root, await store());
  assert.deepEqual(doc(reopened, file).data, data);
  reopened.edit({ type: 'batch', edits: [{ type: 'delete', file }] }, reopened.revision);
  await reopened.save(reopened.revision);
  await assert.rejects(readFile(path.join(root, file)), { code: 'ENOENT' });
});

test('animation edits never change contentFingerprint; plugin element edits do', async () => {
  const root = await newProject();
  await addCutout(root); await addElement(root);
  const editor = await open(root, await store());
  const first = await editor.snapshot(editor.revision);
  assert.equal(first.ok, true, JSON.stringify(first.diagnostics.filter(d => d.severity === 'error')));
  const rig = structuredClone(doc(editor, RIG).data); rig.data.clips[0].durationMs = 3000; rig.data.clips[0].tracks[0].keys[2].timeMs = 3000;
  editor.edit({ type: 'replace', file: RIG, data: rig }, editor.revision);
  const second = await editor.snapshot(editor.revision);
  assert.equal(second.ok, true);
  assert.equal(second.snapshot.contentFingerprint, first.snapshot.contentFingerprint);
  const project = structuredClone(doc(editor, 'project.json').data);
  project.visual.screens[0].elements.at(-1).extension.props.flipX = true;
  editor.edit({ type: 'replace', file: 'project.json', data: project }, editor.revision);
  const third = await editor.snapshot(editor.revision);
  assert.notEqual(third.snapshot.contentFingerprint, first.snapshot.contentFingerprint);
  await rm(path.join(root, '.studio'), { recursive: true, force: true });
});

test('skins and expressions (plugin 1.2.0): schema-valid documents, per-skin image checks, element props.skin', async () => {
  const root = await newProject();
  const rig = rigData({
    skins: [{ id: 'winter', name: '겨울', attachments: [
      { slot: 'torso', name: 'torso', imageAssetId: 'hero-torso', width: 44, height: 60, pivotX: 0.5, pivotY: 1 },
      { slot: 'torso', name: 'scarf', imageAssetId: 'no-image', width: 10, height: 10 }] }],
    expressions: [{ id: 'calm', name: '차분', slots: { torso: null }, bones: { body: { rotation: 3 } } }],
  });
  rig.clips[0].tracks.push({ target: 'rig', property: 'expression', keys: [{ timeMs: 0, value: 'calm' }, { timeMs: 500, value: null }] });
  await addCutout(root, { rig, controller: controllerData({ defaultSkin: 'winter', defaultExpression: null, bindings: [{ id: 'face', signal: 'portraitChanged', match: { portraitId: 'calm' }, expression: 'calm' }] }) });
  await addElement(root, { plugin: 'cutout-animation', type: 'character', ref: 'hero-main', props: { scale: 1, skin: 'winter' } });
  const element = `project.json/visual/screens/0/elements/${JSON.parse(await readFile(path.join(root, 'project.json'), 'utf8')).visual.screens[0].elements.length - 1}/extension`;
  const editor = await open(root, await store());
  let all = codes(editor);
  assert.equal(all.some(c => c.includes('SCHEMA_INVALID')), false, all.join('\n'));
  assert.ok(all.includes(`error:ANIM_IMAGE_MISSING:${RIG}/data/skins/0/attachments/1/imageAssetId`), all.join('\n'));
  assert.ok(all.includes(`warning:ANIM_IMAGE_SIZE_MISMATCH:${RIG}/data/skins/0/attachments/0/width`), 'skin images get the PNG size check too');
  assert.deepEqual(all.filter(c => c.startsWith(`error:`) && c.includes(CONTROLLER)), [], 'clip-less expression binding and defaultSkin are valid');
  assert.deepEqual(codes(editor, 'project.json').filter(c => c.includes('ANIM_')), [], 'props.skin naming a rig skin is fine');
  const project = structuredClone(doc(editor, 'project.json').data);
  project.visual.screens[0].elements.at(-1).extension.props.skin = 'summer';
  editor.edit({ type: 'replace', file: 'project.json', data: project }, editor.revision); await editor.validate(editor.revision);
  assert.ok(codes(editor).includes(`warning:ANIM_SKIN_UNKNOWN:${element}/props/skin`), codes(editor).join('\n'));
  project.visual.screens[0].elements.at(-1).extension.props.skin = 7;
  editor.edit({ type: 'replace', file: 'project.json', data: project }, editor.revision); await editor.validate(editor.revision);
  assert.ok(codes(editor).includes(`error:ANIM_ELEMENT_PROP:${element}/props/skin`));
  // A rig track may only key expression (the schema allows the shape, the core checks the pairing).
  const bad = structuredClone(doc(editor, RIG).data);
  bad.data.clips[0].tracks.at(-1).property = 'x';
  editor.edit({ type: 'replace', file: RIG, data: bad }, editor.revision); await editor.validate(editor.revision);
  all = codes(editor);
  assert.ok(all.some(c => c.startsWith(`error:ANIM_PROPERTY:${RIG}/data/clips/0/tracks/1/property`)), all.join('\n'));
});

test('host-reserved posterAssetId must name an image asset, also without the plugin, and the cutout service ignores it', async () => {
  const root = await newProject();
  await addCutout(root); await addElement(root, { plugin: 'cutout-animation', type: 'character', ref: 'hero-main', props: { scale: 1, posterAssetId: 'hero-torso' } });
  const element = `project.json/visual/screens/0/elements/${JSON.parse(await readFile(path.join(root, 'project.json'), 'utf8')).visual.screens[0].elements.length - 1}/extension`;
  let editor = await open(root, await store());
  assert.deepEqual(codes(editor, 'project.json').filter(c => c.includes('POSTER') || c.includes('ANIM_ELEMENT_PROP')), []);
  const project = structuredClone(doc(editor, 'project.json').data);
  project.visual.screens[0].elements.at(-1).extension.props.posterAssetId = 'no-such-image';
  editor.edit({ type: 'replace', file: 'project.json', data: project }, editor.revision); await editor.validate(editor.revision);
  assert.ok(codes(editor).includes(`error:PLUGIN_POSTER_ASSET:${element}/props/posterAssetId`), codes(editor).join('\n'));
  editor = await open(root, await temp('empty-store'));
  editor.edit({ type: 'replace', file: 'project.json', data: project }, editor.revision); await editor.validate(editor.revision);
  assert.ok(codes(editor).includes(`error:PLUGIN_POSTER_ASSET:${element}/props/posterAssetId`), codes(editor).join('\n'));
});

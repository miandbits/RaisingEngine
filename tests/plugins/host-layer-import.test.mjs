// Layer import through the hosts (plugin 1.11.0, packages/plugin-host CONTRACT.md §11): the Qt
// service's `pluginImport` (preview, then ONE undoable edit with content-hashed PNG assets),
// re-import onto a rigged character (bones, keys and placements kept; changed pictures swapped in
// place; new layers added), and the MCP path (plugin_call preview, plugin_import save with
// version checks). The real cutout package runs in an isolated store.
import test from 'node:test';
import assert from 'node:assert/strict';
import { existsSync } from 'node:fs';
import { readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { QtStudioService } from '../../apps/qt-studio/service.mjs';
import { McpWorkspace, toolError } from '../../dist/packages/mcp-service/workspace.js';
import { CUTOUT_VERSION, cutoutPackage, lock, newProject, serialize, temp } from './host-helpers.mjs';
import { flattenImage, writePsd } from './psd-fixture.mjs';
import { characterLayers, maxDifference, renderSetup } from './layer-import-fixture.mjs';

const RIG = 'plugins/cutout-animation/rig/hero.json', CONTROLLER = 'plugins/cutout-animation/controller/hero.json';
const W = 140, H = 200;
const block = (w, h, rgb) => ({ width: w, height: h, pixel: (x, y) => [(rgb[0] + x * 5) & 255, (rgb[1] + y * 9) & 255, rgb[2], 255] });
/** Version 2 of the art: a bigger, moved face, a new hand in the left group. */
function revisedLayers() {
  const layers = characterLayers();
  const head = layers[4].children;
  head[0] = { name: 'face', x: 44, y: 22, ...block(64, 72, [250, 190, 150]) };
  layers[2].children.push({ name: 'hand', x: 18, y: 150, ...block(14, 12, [230, 180, 150]) });
  return layers;
}
async function setup() {
  const root = await newProject(), store = await temp('store'), art = await temp('art');
  const service = new QtStudioService({ pluginStore: store });
  await service.handle({ method: 'pluginInstall', params: { file: await cutoutPackage() } });
  let reply = await service.handle({ method: 'open', params: { root } });
  reply = await service.handle({ method: 'pluginEnable', params: { id: 'cutout-animation', revision: reply.view.revision } });
  reply = await service.handle({ method: 'save', params: { revision: reply.view.revision } });
  const v1 = path.join(art, 'hero.psd'), v2 = path.join(art, 'hero-v2.psd');
  await writeFile(v1, writePsd({ width: W, height: H, layers: characterLayers() }));
  await writeFile(v2, writePsd({ width: W, height: H, layers: revisedLayers(), rle: false }));
  return { root, store, service, reply, v1, v2 };
}
const doc = (reply, file) => reply.view.documents.find(d => d.file === file)?.data?.data;
const pngsOf = async (root, rig) => new Map(await Promise.all([...new Set(rig.attachments.map(a => a.imageAssetId))].map(async id => [id, await readFile(path.join(root, 'assets', `${id}.png`))])));

test('Qt pluginImport: preview, one undoable edit with hashed assets, validation, save', async () => {
  const { root, service, reply: opened, v1 } = await setup();
  let reply = opened;
  const before = reply.view.revision, history = reply.view.undo;
  reply = await service.handle({ method: 'pluginImport', params: { plugin: 'cutout-animation', op: 'importLayers', payload: { file: v1, id: 'hero', exclude: ['0'] }, dryRun: true, revision: reply.view.revision } });
  const preview = reply.pluginImport;
  assert.deepEqual([preview.dryRun, preview.changed, reply.view.revision, reply.view.undo, reply.view.dirty], [true, false, before, history, false], 'a preview changes nothing');
  assert.deepEqual([preview.report.mode, preview.report.rigId, preview.report.source.format], ['create', 'hero', 'psd']);
  assert.equal(preview.report.layers[0].role, 'excluded');
  assert.equal(preview.report.summary.images, null);

  reply = await service.handle({ method: 'pluginImport', params: { plugin: 'cutout-animation', op: 'importLayers', payload: { file: v1, id: 'hero' }, revision: reply.view.revision } });
  const done = reply.pluginImport;
  assert.equal(done.changed, true);
  assert.equal(reply.view.revision, before + 1, 'exactly one edit');
  assert.deepEqual(done.files, [RIG, CONTROLLER]);
  assert.equal(done.addedAssetIds.length, 10);
  for (const id of done.addedAssetIds) assert.ok(existsSync(path.join(root, 'assets', `${id}.png`)), `published ${id}`);
  const manifest = reply.view.documents.find(d => d.kind === 'assets').data.assets;
  assert.ok(done.addedAssetIds.every(id => manifest.some(a => a.id === id && a.kind === 'image' && a.path === `assets/${id}.png`)));
  assert.deepEqual(reply.view.diagnostics.filter(d => d.severity === 'error'), []);
  const rig = doc(reply, RIG);
  assert.equal(rig.slots.length, 8);
  assert.ok(maxDifference(await renderSetup(rig, await pngsOf(root, rig)), flattenImage(W, H, characterLayers())) <= 1, 'the imported character looks like the PSD');

  // One undo removes rig, controller and the manifest entries together; redo brings them back.
  reply = await service.handle({ method: 'edit', params: { revision: reply.view.revision, edit: { type: 'undo' } } });
  assert.equal(doc(reply, RIG), undefined); assert.equal(doc(reply, CONTROLLER), undefined);
  assert.ok(!reply.view.documents.find(d => d.kind === 'assets').data.assets.some(a => done.addedAssetIds.includes(a.id)));
  reply = await service.handle({ method: 'edit', params: { revision: reply.view.revision, edit: { type: 'redo' } } });
  assert.deepEqual(doc(reply, RIG), rig);
  // The same PSD again as a new character: the asset ids are content hashes, nothing new is stored.
  const again = await service.handle({ method: 'pluginImport', params: { plugin: 'cutout-animation', op: 'importLayers', payload: { file: v1, id: 'hero-copy' }, revision: reply.view.revision } });
  assert.deepEqual([again.pluginImport.addedAssetIds, again.pluginImport.assetIds.length], [[], 10]);
  await assert.rejects(service.handle({ method: 'pluginImport', params: { plugin: 'cutout-animation', op: 'importLayers', payload: { file: v1, id: 'hero' }, revision: again.view.revision } }), /이미 있는 캐릭터 ID/);
  await assert.rejects(service.handle({ method: 'pluginImport', params: { plugin: 'cutout-animation', op: 'importLayers', payload: { file: v1, id: 'x' }, revision: reply.view.revision } }), /갱신/, 'stale revision');
  reply = await service.handle({ method: 'save', params: { revision: again.view.revision } });
  assert.equal(JSON.parse(await readFile(path.join(root, RIG), 'utf8')).data.slots.length, 8);
});

test('re-import onto a rigged character: rig work kept, pictures swapped in place, new layers added', async () => {
  const { root, service, reply: opened, v1, v2 } = await setup();
  let reply = await service.handle({ method: 'pluginImport', params: { plugin: 'cutout-animation', op: 'importLayers', payload: { file: v1, id: 'hero' }, revision: opened.view.revision } });
  const imported = doc(reply, RIG);
  // Untouched rig + v2: the setup frame is the v2 composite (moved/bigger face, new hand).
  const fresh = await service.handle({ method: 'pluginImport', params: { plugin: 'cutout-animation', op: 'importLayers', payload: { file: v2, rig: 'hero' }, dryRun: true, revision: reply.view.revision } });
  assert.deepEqual(fresh.pluginImport.report.changes.attachments.updated, ['face/face']);
  assert.deepEqual(fresh.pluginImport.report.changes.attachments.added, ['hand/hand']);
  assert.equal(fresh.pluginImport.report.changes.attachments.unchanged.length, 9);
  let check = await service.handle({ method: 'pluginImport', params: { plugin: 'cutout-animation', op: 'importLayers', payload: { file: v2, rig: 'hero' }, revision: reply.view.revision } });
  const updated = doc(check, RIG);
  assert.ok(maxDifference(await renderSetup(updated, await pngsOf(root, updated)), flattenImage(W, H, revisedLayers())) <= 1, 're-imported setup equals the new PSD');
  assert.deepEqual(updated.slots.map(s => s.id), ['shadow', 'body', 'arm', 'hand', 'right-arm', 'face', 'eyes', 'mouth', 'part-1'], 'the new slot sits in PSD order');
  check = await service.handle({ method: 'edit', params: { revision: check.view.revision, edit: { type: 'undo' } } });
  assert.deepEqual(doc(check, RIG), imported);

  // Rig work: the head bone turned, a key on it, the face nudged by the animator.
  const worked = structuredClone(imported);
  worked.bones.find(b => b.id === 'head').rotation = 12;
  worked.clips[0].tracks.push({ target: 'bone:head', property: 'rotation', keys: [{ timeMs: 0, value: 12 }, { timeMs: 500, value: 18 }] });
  const face = worked.attachments.find(a => a.slot === 'face');
  face.x += 3; face.rotation = 5;
  reply = await service.handle({ method: 'edit', params: { revision: check.view.revision, edit: { type: 'replace', file: RIG, data: { ...check.view.documents.find(d => d.file === RIG).data, data: worked } } } });
  const revision = reply.view.revision;
  reply = await service.handle({ method: 'pluginImport', params: { plugin: 'cutout-animation', op: 'importLayers', payload: { file: v2, rig: 'hero' }, revision } });
  assert.equal(reply.view.revision, revision + 1, 'one edit');
  const after = doc(reply, RIG), report = reply.pluginImport.report;
  assert.deepEqual([report.mode, report.changes.bones.added, report.changes.bones.kept.sort()], ['update', [], ['head', 'left', 'right']]);
  assert.deepEqual(after.bones.find(b => b.id === 'head'), worked.bones.find(b => b.id === 'head'), 'bones untouched');
  assert.deepEqual(after.clips, worked.clips, 'keys untouched');
  const newFace = after.attachments.find(a => a.slot === 'face');
  assert.deepEqual([newFace.x, newFace.y, newFace.rotation], [face.x, face.y, 5], 'placement kept');
  assert.deepEqual([newFace.width, newFace.height], [66, 74]);
  // The same picture content stays under (x, y): pivot · size moved by how far the corner moved (+4, +2).
  assert.ok(Math.abs(newFace.pivotX * 66 - (0.5 * face.width - 4)) < 1e-6 && Math.abs(newFace.pivotY * 74 - (0.5 * face.height - 2)) < 1e-6);
  assert.notEqual(newFace.imageAssetId, face.imageAssetId);
  const hand = after.attachments.find(a => a.slot === 'hand');
  assert.equal(after.slots.find(s => s.id === 'hand').bone, 'left');
  assert.ok(hand && after.editor.layerImport.layers['hand/hand'], 'provenance recorded for the new layer');
  assert.deepEqual(reply.view.diagnostics.filter(d => d.severity === 'error'), []);
  // A layer missing from the PSD (here: excluded) is reported and kept.
  const missing = await service.handle({ method: 'pluginImport', params: { plugin: 'cutout-animation', op: 'importLayers', payload: { file: v2, rig: 'hero', exclude: ['0'] }, dryRun: true, revision: reply.view.revision } });
  assert.deepEqual(missing.pluginImport.report.changes.attachments.missing, ['shadow/shadow']);
  // Undo returns exactly the worked rig.
  reply = await service.handle({ method: 'edit', params: { revision: reply.view.revision, edit: { type: 'undo' } } });
  assert.deepEqual(doc(reply, RIG), worked);
});

test('MCP: plugin_call preview and plugin_import save with version checks', async () => {
  const { root, store, v1 } = await setup();
  const mcp = new McpWorkspace(root, false, undefined, store);
  let { version } = await mcp.readProject();
  const preview = await mcp.pluginCall(version, 'cutout-animation', 'importLayers', { file: v1, id: 'hero' });
  assert.equal(preview.result.ok, true);
  assert.deepEqual([preview.result.mode, preview.result.documents.map(d => d.type), preview.result.images], ['create', ['rig', 'controller'], []]);
  const withImages = await mcp.pluginCall(version, 'cutout-animation', 'importLayers', { file: v1, id: 'hero', includeImages: true });
  assert.equal(withImages.result.images.length, 10);
  assert.ok(Buffer.from(withImages.result.images[0].png, 'base64').subarray(1, 4).equals(Buffer.from('PNG')));
  const dry = await mcp.pluginImport(version, 'cutout-animation', 'importLayers', { file: v1, id: 'hero' }, true, 'preview');
  assert.deepEqual([dry.dryRun, dry.version], [true, version]);
  assert.equal(existsSync(path.join(root, RIG)), false);
  const saved = await mcp.pluginImport(version, 'cutout-animation', 'importLayers', { file: v1, id: 'hero' }, false, 'import art');
  assert.notEqual(saved.version, version);
  assert.ok(saved.changes.some(c => c.file === RIG && c.before === null) && saved.changes.some(c => c.file === 'assets/manifest.json'));
  const rigFile = JSON.parse(await readFile(path.join(root, RIG), 'utf8'));
  assert.equal(rigFile.data.slots.length, 8);
  for (const a of rigFile.data.attachments) assert.ok(existsSync(path.join(root, 'assets', `${a.imageAssetId}.png`)));
  const stale = await mcp.pluginImport(version, 'cutout-animation', 'importLayers', { file: v1, id: 'other' }, false, 'stale').catch(error => toolError(error));
  assert.equal(stale.error.code, 'VERSION_CONFLICT');
  const bad = await mcp.pluginImport(saved.version, 'cutout-animation', 'importLayers', { file: path.join(root, 'project.json') }, true, 'bad').catch(error => toolError(error));
  assert.equal(bad.ok, false);
  assert.match(bad.error.message, /layers 배열/);
  const sixteen = path.join(path.dirname(v1), 'deep.psd');
  await writeFile(sixteen, writePsd({ width: 8, height: 8, depth: 16 }));
  const refused = await mcp.pluginImport(saved.version, 'cutout-animation', 'importLayers', { file: sixteen }, true, 'deep').catch(error => toolError(error));
  assert.match(refused.error.message, /16비트 PSD는 지원하지 않습니다/);
  assert.equal(JSON.parse(await readFile(path.join(root, 'plugins/lock.json'), 'utf8')).plugins[0].version, CUTOUT_VERSION);
  assert.ok(serialize(lock()).includes(CUTOUT_VERSION));
});

// Layered-art import (plugin 1.11.0): the service's PSD/PSB reader and the layer → rig mapping,
// on PSDs written by tests/plugins/psd-fixture.mjs (RGBA layers, groups, hidden layers, RLE + raw
// channels, masks, Unicode names). Checked: parsed bounds/names/groups/opacity/visibility, the
// merged image, refusals (16-bit, CMYK, indexed, damaged files, ZIP channels), the bone / slot /
// attachment / draw-order mapping and naming conventions, PNG folders with layout.json, and that a
// frame the Wasm core draws from the imported rig equals the composited PSD pixel for pixel.
import test from 'node:test';
import assert from 'node:assert/strict';
import { existsSync } from 'node:fs';
import { mkdir, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { readPsd } from '../../plugins/cutout-animation/service/psd.mjs';
import { importLayers, importOptions, planImport, readLayerSource, toId } from '../../plugins/cutout-animation/service/layer-import.mjs';
import { encodePng } from '../../plugins/cutout-animation/service/atlas.mjs';
import { flattenImage, writePsd } from './psd-fixture.mjs';
import { block, characterLayers, H, maxDifference, renderSetup, W, wasm } from './layer-import-fixture.mjs';
import { temp } from './host-helpers.mjs';


test('PSD reader: bounds, Unicode names, groups, opacity, visibility, RLE + raw, masks, PSB, merged image', () => {
  for (const psb of [false, true]) for (const rle of [true, false]) {
    const layers = characterLayers();
    layers[1].mask = true;  // an extra -2 channel the reader must skip without losing the next layer
    const psd = readPsd(writePsd({ width: W, height: H, layers, psb, rle }));
    const where = `${psb ? 'psb' : 'psd'} ${rle ? 'rle' : 'raw'}`;
    assert.equal(psd.format, psb ? 'psb' : 'psd');
    assert.deepEqual([psd.width, psd.height], [W, H]);
    assert.deepEqual(psd.layers.map(l => [l.kind, l.name]), [['layer', 'shadow'], ['layer', 'body'], ['group', 'left'], ['group', 'right'], ['group', 'Head'], ['layer', '메모 [ignore]'], ['layer', '모자']], where);
    const head = psd.layers[4];
    assert.deepEqual(head.children.map(l => l.name), ['face', 'eyes/closed', 'eyes/open', 'mouth [slot]', '[pivot]']);
    assert.deepEqual(head.children[3].children.map(l => [l.name, l.visible]), [['smile', true], ['open', false]]);
    const face = head.children[0];
    assert.deepEqual([face.left, face.top, face.width, face.height], [40, 20, 60, 70]);
    assert.equal(head.children[1].visible, false);
    assert.equal(Math.round(psd.layers[0].opacity * 255), Math.round(0.4 * 255));
    assert.equal(psd.layers[1].mask, true);
    const want = (({ width, height, pixel }) => { const px = Buffer.alloc(width * height * 4); for (let y = 0; y < height; y++) for (let x = 0; x < width; x++) px.set(pixel(x, y), (y * width + x) * 4); return px; })(characterLayers()[4].children[0]);
    assert.ok(face.rgba.equals(want), `${where}: face pixels`);
    assert.ok(psd.composite().rgba.equals(flattenImage(W, H, characterLayers())), `${where}: merged image`);
  }
  // A flat document (no layer records) imports its merged image as one layer.
  const flat = readPsd(writePsd({ width: 8, height: 4, noLayers: true, channels: 3, composite: Buffer.alloc(8 * 4 * 4, 200) }));
  assert.deepEqual(flat.layers, []);
  assert.equal(flat.composite().rgba[3], 255);
});

test('PSD reader refusals: 16/32-bit, CMYK, indexed, grayscale, signature, truncation, ZIP channels, oversize', () => {
  const base = { width: 16, height: 16, layers: [{ name: 'a', x: 0, y: 0, ...block(4, 4, [1, 2, 3]) }] };
  assert.throws(() => readPsd(writePsd({ ...base, depth: 16 })), /16비트 PSD는 지원하지 않습니다.*8비트\/채널/);
  assert.throws(() => readPsd(writePsd({ ...base, depth: 32 })), /32비트/);
  assert.throws(() => readPsd(writePsd({ ...base, mode: 4 })), /CMYK PSD는 지원하지 않습니다.*RGB 색상/);
  assert.throws(() => readPsd(writePsd({ ...base, mode: 2 })), /인덱스 색상 PSD/);
  assert.throws(() => readPsd(writePsd({ ...base, mode: 1 })), /회색조 PSD/);
  assert.throws(() => readPsd(Buffer.from('GIF89a........................')), /PSD 파일이 아닙니다/);
  const good = writePsd(base);
  assert.throws(() => readPsd(good.subarray(0, 60)), /잘렸거나 손상/);
  assert.throws(() => readPsd(writePsd({ ...base, width: 9000 })), /캔버스 9000×16/);
  // Compression 2 (ZIP) in a layer channel: refused with a message (Photoshop writes it for 16-bit).
  assert.throws(() => readPsd(writePsd({ ...base, layers: [{ ...base.layers[0], compression: 2 }] })), /ZIP 압축 채널/);
});

test('mapping: groups → bones, layers → slots/attachments, variants, [slot], [pivot], [ignore], draw order', () => {
  const psd = readPsd(writePsd({ width: W, height: H, layers: characterLayers() }));
  const { rig, images, report } = planImport({ source: { ...psd, file: '/art/Hero Girl.psd' }, options: importOptions({}) });
  assert.equal(rig.name, 'Hero Girl');
  assert.deepEqual(rig.canvas, { width: W, height: H, originX: W / 2, originY: H }, 'origin default bottom-center');
  assert.deepEqual(rig.bones.map(b => [b.id, b.parent]), [['root', null], ['left', 'root'], ['right', 'root'], ['head', 'root']]);
  // Head bone at the [pivot] marker centre (70, 88); arm bones at their content centres (trim ring included).
  const head = rig.bones.find(b => b.id === 'head');
  assert.deepEqual([head.x + W / 2, head.y + H], [70, 88]);
  assert.deepEqual(rig.slots.map(s => [s.id, s.bone, s.attachment]),
    [['shadow', 'root', 'shadow'], ['body', 'root', 'body'], ['arm', 'left', 'arm'], ['right-arm', 'right', 'right-arm'], ['face', 'head', 'face'],
      ['eyes', 'head', 'open'], ['mouth', 'head', 'smile'], ['part-1', 'root', 'part-1']], 'draw order = layer order; the Korean name falls back to part-N');
  assert.equal(rig.slots[0].alpha, Math.round(102 / 255 * 1e6) / 1e6, 'layer opacity → slot alpha');
  const names = rig.attachments.map(a => `${a.slot}/${a.name}`);
  assert.deepEqual(names.sort(), ['arm/arm', 'body/body', 'eyes/closed', 'eyes/open', 'face/face', 'mouth/open', 'mouth/smile', 'part-1/part-1', 'right-arm/right-arm', 'shadow/shadow'].sort());
  assert.equal(images.length, 10, 'one picture per attachment');
  const body = rig.attachments.find(a => a.slot === 'body');
  assert.deepEqual([body.width, body.height], [52, 92], 'trimmed + 1 px ring');
  assert.deepEqual([body.x, body.y], [70 - W / 2, 135 - H], 'centre of the picture in bone space (pivot 0.5)');
  // Report: every layer with its role and mapping; the ignored note and the marker are not pictures.
  const flat = []; const walk = l => { for (const n of l) { flat.push(n); if (n.children) walk(n.children); } }; walk(report.layers);
  assert.deepEqual(flat.filter(n => n.role === 'ignored').map(n => n.name), ['메모 [ignore]']);
  assert.deepEqual(flat.filter(n => n.role === 'pivot').map(n => n.name), ['[pivot]']);
  assert.deepEqual(flat.find(n => n.name === 'eyes/closed'), { ...flat.find(n => n.name === 'eyes/closed'), slot: 'eyes', attachment: 'closed', bone: 'head', setup: false, status: 'add' });
  assert.equal(flat.find(n => n.name === 'Head').bone, 'head');
  assert.deepEqual(report.summary, { bones: 3, slots: 8, attachments: 10, hidden: 2, images: 10, added: 10, updated: 0, unchanged: 0 });
  assert.equal(toId('Left Arm 2'), 'left-arm-2'); assert.equal(toId('2nd', 'x'), 'n2nd'); assert.equal(toId('머리', 'part-3'), 'part-3'); assert.equal(toId('Émile'), 'emile');
});

test('mapping options: exclude keys, origin, scale, trim off, warnings for blend / clipping / masks / two visible variants', () => {
  const layers = characterLayers();
  layers[1].blend = 'mul '; layers[1].clipping = true; layers[1].mask = true;
  layers[4].children[1].visible = true;  // both eye variants visible
  const psd = readPsd(writePsd({ width: W, height: H, layers }));
  const source = { ...psd, file: '/art/hero.psd' };
  const { rig, report } = planImport({ source, options: importOptions({ exclude: ['2', '4.3'], origin: 'center', scale: 0.5, trim: false }), dryRun: true });
  assert.deepEqual(rig.canvas, { width: W / 2, height: H / 2, originX: W / 4, originY: H / 4 });
  assert.ok(!rig.bones.some(b => b.id === 'left'), 'excluded group: no bone');
  assert.ok(!rig.slots.some(s => s.id === 'mouth'), 'excluded [slot] group');
  assert.equal(rig.slots.find(s => s.id === 'arm').bone, 'right', 'the right arm takes the free id');
  const body = rig.attachments.find(a => a.slot === 'body');
  assert.deepEqual([body.width, body.height, body.scaleX, body.scaleY, body.imageAssetId], [50, 90, 0.5, 0.5, ''], 'trim off = the layer rect; scale on the placement; preview has no ids');
  assert.deepEqual(report.warnings.map(w => w.code).sort(), ['ANIM_IMPORT_BLEND', 'ANIM_IMPORT_CLIPPING', 'ANIM_IMPORT_MASK', 'ANIM_IMPORT_SLOT_VISIBLE']);
  assert.equal(rig.slots.find(s => s.id === 'eyes').attachment, 'open', 'the topmost visible variant shows');
  assert.equal(report.layers[2].role, 'excluded');
  assert.equal(report.layers[2].children[0].include, false);
  assert.throws(() => importOptions({ origin: 'middle' }), /origin/);
  assert.throws(() => importOptions({ scale: 9 }), /scale/);
});

test('rendered setup frame equals the composited PSD (Wasm core draw list)', async () => {
  assert.ok(existsSync(wasm), 'build first: node plugins/cutout-animation/tools/build.mjs wasm');
  const layers = characterLayers();
  const psd = readPsd(writePsd({ width: W, height: H, layers }));
  const { rig, images } = planImport({ source: { ...psd, file: '/art/hero.psd' }, options: importOptions({}) });
  const pngs = new Map(images.map(i => [i.id, i.bytes]));
  const drawn = await renderSetup(rig, pngs), expected = flattenImage(W, H, layers);
  assert.ok(maxDifference(drawn, expected) <= 1, `max channel difference ${maxDifference(drawn, expected)}`);
  // Hidden variants are there for expressions: swapping the eyes shows exactly the closed layer.
  const closed = structuredClone(rig);
  closed.slots.find(s => s.id === 'eyes').attachment = 'closed';
  const variant = characterLayers();
  variant[4].children[1].visible = true; variant[4].children[2].visible = false;
  assert.ok(maxDifference(await renderSetup(closed, pngs), flattenImage(W, H, variant)) <= 1, 'eyes closed variant');
});

test('PNG folder: layout.json positions, groups, hidden and order; plain folders with sub-folders', async () => {
  const dir = await temp('png-layers');
  const pic = (w, h, rgb) => { const b = block(w, h, rgb), px = Buffer.alloc(w * h * 4); for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) px.set(b.pixel(x, y), (y * w + x) * 4); return encodePng(w, h, px); };
  await writeFile(path.join(dir, 'body.png'), pic(20, 30, [200, 0, 0]));
  await writeFile(path.join(dir, 'eye-open.png'), pic(8, 4, [0, 0, 200]));
  await writeFile(path.join(dir, 'eye-closed.png'), pic(8, 2, [0, 0, 0]));
  await writeFile(path.join(dir, 'layout.json'), JSON.stringify({ width: 64, height: 80, layers: [
    { file: 'body.png', x: 22, y: 40 },
    { file: 'eye-closed.png', name: 'eyes/closed', x: 28, y: 21, visible: false, group: 'head' },
    { file: 'eye-open.png', name: 'eyes/open', x: 28, y: 20, group: 'head', opacity: 0.5 },
  ] }));
  const source = await readLayerSource(path.join(dir, 'body.png'));  // a PNG of the folder = the folder (with its layout.json)
  assert.deepEqual([source.format, source.width, source.height], ['png-folder', 64, 80]);
  const { rig } = planImport({ source, options: importOptions({}) });
  assert.deepEqual(rig.bones.map(b => b.id), ['root', 'head']);
  assert.deepEqual(rig.slots.map(s => [s.id, s.bone, s.attachment, s.alpha]), [['body', 'root', 'body', undefined], ['eyes', 'head', 'open', 0.5]]);
  const body = rig.attachments.find(a => a.slot === 'body');
  assert.deepEqual([body.x + 32, body.y + 80, body.width, body.height], [32, 55, 22, 32], 'centre at the layout position; the 1 px ring also around an opaque PNG');
  // Without a layout: every PNG at 0,0, sub-folders = groups, numbered names only sort.
  const plain = await temp('png-plain');
  await mkdir(path.join(plain, '02-head'));
  await writeFile(path.join(plain, '01-body.png'), pic(10, 10, [9, 9, 9]));
  await writeFile(path.join(plain, '02-head', 'face.png'), pic(6, 6, [8, 8, 8]));
  const folder = await readLayerSource(plain);
  assert.deepEqual(folder.layers.map(l => [l.kind, l.name]), [['layer', 'body'], ['group', 'head']]);
  await assert.rejects(readLayerSource(path.join(dir, 'missing.psd')), /찾을 수 없습니다/);
  await assert.rejects(readLayerSource('relative.psd'), /절대 경로/);
});

test('importLayers op: create vs update modes, id checks', async () => {
  const dir = await temp('psd-op'), file = path.join(dir, 'hero.psd');
  await writeFile(file, writePsd({ width: W, height: H, layers: characterLayers() }));
  const created = await importLayers({ payload: { file }, dryRun: true });
  assert.deepEqual([created.report.mode, created.report.rigId, created.documents.map(d => d.type)], ['create', 'hero', ['rig', 'controller']]);
  assert.deepEqual(created.documents[1].data, { schemaVersion: 1, rig: 'hero', defaultClip: 'idle', layers: [], bindings: [] });
  await assert.rejects(importLayers({ payload: { file }, documents: [{ type: 'rig', id: 'hero', data: created.documents[0].data }] }), /이미 있는 캐릭터 ID입니다: hero/);
  await assert.rejects(importLayers({ payload: { file, id: 'Bad Id' } }), /캐릭터 ID/);
  await assert.rejects(importLayers({ payload: { file, rig: 'nobody' } }), /갱신할 리그 'nobody'/);
  const updated = await importLayers({ payload: { file, rig: 'hero' }, documents: [{ type: 'rig', id: 'hero', data: created.documents[0].data }], dryRun: true });
  assert.deepEqual([updated.report.mode, updated.documents.length], ['update', 1]);
});

// Layered-art import (plugin 1.11.0): the service's PSD/PSB reader and the layer → rig mapping,
// on PSDs written by tests/plugins/psd-fixture.mjs (RGBA layers, groups, hidden layers, RLE + raw
// channels, masks, Unicode names). Checked: parsed bounds/names/groups/opacity/visibility, the
// merged image, refusals (16-bit, CMYK, indexed, damaged files, ZIP channels), the bone / slot /
// attachment / draw-order mapping and naming conventions, PNG folders with layout.json, and that a
// frame the Wasm core draws from the imported rig equals the composited PSD pixel for pixel.
// Production files (plugin 1.15.0): scale "auto", downscaling while reading (edge alpha, exact
// box filter), clipping groups baked to match the composite, effects keep/skip, merged groups
// ([merge] and merge keys), the over-limit preview and its auto-merge proposal, transliterated
// ids, one summary for empty layers, and a 5000×7000 / ~500-layer file imported within budget.
// Clipping onto groups (plugin 1.19.1): layers clipped to a group that stays bones / slots are
// painted into its pictures (each equal to the composite without overlap, the setup frame with
// see-through overlap, hidden variants, hidden groups, scale 0.25, effects skip, re-import hashes).
// Joint guess (plugin 1.20.0): groups without a marker get their origin at the edge they hang from.
import test from 'node:test';
import assert from 'node:assert/strict';
import { existsSync } from 'node:fs';
import { mkdir, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { readPsd } from '../../plugins/cutout-animation/service/psd.mjs';
import { autoScale, importLayers, importOptions, planImport, readLayerSource, toId } from '../../plugins/cutout-animation/service/layer-import.mjs';
import { decodePng, encodePng } from '../../plugins/cutout-animation/service/atlas.mjs';
import { flattenImage, pixelsOf, writePsd } from './psd-fixture.mjs';
import { countRecords, realLikeLayers, realLikePsd } from './fixtures-gen/real-like-psd.mjs';
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
  // Head bone at the [pivot] marker centre (70, 88); the arm bones (no marker, beside the body) get joints (1.20.0).
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
  // 1.20.0: the original layer / group names stay in the provenance (the editor's hierarchy filter).
  assert.equal(rig.editor.layerImport.layers['eyes/closed'].name, 'eyes/closed');
  assert.equal(rig.editor.layerImport.layers['part-1/part-1'].name, '모자');
  assert.deepEqual(rig.editor.layerImport.bones, { left: 'left', right: 'right', head: 'Head' });
  assert.deepEqual(report.summary, { bones: 3, joints: 2, slots: 8, attachments: 10, hidden: 2, images: 10, added: 10, updated: 0, unchanged: 0, baked: 0, merged: 0, effects: 0, empty: 0, pixels: report.summary.pixels });
  assert.deepEqual(report.limits, { bones: { count: 4, max: 256 }, slots: { count: 8, max: 256 }, attachments: { count: 10, max: 1024 }, exceeded: false });
  assert.equal(toId('Left Arm 2'), 'left-arm-2'); assert.equal(toId('2nd', 'x'), 'n2nd'); assert.equal(toId('머리', 'part-3'), 'part-3'); assert.equal(toId('Émile'), 'emile');
});

test('mapping options: exclude keys, origin, scale, trim off, warnings for blend / masks / two visible variants', () => {
  const layers = characterLayers();
  layers[1].blend = 'mul '; layers[1].mask = true;
  layers[4].children[1].visible = true;  // both eye variants visible
  const psd = readPsd(writePsd({ width: W, height: H, layers }));
  const source = { ...psd, file: '/art/hero.psd' };
  const { rig, report } = planImport({ source, options: importOptions({ exclude: ['2', '4.3'], origin: 'center', scale: 0.5, trim: false }), dryRun: true });
  assert.deepEqual(rig.canvas, { width: W / 2, height: H / 2, originX: W / 4, originY: H / 4 });
  assert.ok(!rig.bones.some(b => b.id === 'left'), 'excluded group: no bone');
  assert.ok(!rig.slots.some(s => s.id === 'mouth'), 'excluded [slot] group');
  assert.equal(rig.slots.find(s => s.id === 'arm').bone, 'right', 'the right arm takes the free id');
  const body = rig.attachments.find(a => a.slot === 'body');
  // 1.15.0: scale < 1 downscales the pictures (45..95 → 22..48 px), placed at scale 1.
  assert.deepEqual([body.width, body.height, body.scaleX, body.scaleY, body.imageAssetId], [26, 45, undefined, undefined, ''], 'trim off = the layer rect at the scale; preview has no ids');
  assert.deepEqual([report.options.scale, report.options.scaleMode, report.options.resolution], [0.5, 'fixed', 0.5]);
  assert.deepEqual(report.warnings.map(w => w.code).sort(), ['ANIM_IMPORT_BLEND', 'ANIM_IMPORT_MASK', 'ANIM_IMPORT_SLOT_VISIBLE']);
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

test('bone direction (1.14.0): toward the only child, the pictures, up or from a joint; lengths; render unchanged; none; re-import keeps bones', async () => {
  // An arm whose [pivot] is the shoulder and whose picture hangs down-right; a torso with one child
  // group (neck → aims at it); a badge group without a marker centred on the art (origin = its
  // centre → up); a hat group without a marker above the art (1.20.0: a joint at its lower edge).
  const layers = [
    { name: 'torso', children: [
      { name: 'chest', x: 50, y: 100, ...block(40, 60, [180, 60, 40]) },
      { name: 'neck', children: [{ name: 'collar', x: 60, y: 84, ...block(20, 12, [90, 90, 200]) }, { name: '[pivot]', x: 68, y: 94, ...block(4, 4, [255, 0, 255]), ignoreInComposite: true }] },
      { name: '[pivot]', x: 68, y: 156, ...block(4, 4, [255, 0, 255]), ignoreInComposite: true },
    ] },
    { name: 'arm', children: [
      { name: 'sleeve', x: 92, y: 104, ...block(30, 30, [60, 140, 60]) },
      { name: '[pivot]', x: 90, y: 102, ...block(4, 4, [255, 0, 255]), ignoreInComposite: true },
    ] },
    { name: 'hat', children: [{ name: 'cap', x: 50, y: 10, ...block(40, 24, [250, 220, 0]) }] },
    { name: 'badge', children: [{ name: 'pin', x: 81, y: 80, ...block(10, 10, [20, 200, 200]) }] },
  ];
  const psd = readPsd(writePsd({ width: W, height: H, layers }));
  const source = { ...psd, file: '/art/aim.psd' };
  const { rig, images, report } = planImport({ source, options: importOptions({}) });
  const bone = id => rig.bones.find(b => b.id === id);
  assert.equal(bone('root').rotation, undefined, 'the root keeps the character axis');
  assert.equal(bone('torso').rotation, -90, 'one child bone straight above: aims at it (snapped)');
  assert.equal(bone('torso').length, 62, 'length = distance to the child origin (158 → 96)');
  assert.equal(bone('neck').rotation, undefined, 'a child of a rotated parent: local 0 = still up');
  assert.ok(bone('neck').length > 0);
  const arm = bone('arm');
  assert.ok(arm.rotation > 30 && arm.rotation < 60, `the sleeve hangs down-right: ${arm.rotation}`);
  assert.ok(arm.length > 35 && arm.length < 46, `reaches the far corner of the sleeve: ${arm.length}`);
  assert.equal(bone('badge').rotation, -90, 'no marker, no child, centred on the art: straight up');
  assert.equal(bone('badge').length, 6, 'half the picture height (10 + 2 ring)');
  const hat = bone('hat');
  assert.ok(hat.rotation > -110 && hat.rotation < -100, `the hat points from the art centre through its own: ${hat.rotation}`);
  assert.ok(hat.y + H > 30 && hat.y + H < 34.5 && Math.abs(hat.x + W / 2 - 72.5) < 2, `joint at the lower edge of the cap: ${[hat.x + W / 2, hat.y + H]}`);
  const flat = []; const walk = l => { for (const n of l) { flat.push(n); if (n.children) walk(n.children); } }; walk(report.layers);
  assert.deepEqual(flat.find(n => n.name === 'torso').direction, { angle: -90, length: 62, from: 'child' });
  assert.equal(flat.find(n => n.name === 'arm').direction.from, 'content');
  assert.equal(flat.find(n => n.name === 'hat').direction.from, 'joint');
  assert.equal(flat.find(n => n.name === 'badge').direction.from, 'up');
  assert.equal(report.options.boneDirection, 'auto');
  // The pictures are counter-rotated: the setup frame is still the PSD, pixel for pixel.
  const pngs = new Map(images.map(i => [i.id, i.bytes]));
  assert.ok(maxDifference(await renderSetup(rig, pngs), flattenImage(W, H, layers)) <= 1, 'aimed bones draw the same picture');
  // `none` = the 1.11.0 mapping (axis-aligned, no length).
  const plain = planImport({ source, options: importOptions({ boneDirection: 'none' }), dryRun: true }).rig;
  assert.ok(plain.bones.every(b => b.rotation === undefined && b.length === undefined));
  assert.equal(plain.attachments.find(a => a.slot === 'sleeve').rotation, undefined);
  assert.throws(() => importOptions({ boneDirection: 'sideways' }), /boneDirection/);
  // Re-import onto a rig whose arm bone was re-aimed by hand: kept as is; a new group gets aimed.
  const existing = structuredClone(rig);
  existing.bones.find(b => b.id === 'arm').rotation = 10;
  const more = [...layers, { name: 'tail', children: [{ name: 'tip', x: 20, y: 150, ...block(30, 10, [10, 10, 10]) }, { name: '[pivot]', x: 48, y: 153, ...block(4, 4, [255, 0, 255]), ignoreInComposite: true }] }];
  const again = planImport({ source: { ...readPsd(writePsd({ width: W, height: H, layers: more })), file: '/art/aim.psd' }, options: importOptions({}), existing, dryRun: true }).rig;
  assert.equal(again.bones.find(b => b.id === 'arm').rotation, 10, 'kept bones keep their rig work');
  assert.equal(again.bones.find(b => b.id === 'tail').rotation, 180, 'the new tail aims left, at its picture');
});

test('joint guess (1.20.0): groups without a marker attach at the edge they hang from; setup frame unchanged; none and markers as before', async () => {
  // A front-facing character, every group without a [pivot]: legs below the art's centre, a torso
  // on it with sleeves at its sides, a head above it with ears on top and pigtails at its sides.
  const side = (name, x, w) => ({ name, children: [{ name: `${name}-art`, x, y: 200, ...block(w, 100, [40, 40, 120]) }] });
  const layers = [
    side('leg-l', 78, 18), side('leg-r', 104, 18),
    { name: 'torso', children: [
      { name: 'chest', x: 70, y: 110, ...block(60, 95, [180, 60, 40]) },
      { name: 'sleeve-l', children: [{ name: 'cuff-l', x: 52, y: 112, ...block(16, 78, [60, 140, 60]) }] },
      { name: 'sleeve-r', children: [{ name: 'cuff-r', x: 132, y: 112, ...block(16, 78, [60, 140, 60]) }] },
    ] },
    { name: 'head', children: [
      { name: 'pigtail-l', children: [{ name: 'strand-l', x: 40, y: 60, ...block(22, 110, [90, 60, 20]) }] },
      { name: 'pigtail-r', children: [{ name: 'strand-r', x: 138, y: 60, ...block(22, 110, [90, 60, 20]) }] },
      { name: 'face', x: 72, y: 40, ...block(56, 68, [240, 200, 160]) },
      { name: 'ear-l', children: [{ name: 'lobe-l', x: 64, y: 22, ...block(10, 26, [230, 190, 150]) }] },
      { name: 'ear-r', children: [{ name: 'lobe-r', x: 126, y: 22, ...block(10, 26, [230, 190, 150]) }] },
    ] },
  ];
  const size = { width: 200, height: 320 };
  const source = { ...readPsd(writePsd({ ...size, layers })), file: '/art/joints.psd' };
  const { rig, images, report } = planImport({ source, options: importOptions({ origin: 'top-left' }) });
  const world = new Map([['root', { x: 0, y: 0, a: 0 }]]);
  for (const b of rig.bones.slice(1)) {
    const p = world.get(b.parent), r = p.a * Math.PI / 180;
    world.set(b.id, { x: p.x + b.x * Math.cos(r) - b.y * Math.sin(r), y: p.y + b.x * Math.sin(r) + b.y * Math.cos(r), a: p.a + (b.rotation ?? 0) });
  }
  const flat = []; const walk = l => { for (const n of l) { flat.push(n); if (n.children) walk(n.children); } }; walk(report.layers);
  const near = (id, x, y, angle, tol = 2) => {
    const w = world.get(id), dir = flat.find(n => n.bone === id && n.kind === 'group').direction;
    assert.ok(Math.abs(w.x - x) <= tol && Math.abs(w.y - y) <= tol, `${id} origin ${w.x.toFixed(2)},${w.y.toFixed(2)} ≈ ${x},${y}`);
    assert.equal(dir.from, 'joint', id);
    assert.ok(Math.abs(normalize(w.a - angle)) < 1e-6, `${id} points ${w.a} (expected ${angle})`);
    assert.ok(dir.length > 0, `${id} has a length`);
  };
  const normalize = d => ((d + 180) % 360 + 360) % 360 - 180;
  // Legs below the art's centre: top edge (the hip), pointing down; inset 3 % of 102 px.
  near('leg-l', 87, 203, 90); near('leg-r', 113, 203, 90);
  // Sleeves beside the torso (its origin within their height): hang from their top.
  near('sleeve-l', 60, 114.4, 90); near('sleeve-r', 140, 114.4, 90);
  // The head above the art's centre: the bottom of the face (neck), pointing up.
  near('head', 100, 102.5, -90, 2.5);
  // Ears above the head's joint: their base, pointing up; pigtails reaching past it: their top, down.
  near('ear-l', 69, 47.2, -90); near('ear-r', 131, 47.2, -90);
  near('pigtail-l', 51, 63.4, 90); near('pigtail-r', 149, 63.4, 90);
  // The torso sits on the art's centre: centre origin, as before.
  const torso = world.get('torso');
  assert.deepEqual([torso.x, torso.y], [100, 157.5]);
  assert.equal(flat.find(n => n.name === 'torso').direction.from, 'up');
  assert.equal(report.summary.joints, 9);
  // Lengths reach the far end: a leg 102 px tall from 3 px below its top.
  assert.ok(Math.abs(flat.find(n => n.name === 'leg-l').direction.length - 98) < 1.5, `${flat.find(n => n.name === 'leg-l').direction.length}`);
  // The pictures are counter-placed: the setup frame is still the PSD, pixel for pixel.
  const pngs = new Map(images.map(i => [i.id, i.bytes]));
  assert.ok(maxDifference(await renderSetup(rig, pngs), flattenImage(size.width, size.height, layers)) <= 1, 'joint bones draw the same picture');
  // The preview (alpha only) guesses the same joints as the import.
  const preview = planImport({ source, options: importOptions({ origin: 'top-left' }), dryRun: true }).rig;
  assert.deepEqual(preview.bones, rig.bones);
  // boneDirection "none": every origin at its content centre, unrotated (1.11.0).
  const plain = planImport({ source, options: importOptions({ origin: 'top-left', boneDirection: 'none' }), dryRun: true }).rig;
  const leg = plain.bones.find(b => b.id === 'leg-l');
  assert.deepEqual([leg.x, leg.y, leg.rotation, leg.length], [87, 250, undefined, undefined]);
  // A [pivot] marker stays authoritative.
  const marked = [{ ...layers[0], children: [...layers[0].children] }, ...layers.slice(1)];
  marked[0].children.push({ name: '[pivot]', x: 85, y: 290, ...block(4, 4, [255, 0, 255]), ignoreInComposite: true });
  const pinned = planImport({ source: { ...readPsd(writePsd({ ...size, layers: marked })), file: '/art/joints.psd' }, options: importOptions({ origin: 'top-left' }), dryRun: true });
  const pinnedLeg = pinned.rig.bones.find(b => b.id === 'leg-l');
  assert.deepEqual([pinnedLeg.x, pinnedLeg.y], [87, 292]);
  const flatPinned = []; const walkPinned = l => { for (const n of l) { flatPinned.push(n); if (n.children) walkPinned(n.children); } }; walkPinned(pinned.report.layers);
  assert.notEqual(flatPinned.find(n => n.name === 'leg-l').direction.from, 'joint');
  // Re-import: kept bones keep their place and aim.
  const again = planImport({ source, options: importOptions({ origin: 'top-left' }), existing: structuredClone(rig), dryRun: true }).rig;
  assert.deepEqual(again.bones, rig.bones);
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

// ------------------------------------------------------------------ production files (plugin 1.15.0)

/** A disc of one colour whose alpha ramps to 0 over `soft` px at its edge (antialiased art). */
const disc = (w, h, rgb, soft = 1.5) => ({ width: w, height: h, pixel: (x, y) => {
  const d = Math.hypot((x + 0.5 - w / 2) / (w / 2), (y + 0.5 - h / 2) / (h / 2));
  return [...rgb, Math.max(0, Math.min(255, Math.round((1 - d) * (Math.min(w, h) / 2) / soft * 255)))];
} });
/** Exact area-average downscale (premultiplied) of a straight RGBA image: the reference for the reader's. */
function boxDownscale(width, height, rgba, r) {
  const ow = Math.ceil(width * r - 1e-9), oh = Math.ceil(height * r - 1e-9), acc = new Float64Array(ow * oh * 4);
  const cover = i => { const s = i * r, e = (i + 1) * r, out = []; for (let j = Math.floor(s + 1e-9); j < e - 1e-9; j++) out.push([j, Math.min(e, j + 1) - Math.max(s, j)]); return out; };
  for (let y = 0; y < height; y++) for (const [oy, wy] of cover(y)) for (let x = 0; x < width; x++) {
    const p = (y * width + x) * 4, a = rgba[p + 3];
    if (!a) continue;
    for (const [ox, wx] of cover(x)) { const q = (oy * ow + ox) * 4, w = wx * wy * a; acc[q] += rgba[p] * w; acc[q + 1] += rgba[p + 1] * w; acc[q + 2] += rgba[p + 2] * w; acc[q + 3] += w; }
  }
  const out = Buffer.alloc(ow * oh * 4);
  for (let i = 0; i < ow * oh; i++) { const a = acc[i * 4 + 3]; if (a < 0.5) continue; out[i * 4 + 3] = Math.round(a); for (let k = 0; k < 3; k++) out[i * 4 + k] = Math.round(acc[i * 4 + k] / a); }
  return out;
}
const flatItems = layers => { const out = []; const walk = l => { for (const n of l) { out.push(n); if (n.children) walk(n.children); } }; walk(layers); return out; };
const pngMap = images => new Map(images.map(i => [i.id, i.bytes]));

test('scale "auto" (1.15.0): canvas long edge ≤ 2048 and pictures ≤ 64 Mpx; options; re-import keeps the first scale', () => {
  const doc = (width, height, layers = [{ kind: 'layer', left: 0, top: 0, width: 10, height: 10 }]) => ({ width, height, layers });
  assert.equal(autoScale(doc(2000, 1500)), 1);
  assert.equal(autoScale(doc(2048, 2048)), 1);
  assert.equal(autoScale(doc(3000, 2000)), 0.5, '0.75 would leave 2250 px');
  assert.equal(autoScale(doc(2600, 1000)), 0.75);
  assert.equal(autoScale(doc(4000, 6000)), 0.25, '0.375 = 2250 px');
  assert.equal(autoScale(doc(5656, 8000)), 0.25, 'the production file: 2000 px');
  assert.equal(autoScale(doc(8192, 8192)), 0.25);
  const huge = Array.from({ length: 25 }, () => ({ kind: 'layer', left: 0, top: 0, width: 8000, height: 8000 }));
  assert.equal(autoScale(doc(8000, 8000, huge)), 0.125, '1600 Mpx of rectangles: 100 Mpx at 0.25');
  // The pixel budget: 100 Mpx of layer rectangles on a 2000 px canvas needs 0.75 (56 Mpx).
  const stack = Array.from({ length: 25 }, () => ({ kind: 'layer', left: 0, top: 0, width: 2000, height: 2000 }));
  assert.equal(autoScale(doc(2000, 2000, stack)), 0.75);
  assert.equal(importOptions({ scale: 'auto' }).scale, 'auto');
  assert.throws(() => importOptions({ scale: 'big' }), /scale은 0.05..4 사이 숫자 또는 "auto"/);
  assert.throws(() => importOptions({ merge: 'hair' }), /merge는 레이어 키 문자열 배열/);
  assert.throws(() => importOptions({ effects: 'drop' }), /effects는 keep 또는 skip/);
  // On a small document auto = 1 (the 1.14 import, bit for bit); a fixed scale is reported as such.
  const psd = readPsd(writePsd({ width: W, height: H, layers: characterLayers() })), source = { ...psd, file: '/art/hero.psd' };
  const auto = planImport({ source, options: importOptions({ scale: 'auto' }) }), one = planImport({ source, options: importOptions({}) });
  assert.deepEqual([auto.report.options.scale, auto.report.options.scaleMode, auto.report.options.resolution], [1, 'auto', 1]);
  assert.deepEqual(auto.rig, one.rig);
  // A re-import uses the first import's scale and resolution (rigs from before 1.15.0: full-size pictures).
  const half = planImport({ source, options: importOptions({ scale: 0.5 }) }).rig;
  assert.deepEqual([half.editor.layerImport.scale, half.editor.layerImport.resolution], [0.5, 0.5]);
  const again = planImport({ source, options: importOptions({ scale: 'auto' }), existing: half, dryRun: true });
  assert.deepEqual([again.report.options.scale, again.report.options.scaleMode, again.report.summary.unchanged, again.report.warnings.filter(w => w.code === 'ANIM_IMPORT_SCALE').length], [0.5, 'kept', 10, 0]);
  const old = structuredClone(half);
  delete old.editor.layerImport.resolution;
  assert.equal(planImport({ source, options: importOptions({}), existing: old, dryRun: true }).report.options.resolution, 1, 'no recorded resolution = full-size pictures');
  assert.match(planImport({ source, options: importOptions({ scale: 2 }), existing: half, dryRun: true }).report.warnings.find(w => w.code === 'ANIM_IMPORT_SCALE').message, /0.5/);
});

test('downscale while reading: exact box filter in premultiplied alpha, clean edges, lazy = eager, setup frame = downscaled document', async () => {
  const DW = 160, DH = 120;
  const layers = [
    { name: 'red', x: 3, y: 5, ...disc(41, 37, [200, 40, 40], 2) },  // odd offsets: partial coverage at every edge
    { name: 'green', x: 97, y: 51, ...disc(50, 50, [30, 160, 60], 3), rle: false },
  ];
  const dir = await temp('psd-downscale'), file = path.join(dir, 'small.psd');
  await writeFile(file, writePsd({ width: DW, height: DH, layers }));
  const eager = { ...readPsd(writePsd({ width: DW, height: DH, layers })), file };
  for (const scale of [0.5, 0.375, 0.25]) {
    const plan = planImport({ source: eager, options: importOptions({ scale }) });
    const lazy = await importLayers({ payload: { file, id: 'small', scale } });
    assert.deepEqual(lazy.documents[0].data.attachments.map(a => a.imageAssetId), plan.rig.attachments.map(a => a.imageAssetId), `${scale}: streamed decode = decoded-then-resampled`);
    const pngs = pngMap(plan.images), drawn = await renderSetup(plan.rig, pngs);
    const expected = boxDownscale(DW, DH, flattenImage(DW, DH, layers), scale);
    assert.ok(maxDifference(drawn, expected) <= 1, `${scale}: frame vs box-filtered document, max difference ${maxDifference(drawn, expected)}`);
    assert.ok(plan.rig.attachments.every(a => a.scaleX === undefined && a.scaleY === undefined), `${scale}: downscaled pictures are placed 1:1`);
    // No dark fringe: every visible edge pixel keeps the layer colour; coverage goes into alpha only.
    const red = decodePng(pngs.get(plan.rig.attachments.find(a => a.slot === 'red').imageAssetId));
    let alpha = 0, worst = 0;
    for (let i = 0; i < red.rgba.length; i += 4) if (red.rgba[i + 3]) { alpha += red.rgba[i + 3]; worst = Math.max(worst, Math.abs(red.rgba[i] - 200), Math.abs(red.rgba[i + 1] - 40), Math.abs(red.rgba[i + 2] - 40)); }
    assert.ok(worst <= 1, `${scale}: edge colour drift ${worst}`);
    const source = pixelsOf(layers[0]);
    let full = 0;
    for (let i = 3; i < source.length; i += 4) full += source[i];
    assert.ok(Math.abs(alpha - full * scale * scale) / (full * scale * scale) < 0.01, `${scale}: coverage kept (${alpha} vs ${full * scale * scale})`);
  }
});

test('clipping groups bake into their base: multiply / linear dodge / screen / overlay / soft light / lighten / darken match the composite', async () => {
  const CW = 120, CH = 100;
  const layers = [
    { name: 'body', x: 10, y: 10, ...disc(60, 70, [220, 180, 150], 2) },
    { name: 'shade', x: 30, y: 5, ...block(50, 60, [120, 90, 160]), clipping: true, blend: 'mul ', opacity: 0.8 },
    { name: 'light', x: 5, y: 20, ...disc(30, 30, [90, 70, 40], 3), clipping: true, blend: 'lddg' },
    { name: 'hidden', x: 10, y: 10, ...block(60, 70, [0, 255, 0]), clipping: true, visible: false },
    { name: 'glow', x: 20, y: 50, ...disc(40, 30, [60, 60, 200]), clipping: true, blend: 'scrn', opacity: 0.6 },
    { name: 'arm', children: [
      { name: 'sleeve', x: 70, y: 30, ...disc(40, 50, [80, 120, 200], 2) },
      { name: 'fold', x: 75, y: 40, ...block(30, 30, [200, 150, 100]), clipping: true, blend: 'over' },
      { name: 'soft', x: 70, y: 55, ...block(40, 25, [40, 40, 40]), clipping: true, blend: 'sLit' },
      { name: 'lighter', x: 72, y: 30, ...block(20, 20, [250, 250, 250]), clipping: true, blend: 'lite', opacity: 0.5 },
      { name: 'darker', x: 92, y: 30, ...block(15, 20, [10, 10, 10]), clipping: true, blend: 'dark', opacity: 0.5 },
    ] },
    { name: 'badge', x: 40, y: 40, ...disc(16, 16, [250, 250, 0]) },
  ];
  const dir = await temp('psd-clip'), file = path.join(dir, 'clip.psd');
  await writeFile(file, writePsd({ width: CW, height: CH, layers }));
  const done = await importLayers({ payload: { file, id: 'clip' } }), rig = done.documents[0].data, report = done.report;
  assert.deepEqual(rig.slots.map(s => s.id), ['body', 'sleeve', 'badge'], 'one slot per clipping group');
  const items = flatItems(report.layers);
  assert.deepEqual(items.filter(i => i.role === 'clipped').map(i => [i.name, i.clippedTo]),
    [['shade', '0'], ['light', '0'], ['hidden', '0'], ['glow', '0'], ['fold', '5.0'], ['soft', '5.0'], ['lighter', '5.0'], ['darker', '5.0']]);
  assert.equal(report.summary.baked, 7, 'hidden clipped layers are not painted');
  assert.ok(!report.warnings.some(w => ['ANIM_IMPORT_CLIPPING', 'ANIM_IMPORT_BLEND'].includes(w.code)), JSON.stringify(report.warnings));
  const drawn = await renderSetup(rig, pngMap(done.images)), expected = flattenImage(CW, CH, layers);
  assert.ok(maxDifference(drawn, expected) <= 2, `baked frame vs composite: max difference ${maxDifference(drawn, expected)}`);
  // The preview reads only alpha (no colour, no baking): the same rectangles and mapping.
  const preview = await importLayers({ payload: { file, id: 'clip' }, dryRun: true });
  assert.deepEqual(preview.documents[0].data.attachments.map(a => [a.slot, a.width, a.height, a.x, a.y]), rig.attachments.map(a => [a.slot, a.width, a.height, a.x, a.y]));
  // 1.19.1: a layer clipped onto a group (that stays a bone) is painted into the group's pictures.
  const onGroup = [layers[5], { name: 'tint', x: 70, y: 30, ...block(30, 30, [90, 60, 30]), clipping: true, blend: 'mul ' }];
  const plan = planImport({ source: { ...readPsd(writePsd({ width: CW, height: CH, layers: onGroup })), file: '/art/g.psd' }, options: importOptions({}) });
  assert.ok(!plan.rig.slots.some(s => s.id === 'tint'));
  assert.ok(!plan.report.warnings.some(w => w.code === 'ANIM_IMPORT_CLIPPING'));
  assert.deepEqual([plan.report.summary.baked, plan.report.layers[1].role, plan.report.layers[1].clippedTo], [5, 'clipped', '0']);
  assert.ok(maxDifference(await renderSetup(plan.rig, pngMap(plan.images)), flattenImage(CW, CH, onGroup)) <= 2, 'clip onto the group vs composite');
});

// ------------------------------------------------------------------ clipping onto groups (plugin 1.19.1)

/** An opaque block of one colour (no gradient: box filtering a product stays exact for the scale test). */
const flat = (w, h, rgb) => ({ width: w, height: h, pixel: () => [...rgb, 255] });
/** The decoded picture of every attachment with its rectangle (picture px) from the provenance. */
const picturesOf = (rig, images) => {
  const pngs = pngMap(images), out = new Map();
  for (const a of rig.attachments) {
    const r = rig.editor.layerImport.layers[`${a.slot}/${a.name}`], image = decodePng(pngs.get(a.imageAssetId));
    out.set(`${a.slot}/${a.name}`, { ...r, rgba: image.rgba });
  }
  return out;
};
/** Largest difference between a picture and the composite inside the picture's alpha (colour where both are visible). */
const pictureVsComposite = (picture, composite, width) => {
  let worst = 0;
  for (let y = 0; y < picture.height; y++) for (let x = 0; x < picture.width; x++) {
    const s = (y * picture.width + x) * 4, d = ((picture.y + y) * width + picture.x + x) * 4;
    if (!picture.rgba[s + 3]) continue;
    worst = Math.max(worst, Math.abs(picture.rgba[s + 3] - composite[d + 3]));
    if (picture.rgba[s + 3] > 8) for (let k = 0; k < 3; k++) worst = Math.max(worst, Math.abs(picture.rgba[s + k] - composite[d + k]));
  }
  return worst;
};

/** Two legs (separate slots) shaded by layers clipped to their group, like painted production art. */
const legLayers = (shade = [120, 90, 160]) => [
  { name: 'legs', children: [
    { name: 'left', x: 10, y: 20, ...disc(40, 60, [230, 190, 160], 2) },
    { name: 'right', x: 62, y: 20, ...disc(40, 60, [200, 170, 150], 2) },
  ] },
  { name: 'shade', x: 0, y: 40, ...block(120, 40, shade), clipping: true, blend: 'mul ', opacity: 0.8 },
  { name: 'light', x: 20, y: 12, ...disc(70, 30, [90, 70, 40], 3), clipping: true, blend: 'lddg' },
  { name: 'off', x: 0, y: 0, ...block(120, 100, [0, 255, 0]), clipping: true, visible: false },
];

test('clipping onto a group (1.19.1): non-overlapping pictures each equal the composite; report; effects skip keeps them; re-import notices a changed shading', async () => {
  const CW = 120, CH = 100, layers = legLayers();
  const source = l => ({ ...readPsd(writePsd({ width: CW, height: CH, layers: l })), file: '/art/legs.psd' });
  const { rig, images, report } = planImport({ source: source(layers), options: importOptions({}) });
  assert.deepEqual(rig.slots.map(s => s.id), ['left', 'right'], 'the group stays a bone with its slots; no clipped slot');
  const expected = flattenImage(CW, CH, layers), pictures = picturesOf(rig, images);
  for (const ref of ['left/left', 'right/right']) {
    const worst = pictureVsComposite(pictures.get(ref), expected, CW);
    assert.ok(worst <= 1, `${ref}: picture vs composite in its region, max difference ${worst}`);
  }
  const bare = planImport({ source: source([layers[0]]), options: importOptions({}) }), plain = picturesOf(bare.rig, bare.images);
  assert.ok(!plain.get('left/left').rgba.equals(pictures.get('left/left').rgba), 'the shading is in the picture');
  assert.ok(maxDifference(await renderSetup(rig, pngMap(images)), expected) <= 1, 'setup frame vs composite');
  const items = flatItems(report.layers);
  assert.deepEqual(items.filter(i => i.role === 'clipped').map(i => [i.name, i.clippedTo]), [['shade', '0'], ['light', '0'], ['off', '0']]);
  assert.equal(report.summary.baked, 2, 'the hidden clipped layer is not painted');
  assert.ok(!report.warnings.some(w => ['ANIM_IMPORT_CLIPPING', 'ANIM_IMPORT_BLEND'].includes(w.code)), JSON.stringify(report.warnings));
  // Clipped layers are not "effects": effects "skip" keeps them (same pictures, same counts).
  const skip = planImport({ source: source(layers), options: importOptions({ effects: 'skip' }) });
  assert.deepEqual(skip.rig.attachments.map(a => a.imageAssetId), rig.attachments.map(a => a.imageAssetId));
  assert.deepEqual([skip.report.summary.baked, skip.report.summary.effects, flatItems(skip.report.layers).filter(i => i.role === 'effect').length], [2, 0, 0]);
  // The preview reads only alpha: same rectangles and mapping, nothing painted.
  const preview = planImport({ source: source(layers), options: importOptions({}), dryRun: true });
  assert.deepEqual(preview.rig.attachments.map(a => [a.slot, a.width, a.height, a.x, a.y]), rig.attachments.map(a => [a.slot, a.width, a.height, a.x, a.y]));
  assert.equal(preview.report.summary.baked, 2);
  // Re-import: the pixel hash includes the baked shading, so a recoloured shade layer updates both pictures.
  const same = planImport({ source: source(layers), options: importOptions({}), existing: rig, dryRun: true });
  assert.deepEqual(same.report.changes.attachments.unchanged.sort(), ['left/left', 'right/right']);
  const changed = planImport({ source: source(legLayers([90, 140, 90])), options: importOptions({}), existing: rig });
  assert.deepEqual(changed.report.changes.attachments.updated.sort(), ['left/left', 'right/right']);
  assert.ok(maxDifference(await renderSetup(changed.rig, pngMap(changed.images)), flattenImage(CW, CH, legLayers([90, 140, 90]))) <= 1, 're-imported frame = the changed document');
});

test('clipping onto a group (1.19.1): overlapping semi-transparent pictures split the effect by coverage; covered pixels untouched; setup frame = composite', async () => {
  const CW = 120, CH = 100;
  const layers = [
    { name: 'back', x: 0, y: 70, ...flat(120, 30, [40, 60, 40]) },  // below the group: not clipped
    { name: 'arm', children: [
      { name: 'upper', x: 20, y: 20, ...block(50, 40, [200, 160, 140]) },
      { name: 'sleeve', x: 40, y: 30, ...disc(50, 50, [80, 120, 200], 6), opacity: 0.6 },  // see-through over the upper arm
      { name: 'cuff', x: 30, y: 25, ...block(15, 15, [250, 250, 250]) },  // opaque over the upper arm
    ] },
    { name: 'shade', x: 0, y: 0, ...block(120, 100, [140, 110, 170]), clipping: true, blend: 'mul ', opacity: 0.7 },
    { name: 'glow', x: 30, y: 30, ...disc(60, 40, [60, 60, 30], 4), clipping: true, blend: 'scrn' },
  ];
  const { rig, images, report } = planImport({ source: { ...readPsd(writePsd({ width: CW, height: CH, layers })), file: '/art/arm.psd' }, options: importOptions({}) });
  assert.deepEqual(rig.slots.map(s => s.id), ['back', 'upper', 'sleeve', 'cuff']);
  assert.equal(report.summary.baked, 2);
  const drawn = await renderSetup(rig, pngMap(images)), expected = flattenImage(CW, CH, layers);
  assert.ok(maxDifference(drawn, expected) <= 2, `overlapping pictures: frame vs composite, max difference ${maxDifference(drawn, expected)}`);
  const pictures = picturesOf(rig, images), upper = pictures.get('upper/upper'), back = pictures.get('back/back');
  const at = (p, x, y) => [...p.rgba.subarray(((y - p.y) * p.width + x - p.x) * 4, ((y - p.y) * p.width + x - p.x) * 4 + 3)];
  const original = (x, y) => block(50, 40, [200, 160, 140]).pixel(x - 20, y - 20).slice(0, 3);
  assert.deepEqual(at(upper, 36, 31), original(36, 31), 'under the opaque cuff the upper arm gets nothing');
  assert.notDeepEqual(at(upper, 60, 50), original(60, 50), 'under the see-through sleeve it is shaded (it shows through)');
  assert.notDeepEqual(at(upper, 22, 50), original(22, 50), 'uncovered: shaded');
  assert.deepEqual(at(back, 10, 90), [40, 60, 40], 'a layer below the group is not clipped');
});

test('clipping onto a group (1.19.1): hidden variants are shaded as if shown in place of their slot\'s picture', async () => {
  const CW = 120, CH = 100;
  const layers = (open = true) => [
    { name: 'face', x: 20, y: 10, ...disc(80, 80, [240, 200, 170], 2) },
    { name: 'eyes', children: [
      { name: 'eyes/closed', x: 40, y: 45, ...block(40, 6, [30, 20, 20]), visible: !open },
      { name: 'eyes/open', x: 40, y: 38, ...disc(40, 16, [60, 120, 200], 2), visible: open },
      { name: 'lash', x: 38, y: 44, ...block(44, 3, [10, 10, 10]) },  // over the closed eyes too
    ] },
    { name: 'pupil shade', x: 30, y: 30, ...block(60, 30, [120, 100, 160]), clipping: true, blend: 'mul ' },
    { name: 'shine', x: 45, y: 40, ...disc(20, 12, [80, 80, 60], 3), clipping: true, blend: 'lddg', opacity: 0.7 },
  ];
  const { rig, images } = planImport({ source: { ...readPsd(writePsd({ width: CW, height: CH, layers: layers() })), file: '/art/eyes.psd' }, options: importOptions({}) });
  assert.equal(rig.slots.find(s => s.id === 'eyes').attachment, 'open');
  const pngs = pngMap(images);
  assert.ok(maxDifference(await renderSetup(rig, pngs), flattenImage(CW, CH, layers())) <= 1, 'eyes open');
  const closed = structuredClone(rig);
  closed.slots.find(s => s.id === 'eyes').attachment = 'closed';
  const swapped = maxDifference(await renderSetup(closed, pngs), flattenImage(CW, CH, layers(false)));
  assert.ok(swapped <= 1, `eyes closed (hidden variant shaded within its own alpha): max difference ${swapped}`);
  // A hidden group whose layers are clipped to it: baked as the group would show (all variants).
  const hidden = [{ ...layers()[1], visible: false }, layers()[2]];
  const plan = planImport({ source: { ...readPsd(writePsd({ width: CW, height: CH, layers: hidden })), file: '/art/h.psd' }, options: importOptions({}) });
  const shown = planImport({ source: { ...readPsd(writePsd({ width: CW, height: CH, layers: [layers()[1], layers()[2]] })), file: '/art/h.psd' }, options: importOptions({}) });
  assert.deepEqual(plan.rig.attachments.map(a => a.imageAssetId), shown.rig.attachments.map(a => a.imageAssetId));
});

test('clipping onto a group (1.19.1): scale 0.25 bakes at the import resolution (= the per-layer clipping bake; setup frame = box-filtered composite)', async () => {
  const DW = 160, DH = 120;
  const legs = [
    { name: 'left', x: 12, y: 20, ...disc(56, 80, [230, 190, 160], 3) },
    { name: 'right', x: 88, y: 20, ...disc(56, 80, [200, 170, 150], 3) },
  ];
  const clips = [
    { name: 'shade', x: 0, y: 64, ...flat(160, 56, [120, 90, 160]), clipping: true, blend: 'mul ', opacity: 0.8 },
    { name: 'light', x: 24, y: 16, ...disc(100, 40, [90, 70, 40], 4), clipping: true, blend: 'lddg', opacity: 0.6 },
  ];
  const layers = [{ name: 'legs', children: legs }, ...clips];
  const dir = await temp('psd-group-clip'), file = path.join(dir, 'legs.psd');
  await writeFile(file, writePsd({ width: DW, height: DH, layers }));
  const done = await importLayers({ payload: { file, id: 'legs', scale: 0.25 } }), rig = done.documents[0].data;
  assert.deepEqual([done.report.options.resolution, done.report.summary.baked], [0.25, 2]);
  // Without overlap the group bake is the 1.15.0 bake of each picture with its own copy of the clipped layers.
  const perLayer = [{ name: 'legs', children: [legs[0], ...clips, legs[1], ...clips] }];
  const each = planImport({ source: { ...readPsd(writePsd({ width: DW, height: DH, layers: perLayer })), file: '/art/legs.psd' }, options: importOptions({ scale: 0.25 }) });
  assert.deepEqual(rig.attachments.map(a => a.imageAssetId), each.rig.attachments.map(a => a.imageAssetId), 'bit-identical to clipping each picture');
  // Downscaling before baking: faint edge pixels can differ like any baked clip; the rest is exact.
  const drawn = await renderSetup(rig, pngMap(done.images)), expected = boxDownscale(DW, DH, flattenImage(DW, DH, layers), 0.25);
  const solid = image => { const out = Buffer.from(image); for (let i = 0; i < out.length; i += 4) if (out[i + 3] < 128) out.fill(0, i, i + 4); return out; };
  assert.ok(maxDifference(solid(drawn), solid(expected)) <= 2, `0.25: frame vs box-filtered composite (alpha ≥ 128), max difference ${maxDifference(solid(drawn), solid(expected))}`);
  assert.ok(maxDifference(drawn, expected) <= 10, `0.25: edges ${maxDifference(drawn, expected)}`);
});

test('effects "keep" | "skip": unclipped non-normal blend layers, one summary warning', () => {
  const layers = [...characterLayers(), { name: 'shadow fx', x: 20, y: 150, ...block(80, 30, [80, 60, 90]), blend: 'mul ' }, { name: 'glow fx', x: 30, y: 10, ...disc(60, 60, [90, 90, 40]), blend: 'lddg', opacity: 0.5 }];
  const source = { ...readPsd(writePsd({ width: W, height: H, layers })), file: '/art/fx.psd' };
  const kept = planImport({ source, options: importOptions({}), dryRun: true });
  assert.ok(['shadow-fx', 'glow-fx'].every(id => kept.rig.slots.some(s => s.id === id)));
  const blend = kept.report.warnings.filter(w => w.code === 'ANIM_IMPORT_BLEND');
  assert.equal(blend.length, 1);
  assert.match(blend[0].message, /2개\(곱하기 1 · 선형 닷지 1\)는 표준으로 그립니다.*effects "skip"/);
  assert.equal(kept.report.summary.effects, 2);
  const skipped = planImport({ source, options: importOptions({ effects: 'skip' }), dryRun: true });
  assert.ok(!skipped.rig.slots.some(s => ['shadow-fx', 'glow-fx'].includes(s.id)));
  assert.deepEqual(skipped.report.layers.slice(-2).map(i => i.role), ['effect', 'effect']);
  assert.match(skipped.report.warnings.find(w => w.code === 'ANIM_IMPORT_BLEND').message, /2개.*뺐습니다/);
  assert.equal(skipped.report.options.effects, 'skip');
  assert.equal(skipped.rig.slots.length, kept.rig.slots.length - 2);
});

test('merging groups: [merge] tag and merge keys flatten a group (blend modes, clipping, nested groups) into one picture / slot', async () => {
  const layers = [
    { name: 'body', x: 20, y: 90, ...block(60, 50, [180, 60, 40]) },
    { name: 'hair [merge]', blend: 'norm', children: [
      { name: 'back', x: 10, y: 5, ...disc(80, 50, [120, 70, 50], 2) },
      { name: 'shade', x: 30, y: 10, ...block(50, 40, [150, 120, 170]), clipping: true, blend: 'mul ' },
      { name: 'strands', blend: 'norm', children: [{ name: 's1', x: 15, y: 20, ...disc(20, 40, [140, 80, 60]) }, { name: 's2', x: 60, y: 20, ...disc(20, 40, [140, 80, 60]) }] },
      { name: 'shine', x: 35, y: 12, ...disc(30, 12, [80, 80, 80]), blend: 'scrn' },
      { name: 'sketch', x: 0, y: 0, ...block(100, 100, [0, 255, 0]), visible: false },
      { name: '[pivot]', x: 48, y: 40, ...block(4, 4, [255, 0, 255]), ignoreInComposite: true },
    ] },
    { name: 'face', blend: 'norm', children: [
      { name: 'skin', x: 30, y: 30, ...disc(40, 40, [240, 200, 170]) },
      { name: 'blush', x: 35, y: 50, ...disc(30, 10, [240, 120, 120]), blend: 'mul ', opacity: 0.5 },
    ] },
    { name: 'mouth/smile [merge]', blend: 'norm', children: [{ name: 'lips', x: 42, y: 62, ...block(16, 4, [200, 40, 40]) }] },
    { name: 'mouth/open [merge]', blend: 'norm', visible: false, children: [{ name: 'lips', x: 44, y: 60, ...disc(12, 10, [120, 10, 10]) }] },
  ];
  const source = { ...readPsd(writePsd({ width: W, height: H, layers })), file: '/art/merge.psd' };
  const { rig, images, report } = planImport({ source, options: importOptions({ merge: ['2'] }) });
  assert.deepEqual(rig.bones.map(b => b.id), ['root'], 'a merged group is a picture, not a bone');
  assert.deepEqual(rig.slots.map(s => [s.id, s.attachment]), [['body', 'body'], ['hair', 'hair'], ['face', 'face'], ['mouth', 'smile']]);
  assert.ok(rig.attachments.some(a => a.slot === 'mouth' && a.name === 'open'), 'merged variants of one slot');
  assert.deepEqual([report.layers[1].role, report.layers[1].merge, report.layers[2].role, report.layers[2].merge], ['merged', 'tag', 'merged', 'key']);
  assert.deepEqual(report.layers[1].children.map(i => i.role), ['in-merge', 'clipped', 'in-merge', 'in-merge', 'in-merge', 'in-merge']);
  assert.equal(report.summary.merged, 4);
  assert.deepEqual(report.options.merge, ['2']);
  const pngs = pngMap(images), frame = await renderSetup(rig, pngs), expected = flattenImage(W, H, layers);
  assert.ok(maxDifference(frame, expected) <= 2, `merged frame vs composite: ${maxDifference(frame, expected)}`);
  const open = structuredClone(rig);
  open.slots.find(s => s.id === 'mouth').attachment = 'open';
  const variant = [...layers];
  variant[3] = { ...layers[3], visible: false }; variant[4] = { ...layers[4], visible: true };
  assert.ok(maxDifference(await renderSetup(open, pngs), flattenImage(W, H, variant)) <= 2, 'the hidden merged variant');
  // Without the key, `face` is a bone again (its multiply layer drawn normal and warned).
  const plain = planImport({ source, options: importOptions({}), dryRun: true });
  assert.ok(plain.rig.bones.some(b => b.id === 'face'));
  assert.ok(plain.report.warnings.some(w => w.code === 'ANIM_IMPORT_BLEND'));
});

test('over the rig limits: the preview reports and proposes merges (leaf groups, most pictures, hidden variants last); the import refuses; merging fits', async () => {
  const parts = Array.from({ length: 30 }, (_, g) => ({ name: `part ${g + 1}`, children: Array.from({ length: 10 }, (_, i) => ({
    name: `piece ${i + 1}`, x: (g % 6) * 20 + (i % 5) * 3, y: Math.floor(g / 6) * 30 + Math.floor(i / 5) * 5, ...block(6, 6, [g * 8, i * 20, 100]), visible: !(g === 0 && i === 9) })) }));
  parts[29].children.push({ name: 'extra', x: 100, y: 140, ...block(6, 6, [1, 2, 3]) });  // the biggest group: 11 pictures
  const dir = await temp('psd-limits'), file = path.join(dir, 'many.psd');
  await writeFile(file, writePsd({ width: 120, height: 150, layers: [{ name: 'body', children: parts }] }));
  const preview = await importLayers({ payload: { file, id: 'many' }, dryRun: true });
  const { limits, autoMerge, warnings } = preview.report;
  assert.deepEqual([limits.exceeded, limits.slots.count, limits.slots.max], [true, 301, 256]);
  assert.match(warnings.find(w => w.code === 'ANIM_IMPORT_LIMIT').message, /슬롯이 256개를 넘습니다\(301개\).*자동 합치기/);
  assert.equal(autoMerge.fits, true);
  assert.equal(autoMerge.keys[0], '0.29', 'the leaf group with the most pictures first');
  assert.equal(autoMerge.keys.length, 5, '301 − 10 − 4 × 9 = 255');
  assert.ok(!autoMerge.keys.includes('0.0'), 'the group with a hidden variant is merged last');
  assert.ok(autoMerge.keys.every(k => k.startsWith('0.')), 'leaf groups, not their parent');
  await assert.rejects(importLayers({ payload: { file, id: 'many' } }), /슬롯이 256개를 넘습니다\(301개\)/);
  const merged = await importLayers({ payload: { file, id: 'many', merge: autoMerge.keys }, dryRun: true });
  assert.deepEqual([merged.report.limits.exceeded, merged.report.limits.slots.count, merged.report.autoMerge], [false, 255, undefined]);
  const done = await importLayers({ payload: { file, id: 'many', merge: autoMerge.keys } });
  assert.equal(done.documents[0].data.slots.length, 255);
  // The bone limit counts too: a chain of 260 nested groups (262 bones, one picture) merges from
  // the leaf up until it fits: one key, the group whose subtree holds the 6 extra bones.
  let chain = [{ name: 'tip', x: 4, y: 4, ...block(4, 4, [9, 0, 0]) }];
  for (let g = 259; g >= 0; g--) chain = [{ name: `g${g}`, children: chain }];
  const plan = planImport({ source: { ...readPsd(writePsd({ width: 20, height: 20, layers: chain })), file: '/art/b.psd' }, options: importOptions({}), dryRun: true });
  assert.deepEqual([plan.report.limits.exceeded, plan.report.limits.bones.count, plan.report.autoMerge.fits, plan.report.autoMerge.keys.length], [true, 261, true, 1]);
  const fitted = planImport({ source: { ...readPsd(writePsd({ width: 20, height: 20, layers: chain })), file: '/art/b.psd' }, options: importOptions({ merge: plan.report.autoMerge.keys }), dryRun: true });
  assert.deepEqual([fitted.report.limits.exceeded, fitted.report.limits.bones.count], [false, 256]);
});

test('ids: Cyrillic and Greek transliterated, Photoshop default names kept, CJK falls back to part-N, ids stay unique', () => {
  assert.equal(toId('рука левая'), 'ruka-levaya');
  assert.equal(toId('тень'), 'ten');
  assert.equal(toId('блик'), 'blik');
  assert.equal(toId('щёка Юли'), 'shchyoka-yuli');
  assert.equal(toId('Їжак і ґанок'), 'yizhak-i-ganok');
  assert.equal(toId('чёлка'), 'chyolka');
  assert.equal(toId('Ελένη φως'), 'eleni-fos');
  assert.equal(toId('Layer 12 Copy 3'), 'layer-12-copy-3');
  assert.equal(toId('Layer 40 Copy 13'), 'layer-40-copy-13');
  assert.equal(toId('머리', 'part-1'), 'part-1');
  assert.equal(toId('頭', 'part-2'), 'part-2');
  const layers = [
    { name: 'рука левая', children: [{ name: 'тень', x: 10, y: 10, ...block(10, 10, [1, 2, 3]) }, { name: 'тень', x: 30, y: 10, ...block(10, 10, [4, 5, 6]) }] },
    { name: 'рука правая', children: [{ name: 'тень', x: 60, y: 10, ...block(10, 10, [7, 8, 9]) }] },
    { name: 'глаза', children: [{ name: 'глаз/открыт', x: 60, y: 50, ...block(10, 4, [1, 1, 1]) }, { name: 'глаз/закрыт', x: 60, y: 52, ...block(10, 2, [2, 2, 2]), visible: false }] },
    { name: '頭', x: 90, y: 90, ...block(10, 10, [3, 3, 3]) },
  ];
  const { rig } = planImport({ source: { ...readPsd(writePsd({ width: W, height: H, layers })), file: '/art/ru.psd' }, options: importOptions({}), dryRun: true });
  assert.deepEqual(rig.bones.map(b => b.id), ['root', 'ruka-levaya', 'ruka-pravaya', 'glaza']);
  assert.deepEqual(rig.slots.map(s => [s.id, s.bone]), [['ten', 'ruka-levaya'], ['ten-2', 'ruka-levaya'], ['ruka-pravaya-ten', 'ruka-pravaya'], ['glaz', 'glaza'], ['part-1', 'root']]);
  assert.deepEqual(rig.attachments.filter(a => a.slot === 'glaz').map(a => a.name).sort(), ['otkryt', 'zakryt']);
});

test('empty (0×0 or transparent) layers and empty groups are skipped with ONE summary warning', () => {
  const layers = [
    { name: 'body', x: 10, y: 10, ...block(20, 20, [9, 9, 9]) },
    ...Array.from({ length: 25 }, (_, i) => ({ name: `Layer ${i + 1}`, x: 0, y: 0, width: 0, height: 0, rgba: Buffer.alloc(0) })),
    ...Array.from({ length: 25 }, (_, i) => ({ name: `Layer ${i + 26}`, x: i, y: i, width: 8, height: 8, rgba: Buffer.alloc(8 * 8 * 4) })),
    { name: 'Group 1', children: [] }, { name: 'Group 2', children: [] },
    { name: 'Group 3', children: [{ name: 'Layer 99', x: 4, y: 4, width: 6, height: 6, rgba: Buffer.alloc(6 * 6 * 4) }] },
  ];
  const { report } = planImport({ source: { ...readPsd(writePsd({ width: W, height: H, layers })), file: '/art/e.psd' }, options: importOptions({}), dryRun: true });
  const empty = report.warnings.filter(w => w.code === 'ANIM_IMPORT_EMPTY');
  assert.equal(empty.length, 1);
  assert.match(empty[0].message, /레이어 51개와 그림이 없는 그룹 3개/);
  assert.equal(report.warnings.length, 1, JSON.stringify(report.warnings));
  const items = flatItems(report.layers);
  assert.equal(items.filter(i => i.role === 'empty').length, 51);
  assert.equal(items.filter(i => i.role === 'empty-group').length, 3);
  assert.ok(items.every(i => !i.warnings.length));
  assert.equal(report.summary.empty, 54);
});

test('a 5000×7000, ~500-layer production-like PSD imports at auto scale within budget (timings printed)', async () => {
  const dir = await temp('psd-big'), file = path.join(dir, 'big.psd');
  const options = { width: 5000, height: 7000, strands: 80, bangs: 45 };
  let started = performance.now();
  const bytes = realLikePsd(options);
  await writeFile(file, bytes);
  const writeMs = performance.now() - started;
  const psd = readPsd(bytes, { pixels: false });
  const layerCount = flatItems(psd.layers).filter(n => n.kind === 'layer').length;
  assert.ok(layerCount >= 480, `${layerCount} layers`);
  assert.ok(psd.layerPixels > 150 * 1048576, `${(psd.layerPixels / 1048576).toFixed(0)} Mpx of layer rectangles (more than the 1.14 limit of 64)`);
  started = performance.now();
  const preview = await importLayers({ payload: { file, id: 'big', scale: 'auto' }, dryRun: true });
  const previewMs = performance.now() - started;
  started = performance.now();
  const done = await importLayers({ payload: { file, id: 'big', scale: 'auto' } });
  const importMs = performance.now() - started;
  const pngBytes = done.images.reduce((s, i) => s + i.bytes.length, 0);
  console.log(`big PSD: ${(bytes.length / 1048576).toFixed(1)} MiB, ${countRecords(psd.layers)} records (${layerCount} layers), ${(psd.layerPixels / 1048576).toFixed(0)} Mpx; write ${Math.round(writeMs)} ms, ` +
    `preview ${Math.round(previewMs)} ms, import ${Math.round(importMs)} ms (${done.images.length} PNG, ${(pngBytes / 1048576).toFixed(1)} MiB, ` +
    `${(done.report.summary.pixels / 1048576).toFixed(1)} Mpx at ${done.report.options.scale}); rss ${(process.memoryUsage().rss / 1048576).toFixed(0)} MiB`);
  assert.deepEqual([done.report.options.scale, done.report.options.scaleMode, done.report.canvas.width, done.report.canvas.height], [0.25, 'auto', 1250, 1750]);
  assert.equal(done.report.limits.exceeded, false);
  assert.ok(done.report.summary.baked > 300, `${done.report.summary.baked} clipped layers baked`);
  // 1.19.1: shading clipped onto groups (legs, hands, eyes) is baked too: no clipping warning left.
  const items = flatItems(done.report.layers), byKey = new Map(items.map(i => [i.key, i]));
  assert.equal(items.filter(i => i.role === 'clipped' && byKey.get(i.clippedTo).kind === 'group').length, 6);
  assert.ok(!done.report.warnings.some(w => w.code === 'ANIM_IMPORT_CLIPPING'), JSON.stringify(done.report.warnings.map(w => w.code)));
  assert.equal(preview.report.summary.attachments, done.report.summary.attachments);
  assert.ok(previewMs < 10000, `preview ${previewMs} ms`);
  assert.ok(importMs < 20000, `import ${importMs} ms`);
  const rig = done.documents[0].data;
  assert.ok(rig.bones.some(b => b.id === 'ruka-levaya') && rig.bones.some(b => b.id === 'golova'), rig.bones.map(b => b.id).join());
  for (const image of done.images.slice(0, 5)) assert.ok(!decodePng(image.bytes).reason);
});

test('the production-like tree at a small size: setup frame = composite with shading clipped onto layers and groups (effects skip)', async () => {
  const size = { width: 400, height: 600, strands: 4, bangs: 3 };
  const layers = realLikeLayers(size);
  // The unclipped light / shadow overlays are left out (effects "skip"), so the composite leaves them out too.
  const skipFx = list => list.map(n => ({ ...n, ...(n.children ? { children: skipFx(n.children) } : {}),
    ...(!n.clipping && !['norm', 'pass', undefined].includes(n.blend) ? { ignoreInComposite: true } : {}) }));
  const source = { ...readPsd(writePsd({ ...size, layers, composite: 'none' })), file: '/art/real.psd' };
  const { rig, images, report } = planImport({ source, options: importOptions({ effects: 'skip', boneDirection: 'none' }) });
  assert.ok(!report.warnings.some(w => w.code === 'ANIM_IMPORT_CLIPPING'));
  const drawn = await renderSetup(rig, pngMap(images)), expected = flattenImage(size.width, size.height, skipFx(layers));
  // Exact but where linear dodge clamps on two differently coloured pictures overlapping at an
  // antialiased edge (each picture clamps on its own, Photoshop clamps the mix): a few pixels.
  let off = 0;
  for (let i = 0; i < drawn.length; i += 4) if (maxDifference(drawn.subarray(i, i + 4), expected.subarray(i, i + 4)) > 2) off++;
  assert.ok(off <= 8 && maxDifference(drawn, expected) <= 20, `production-like frame vs composite: ${off} pixels over 2, max difference ${maxDifference(drawn, expected)}`);
  // The hidden fist and closed-eyes variants carry the shading clipped onto their groups.
  const plain = planImport({ source: { ...readPsd(writePsd({ ...size, layers: realLikeLayers(size).map(n => ({ ...n, children: n.children?.filter(c => !c.clipping) ?? n.children })), composite: 'none' })), file: '/art/real.psd' }, options: importOptions({ effects: 'skip', boneDirection: 'none' }) });
  for (const ref of ['kist/kulak', 'eyes/closed']) {
    const id = rig => rig.attachments.find(a => `${a.slot}/${a.name}` === ref)?.imageAssetId;
    assert.ok(id(rig), ref);
    assert.notEqual(id(rig), id(plain.rig), `${ref} is shaded`);
  }
});

test('[box] / [point] layers (1.17.0): a traced bounding box and a point instead of pictures; bones unchanged; re-import updates them, never pictures', async () => {
  assert.ok(existsSync(wasm), 'build first: node plugins/cutout-animation/tools/build.mjs wasm');
  const { createService } = await import('../../plugins/cutout-animation/service/index.mjs');
  const service = await createService({ pluginDir: path.dirname(wasm), manifest: { modules: [{ role: 'runtime', platform: 'web-wasm', path: path.basename(wasm) }] } });
  // The head gets a round hit area (a disc: traced, not its rectangle) and a mouth anchor; a hidden box too.
  const tagged = () => {
    const layers = characterLayers();
    layers[4].children.splice(1, 0, { name: 'face [box]', x: 40, y: 20, width: 60, height: 70, pixel: (x, y) => [0, 0, 0, Math.hypot((x + 0.5 - 30) / 30, (y + 0.5 - 35) / 35) <= 1 ? 255 : 0], ignoreInComposite: true },
      { name: 'mouth [point]', x: 66, y: 72, ...block(8, 4, [255, 0, 255]), ignoreInComposite: true },
      { name: 'secret [box]', x: 0, y: 0, ...block(10, 10, [0, 0, 0]), visible: false, ignoreInComposite: true });
    return layers;
  };
  const dir = await temp('psd-boxes'), file = path.join(dir, 'boxes.psd');
  await writeFile(file, writePsd({ width: W, height: H, layers: tagged() }));
  const plain = planImport({ source: { ...readPsd(writePsd({ width: W, height: H, layers: characterLayers() })), file: '/art/boxes.psd' }, options: importOptions({}) });
  const done = await service.prepareImport({ op: 'importLayers', payload: { file, id: 'boxes' }, documents: [] });
  const rig = done.documents[0].data;
  assert.deepEqual(rig.bones, plain.rig.bones, 'boxes and points never move bones');
  const box = rig.attachments.find(a => a.slot === 'face-box'), point = rig.attachments.find(a => a.slot === 'mouth-point');
  assert.equal(box.type, 'boundingBox'); assert.equal(box.name, 'face'); assert.equal(point.type, 'point'); assert.equal(point.name, 'mouth');
  assert.ok(!('imageAssetId' in box) && !('imageAssetId' in point), 'no pictures');
  assert.equal(done.images.length, plain.images.length, 'no extra images');
  assert.ok(box.vertices.length / 2 > 4, `traced outline (${box.vertices.length / 2} points), not the rectangle`);
  const slots = rig.slots.map(s => s.id);
  assert.ok(slots.indexOf('face') < slots.indexOf('face-box') && slots.indexOf('face-box') < slots.indexOf('eyes'), 'draw order = layer order');
  assert.equal(rig.slots.find(s => s.id === 'secret-box').attachment, null, 'a hidden [box] is an attachment that is not shown');
  const head = rig.bones.find(b => b.id === 'head'), turn = (head.rotation ?? 0) * Math.PI / 180;
  // Document px of a head-bone point (the head is a child of the root at the canvas origin, maybe turned by the bone direction).
  const doc = (x = 0, y = 0) => [head.x + W / 2 + Math.cos(turn) * x - Math.sin(turn) * y, head.y + H + Math.sin(turn) * x + Math.cos(turn) * y].map(v => Math.round(v * 1e6) / 1e6);
  // The point sits at the centre of its pixels (70, 74); the box's points lie on the disc rim.
  assert.deepEqual(doc(point.x, point.y), [70, 74]);
  for (let i = 0; i < box.vertices.length; i += 2) {
    const [x, y] = doc(box.vertices[i], box.vertices[i + 1]), r = Math.hypot((x - 70) / 30, (y - 55) / 35);
    assert.ok(r > 0.85 && r < 1.2, `box point ${i / 2} on the rim (${r.toFixed(3)})`);
  }
  const flat = []; const walk = l => { for (const n of l) { flat.push(n); if (n.children) walk(n.children); } }; walk(done.report.layers);
  assert.deepEqual(flat.filter(n => n.role === 'box' || n.role === 'point').map(n => [n.name, n.role, n.slot]), [['face [box]', 'box', 'face-box'], ['mouth [point]', 'point', 'mouth-point'], ['secret [box]', 'box', 'secret-box']]);
  assert.deepEqual([done.report.summary.boxes, done.report.summary.points], [2, 1]);
  // The rendered setup frame still equals the composite: boxes and points draw nothing.
  const pngs = new Map(done.images.map(i => [i.id, i.bytes]));
  assert.ok(maxDifference(await renderSetup(rig, pngs), flattenImage(W, H, tagged())) <= 1);
  // Re-import: the art moved the mouth; the point follows, the box (weighted by rig work) keeps its weights.
  rig.attachments.find(a => a.slot === 'face-box').weights = box.vertices.filter((_, i) => i % 2 === 0).map(() => [{ bone: 'head', weight: 1 }]);
  const moved = tagged();
  moved[4].children[2].x = 76;
  await writeFile(file, writePsd({ width: W, height: H, layers: moved }));
  const again = await service.prepareImport({ op: 'importLayers', payload: { file, rig: 'boxes' }, documents: [{ type: 'rig', id: 'boxes', data: rig }] });
  const next = again.documents[0].data;
  const p2 = next.attachments.find(a => a.slot === 'mouth-point');
  assert.deepEqual(doc(p2.x, p2.y), [80, 74], 'the point follows the art');
  assert.ok(next.attachments.find(a => a.slot === 'face-box').weights, 'same outline: weights kept');
  assert.deepEqual(again.report.changes.attachments.updated, ['mouth-point/mouth']);
  // A picture layer landing on a box of the same name is skipped, never turned into a picture.
  const clash = structuredClone(rig);
  clash.attachments.find(a => a.slot === 'face' && a.name === 'face').type = 'boundingBox';
  Object.assign(clash.attachments.find(a => a.slot === 'face' && a.name === 'face'), { vertices: [0, 0, 10, 0, 10, 10] });
  const kept = await service.prepareImport({ op: 'importLayers', payload: { file, rig: 'boxes' }, dryRun: true, documents: [{ type: 'rig', id: 'boxes', data: clash }] });
  assert.ok(kept.report.changes.attachments.skipped.includes('face/face') && kept.report.warnings.some(w => w.code === 'ANIM_IMPORT_KIND'));
  // Without a tracer (planImport alone) a box is its pixels' rectangle.
  const rect = planImport({ source: { ...readPsd(writePsd({ width: W, height: H, layers: tagged() })), file: '/art/b.psd' }, options: importOptions({}) });
  assert.equal(rect.rig.attachments.find(a => a.slot === 'face-box').vertices.length, 8);
});

test('linked meshes (1.18.0): re-import swaps a linked mesh picture only at the same size, keeping the link', () => {
  // A 40x60 chest made a mesh by rig work, a 20x30 patch (same aspect) linked to it.
  const layers = (patch = block(20, 30, [20, 60, 140])) => [{ name: 'body', children: [
    { name: 'chest', x: 50, y: 100, ...block(40, 60, [180, 60, 40]) }, { name: 'patch', x: 52, y: 102, ...patch }] }];
  const source = l => ({ ...readPsd(writePsd({ width: W, height: H, layers: l })), file: '/art/link.psd' });
  const rig = structuredClone(planImport({ source: source(layers()), options: importOptions({}) }).rig);
  const chest = rig.attachments.find(a => a.slot === 'chest'), patch = rig.attachments.find(a => a.slot === 'patch');
  Object.assign(chest, { type: 'mesh', vertices: [0, 0, 40, 0, 40, 60, 0, 60], uvs: [0, 0, 1, 0, 1, 1, 0, 1], triangles: [0, 1, 2, 0, 2, 3] });
  for (const k of ['x', 'y', 'rotation', 'pivotX', 'pivotY']) delete patch[k];
  Object.assign(patch, { type: 'mesh', linkedMesh: { parent: 'chest/chest' } });
  // Same size, new colour: the picture swaps, the link stays (no placement fields come back).
  const same = planImport({ source: source(layers(block(20, 30, [200, 200, 20]))), options: importOptions({}), existing: rig });
  const swapped = same.rig.attachments.find(a => a.slot === 'patch');
  assert.deepEqual(swapped.linkedMesh, { parent: 'chest/chest' });
  assert.notEqual(swapped.imageAssetId, patch.imageAssetId);
  assert.ok(same.report.changes.attachments.updated.includes('patch/patch') && !('x' in swapped));
  // Another size: skipped, with the linked-mesh reason.
  const other = planImport({ source: source(layers(block(24, 30, [200, 200, 20]))), options: importOptions({}), existing: rig, dryRun: true });
  assert.ok(other.report.changes.attachments.skipped.includes('patch/patch'));
  assert.ok(other.report.warnings.some(w => w.code === 'ANIM_IMPORT_MESH' && w.message.includes('링크드 메시')), JSON.stringify(other.report.warnings));
  assert.deepEqual(other.rig.attachments.find(a => a.slot === 'patch'), patch);
});

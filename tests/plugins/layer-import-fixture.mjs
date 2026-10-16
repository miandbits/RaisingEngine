// Shared fixtures of the layer-import tests (plugin 1.11.0): a small layered character, the Wasm
// core's setup frame composited in straight-alpha "over", and an image difference measure.
import assert from 'node:assert/strict';
import path from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { decodePng } from '../../plugins/cutout-animation/service/atlas.mjs';

const engineRoot = fileURLToPath(new URL('../../', import.meta.url));
export const wasm = path.join(engineRoot, 'dist/cutout/build-wasm/cutout-runtime.mjs');

/** A soft disc (antialiased alpha, transparent margins) and an opaque gradient block. */
const disc = (w, h, rgb) => ({ width: w, height: h, pixel: (x, y) => {
  const d = Math.hypot((x + 0.5 - w / 2) / (w / 2), (y + 0.5 - h / 2) / (h / 2));
  return [rgb[0], (rgb[1] + x * 3) & 255, (rgb[2] + y * 5) & 255, Math.max(0, Math.min(255, Math.round((1 - d) * 4 * 255)))];
} });
export const block = (w, h, rgb) => ({ width: w, height: h, pixel: (x, y) => [(rgb[0] + x * 7) & 255, (rgb[1] + y * 3) & 255, rgb[2], 255] });

/** A small character: body, a head group with a pivot marker and eye variants, a [slot] mouth group, arms in two groups. */
export const characterLayers = () => [
  { name: 'shadow', x: 30, y: 180, ...disc(80, 14, [20, 20, 20]), opacity: 0.4 },
  { name: 'body', x: 45, y: 90, ...block(50, 90, [180, 60, 40]), rle: false },
  { name: 'left', children: [{ name: 'arm', x: 20, y: 95, ...block(25, 60, [90, 140, 60]) }] },
  { name: 'right', children: [{ name: 'arm', x: 95, y: 95, ...block(25, 60, [60, 90, 140]) }] },
  { name: 'Head', children: [
    { name: 'face', x: 40, y: 20, ...disc(60, 70, [240, 200, 160]) },
    { name: 'eyes/closed', x: 52, y: 45, ...block(36, 6, [10, 10, 10]), visible: false },
    { name: 'eyes/open', x: 52, y: 40, ...disc(36, 14, [30, 30, 200]) },
    { name: 'mouth [slot]', children: [
      { name: 'smile', x: 60, y: 70, ...block(20, 6, [200, 40, 40]) },
      { name: 'open', x: 62, y: 66, ...disc(16, 14, [120, 10, 10]), visible: false },
    ] },
    { name: '[pivot]', x: 68, y: 86, ...block(4, 4, [255, 0, 255]), ignoreInComposite: true },
  ] },
  { name: '메모 [ignore]', x: 0, y: 0, ...block(10, 10, [0, 255, 0]), ignoreInComposite: true },
  { name: '모자', x: 50, y: 8, ...disc(40, 20, [250, 220, 0]) },
];
export const W = 140, H = 200;

async function runtime() {
  const { default: factory } = await import(pathToFileURL(wasm).href);
  const m = await factory(), raw = m.cwrap('raising_extension_request', 'string', ['string']);
  return value => JSON.parse(raw(JSON.stringify(value)));
}
/** Setup frame of `rig` drawn by the Wasm core (translation-only items, nearest pixels), straight-alpha "over". */
export async function renderSetup(rig, pngs) {
  const request = await runtime();
  assert.equal(request({ op: 'loadRig', id: 'imported', data: rig }).ok, true);
  assert.equal(request({ op: 'create', rig: 'imported', instance: 'view' }).ok, true);
  const frame = request({ op: 'frame', instance: 'view' });
  assert.equal(frame.ok, true, frame.error);
  const { width, height, originX, originY } = rig.canvas;
  const dst = new Float64Array(width * height * 4);
  for (const item of frame.items) {
    const [a, b, c, d, e, f] = item.matrix;
    // Unrotated at scale 1 (an aimed bone's counter-rotated picture composes to 1 within float rounding).
    assert.ok([a - 1, b, c, d - 1].every(v => Math.abs(v) < 1e-9), `${item.slot}: the import places pictures unrotated at scale 1 (${[a, b, c, d]})`);
    const image = decodePng(pngs.get(item.imageAssetId)), ox = Math.round(e + originX), oy = Math.round(f + originY);
    // Frames are printed with 6 decimals: "whole pixels" = within that rounding.
    assert.ok(Math.abs(ox - e - originX) < 1e-5 && Math.abs(oy - f - originY) < 1e-5, `${item.slot} lands on whole pixels (${e + originX}, ${f + originY})`);
    for (let y = 0; y < image.height; y++) for (let x = 0; x < image.width; x++) {
      const cx = ox + x, cy = oy + y;
      if (cx < 0 || cy < 0 || cx >= width || cy >= height) continue;
      const s = (y * image.width + x) * 4, t = (cy * width + cx) * 4, sa = image.rgba[s + 3] / 255 * item.alpha, da = dst[t + 3], oa = sa + da * (1 - sa);
      if (oa <= 0) continue;
      for (let k = 0; k < 3; k++) dst[t + k] = (image.rgba[s + k] * sa + dst[t + k] * da * (1 - sa)) / oa;
      dst[t + 3] = oa;
    }
  }
  const out = Buffer.alloc(width * height * 4);
  for (let i = 0; i < width * height; i++) { for (let k = 0; k < 3; k++) out[i * 4 + k] = Math.round(dst[i * 4 + k]); out[i * 4 + 3] = Math.round(dst[i * 4 + 3] * 255); }
  return out;
}
/** Largest channel difference where either image is visible (colour under alpha 0 is meaningless). */
export function maxDifference(a, b) {
  let worst = 0;
  for (let i = 0; i < a.length; i += 4) {
    if (!a[i + 3] && !b[i + 3]) continue;
    worst = Math.max(worst, Math.abs(a[i + 3] - b[i + 3]));
    if (a[i + 3] > 8 && b[i + 3] > 8) for (let k = 0; k < 3; k++) worst = Math.max(worst, Math.abs(a[i + k] - b[i + k]));
  }
  return worst;
}


// Generates the rights-free demo character parts (simple vector shapes) as PNGs.
// This script only makes IMAGE FILES. Rig, clips and game connections are authored
// in the Qt editor (see demo/PROVENANCE.md).
//   node plugins/cutout-animation/demo/make-parts.mjs <outDir>
import { mkdirSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import path from 'node:path';
import { encodeRgbaPng } from '../../../dist/packages/studio-service/png.js';

const out = path.resolve(process.argv[2] ?? path.join(import.meta.dirname, 'parts'));
mkdirSync(out, { recursive: true });

// Signed distance helpers (negative = inside).
const circle = (cx, cy, r) => (x, y) => Math.hypot(x - cx, y - cy) - r;
const ellipse = (cx, cy, rx, ry) => (x, y) => (Math.hypot((x - cx) / rx, (y - cy) / ry) - 1) * Math.min(rx, ry);
const roundRect = (x0, y0, x1, y1, r) => (x, y) => {
  const qx = Math.abs(x - (x0 + x1) / 2) - ((x1 - x0) / 2 - r), qy = Math.abs(y - (y0 + y1) / 2) - ((y1 - y0) / 2 - r);
  return Math.hypot(Math.max(qx, 0), Math.max(qy, 0)) + Math.min(Math.max(qx, qy), 0) - r;
};
const segment = (ax, ay, bx, by, r) => (x, y) => {
  const px = x - ax, py = y - ay, dx = bx - ax, dy = by - ay;
  const h = Math.max(0, Math.min(1, (px * dx + py * dy) / (dx * dx + dy * dy)));
  return Math.hypot(px - dx * h, py - dy * h) - r;
};
const arc = (cx, cy, rx, ry, from, to, r, steps = 16) => {
  const parts = [];
  for (let i = 0; i < steps; i++) {
    const a0 = from + (to - from) * i / steps, a1 = from + (to - from) * (i + 1) / steps;
    parts.push(segment(cx + rx * Math.cos(a0), cy + ry * Math.sin(a0), cx + rx * Math.cos(a1), cy + ry * Math.sin(a1), r));
  }
  return (x, y) => Math.min(...parts.map(f => f(x, y)));
};
const trapezoid = (cx, top, bottom, halfTop, halfBottom, r) => (x, y) => {
  const t = Math.max(0, Math.min(1, (y - top) / (bottom - top)));
  const half = halfTop + (halfBottom - halfTop) * t;
  const dx = Math.abs(x - cx) - half, dy = Math.max(top - y, y - bottom);
  return Math.hypot(Math.max(dx, 0), Math.max(dy, 0)) + Math.min(Math.max(dx, dy), 0) - r;
};

function draw(width, height, layers) {
  const pixels = Buffer.alloc(width * height * 4);
  const S = 4;  // 4x4 supersampling for smooth edges
  for (let y = 0; y < height; y++) for (let x = 0; x < width; x++) {
    let r = 0, g = 0, b = 0, a = 0;
    for (const [shape, [lr, lg, lb, la = 255]] of layers) {
      let cover = 0;
      for (let sy = 0; sy < S; sy++) for (let sx = 0; sx < S; sx++) if (shape(x + (sx + 0.5) / S, y + (sy + 0.5) / S) <= 0) cover++;
      const alpha = cover / (S * S) * la / 255;
      if (!alpha) continue;
      // Source-over in premultiplied space.
      r = lr * alpha + r * (1 - alpha); g = lg * alpha + g * (1 - alpha); b = lb * alpha + b * (1 - alpha); a = alpha + a * (1 - alpha);
    }
    const i = (y * width + x) * 4;
    pixels[i] = a ? Math.round(r / a) : 0; pixels[i + 1] = a ? Math.round(g / a) : 0; pixels[i + 2] = a ? Math.round(b / a) : 0; pixels[i + 3] = Math.round(a * 255);
  }
  return encodeRgbaPng({ width, height, pixels });
}

const skin = [255, 224, 200], skinShade = [236, 196, 172], hair = [92, 64, 120], hairLight = [124, 92, 156];
const dress = [120, 170, 230], dressDark = [84, 132, 196], ink = [52, 40, 60], white = [255, 255, 255], blushPink = [255, 150, 160, 120];
const parts = {
  'hair-back': [170, 190, [[roundRect(10, 10, 160, 185, 60), hair]]],
  'body': [130, 170, [[trapezoid(65, 8, 160, 34, 60, 8), dress], [roundRect(45, 0, 85, 16, 6), skin], [roundRect(10, 150, 120, 168, 6), dressDark]]],
  'arm-left': [40, 120, [[roundRect(4, 4, 36, 70, 14), dress], [roundRect(8, 60, 32, 116, 12), skin]]],
  'arm-right': [40, 120, [[roundRect(4, 4, 36, 70, 14), dress], [roundRect(8, 60, 32, 116, 12), skin]]],
  'head': [150, 150, [[circle(75, 78, 66), skin], [ellipse(75, 132, 30, 8), skinShade]]],
  'hair-front': [160, 80, [[(x, y) => Math.max(ellipse(80, 40, 76, 40)(x, y), -ellipse(80, 96, 64, 44)(x, y)), hair], [ellipse(52, 22, 20, 8), hairLight]]],
  'eyes-open': [90, 30, [[ellipse(18, 15, 11, 14), ink], [ellipse(72, 15, 11, 14), ink], [circle(14, 9, 4), white], [circle(68, 9, 4), white]]],
  'eyes-half': [90, 30, [[(x, y) => Math.max(ellipse(18, 15, 11, 14)(x, y), 14 - y), ink], [(x, y) => Math.max(ellipse(72, 15, 11, 14)(x, y), 14 - y), ink], [segment(6, 14, 30, 14, 2), ink], [segment(60, 14, 84, 14, 2), ink]]],
  'eyes-closed': [90, 30, [[arc(18, 10, 12, 8, 0.15, Math.PI - 0.15, 2.2), ink], [arc(72, 10, 12, 8, 0.15, Math.PI - 0.15, 2.2), ink]]],
  'mouth-neutral': [40, 20, [[segment(10, 10, 30, 10, 2), ink]]],
  'mouth-smile': [40, 20, [[arc(20, 4, 13, 10, 0.2, Math.PI - 0.2, 2.2), ink]]],
  'mouth-open': [40, 24, [[ellipse(20, 12, 9, 10), [170, 60, 80]], [ellipse(20, 17, 6, 4), [240, 120, 130]]]],
  'blush': [110, 20, [[ellipse(18, 10, 14, 7), blushPink], [ellipse(92, 10, 14, 7), blushPink]]],
};
const manifest = [];
for (const [name, [w, h, layers]] of Object.entries(parts)) {
  const bytes = draw(w, h, layers);
  writeFileSync(path.join(out, `${name}.png`), bytes);
  manifest.push({ file: `${name}.png`, width: w, height: h, sha256: createHash('sha256').update(bytes).digest('hex') });
}
writeFileSync(path.join(out, 'parts.json'), JSON.stringify({ generator: 'plugins/cutout-animation/demo/make-parts.mjs', parts: manifest }, null, 2) + '\n');
console.log(`${manifest.length} parts -> ${out}`);

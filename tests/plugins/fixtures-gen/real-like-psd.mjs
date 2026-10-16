// A production-like character PSD, generated (plugin 1.15.0 layer import): the structure artists
// actually hand over — groups nested 3–4 deep, Cyrillic layer names ("рука левая", "тень",
// "блик"), Photoshop default names ("Layer 40 Copy 13"), every part shaded with clipped multiply /
// linear-dodge / overlay layers, unclipped light and shadow overlays, eye and mouth variants
// (hidden layers), a hidden sketch, layer masks, empty 0×0 and fully transparent layers and
// empty groups. The pictures are soft-edged ellipses (antialiased alpha, transparent margins,
// written with PackBits like Photoshop) so big documents stay small and fast to write.
//
//   node tests/plugins/fixtures-gen/real-like-psd.mjs <out.psd> [width height strands bangs]
//
// writes a ~4000×6000, ~250-layer file (for GUI checks; never committed). The tests import
// realLikeLayers() directly (tests/plugins/cutout-layer-import.test.mjs: 5000×7000, ~500 layers).
import { writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { writePsd } from '../psd-fixture.mjs';

/**
 * A soft ellipse layer filling its rectangle minus a transparent margin: `rows(y, r, g, b, a)`
 * writes one row (1 px antialiased edge, a slight vertical gradient so colours are not flat).
 */
export function ellipse(name, x, y, width, height, rgb, extra = {}) {
  width = Math.max(4, Math.round(width)); height = Math.max(4, Math.round(height));
  const pad = Math.max(1, Math.round(Math.min(width, height) * 0.04)), rx = width / 2 - pad, ry = height / 2 - pad, cx = width / 2, cy = height / 2;
  const rows = (row, r, g, b, a) => {
    const dy = (row + 0.5 - cy) / ry;
    if (Math.abs(dy) >= 1) return;
    const half = rx * Math.sqrt(1 - dy * dy), x0 = cx - half, x1 = cx + half;
    const shade = Math.round((row / height) * 24);
    for (let i = Math.max(0, Math.floor(x0)); i < Math.min(width, Math.ceil(x1)); i++) {
      const cover = Math.max(0, Math.min(1, Math.min(i + 1, x1) - Math.max(i, x0)));
      if (cover <= 0) continue;
      a[i] = Math.round(cover * 255);
      r[i] = Math.max(0, Math.min(255, rgb[0] - shade)); g[i] = Math.max(0, Math.min(255, rgb[1] - shade)); b[i] = Math.max(0, Math.min(255, rgb[2] - shade));
    }
  };
  return { name, x: Math.round(x), y: Math.round(y), width, height, rows, ...extra };
}
/** A part shaded like painted art: base + clipped multiply shadow + linear-dodge highlight (+ overlay). */
function shaded(name, x, y, w, h, rgb, { overlay = false, shadeName = 'тень', lightName = 'блик' } = {}) {
  return [
    ellipse(name, x, y, w, h, rgb),
    ellipse(shadeName, x + w * 0.35, y + h * 0.1, w * 0.8, h * 0.95, [150, 110, 160], { clipping: true, blend: 'mul ', opacity: 0.7 }),
    ellipse(lightName, x - w * 0.05, y + h * 0.05, w * 0.45, h * 0.4, [90, 80, 60], { clipping: true, blend: 'lddg', opacity: 0.8 }),
    ...(overlay ? [ellipse('складки', x + w * 0.1, y + h * 0.3, w * 0.7, h * 0.5, [200, 160, 140], { clipping: true, blend: 'over', opacity: 0.5 })] : []),
  ];
}

/**
 * The layer tree (bottom first) of a W×H character: `strands` back-hair and `bangs` front-hair
 * strands (3 layers each with their shading) set the size; 24/16 ≈ 250 layers, 80/45 ≈ 500.
 */
export function realLikeLayers({ width: W = 4000, height: H = 6000, strands = 24, bangs = 16 } = {}) {
  const X = f => W * f, Y = f => H * f;
  const leg = side => {
    const s = side === 'левая' ? -1 : 1, lx = X(0.5 + s * 0.08) - X(0.06);
    return { name: `нога ${side}`, children: [
      { name: 'бедро', children: [...shaded('бедро', lx, Y(0.55), X(0.12), Y(0.17), [240, 205, 180]), { name: '[pivot]', ...ellipse('[pivot]', lx + X(0.05), Y(0.56), 8, 8, [255, 0, 255]), ignoreInComposite: true }] },
      { name: 'голень', children: [...shaded('голень', lx + X(0.01), Y(0.7), X(0.1), Y(0.17), [240, 205, 180])] },
      ...shaded('ступня', lx, Y(0.86), X(0.12), Y(0.06), [80, 60, 50]),
      ...shaded('носок', lx + X(0.005), Y(0.8), X(0.11), Y(0.08), [250, 250, 250]),
    ] };
  };
  const arm = side => {
    const s = side === 'левая' ? -1 : 1, ax = X(0.5 + s * 0.2) - X(0.05);
    return { name: `рука ${side}`, children: [
      { name: 'плечо', children: shaded('плечо', ax, Y(0.3), X(0.1), Y(0.16), [240, 205, 180]) },
      { name: 'предплечье', children: shaded('предплечье', ax + s * X(0.01), Y(0.44), X(0.09), Y(0.15), [240, 205, 180]) },
      { name: 'кисть', children: [
        ...shaded('кисть/открытая', ax, Y(0.58), X(0.09), Y(0.07), [240, 205, 180]),
        ellipse('кисть/кулак', ax + X(0.01), Y(0.58), X(0.07), Y(0.06), [235, 200, 175], { visible: false }),
      ] },
    ] };
  };
  const eye = (side, x) => ({ name: `глаз ${side}`, children: [
    ellipse('белок', x, Y(0.155), X(0.07), Y(0.03), [250, 250, 250]),
    ellipse('радужка', x + X(0.015), Y(0.152), X(0.04), Y(0.035), [60, 120, 200], { clipping: true }),
    ellipse('зрачок', x + X(0.025), Y(0.16), X(0.02), Y(0.02), [10, 10, 30], { clipping: true }),
    ellipse('блик', x + X(0.02), Y(0.155), X(0.012), Y(0.01), [255, 255, 255], { blend: 'scrn' }),
    ellipse('ресницы', x - X(0.005), Y(0.145), X(0.08), Y(0.012), [30, 20, 20]),
  ] });
  const strand = (i, n, top, length, spread, name) => {
    const x = X(0.5 - spread / 2) + (X(spread) * i) / Math.max(1, n - 1) - X(0.015);
    return shaded(`${name} ${i + 1}`, x, Y(top) + (i % 5) * Y(0.004), X(0.035), Y(length) * (0.8 + (i % 7) * 0.05), [120 + (i % 3) * 10, 70, 50]);
  };
  return [
    ellipse('эскиз', 0, 0, X(0.9), Y(0.95), [120, 120, 120], { visible: false, blend: 'mul ' }),
    ellipse('тень на полу', X(0.3), Y(0.9), X(0.4), Y(0.05), [60, 50, 70], { blend: 'mul ', opacity: 0.5 }),
    { name: 'задние волосы', children: Array.from({ length: strands }, (_, i) => ({ name: `прядь ${i + 1}`, children: strand(i, strands, 0.08, 0.42, 0.42, 'прядь') })).flat() },
    { name: 'ноги', children: [leg('левая'), leg('правая')] },
    { name: 'тело', children: [
      ...shaded('шея', X(0.46), Y(0.24), X(0.08), Y(0.06), [240, 205, 180]),
      ...shaded('торс', X(0.38), Y(0.28), X(0.24), Y(0.24), [90, 110, 170], { overlay: true }),
      { name: 'юбка', children: [
        ...shaded('юбка', X(0.33), Y(0.46), X(0.34), Y(0.14), [60, 70, 120], { overlay: true }),
        ...Array.from({ length: 8 }, (_, i) => [
          ellipse(`Layer 40 Copy ${i + 1}`, X(0.34 + i * 0.04), Y(0.47), X(0.04), Y(0.13), [70, 80, 140], { mask: i === 0 }),
          ellipse('тень', X(0.35 + i * 0.04), Y(0.5), X(0.03), Y(0.1), [140, 120, 160], { clipping: true, blend: 'mul ' }),
        ]).flat(),
      ] },
    ] },
    arm('левая'), arm('правая'),
    { name: 'голова', children: [
      ...shaded('уши', X(0.36), Y(0.12), X(0.28), Y(0.06), [240, 200, 175]),
      ...shaded('лицо', X(0.38), Y(0.06), X(0.24), Y(0.17), [245, 215, 190]),
      ellipse('румянец', X(0.41), Y(0.18), X(0.18), Y(0.02), [240, 150, 150], { blend: 'mul ', opacity: 0.4 }),
      ellipse('нос', X(0.49), Y(0.17), X(0.02), Y(0.015), [220, 170, 150]),
      { name: 'глаза', children: [eye('левый', X(0.41)), eye('правый', X(0.52)), ellipse('eyes/closed', X(0.41), Y(0.16), X(0.18), Y(0.01), [40, 30, 30], { visible: false })] },
      ellipse('бровь левая', X(0.41), Y(0.135), X(0.07), Y(0.008), [90, 60, 40]),
      ellipse('бровь правая', X(0.52), Y(0.135), X(0.07), Y(0.008), [90, 60, 40]),
      { name: 'рот [slot]', children: [
        ellipse('smile', X(0.47), Y(0.2), X(0.06), Y(0.012), [200, 80, 80]),
        ellipse('open', X(0.475), Y(0.195), X(0.05), Y(0.025), [150, 40, 40], { visible: false }),
        ellipse('o', X(0.485), Y(0.195), X(0.03), Y(0.025), [150, 40, 40], { visible: false }),
      ] },
      { name: 'чёлка', children: Array.from({ length: bangs }, (_, i) => strand(i, bangs, 0.04, 0.12, 0.26, 'прядь')).flat() },
      { name: '[pivot]', ...ellipse('[pivot]', X(0.5), Y(0.23), 8, 8, [255, 0, 255]), ignoreInComposite: true },
    ] },
    { name: 'аксессуары', children: [
      ...shaded('бант', X(0.44), Y(0.02), X(0.12), Y(0.05), [220, 40, 60]),
      ...shaded('Layer 12 Copy 3', X(0.47), Y(0.3), X(0.06), Y(0.03), [230, 200, 60]),
      ellipse('серьга левая', X(0.37), Y(0.17), X(0.015), Y(0.02), [240, 220, 90], { mask: true }),
      ellipse('серьга правая', X(0.615), Y(0.17), X(0.015), Y(0.02), [240, 220, 90]),
      { name: 'Group 3', children: [] },
      { name: 'Group 4', children: [{ name: 'Layer 9', x: 0, y: 0, width: 0, height: 0, rgba: Buffer.alloc(0) }] },
    ] },
    { name: 'Layer 7', x: 0, y: 0, width: 0, height: 0, rgba: Buffer.alloc(0) },
    { name: 'Layer 8', x: X(0.1), y: Y(0.1), width: 64, height: 64, rgba: Buffer.alloc(64 * 64 * 4) },
    ellipse('свет', X(0.2), Y(0.05), X(0.5), Y(0.5), [60, 50, 30], { blend: 'scrn', opacity: 0.5 }),
    ellipse('свечение', X(0.6), Y(0.3), X(0.3), Y(0.3), [40, 40, 80], { blend: 'lddg', opacity: 0.4 }),
    ellipse('тень общая', X(0.3), Y(0.5), X(0.5), Y(0.4), [180, 170, 200], { blend: 'mul ', opacity: 0.4 }),
  ];
}
/** Number of layer records (groups count as two: the folder and its divider). */
export function countRecords(nodes) { return nodes.reduce((n, l) => n + (l.children ? 2 + countRecords(l.children) : 1), 0); }
/** PSD bytes of realLikeLayers (no merged image: nothing reads it, and it would take a 35 Mpx composite). */
export const realLikePsd = (options = {}) => writePsd({ width: options.width ?? 4000, height: options.height ?? 6000, layers: realLikeLayers(options), composite: 'none' });

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const [out, width, height, strands, bangs] = process.argv.slice(2);
  if (!out) { console.error('usage: node tests/plugins/fixtures-gen/real-like-psd.mjs <out.psd> [width height strands bangs]'); process.exit(2); }
  const options = { width: Number(width) || 4000, height: Number(height) || 6000, strands: Number(strands) || 24, bangs: Number(bangs) || 16 };
  const started = performance.now(), bytes = realLikePsd(options);
  writeFileSync(out, bytes);
  console.log(`${out}: ${options.width}×${options.height}, ${countRecords(realLikeLayers(options))} layer records, ${(bytes.length / 1048576).toFixed(1)} MiB in ${Math.round(performance.now() - started)} ms`);
}

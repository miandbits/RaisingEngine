// Tiny PSD/PSB writer for the layer-import tests (plugin 1.11.0): RGBA layers, groups (section
// dividers), hidden layers, opacity, blend keys, clipping, layer masks, Unicode names, raw and
// PackBits channels, and the merged composite. The composite is computed independently of the
// importer (plugin 1.15.0): straight-alpha W3C blending (normal, multiply, screen, overlay, linear
// dodge, lighten, darken, soft light), Photoshop clipping (a clipped layer paints inside its
// base's alpha), pass-through groups (group opacity multiplied into each layer), isolated
// groups (`blend: 'norm'`) and groups as clipping bases (1.19.1: the group's composite alpha). Big sparse documents: layers given as `rows(y, r, g, b, a)` row
// fillers and `composite: 'none'` write a 5000×7000, 500-layer file in seconds. No dependency.

/** PackBits rows of one plane (Apple/Adobe RLE): returns [counts per row, bytes]. */
export function packBits(plane, width, height) {
  const counts = new Array(height), out = Buffer.alloc(height * (width + Math.ceil(width / 128) + 1));
  let o = 0;
  for (let y = 0; y < height; y++) {
    const start = o, row = y * width, end = row + width;
    let i = row;
    while (i < end) {
      let run = 1;
      while (i + run < end && run < 128 && plane[i + run] === plane[i]) run++;
      if (run >= 2) { out[o++] = 257 - run; out[o++] = plane[i]; i += run; continue; }
      let lit = 1;
      while (i + lit < end && lit < 128 && !(i + lit + 1 < end && plane[i + lit] === plane[i + lit + 1])) lit++;
      out[o++] = lit - 1;
      for (let k = 0; k < lit; k++) out[o++] = plane[i + k];
      i += lit;
    }
    counts[y] = o - start;
  }
  return [counts, out.subarray(0, o)];
}

const u16 = v => { const b = Buffer.alloc(2); b.writeUInt16BE(v); return b; };
const i16 = v => { const b = Buffer.alloc(2); b.writeInt16BE(v); return b; };
const u32 = v => { const b = Buffer.alloc(4); b.writeUInt32BE(v); return b; };
const i32 = v => { const b = Buffer.alloc(4); b.writeInt32BE(v); return b; };
const u64 = v => { const b = Buffer.alloc(8); b.writeBigUInt64BE(BigInt(v)); return b; };

/** Pixels of a layer spec: `rgba` Buffer, `pixel(x, y) → [r, g, b, a]` or `rows(y, r, g, b, a)` over width × height. */
export function pixelsOf(layer) {
  if (layer.rgba) return layer.rgba;
  const px = Buffer.alloc(layer.width * layer.height * 4);
  if (layer.rows) {
    const [r, g, b, a] = planesOf(layer);
    for (let i = 0; i < layer.width * layer.height; i++) { px[i * 4] = r[i]; px[i * 4 + 1] = g[i]; px[i * 4 + 2] = b[i]; px[i * 4 + 3] = a[i]; }
    return px;
  }
  for (let y = 0; y < layer.height; y++) for (let x = 0; x < layer.width; x++) px.set(layer.pixel(x, y), (y * layer.width + x) * 4);
  return px;
}
/** [r, g, b, a] planes of a layer spec (row fillers write straight into them). */
function planesOf(layer) {
  const count = layer.width * layer.height, planes = [0, 1, 2, 3].map(() => Buffer.alloc(count));
  if (layer.rows) {
    for (let y = 0; y < layer.height; y++) layer.rows(y, ...planes.map(p => p.subarray(y * layer.width, (y + 1) * layer.width)));
    return planes;
  }
  const px = pixelsOf(layer);
  for (let i = 0; i < count; i++) for (let k = 0; k < 4; k++) planes[k][i] = px[i * 4 + k];
  return planes;
}
function channelData(plane, width, height, rle, psb) {
  if (!rle) return Buffer.concat([u16(0), plane]);
  const [counts, data] = packBits(plane, width, height);
  return Buffer.concat([u16(1), ...counts.map(c => psb ? u32(c) : u16(c)), data]);
}
function pascal(name) {
  const bytes = Buffer.from(name.replace(/[^\x20-\x7e]/g, '?').slice(0, 255), 'latin1');
  const total = 1 + bytes.length, pad = (4 - (total % 4)) % 4;
  return Buffer.concat([Buffer.from([bytes.length]), bytes, Buffer.alloc(pad)]);
}
function info(key, data, psb, long = false) { return Buffer.concat([Buffer.from('8BIM'), Buffer.from(key, 'latin1'), psb && long ? u64(data.length) : u32(data.length), data]); }
function luni(name) { const s = Buffer.from(name, 'utf16le').swap16(); return Buffer.concat([u32(name.length), s, Buffer.alloc((4 - ((4 + s.length) % 4)) % 4)]); }

/** Flattened records bottom → top: groups = [divider, ...children, folder]. */
function flatten(nodes, out = []) {
  for (const n of nodes) {
    if (n.children) {
      out.push({ divider: true, name: '</Layer group>' });
      flatten(n.children, out);
      out.push({ ...n, folder: true });
    } else out.push(n);
  }
  return out;
}

/** W3C separable blend functions on 0..1 (backdrop b, source s); unknown keys draw normal. */
export const BLEND = {
  'mul ': (b, s) => b * s,
  scrn: (b, s) => b + s - b * s,
  over: (b, s) => (b <= 0.5 ? 2 * b * s : 1 - 2 * (1 - b) * (1 - s)),
  lddg: (b, s) => Math.min(1, b + s),
  lite: (b, s) => Math.max(b, s),
  dark: (b, s) => Math.min(b, s),
  sLit: (b, s) => {
    if (s <= 0.5) return b - (1 - 2 * s) * b * (1 - b);
    const d = b <= 0.25 ? ((16 * b - 12) * b + 4) * b : Math.sqrt(b);
    return b + (2 * s - 1) * (d - b);
  },
};
const blendFn = key => BLEND[key] ?? ((b, s) => s);

/**
 * The document's merged image (what Photoshop would show), RGBA width × height: visible layers
 * back to front, each blended onto what is below it; clipped layers paint inside their base's
 * alpha (transparency locked) before the clipping group is drawn with the base's blend mode and
 * opacity; a pass-through group multiplies its opacity into its layers; a `blend: 'norm'` group
 * — or any group with layers clipped to it — is composited on its own first. Colour is straight (0..255), alpha 0..1, rounded once.
 */
export function flattenImage(width, height, nodes) {
  const canvas = () => new Float64Array(width * height * 4);
  // One source pixel (colour 0..255, alpha `sa` 0..1) onto dst[d] with blend B.
  const over = (dst, d, sr, sg, sb, sa, B) => {
    const da = dst[d + 3], oa = sa + da * (1 - sa);
    if (oa <= 0) return;
    const s = [sr, sg, sb];
    for (let k = 0; k < 3; k++) {
      const cs = s[k] / 255, cb = dst[d + k] / 255, mix = (1 - da) * cs + da * B(cb, cs);
      dst[d + k] = (sa * mix + da * (1 - sa) * cb) / oa * 255;
    }
    dst[d + 3] = oa;
  };
  const layerCanvas = n => {
    const c = canvas(), px = pixelsOf(n);
    for (let y = 0; y < n.height; y++) for (let x = 0; x < n.width; x++) {
      const cx = n.x + x, cy = n.y + y;
      if (cx < 0 || cy < 0 || cx >= width || cy >= height) continue;
      const s = (y * n.width + x) * 4, d = (cy * width + cx) * 4;
      c[d] = px[s]; c[d + 1] = px[s + 1]; c[d + 2] = px[s + 2]; c[d + 3] = px[s + 3] / 255;
    }
    return c;
  };
  const draw = (list, dst, visible, opacity) => {
    let base = -1;
    const baseOf = list.map((n, i) => (n.clipping && base >= 0 ? base : ((base = n.clipping ? base : i), -1)));
    list.forEach((n, i) => {
      if (baseOf[i] >= 0) return;  // drawn with its base
      const shown = visible && n.visible !== false;
      if (!shown || n.ignoreInComposite) return;
      let c, o = opacity * (n.opacity ?? 1);
      // A group with layers clipped to it is their base: composited on its own first (its alpha is
      // the clipping mask), even a pass-through one.
      const clipped = list.some((m, j) => baseOf[j] === i && m.visible !== false && !m.ignoreInComposite);
      if (n.children && (n.blend ?? 'pass') === 'pass' && !clipped) { draw(n.children, dst, shown, o); return; }
      if (n.children) { c = canvas(); draw(n.children, c, true, 1); } else c = layerCanvas(n);
      list.forEach((m, j) => {
        if (baseOf[j] !== i || m.visible === false || m.ignoreInComposite) return;
        const p = m.children ? (() => { const g = canvas(); draw(m.children, g, true, 1); return g; })() : layerCanvas(m), B = blendFn(m.blend), mo = m.opacity ?? 1;
        for (let d = 0; d < c.length; d += 4) {
          const t = p[d + 3] * mo;
          if (t <= 0 || c[d + 3] <= 0) continue;
          for (let k = 0; k < 3; k++) { const cb = c[d + k] / 255, cs = p[d + k] / 255; c[d + k] = ((1 - t) * cb + t * B(cb, cs)) * 255; }
        }
      });
      const B = blendFn(n.blend);
      for (let d = 0; d < c.length; d += 4) if (c[d + 3] > 0) over(dst, d, c[d], c[d + 1], c[d + 2], c[d + 3] * o, B);
    });
  };
  const dst = canvas();
  draw(nodes, dst, true, 1);
  const out = Buffer.alloc(width * height * 4);
  for (let i = 0; i < width * height; i++) { for (let k = 0; k < 3; k++) out[i * 4 + k] = Math.round(dst[i * 4 + k]); out[i * 4 + 3] = Math.round(dst[i * 4 + 3] * 255); }
  return out;
}

/**
 * PSD bytes. `layers`: tree of {name, x, y, width, height, rgba | pixel | rows, visible?, opacity?,
 * blend?, clipping?, mask?, rle?, compression?, children?} (bottom first). Options: psb, rle (default for every
 * channel), depth, mode, channels, composite (default: flattenImage; 'none' = transparent), noLayers.
 */
export function writePsd({ width, height, layers = [], psb = false, rle = true, depth = 8, mode = 3, channels = 4, noLayers = false, composite } = {}) {
  const header = Buffer.concat([Buffer.from('8BPS'), u16(psb ? 2 : 1), Buffer.alloc(6), u16(channels), u32(height), u32(width), u16(depth), u16(mode)]);
  const records = [], images = [];
  for (const n of noLayers ? [] : flatten(layers)) {
    const layer = n.divider || n.folder ? { x: 0, y: 0, width: 0, height: 0 } : n;
    const useRle = n.rle ?? rle, planes = [];
    if (layer.width && layer.height) {
      const [r, g, b, a] = planesOf(layer);
      planes.push([-1, a], [0, r], [1, g], [2, b]);
    } else planes.push([-1, Buffer.alloc(0)], [0, Buffer.alloc(0)], [1, Buffer.alloc(0)], [2, Buffer.alloc(0)]);
    const data = planes.map(([id, p]) => [id, layer.width && layer.height ? channelData(p, layer.width, layer.height, useRle, psb) : u16(0)]);
    // `compression: 2` marks the first channel as ZIP (the reader must refuse it).
    if (n.compression !== undefined && data.length) data[0][1] = Buffer.concat([u16(n.compression), data[0][1].subarray(2)]);
    let maskData = u32(0);
    if (n.mask) {
      // A 4×4 user mask (channel -2) at the layer's corner: the reader must skip it.
      maskData = Buffer.concat([u32(20), i32(layer.y), i32(layer.x), i32(layer.y + 4), i32(layer.x + 4), Buffer.from([255, 0]), Buffer.alloc(2)]);
      data.push([-2, channelData(Buffer.alloc(16, 128), 4, 4, useRle, psb)]);
    }
    const name = n.name ?? '';
    const extras = [info('luni', luni(name), psb)];
    if (n.divider) extras.push(info('lsct', u32(3), psb));
    if (n.folder) extras.push(info('lsct', Buffer.concat([u32(n.closed ? 2 : 1), Buffer.from('8BIM'), Buffer.from(n.blend ?? 'pass', 'latin1')]), psb));
    const extra = Buffer.concat([maskData, u32(0), pascal(name), ...extras]);
    const flags = n.visible === false ? 2 : 0;
    records.push(Buffer.concat([i32(layer.y), i32(layer.x), i32(layer.y + layer.height), i32(layer.x + layer.width), u16(data.length),
      ...data.map(([id, d]) => Buffer.concat([i16(id), psb ? u64(d.length) : u32(d.length)])),
      Buffer.from('8BIM'), Buffer.from(n.folder ? (n.blend ?? 'pass') : (n.blend ?? 'norm'), 'latin1'), Buffer.from([Math.round((n.opacity ?? 1) * 255), n.clipping ? 1 : 0, flags, 0]),
      u32(extra.length), extra]));
    images.push(...data.map(([, d]) => d));
  }
  let layerInfo = Buffer.alloc(0);
  if (records.length) {
    let body = Buffer.concat([i16(-records.length), ...records, ...images]);
    if (body.length % 2) body = Buffer.concat([body, Buffer.alloc(1)]);
    layerInfo = Buffer.concat([psb ? u64(body.length) : u32(body.length), body]);
  }
  const layerSection = Buffer.concat([layerInfo, records.length ? u32(0) : Buffer.alloc(0)]);
  // `composite: 'none'`: a transparent merged image (big documents whose composite no test reads).
  const merged = composite === 'none' ? null : composite ?? flattenImage(width, height, layers);
  const plane = k => { const p = Buffer.alloc(width * height); if (merged) for (let i = 0; i < width * height; i++) p[i] = merged[i * 4 + k]; return p; };
  const mergedPlanes = Array.from({ length: channels }, (_, k) => plane(k < 4 ? [0, 1, 2, 3][k] : 3));
  let image;
  if (rle) {
    const packed = mergedPlanes.map(p => packBits(p, width, height));
    image = Buffer.concat([u16(1), ...packed.flatMap(([counts]) => counts.map(c => psb ? u32(c) : u16(c))), ...packed.map(([, d]) => d)]);
  } else image = Buffer.concat([u16(0), ...mergedPlanes]);
  return Buffer.concat([header, u32(0), u32(0), psb ? u64(layerSection.length) : u32(layerSection.length), layerSection, image]);
}

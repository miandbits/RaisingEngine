// Tiny PSD/PSB writer for the layer-import tests (plugin 1.11.0): RGBA layers, groups (section
// dividers), hidden layers, opacity, blend keys, clipping, layer masks, Unicode names, raw and
// PackBits channels, and the merged composite (flattened here with the importer's semantics:
// straight-alpha "over", group opacity multiplied into each layer). No external dependency.

/** PackBits rows of one plane (Apple/Adobe RLE): returns [counts per row, bytes]. */
export function packBits(plane, width, height) {
  const counts = [], parts = [];
  for (let y = 0; y < height; y++) {
    const row = plane.subarray(y * width, (y + 1) * width), out = [];
    let i = 0;
    while (i < row.length) {
      let run = 1;
      while (i + run < row.length && run < 128 && row[i + run] === row[i]) run++;
      if (run >= 2) { out.push(257 - run, row[i]); i += run; continue; }
      let lit = 1;
      while (i + lit < row.length && lit < 128 && !(i + lit + 1 < row.length && row[i + lit] === row[i + lit + 1])) lit++;
      out.push(lit - 1, ...row.subarray(i, i + lit)); i += lit;
    }
    counts.push(out.length); parts.push(Buffer.from(out));
  }
  return [counts, Buffer.concat(parts)];
}

const u16 = v => { const b = Buffer.alloc(2); b.writeUInt16BE(v); return b; };
const i16 = v => { const b = Buffer.alloc(2); b.writeInt16BE(v); return b; };
const u32 = v => { const b = Buffer.alloc(4); b.writeUInt32BE(v); return b; };
const i32 = v => { const b = Buffer.alloc(4); b.writeInt32BE(v); return b; };
const u64 = v => { const b = Buffer.alloc(8); b.writeBigUInt64BE(BigInt(v)); return b; };

/** Pixels of a layer spec: `rgba` Buffer or `pixel(x, y) → [r, g, b, a]` over width × height. */
function pixelsOf(layer) {
  if (layer.rgba) return layer.rgba;
  const px = Buffer.alloc(layer.width * layer.height * 4);
  for (let y = 0; y < layer.height; y++) for (let x = 0; x < layer.width; x++) px.set(layer.pixel(x, y), (y * layer.width + x) * 4);
  return px;
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

/** Straight-alpha composite of visible layers (group opacity multiplied in), RGBA width × height. */
export function flattenImage(width, height, nodes) {
  const dst = new Float64Array(width * height * 4);
  const draw = (list, visible, opacity) => {
    for (const n of list) {
      const shown = visible && n.visible !== false, o = opacity * (n.opacity ?? 1);
      if (n.children) { draw(n.children, shown, o); continue; }
      if (!shown || n.ignoreInComposite) continue;
      const px = pixelsOf(n);
      for (let y = 0; y < n.height; y++) for (let x = 0; x < n.width; x++) {
        const cx = n.x + x, cy = n.y + y;
        if (cx < 0 || cy < 0 || cx >= width || cy >= height) continue;
        const s = (y * n.width + x) * 4, d = (cy * width + cx) * 4, sa = px[s + 3] / 255 * o, da = dst[d + 3];
        const oa = sa + da * (1 - sa);
        if (oa <= 0) continue;
        for (let k = 0; k < 3; k++) dst[d + k] = (px[s + k] * sa + dst[d + k] * da * (1 - sa)) / oa;
        dst[d + 3] = oa;
      }
    }
  };
  draw(nodes, true, 1);
  const out = Buffer.alloc(width * height * 4);
  for (let i = 0; i < width * height; i++) { for (let k = 0; k < 3; k++) out[i * 4 + k] = Math.round(dst[i * 4 + k]); out[i * 4 + 3] = Math.round(dst[i * 4 + 3] * 255); }
  return out;
}

/**
 * PSD bytes. `layers`: tree of {name, x, y, width, height, rgba | pixel, visible?, opacity?,
 * blend?, clipping?, mask?, rle?, compression?, children?} (bottom first). Options: psb, rle (default for every
 * channel), depth, mode, channels, composite (default: flattenImage), noLayers.
 */
export function writePsd({ width, height, layers = [], psb = false, rle = true, depth = 8, mode = 3, channels = 4, noLayers = false, composite } = {}) {
  const header = Buffer.concat([Buffer.from('8BPS'), u16(psb ? 2 : 1), Buffer.alloc(6), u16(channels), u32(height), u32(width), u16(depth), u16(mode)]);
  const records = [], images = [];
  for (const n of noLayers ? [] : flatten(layers)) {
    const layer = n.divider || n.folder ? { x: 0, y: 0, width: 0, height: 0 } : n;
    const useRle = n.rle ?? rle, planes = [];
    if (layer.width && layer.height) {
      const px = pixelsOf(layer), count = layer.width * layer.height;
      const plane = k => { const p = Buffer.alloc(count); for (let i = 0; i < count; i++) p[i] = px[i * 4 + k]; return p; };
      planes.push([-1, plane(3)], [0, plane(0)], [1, plane(1)], [2, plane(2)]);
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
  const merged = composite ?? flattenImage(width, height, layers);
  const plane = k => { const p = Buffer.alloc(width * height); for (let i = 0; i < width * height; i++) p[i] = merged[i * 4 + k]; return p; };
  const mergedPlanes = Array.from({ length: channels }, (_, k) => plane(k < 4 ? [0, 1, 2, 3][k] : 3));
  let image;
  if (rle) {
    const packed = mergedPlanes.map(p => packBits(p, width, height));
    image = Buffer.concat([u16(1), ...packed.flatMap(([counts]) => counts.map(c => psb ? u32(c) : u16(c))), ...packed.map(([, d]) => d)]);
  } else image = Buffer.concat([u16(0), ...mergedPlanes]);
  return Buffer.concat([header, u32(0), u32(0), psb ? u64(layerSection.length) : u32(layerSection.length), layerSection, image]);
}

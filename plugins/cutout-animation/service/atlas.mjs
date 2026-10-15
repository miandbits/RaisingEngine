// Export-time atlas packing of the cutout plugin (plugin 1.9.0, service hook `prepareExport`).
// Every PNG that an exported rig draws goes into one or more page images; the rig documents then
// name the page and the sub-image (attachment `atlas`), and players draw that region (capability
// plugin-atlas-v1). Pixels never change: pages hold exact copies, a 1 px transparent ring is kept
// around trimmed pictures and every cell has `padding` pixels of duplicated edge pixels, so linear
// filtering at a region's edge samples what it sampled at the separate image's edge (transparent
// neighbours or the clamped edge pixel). Images the packer cannot reproduce exactly (non-PNG,
// 16-bit, interlaced, colour-managed by gAMA/iCCP/cHRM, larger than a page) stay separate files.
import { deflateSync, inflateSync, crc32 } from 'node:zlib';

const SIGNATURE = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
export const ATLAS_DEFAULTS = Object.freeze({ enabled: true, maxPageSize: 2048, padding: 2, trim: true });
export const PAGE_SIZES = [512, 1024, 2048, 4096];

/** RGBA pixels of an 8-bit (or lower) non-interlaced PNG, or {reason} when it cannot be packed exactly. */
export function decodePng(bytes) {
  if (bytes.length < 33 || !bytes.subarray(0, 8).equals(SIGNATURE)) return { reason: 'PNG가 아닙니다' };
  let at = 8, header = null, palette = null, trns = null;
  const data = [];
  while (at + 8 <= bytes.length) {
    const length = bytes.readUInt32BE(at), type = bytes.toString('ascii', at + 4, at + 8), body = bytes.subarray(at + 8, at + 8 + length);
    if (type === 'IHDR') header = { width: body.readUInt32BE(0), height: body.readUInt32BE(4), depth: body[8], color: body[9], interlace: body[12] };
    else if (type === 'PLTE') palette = body;
    else if (type === 'tRNS') trns = body;
    else if (type === 'IDAT') data.push(body);
    // Browsers colour-correct these; a page without them would not look the same.
    else if (type === 'gAMA' || type === 'iCCP' || type === 'cHRM') return { reason: `색 보정 정보(${type})가 있는 PNG` };
    else if (type === 'IEND') break;
    at += 12 + length;
  }
  if (!header) return { reason: 'PNG 헤더가 없습니다' };
  const { width, height, depth, color, interlace } = header;
  const channels = { 0: 1, 2: 3, 3: 1, 4: 2, 6: 4 }[color];
  if (!channels || ![1, 2, 4, 8].includes(depth) || (color !== 0 && color !== 3 && depth !== 8)) return { reason: `지원하지 않는 PNG 형식(색 ${color}, ${depth}비트)` };
  if (interlace) return { reason: '인터레이스 PNG' };
  if (!width || !height || width * height > 8192 * 8192) return { reason: 'PNG 크기' };
  let raw;
  try { raw = inflateSync(Buffer.concat(data)); } catch { return { reason: '손상된 PNG' }; }
  const bits = channels * depth, bpp = Math.max(1, bits >> 3), stride = Math.ceil(width * bits / 8);
  if (raw.length < (stride + 1) * height) return { reason: '손상된 PNG' };
  const rows = Buffer.alloc(stride * height);
  let prev = Buffer.alloc(stride);
  for (let y = 0; y < height; y++) {
    const filter = raw[y * (stride + 1)], line = raw.subarray(y * (stride + 1) + 1, (y + 1) * (stride + 1)), out = rows.subarray(y * stride, (y + 1) * stride);
    if (filter > 4) return { reason: '손상된 PNG' };
    for (let i = 0; i < stride; i++) {
      const a = i >= bpp ? out[i - bpp] : 0, b = prev[i], c = i >= bpp ? prev[i - bpp] : 0;
      let predictor = 0;
      if (filter === 1) predictor = a; else if (filter === 2) predictor = b; else if (filter === 3) predictor = (a + b) >> 1;
      else if (filter === 4) { const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c); predictor = pa <= pb && pa <= pc ? a : pb <= pc ? b : c; }
      out[i] = (line[i] + predictor) & 255;
    }
    prev = out;
  }
  const rgba = Buffer.alloc(width * height * 4);
  const sample = (row, x) => depth === 8 ? row[x] : (row[(x * depth) >> 3] >> (8 - depth - ((x * depth) & 7))) & ((1 << depth) - 1);
  const scale = depth === 8 ? 1 : 255 / ((1 << depth) - 1);
  for (let y = 0; y < height; y++) {
    const row = rows.subarray(y * stride, (y + 1) * stride);
    for (let x = 0; x < width; x++) {
      const o = (y * width + x) * 4;
      if (color === 6) row.copy(rgba, o, x * 4, x * 4 + 4);
      else if (color === 2) {
        rgba[o] = row[x * 3]; rgba[o + 1] = row[x * 3 + 1]; rgba[o + 2] = row[x * 3 + 2];
        // tRNS colour key (8-bit samples are stored in the low byte of each 16-bit value).
        rgba[o + 3] = trns && trns.length >= 6 && row[x * 3] === trns[1] && row[x * 3 + 1] === trns[3] && row[x * 3 + 2] === trns[5] ? 0 : 255;
      } else if (color === 4) { rgba[o] = rgba[o + 1] = rgba[o + 2] = row[x * 2]; rgba[o + 3] = row[x * 2 + 1]; }
      else if (color === 3) {
        const i = sample(row, x);
        if (!palette || i * 3 + 2 >= palette.length) return { reason: '손상된 PNG(팔레트)' };
        rgba[o] = palette[i * 3]; rgba[o + 1] = palette[i * 3 + 1]; rgba[o + 2] = palette[i * 3 + 2]; rgba[o + 3] = trns && i < trns.length ? trns[i] : 255;
      } else {
        const v = sample(row, x);
        rgba[o] = rgba[o + 1] = rgba[o + 2] = Math.round(v * scale);
        rgba[o + 3] = trns && trns.length >= 2 && v === ((trns[0] << 8) | trns[1]) ? 0 : 255;
      }
    }
  }
  return { width, height, rgba };
}

/** 8-bit RGBA PNG (per-row adaptive filter, deflate level 9). */
export function encodePng(width, height, rgba) {
  const stride = width * 4, raw = Buffer.alloc((stride + 1) * height), candidate = Buffer.alloc(stride);
  let prev = Buffer.alloc(stride);
  for (let y = 0; y < height; y++) {
    const row = rgba.subarray(y * stride, (y + 1) * stride);
    let best = -1, bestSum = Infinity;
    for (let filter = 0; filter <= 4; filter++) {
      let sum = 0;
      for (let i = 0; i < stride; i++) {
        const a = i >= 4 ? row[i - 4] : 0, b = prev[i], c = i >= 4 ? prev[i - 4] : 0;
        let predictor = 0;
        if (filter === 1) predictor = a; else if (filter === 2) predictor = b; else if (filter === 3) predictor = (a + b) >> 1;
        else if (filter === 4) { const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c); predictor = pa <= pb && pa <= pc ? a : pb <= pc ? b : c; }
        const v = (row[i] - predictor) & 255;
        candidate[i] = v; sum += v < 128 ? v : 256 - v;
      }
      if (sum < bestSum) { bestSum = sum; best = filter; candidate.copy(raw, y * (stride + 1) + 1); }
    }
    raw[y * (stride + 1)] = best;
    prev = row;
  }
  const chunk = (type, data) => {
    const head = Buffer.alloc(8); head.writeUInt32BE(data.length, 0); head.write(type, 4, 'ascii');
    const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(Buffer.concat([head.subarray(4), data])) >>> 0, 0);
    return Buffer.concat([head, data, crc]);
  };
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(width, 0); ihdr.writeUInt32BE(height, 4); ihdr.set([8, 6, 0, 0, 0], 8);
  return Buffer.concat([SIGNATURE, chunk('IHDR', ihdr), chunk('IDAT', deflateSync(raw, { level: 9 })), chunk('IEND', Buffer.alloc(0))]);
}

/** Opaque bounds (alpha > 0) grown by one transparent pixel, inside the image; null = fully transparent. */
export function trimRect(image) {
  const { width, height, rgba } = image;
  let x0 = width, y0 = height, x1 = -1, y1 = -1;
  for (let y = 0; y < height; y++) for (let x = 0; x < width; x++)
    if (rgba[(y * width + x) * 4 + 3]) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
  if (x1 < 0) return null;
  x0 = Math.max(0, x0 - 1); y0 = Math.max(0, y0 - 1); x1 = Math.min(width - 1, x1 + 1); y1 = Math.min(height - 1, y1 + 1);
  return { x: x0, y: y0, width: x1 - x0 + 1, height: y1 - y0 + 1 };
}

/**
 * Skyline bottom-left packing of `cells` ({w, h} incl. padding) into pages of at most size × size:
 * each cell gets {page, x, y}. Pages are cropped to what they use afterwards.
 */
export function packCells(cells, size) {
  const pages = [];
  const place = (skyline, w, h) => {
    let best = null;
    for (let i = 0; i < skyline.length; i++) {
      const x = skyline[i].x;
      if (x + w > size) break;
      let y = 0, width = 0;
      for (let j = i; j < skyline.length && width < w; j++) { y = Math.max(y, skyline[j].y); width = skyline[j].x + skyline[j].w - x; }
      if (width < w || y + h > size) continue;
      if (!best || y < best.y || (y === best.y && x < best.x)) best = { x, y };
    }
    return best;
  };
  const raise = (skyline, x, y, w) => {
    const next = [];
    for (const s of skyline) {
      if (s.x + s.w <= x || s.x >= x + w) { next.push(s); continue; }
      if (s.x < x) next.push({ x: s.x, y: s.y, w: x - s.x });
      if (s.x + s.w > x + w) next.push({ x: x + w, y: s.y, w: s.x + s.w - (x + w) });
    }
    next.push({ x, y, w });
    next.sort((a, b) => a.x - b.x);
    // Merge neighbours at the same height so later wide cells find room.
    const merged = [];
    for (const s of next) { const last = merged[merged.length - 1]; if (last && last.y === s.y && last.x + last.w === s.x) last.w += s.w; else merged.push({ ...s }); }
    return merged;
  };
  for (const cell of cells) {
    let placed = false;
    for (const [index, page] of pages.entries()) {
      const at = place(page.skyline, cell.w, cell.h);
      if (!at) continue;
      page.skyline = raise(page.skyline, at.x, at.y + cell.h, cell.w);
      Object.assign(cell, { page: index, x: at.x, y: at.y });
      page.width = Math.max(page.width, at.x + cell.w); page.height = Math.max(page.height, at.y + cell.h);
      placed = true; break;
    }
    if (placed) continue;
    const page = { skyline: [{ x: 0, y: 0, w: size }], width: 0, height: 0 };
    pages.push(page);
    page.skyline = raise(page.skyline, 0, cell.h, cell.w);
    Object.assign(cell, { page: pages.length - 1, x: 0, y: 0 });
    page.width = cell.w; page.height = cell.h;
  }
  return pages.map(p => ({ width: p.width, height: p.height }));
}

/** Copies `rect` of `image` to (dx, dy) of a page and repeats its edge pixels `padding` times around it. */
function blit(page, pageWidth, image, rect, dx, dy, padding) {
  const { width, rgba } = image;
  for (let y = -padding; y < rect.height + padding; y++) {
    const sy = rect.y + Math.min(rect.height - 1, Math.max(0, y));
    for (let x = -padding; x < rect.width + padding; x++) {
      const sx = rect.x + Math.min(rect.width - 1, Math.max(0, x));
      const from = (sy * width + sx) * 4, to = ((dy + y) * pageWidth + dx + x) * 4;
      page[to] = rgba[from]; page[to + 1] = rgba[from + 1]; page[to + 2] = rgba[from + 2]; page[to + 3] = rgba[from + 3];
    }
  }
}

/** Every attachment of a rig document (default list and skins), for reading and rewriting. */
export function attachmentsOf(data) {
  const lists = [Array.isArray(data?.attachments) ? data.attachments : []];
  for (const skin of Array.isArray(data?.skins) ? data.skins : []) lists.push(Array.isArray(skin?.attachments) ? skin.attachments : []);
  return lists.flat().filter(a => a && typeof a === 'object');
}

/**
 * Packs the images the rigs draw. `images` maps asset id -> decoded image ({width, height, rgba})
 * or {reason}. Returns {documents (rigs rewritten), pages: [{index, width, height, png}], packed,
 * skipped: [{id, reason}], regions: {id: {page, x, y, width, height, trim?}}}. `pageId(index)` names
 * the page image asset. Pure: the caller reads the files and writes the pages.
 */
export function buildAtlas({ documents, images, settings, pageId }) {
  const options = { ...ATLAS_DEFAULTS, ...settings };
  const size = options.maxPageSize, padding = options.padding;
  const rigs = documents.filter(d => d.type === 'rig');
  const uses = new Map();  // image id -> attachments using it (sequence frames count as the attachment's picture)
  for (const rig of rigs) for (const a of attachmentsOf(rig.data))
    for (const id of new Set([a.imageAssetId, ...(Array.isArray(a.sequence?.frames) ? a.sequence.frames : [])])) {
      if (typeof id !== 'string' || !id) continue;
      if (!uses.has(id)) uses.set(id, []);
      uses.get(id).push(a);
    }
  const skipped = [], cells = [];
  for (const id of [...uses.keys()].sort()) {
    const image = images.get(id);
    if (!image || image.reason) { skipped.push({ id, reason: image?.reason ?? '이미지 에셋이 아닙니다' }); continue; }
    // Trimming is exact only for pictures drawn as rectangles at their own pixel size: a mesh
    // addresses its whole image through uvs, a scaled region would move the kept rectangle.
    const trimmable = options.trim && uses.get(id).every(a => a.type !== 'mesh' && a.width === image.width && a.height === image.height);
    const full = { x: 0, y: 0, width: image.width, height: image.height };
    const rect = trimmable ? trimRect(image) ?? { x: 0, y: 0, width: 1, height: 1 } : full;
    if (rect.width + 2 * padding > size || rect.height + 2 * padding > size) { skipped.push({ id, reason: `페이지(${size}px)보다 큽니다` }); continue; }
    cells.push({ id, image, rect, trimmed: rect.width !== full.width || rect.height !== full.height || rect.x !== 0 || rect.y !== 0, w: rect.width + 2 * padding, h: rect.height + 2 * padding });
  }
  // Tall first, then wide, then by id: deterministic pages for the same input.
  cells.sort((a, b) => b.h - a.h || b.w - a.w || (a.id < b.id ? -1 : 1));
  const layout = packCells(cells, size);
  const pages = layout.map((p, index) => ({ index, width: p.width, height: p.height, rgba: Buffer.alloc(p.width * p.height * 4) }));
  const regions = {};
  for (const cell of cells) {
    const page = pages[cell.page];
    blit(page.rgba, page.width, cell.image, cell.rect, cell.x + padding, cell.y + padding, padding);
    regions[cell.id] = { page: pageId(cell.page), x: cell.x + padding, y: cell.y + padding, width: cell.rect.width, height: cell.rect.height,
      ...(cell.trimmed ? { trim: { x: cell.rect.x, y: cell.rect.y, width: cell.rect.width, height: cell.rect.height } } : {}) };
  }
  const rewritten = documents.map(d => {
    if (d.type !== 'rig') return d;
    const data = structuredClone(d.data);
    for (const a of attachmentsOf(data)) {
      if (regions[a.imageAssetId]) a.atlas = structuredClone(regions[a.imageAssetId]);
      // Sequences (plugin 1.12.0): one region per frame (null = that frame keeps its own file);
      // runtimes draw a sequence only from these, frame 0 included.
      const frames = Array.isArray(a.sequence?.frames) ? a.sequence.frames : [];
      if (frames.some(id => regions[id])) a.sequence.atlas = frames.map(id => regions[id] ? structuredClone(regions[id]) : null);
    }
    return { ...d, data };
  });
  return { documents: rewritten, pages: pages.map(p => ({ index: p.index, width: p.width, height: p.height, png: encodePng(p.width, p.height, p.rgba) })),
    packed: cells.map(c => c.id).sort(), skipped, regions };
}

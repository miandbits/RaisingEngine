// Minimal layered PSD/PSB reader for the layer import (plugin 1.11.0, service op `importLayers`).
// No external dependency: header, layer records (bounds, name incl. the Unicode `luni` name,
// opacity, visibility, blend mode, clipping, section dividers = groups), per-channel image data
// (raw and PackBits RLE) and the merged composite. Only 8-bit RGB documents are read; 16/32-bit,
// CMYK, indexed, grayscale, Lab… are refused with a message saying how to re-save. Layer masks,
// clipping masks and blend modes other than normal are not applied (the importer warns).
// Every offset is bounds-checked: a truncated or hostile file is refused, never over-read.

const MODES = { 0: '비트맵', 1: '회색조', 2: '인덱스 색상', 3: 'RGB', 4: 'CMYK', 7: '다중 채널', 8: '이중톤', 9: 'Lab' };
/** Photoshop blend keys → Korean names for warnings (`norm`/`pass` are drawn exactly). */
export const BLEND_NAMES = {
  norm: '표준', pass: '통과', diss: '디졸브', dark: '어둡게 하기', 'mul ': '곱하기', idiv: '색상 번', lbrn: '선형 번', dkCl: '어두운 색상',
  lite: '밝게 하기', scrn: '스크린', 'div ': '색상 닷지', lddg: '선형 닷지', lgCl: '밝은 색상', over: '오버레이', 'sLit': '소프트 라이트',
  hLit: '하드 라이트', vLit: '선명한 라이트', lLit: '선형 라이트', pLit: '핀 라이트', hMix: '하드 혼합', diff: '차이', smud: '제외',
  fsub: '빼기', fdiv: '나누기', 'hue ': '색조', 'sat ': '채도', colr: '색상', 'lum ': '광도',
};
/** PSB additional-info keys whose length field is 8 bytes (Adobe spec, "Layer and mask information"). */
const LONG_KEYS = new Set(['LMsk', 'Lr16', 'Lr32', 'Layr', 'Mt16', 'Mt32', 'Mtrn', 'Alph', 'FMsk', 'lnk2', 'FEid', 'FXid', 'PxSD']);
export const MAX_PSD_EDGE = 8192;
const MAX_PIXELS = 64 * 1024 * 1024;

export class PsdError extends Error { constructor(message) { super(message); this.code = 'ANIM_PSD'; } }
const fail = message => { throw new PsdError(message); };

class Reader {
  constructor(bytes, psb) { this.b = bytes; this.at = 0; this.psb = psb; }
  need(n) { if (n < 0 || this.at + n > this.b.length) fail('PSD 파일이 잘렸거나 손상되었습니다.'); }
  u8() { this.need(1); return this.b[this.at++]; }
  u16() { this.need(2); const v = this.b.readUInt16BE(this.at); this.at += 2; return v; }
  i16() { this.need(2); const v = this.b.readInt16BE(this.at); this.at += 2; return v; }
  u32() { this.need(4); const v = this.b.readUInt32BE(this.at); this.at += 4; return v; }
  i32() { this.need(4); const v = this.b.readInt32BE(this.at); this.at += 4; return v; }
  u64() { this.need(8); const v = this.b.readBigUInt64BE(this.at); this.at += 8; if (v > BigInt(Number.MAX_SAFE_INTEGER)) fail('PSD 파일이 손상되었습니다(길이).'); return Number(v); }
  /** Section length: 4 bytes in PSD, 8 in PSB (where the spec says so). */
  len(long = this.psb) { return long ? this.u64() : this.u32(); }
  ascii(n) { this.need(n); const v = this.b.toString('latin1', this.at, this.at + n); this.at += n; return v; }
  slice(n) { this.need(n); const v = this.b.subarray(this.at, this.at + n); this.at += n; return v; }
  skip(n) { this.need(n); this.at += n; }
}

/** PackBits rows into `out` (width × height bytes); `counts` = compressed byte count per row. */
function unpackBits(src, at, end, out, width, height, counts) {
  for (let y = 0; y < height; y++) {
    const rowEnd = Math.min(end, at + counts[y]);
    let x = 0;
    const row = y * width;
    while (at < rowEnd && x < width) {
      const n = src[at++];
      if (n < 128) {  // literal run of n + 1 bytes
        const count = Math.min(n + 1, width - x, rowEnd - at);
        src.copy(out, row + x, at, at + count);
        at += n + 1; x += count;
      } else if (n > 128) {  // repeat the next byte 257 - n times
        if (at >= rowEnd) break;
        const value = src[at++], count = Math.min(257 - n, width - x);
        out.fill(value, row + x, row + x + count); x += count;
      }  // 128 = no-op
    }
    at = rowEnd;
  }
}

/** One channel plane (width × height bytes) from `data` = [compression u16][payload]. */
function channelPlane(data, width, height, psb, what) {
  const out = Buffer.alloc(width * height);
  if (!width || !height || data.length < 2) return out;
  const compression = data.readUInt16BE(0);
  if (compression === 0) {
    if (data.length - 2 < width * height) fail(`${what}: 채널 데이터가 잘렸습니다.`);
    data.copy(out, 0, 2, 2 + width * height);
  } else if (compression === 1) {
    const size = psb ? 4 : 2, table = 2 + height * size;
    if (data.length < table) fail(`${what}: RLE 행 길이표가 잘렸습니다.`);
    const counts = new Array(height);
    for (let y = 0; y < height; y++) counts[y] = psb ? data.readUInt32BE(2 + y * 4) : data.readUInt16BE(2 + y * 2);
    unpackBits(data, table, data.length, out, width, height, counts);
  } else fail(`${what}: ZIP 압축 채널(${compression})은 지원하지 않습니다. 8비트로 다시 저장하세요.`);
  return out;
}

/** Interleaved RGBA from planes (missing alpha = opaque). */
function interleave(width, height, r, g, b, a) {
  const px = Buffer.alloc(width * height * 4);
  for (let i = 0, o = 0; i < width * height; i++, o += 4) { px[o] = r[i]; px[o + 1] = g[i]; px[o + 2] = b[i]; px[o + 3] = a ? a[i] : 255; }
  return px;
}

/**
 * Parses a PSD (version 1) or PSB (version 2). Returns
 * `{ format, width, height, layers, composite(), warnings }` where `layers` is the group tree,
 * each list ordered bottom → top (back → front): `{ kind: 'layer'|'group', name, visible,
 * opacity (0..1), blend, clipping, mask, left, top, width, height, rgba? (layers), children? }`.
 */
export function readPsd(bytes) {
  if (!Buffer.isBuffer(bytes)) bytes = Buffer.from(bytes);
  if (bytes.length < 26 || bytes.toString('latin1', 0, 4) !== '8BPS') fail('PSD 파일이 아닙니다(8BPS 서명 없음).');
  const version = bytes.readUInt16BE(4);
  if (version !== 1 && version !== 2) fail(`지원하지 않는 PSD 버전 ${version}입니다.`);
  const psb = version === 2, r = new Reader(bytes, psb);
  r.skip(4 + 2 + 6);
  const channels = r.u16(), height = r.u32(), width = r.u32(), depth = r.u16(), mode = r.u16();
  if (depth !== 8) fail(depth === 16 || depth === 32
    ? `${depth}비트 PSD는 지원하지 않습니다. Photoshop에서 이미지 › 모드 › 8비트/채널로 바꿔 저장하세요.`
    : `${depth}비트 PSD는 지원하지 않습니다. 8비트 RGB로 저장하세요.`);
  if (mode !== 3) fail(`${MODES[mode] ?? `색상 모드 ${mode}`} PSD는 지원하지 않습니다. 이미지 › 모드 › RGB 색상으로 바꿔 저장하세요.`);
  if (!width || !height || width > MAX_PSD_EDGE || height > MAX_PSD_EDGE) fail(`캔버스 ${width}×${height}는 지원하지 않습니다(가로·세로 1..${MAX_PSD_EDGE} px).`);
  if (channels < 3) fail('RGB 채널이 없는 PSD입니다.');
  const warnings = [];
  r.skip(r.u32());  // color mode data
  r.skip(r.u32());  // image resources
  const layerSectionLength = r.len(), layerSectionEnd = r.at + layerSectionLength;
  r.need(layerSectionLength);
  let records = [], mergedAlpha = false;
  if (layerSectionLength > 0) {
    const infoLength = r.len(), infoEnd = r.at + infoLength;
    r.need(infoLength);
    if (infoLength > 0) {
      const count = r.i16();
      // A negative count: the first alpha channel of the merged image is its transparency.
      mergedAlpha = count < 0;
      for (let i = 0; i < Math.abs(count); i++) records.push(readRecord(r, psb));
      let pixels = 0;
      for (const [index, record] of records.entries()) {
        const what = `레이어 '${record.name || index + 1}'`;
        const planes = {};
        for (const channel of record.channels) {
          const data = r.slice(channel.length);
          // Masks (-2 user, -3 real user) have their own rectangle and are not applied.
          if (channel.id < -1 || channel.id > 2) continue;
          if (record.width * record.height > 0) {
            pixels += channel.id === 0 ? record.width * record.height : 0;
            if (pixels > MAX_PIXELS) fail('PSD 레이어의 전체 픽셀 수가 너무 많습니다(64메가픽셀 이하).');
            planes[channel.id] = channelPlane(data, record.width, record.height, psb, what);
          }
        }
        if (record.width * record.height > 0 && planes[0] && planes[1] && planes[2]) record.rgba = interleave(record.width, record.height, planes[0], planes[1], planes[2], planes[-1]);
      }
    }
    r.at = infoEnd;
  }
  r.at = layerSectionEnd;
  const compositeAt = r.at;
  const composite = () => {
    const c = new Reader(bytes, psb);
    c.at = compositeAt;
    const compression = c.u16(), n = Math.min(channels, mergedAlpha ? 4 : 3), planes = [];
    if (compression === 0) for (let i = 0; i < n; i++) planes.push(Buffer.from(c.slice(width * height)));
    else if (compression === 1) {
      const counts = [];
      for (let i = 0; i < channels * height; i++) counts.push(psb ? c.u32() : c.u16());
      for (let i = 0; i < n; i++) {
        const plane = Buffer.alloc(width * height), rows = counts.slice(i * height, (i + 1) * height), size = rows.reduce((s, v) => s + v, 0);
        c.need(size);
        unpackBits(bytes, c.at, c.at + size, plane, width, height, rows);
        c.at += size;
        planes.push(plane);
      }
    } else fail(`합쳐진 이미지 압축(${compression})은 지원하지 않습니다.`);
    return { width, height, rgba: interleave(width, height, planes[0], planes[1], planes[2], planes[3]) };
  };
  return { format: psb ? 'psb' : 'psd', width, height, layers: buildTree(records, warnings), composite, warnings };
}

function readRecord(r, psb) {
  const top = r.i32(), left = r.i32(), bottom = r.i32(), right = r.i32();
  const count = r.u16();
  if (count > 56) fail('PSD 레이어 채널 수가 올바르지 않습니다.');
  const channels = [];
  for (let i = 0; i < count; i++) channels.push({ id: r.i16(), length: r.len() });
  if (r.ascii(4) !== '8BIM') fail('PSD 레이어 혼합 서명이 올바르지 않습니다.');
  const blend = r.ascii(4), opacity = r.u8(), clipping = r.u8(), flags = r.u8();
  r.skip(1);
  const extraLength = r.u32(), extraEnd = r.at + extraLength;
  r.need(extraLength);
  const maskLength = r.u32();
  r.skip(maskLength);
  r.skip(r.u32());  // blending ranges
  const nameLength = r.u8();
  let name = r.ascii(nameLength);
  r.skip((4 - ((nameLength + 1) % 4)) % 4);
  let section = 0;
  while (r.at + 12 <= extraEnd) {
    const signature = r.ascii(4);
    if (signature !== '8BIM' && signature !== '8B64') break;
    const key = r.ascii(4), length = psb && LONG_KEYS.has(key) ? r.u64() : r.u32(), end = r.at + length;
    if (end > extraEnd) break;
    if (key === 'luni' && length >= 4) {
      const chars = r.u32();
      if (chars * 2 <= length - 4) { const raw = r.slice(chars * 2); name = Buffer.from(raw).swap16().toString('utf16le').replace(/\0+$/, ''); }
    } else if ((key === 'lsct' || key === 'lsdk') && length >= 4) section = r.u32();
    r.at = end;
  }
  r.at = extraEnd;
  const width = Math.max(0, right - left), height = Math.max(0, bottom - top);
  if (width > 30000 || height > 30000) fail(`레이어 '${name}'의 크기가 올바르지 않습니다.`);
  return { name, left, top, width, height, channels, blend, opacity: opacity / 255, clipping: clipping !== 0, visible: (flags & 2) === 0, mask: maskLength > 0, section };
}

/** Records are stored bottom → top; a group is [divider (3) … children … folder (1/2)]. */
function buildTree(records, warnings) {
  const root = { kind: 'group', name: '', children: [] }, stack = [root];
  for (const record of records) {
    const { channels, section, ...rest } = record;
    if (section === 3) { const group = { kind: 'group', children: [] }; stack.at(-1).children.push(group); stack.push(group); continue; }
    if (section === 1 || section === 2) {
      if (stack.length === 1) { warnings.push({ code: 'ANIM_PSD_GROUP', message: `그룹 '${record.name}'의 시작 표시가 없어 빈 그룹으로 읽었습니다.` }); continue; }
      const group = stack.pop();
      Object.assign(group, { name: rest.name, visible: rest.visible, opacity: rest.opacity, blend: rest.blend, clipping: rest.clipping, mask: rest.mask, left: 0, top: 0, width: 0, height: 0 });
      continue;
    }
    stack.at(-1).children.push({ kind: 'layer', ...rest });
  }
  if (stack.length > 1) warnings.push({ code: 'ANIM_PSD_GROUP', message: '닫히지 않은 그룹이 있어 마지막까지를 그룹으로 읽었습니다.' });
  for (const open of stack.slice(1)) Object.assign(open, { name: open.name ?? '', visible: open.visible ?? true, opacity: open.opacity ?? 1, blend: open.blend ?? 'pass', clipping: false, mask: false, left: 0, top: 0, width: 0, height: 0 });
  return root.children;
}

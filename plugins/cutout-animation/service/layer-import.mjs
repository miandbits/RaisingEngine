// Layered-art import of the cutout plugin (plugin 1.11.0): a PSD/PSB file, or a folder of PNGs
// with an optional layout.json, becomes a rig whose setup pose looks exactly like the document.
// One implementation for every path: the Qt dialog (host `pluginImport`), MCP `plugin_import` and
// the read-only `plugin_call` op all run importLayers() below. It never writes files: the host
// publishes the returned PNGs (content-hashed) and applies the documents as one undoable edit.
//
// Mapping (README "PSD 가져오기" documents the same rules for artists):
//   * group = bone (id from the group name), nested groups = child bones; the bone origin is the
//     centre of a `[pivot]` / `[bone]` / `#pivot` marker layer inside the group, else the centre
//     of the group's content. A top-level marker sets the character origin (root bone).
//   * layer = slot + attachment at its canvas position (pivot 0.5, trimmed to its pixels with a
//     1 px transparent ring); `eyes/open`, `eyes@open` → slot `eyes`, attachment `open`; the
//     layers of a `[slot]` group are attachments of one slot named after the group.
//   * draw order = layer order; hidden layers become attachments that are not shown (expression
//     variants swap them); opacity → slot alpha; `[ignore]` skips a layer or group.
//   * re-import (payload.rig): bones, keys and placements stay; changed pictures are swapped in
//     place (rig.editor.layerImport remembers where each picture came from), new layers are added.
import { createHash } from 'node:crypto';
import { readFile, readdir, stat } from 'node:fs/promises';
import path from 'node:path';
import { decodePng, encodePng } from './atlas.mjs';
import { BLEND_NAMES, MAX_PSD_EDGE, PsdError, readPsd } from './psd.mjs';

const MAX_FILE_BYTES = 512 * 1024 * 1024, MAX_FOLDER_FILES = 1024;
const LIMITS = { bones: 256, slots: 256, attachments: 1024 };
const ORIGINS = ['bottom-center', 'center', 'top-left', 'top-center'];
const ID = /^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/;
const fail = message => { throw new PsdError(message); };
const round = v => Math.round(v * 1e6) / 1e6;
const sha = bytes => createHash('sha256').update(bytes).digest('hex');
export const imageAssetId = bytes => `image-${sha(bytes).slice(0, 24)}`;

/** Rig/slot/attachment id from a layer name; non-Latin names fall back to `fallback`. */
export function toId(text, fallback) {
  let id = String(text ?? '').normalize('NFKD').replace(/[̀-ͯ]/g, '').toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '');
  if (/^[0-9]/.test(id)) id = `n${id}`;
  id = id.slice(0, 56).replace(/-+$/, '');
  return ID.test(id) ? id : fallback;
}
const unique = (id, taken) => { let out = id, n = 2; while (taken.has(out)) out = `${id.slice(0, 60)}-${n++}`; taken.add(out); return out; };

/** `[tag]`s of a name (lowercase) and the name without them. `#pivot` is the marker spelling of Spine scripts. */
function parseName(raw) {
  const tags = new Set();
  let text = String(raw ?? '').replace(/\[([A-Za-z]+)\]/g, (_, tag) => { tags.add(tag.toLowerCase()); return ''; }).trim();
  if (/^#pivot$/i.test(text)) { tags.add('pivot'); text = ''; }
  if (tags.has('skip')) tags.add('ignore');
  return { text, tags };
}

// ------------------------------------------------------------------ sources

/** `file`: .psd/.psb, a folder, a PNG inside a folder or a layout .json → {format, file, width, height, layers, warnings}. */
export async function readLayerSource(file) {
  if (typeof file !== 'string' || !path.isAbsolute(file)) fail('가져올 PSD 파일이나 PNG 폴더의 절대 경로를 지정하세요.');
  let info;
  try { info = await stat(file); } catch { fail(`파일을 찾을 수 없습니다: ${file}`); }
  const ext = path.extname(file).toLowerCase();
  if (info.isDirectory()) return readFolder(file, null);
  if (ext === '.png') return readFolder(path.dirname(file), null);
  if (ext === '.json') return readFolder(path.dirname(file), file);
  if (info.size > MAX_FILE_BYTES) fail('512 MiB 이하의 PSD 파일을 선택하세요.');
  const bytes = await readFile(file);
  const psd = readPsd(bytes);
  let layers = psd.layers;
  // A flat document (no layer records) is its merged image: one layer.
  if (!layers.length) {
    const merged = psd.composite();
    layers = [{ kind: 'layer', name: path.basename(file, path.extname(file)), visible: true, opacity: 1, blend: 'norm', clipping: false, mask: false, left: 0, top: 0, width: merged.width, height: merged.height, rgba: merged.rgba }];
  }
  return { format: psd.format, file, width: psd.width, height: psd.height, layers, warnings: psd.warnings, composite: psd.composite };
}

const natural = (a, b) => a.localeCompare(b, undefined, { numeric: true, sensitivity: 'base' });
async function pngLayer(file, name, x, y, extra = {}) {
  const info = await stat(file);
  if (info.size > 64 * 1024 * 1024) fail(`PNG가 너무 큽니다: ${path.basename(file)}`);
  const image = decodePng(await readFile(file));
  if (image.reason) fail(`PNG를 읽을 수 없습니다(${image.reason}): ${path.basename(file)}`);
  return { kind: 'layer', name, visible: true, opacity: 1, blend: 'norm', clipping: false, mask: false, left: x, top: y, width: image.width, height: image.height, rgba: image.rgba, ...extra };
}
/**
 * PNG folder: with layout.json `{width?, height?, layers: [{file, name?, x?, y?, visible?, opacity?,
 * group?}]}` (first = back, `group` "body/head" nests groups); without it every PNG (sub-folders =
 * groups) at 0,0 in natural name order, a leading number (`01-body.png`) only sorts.
 */
async function readFolder(directory, layoutFile) {
  const warnings = [];
  if (!layoutFile) { const candidate = path.join(directory, 'layout.json'); if (await stat(candidate).then(s => s.isFile(), () => false)) layoutFile = candidate; }
  let count = 0;
  const counted = () => { if (++count > MAX_FOLDER_FILES) fail(`PNG가 ${MAX_FOLDER_FILES}개를 넘습니다.`); };
  if (layoutFile) {
    let layout;
    try { layout = JSON.parse(await readFile(layoutFile, 'utf8')); } catch (error) { fail(`layout.json을 읽을 수 없습니다: ${error.message}`); }
    if (!layout || typeof layout !== 'object' || !Array.isArray(layout.layers)) fail('layout.json에는 layers 배열이 있어야 합니다.');
    const root = [], groups = new Map();
    const groupOf = spec => {
      if (spec === undefined || spec === null || spec === '') return root;
      if (typeof spec !== 'string') fail('layout.json group은 "몸/머리"처럼 / 로 나눈 문자열입니다.');
      let list = root, key = '';
      for (const part of spec.split('/').map(s => s.trim()).filter(Boolean)) {
        key += '/' + part;
        if (!groups.has(key)) { const g = { kind: 'group', name: part, visible: true, opacity: 1, blend: 'pass', clipping: false, mask: false, left: 0, top: 0, width: 0, height: 0, children: [] }; list.push(g); groups.set(key, g); }
        list = groups.get(key).children;
      }
      return list;
    };
    for (const [i, entry] of layout.layers.entries()) {
      if (!entry || typeof entry.file !== 'string') fail(`layout.json layers[${i}].file이 없습니다.`);
      const file = path.resolve(directory, entry.file), rel = path.relative(directory, file);
      if (rel.startsWith('..') || path.isAbsolute(rel) || path.extname(file).toLowerCase() !== '.png') fail(`layout.json layers[${i}].file은 폴더 안의 PNG여야 합니다: ${entry.file}`);
      for (const k of ['x', 'y']) if (entry[k] !== undefined && !Number.isFinite(entry[k])) fail(`layout.json layers[${i}].${k}는 숫자입니다.`);
      if (entry.opacity !== undefined && !(Number.isFinite(entry.opacity) && entry.opacity >= 0 && entry.opacity <= 1)) fail(`layout.json layers[${i}].opacity는 0..1입니다.`);
      counted();
      groupOf(entry.group).push(await pngLayer(file, typeof entry.name === 'string' ? entry.name : path.basename(file, '.png'), Math.round(entry.x ?? 0), Math.round(entry.y ?? 0),
        { visible: entry.visible !== false, opacity: entry.opacity ?? 1 }));
    }
    const size = extent(root);
    const width = Number.isFinite(layout.width) ? Math.round(layout.width) : size.width, height = Number.isFinite(layout.height) ? Math.round(layout.height) : size.height;
    return checkCanvas({ format: 'png-folder', file: layoutFile, width, height, layers: root, warnings });
  }
  const walk = async dir => {
    const entries = (await readdir(dir, { withFileTypes: true })).filter(e => !e.name.startsWith('.')).sort((a, b) => natural(a.name, b.name));
    const out = [];
    for (const e of entries) {
      const full = path.join(dir, e.name), name = e.name.replace(/\.png$/i, '').replace(/^\d+[\s._-]+(?=\D)/, '');
      if (e.isDirectory()) { const children = await walk(full); if (children.length) out.push({ kind: 'group', name, visible: true, opacity: 1, blend: 'pass', clipping: false, mask: false, left: 0, top: 0, width: 0, height: 0, children }); }
      else if (e.isFile() && /\.png$/i.test(e.name)) { counted(); out.push(await pngLayer(full, name, 0, 0)); }
    }
    return out;
  };
  const layers = await walk(directory);
  if (!layers.length) fail(`폴더에 PNG가 없습니다: ${directory}`);
  const size = extent(layers);
  return checkCanvas({ format: 'png-folder', file: directory, width: size.width, height: size.height, layers, warnings });
}
function extent(list) {
  let width = 1, height = 1;
  const visit = nodes => { for (const n of nodes) if (n.kind === 'group') visit(n.children); else { width = Math.max(width, n.left + n.width); height = Math.max(height, n.top + n.height); } };
  visit(list);
  return { width, height };
}
function checkCanvas(source) {
  if (!(source.width >= 1 && source.height >= 1 && source.width <= MAX_PSD_EDGE && source.height <= MAX_PSD_EDGE)) fail(`캔버스 ${source.width}×${source.height}는 지원하지 않습니다(가로·세로 1..${MAX_PSD_EDGE} px).`);
  return source;
}

// ------------------------------------------------------------------ geometry

const IDENTITY = [1, 0, 0, 1, 0, 0];
const mul = (m, n) => [m[0] * n[0] + m[2] * n[1], m[1] * n[0] + m[3] * n[1], m[0] * n[2] + m[2] * n[3], m[1] * n[2] + m[3] * n[3], m[0] * n[4] + m[2] * n[5] + m[4], m[1] * n[4] + m[3] * n[5] + m[5]];
const compose = (x, y, rotation, sx, sy) => { const r = rotation * Math.PI / 180, c = Math.cos(r), s = Math.sin(r); return [c * sx, s * sx, -s * sy, c * sy, x, y]; };
function invert(m) {
  const det = m[0] * m[3] - m[1] * m[2];
  if (Math.abs(det) < 1e-12) return null;
  const a = m[3] / det, b = -m[1] / det, c = -m[2] / det, d = m[0] / det;
  return [a, b, c, d, -(a * m[4] + c * m[5]), -(b * m[4] + d * m[5])];
}
/** Setup-pose world matrices of a rig's bones (parents first or not: resolved recursively). */
function setupWorld(rig) {
  const byId = new Map((rig.bones ?? []).map(b => [b.id, b])), world = new Map();
  const of = (id, depth = 0) => {
    if (world.has(id)) return world.get(id);
    const b = byId.get(id);
    if (!b || depth > 300) return IDENTITY;
    const local = compose(b.x ?? 0, b.y ?? 0, b.rotation ?? 0, b.scaleX ?? 1, b.scaleY ?? 1);
    const m = b.parent ? mul(of(b.parent, depth + 1), local) : local;
    world.set(id, m);
    return m;
  };
  for (const b of rig.bones ?? []) of(b.id);
  return world;
}
/** x, y, rotation, scaleX, scaleY of an affine without skew (what a placement can store). */
function decompose(m) {
  const sx = Math.hypot(m[0], m[1]), rotation = Math.atan2(m[1], m[0]) * 180 / Math.PI, sy = sx ? (m[0] * m[3] - m[1] * m[2]) / sx : 0;
  return { x: round(m[4]), y: round(m[5]), rotation: round(rotation), scaleX: round(sx), scaleY: round(sy) };
}
/** Placement fields of an attachment whose world matrix must be T(cx, cy)·S(scale) under `boneWorld`. */
function placement(boneWorld, cx, cy, scale) {
  const inv = invert(boneWorld) ?? IDENTITY, p = decompose(mul(inv, [scale, 0, 0, scale, cx, cy]));
  const out = { x: p.x, y: p.y };
  if (Math.abs(p.rotation) > 1e-9) out.rotation = p.rotation;
  if (Math.abs(p.scaleX - 1) > 1e-9) out.scaleX = p.scaleX;
  if (Math.abs(p.scaleY - 1) > 1e-9) out.scaleY = p.scaleY;
  return out;
}

/** Bounds (rig px) of a group's pictures, its own and its descendants'. */
function rectsBounds(rects, scale) {
  let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
  for (const r of rects) { x0 = Math.min(x0, r.x); y0 = Math.min(y0, r.y); x1 = Math.max(x1, r.x + r.width); y1 = Math.max(y1, r.y + r.height); }
  return Number.isFinite(x0) ? { x0: x0 * scale, y0: y0 * scale, x1: x1 * scale, y1: y1 * scale } : null;
}
const normalizeDegrees = d => { let a = ((d + 180) % 360 + 360) % 360 - 180; return Math.abs(a + 180) < 1e-9 ? 180 : a; };
/**
 * Direction and length of a new bone (plugin 1.14.0, `boneDirection: "auto"`), so physics and IK
 * stretch work on an imported rig without rig work: toward its only child bone (length = the
 * distance), else toward the centre of its pictures (length = how far they reach along that
 * line), else straight up (length = half the pictures' height). Within 4° of an axis it snaps.
 */
function boneAim(g, children) {
  const o = g.world;
  let from = 'up', dx = 0, dy = -1, length = 0;
  const b = g.bounds;
  if (children.length === 1) {
    const c = children[0].world, d = Math.hypot(c.x - o.x, c.y - o.y);
    if (d >= 1) { from = 'child'; dx = (c.x - o.x) / d; dy = (c.y - o.y) / d; length = d; }
  }
  if (from === 'up' && b) {
    const cx = (b.x0 + b.x1) / 2 - o.x, cy = (b.y0 + b.y1) / 2 - o.y, d = Math.hypot(cx, cy);
    const size = Math.max(b.x1 - b.x0, b.y1 - b.y0);
    if (d >= Math.max(1, size * 0.1)) { from = 'content'; dx = cx / d; dy = cy / d; }
  }
  let angle = Math.atan2(dy, dx) * 180 / Math.PI;
  const snapped = Math.round(angle / 90) * 90;
  if (Math.abs(angle - snapped) <= 4) angle = normalizeDegrees(snapped);
  if (from !== 'child' && b) {
    const r = angle * Math.PI / 180, ux = Math.cos(r), uy = Math.sin(r);
    // The farthest picture corner along the bone, measured from the origin.
    length = Math.max(0, ...[[b.x0, b.y0], [b.x1, b.y0], [b.x0, b.y1], [b.x1, b.y1]].map(([x, y]) => (x - o.x) * ux + (y - o.y) * uy));
  } else if (from === 'child') {
    const r = angle * Math.PI / 180;
    length = Math.abs((children[0].world.x - o.x) * Math.cos(r) + (children[0].world.y - o.y) * Math.sin(r));
  }
  return { angle, length, from };
}

/** Pixel bounds (alpha > 0) of an RGBA layer, or null when fully transparent. */
function alphaBounds(width, height, rgba) {
  let x0 = width, y0 = height, x1 = -1, y1 = -1;
  for (let y = 0; y < height; y++) {
    const row = y * width * 4;
    for (let x = 0; x < width; x++) if (rgba[row + x * 4 + 3]) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; y1 = y; }
  }
  return x1 < 0 ? null : { x: x0, y: y0, width: x1 - x0 + 1, height: y1 - y0 + 1 };
}
/** The picture of a layer: trimmed + a 1 px transparent ring (clean edges under linear filtering), or the whole layer. */
function cutPicture(layer, trim) {
  if (!layer.rgba || !layer.width || !layer.height) return null;
  const b = alphaBounds(layer.width, layer.height, layer.rgba);
  if (!b) return null;
  const ring = trim ? 1 : 0, src = trim ? b : { x: 0, y: 0, width: layer.width, height: layer.height };
  const width = src.width + 2 * ring, height = src.height + 2 * ring, rgba = Buffer.alloc(width * height * 4);
  for (let y = 0; y < src.height; y++) layer.rgba.copy(rgba, ((y + ring) * width + ring) * 4, ((src.y + y) * layer.width + src.x) * 4, ((src.y + y) * layer.width + src.x + src.width) * 4);
  if (width > MAX_PSD_EDGE || height > MAX_PSD_EDGE) fail(`레이어 '${layer.name}'의 그림이 ${MAX_PSD_EDGE} px보다 큽니다.`);
  return { x: layer.left + src.x - ring, y: layer.top + src.y - ring, width, height, rgba };
}

// ------------------------------------------------------------------ payload

/** Checked import options of a payload (shared by preview and import). */
export function importOptions(payload = {}) {
  const p = payload && typeof payload === 'object' ? payload : {};
  const origin = p.origin ?? 'bottom-center';
  if (!(ORIGINS.includes(origin) || (origin && typeof origin === 'object' && Number.isFinite(origin.x) && Number.isFinite(origin.y))))
    fail(`origin은 ${ORIGINS.join(', ')} 또는 {x, y}(PSD 픽셀)입니다.`);
  const scale = p.scale ?? 1;
  if (!(Number.isFinite(scale) && scale >= 0.05 && scale <= 4)) fail('scale은 0.05..4 사이 숫자입니다.');
  if (p.trim !== undefined && typeof p.trim !== 'boolean') fail('trim은 true/false입니다.');
  if (p.exclude !== undefined && !(Array.isArray(p.exclude) && p.exclude.every(k => typeof k === 'string'))) fail('exclude는 레이어 키 문자열 배열입니다.');
  for (const k of ['id', 'rig', 'name']) if (p[k] !== undefined && typeof p[k] !== 'string') fail(`${k}는 문자열입니다.`);
  const boneDirection = p.boneDirection ?? 'auto';
  if (!['auto', 'none'].includes(boneDirection)) fail('boneDirection은 auto 또는 none입니다.');
  return { origin, scale, trim: p.trim !== false, exclude: new Set(p.exclude ?? []), boneDirection };
}

// ------------------------------------------------------------------ mapping

/**
 * Plans the import of `source` (readLayerSource) into a new rig (`existing` null) or onto an
 * existing rig. Returns {rig, images: [{id, bytes}] (none when dryRun), report}.
 */
export function planImport({ source, options, existing = null, rigName = '', dryRun = false }) {
  const warnings = [];
  const warn = (code, message, layer) => warnings.push({ code, message, ...(layer ? { layer } : {}) });
  for (const w of source.warnings ?? []) warn(w.code, w.message);
  const updating = !!existing;
  const prov = updating && existing.editor?.layerImport && typeof existing.editor.layerImport === 'object' ? existing.editor.layerImport : null;
  const scale = updating && Number.isFinite(prov?.scale) ? prov.scale : options.scale;
  if (updating && Number.isFinite(prov?.scale) && prov.scale !== options.scale) warn('ANIM_IMPORT_SCALE', `처음 가져올 때의 배율 ${prov.scale}을(를) 그대로 씁니다.`);

  // Pass 1: the report tree + groups (future bones) + pictures, in document order (back → front).
  const groups = [], pictures = [], markers = [];
  let order = 0;
  const visit = (nodes, ctx, prefix) => nodes.map((node, index) => {
    const key = prefix ? `${prefix}.${index}` : String(index), { text, tags } = parseName(node.name);
    const visible = ctx.visible && node.visible !== false, opacity = ctx.opacity * (node.opacity ?? 1);
    const item = { key, name: node.name ?? '', kind: node.kind, visible: node.visible !== false, opacity: round(node.opacity ?? 1), blend: node.blend ?? 'norm', include: !options.exclude.has(key) && ctx.include, warnings: [] };
    const note = (code, message) => { item.warnings.push(message); warn(code, message, key); };
    if (node.kind === 'layer') item.bounds = { x: node.left, y: node.top, width: node.width, height: node.height };
    if (tags.has('ignore')) { item.role = 'ignored'; item.include = false; if (node.kind === 'group') item.children = visit(node.children, { ...ctx, include: false }, key); return item; }
    if (!item.include) { item.role = 'excluded'; if (node.kind === 'group') item.children = visit(node.children, { ...ctx, include: false }, key); return item; }
    if (node.blend && !['norm', 'pass'].includes(node.blend)) note('ANIM_IMPORT_BLEND', `'${node.name}': 혼합 모드 ${BLEND_NAMES[node.blend] ?? node.blend}는 지원하지 않아 표준으로 가져옵니다.`);
    if (node.clipping) note('ANIM_IMPORT_CLIPPING', `'${node.name}': 클리핑 마스크는 적용되지 않습니다(아래 레이어에 잘리지 않음).`);
    if (node.mask) note('ANIM_IMPORT_MASK', `'${node.name}': 레이어 마스크는 무시됩니다. 마스크를 적용(병합)한 뒤 다시 가져오세요.`);
    if (node.kind === 'group') {
      if (tags.has('slot') && !ctx.slotGroup) {
        item.role = 'slot-group';
        item.children = visit(node.children, { ...ctx, visible, opacity, slotGroup: { text: text || node.name, key } }, key);
        return item;
      }
      if (ctx.slotGroup) note('ANIM_IMPORT_NESTED_SLOT', `'${node.name}': [slot] 그룹 안의 그룹은 같은 슬롯의 그림으로 가져옵니다.`);
      if (ctx.slotGroup) { item.role = 'group-in-slot'; item.children = visit(node.children, { ...ctx, visible, opacity }, key); return item; }
      if ((node.opacity ?? 1) < 1) note('ANIM_IMPORT_GROUP_OPACITY', `'${node.name}': 그룹 불투명도는 레이어마다 곱해 적용합니다(겹친 부분은 조금 다를 수 있음).`);
      const group = { key, text, name: node.name ?? '', parent: ctx.group, rects: [], marker: null, item };
      groups.push(group);
      item.role = 'bone';
      item.children = visit(node.children, { ...ctx, visible, opacity, group }, key);
      return item;
    }
    if (tags.has('pivot') || tags.has('bone')) {
      item.role = 'pivot';
      const b = node.rgba ? alphaBounds(node.width, node.height, node.rgba) : null, r = b ? { x: node.left + b.x, y: node.top + b.y, width: b.width, height: b.height } : { x: node.left, y: node.top, width: node.width, height: node.height };
      const at = { x: r.x + r.width / 2, y: r.y + r.height / 2 };
      if (ctx.group) { if (ctx.group.marker) note('ANIM_IMPORT_PIVOT', `'${node.name}': 그룹에 회전 중심 표시가 둘 이상입니다. 첫째만 씁니다.`); else ctx.group.marker = at; }
      else markers.push(at);
      return item;
    }
    const picture = cutPicture(node, options.trim);
    if (!picture) { item.role = 'empty'; note('ANIM_IMPORT_EMPTY', `'${node.name}': 비어 있는(투명한) 레이어라 건너뜁니다.`); return item; }
    item.role = 'image';
    let slotText, attText, plain = false;
    if (ctx.slotGroup) { slotText = ctx.slotGroup.text; attText = text || node.name; }
    else {
      const cut = Math.max(text.lastIndexOf('/'), text.lastIndexOf('@'));
      if (cut > 0 && cut < text.length - 1) { slotText = text.slice(0, cut); attText = text.slice(cut + 1); } else { slotText = attText = text; plain = true; }
    }
    for (let g = ctx.group; g; g = g.parent) g.rects.push(picture);
    pictures.push({ item, key, picture, visible, opacity, group: ctx.group, slotText, attText, plain, order: order++, name: node.name ?? '' });
    return item;
  });
  const layers = visit(source.layers, { visible: true, opacity: 1, include: true, group: null, slotGroup: null }, '');

  // Canvas and origin (rig space = PSD px × scale).
  const canvasW = round(source.width * scale), canvasH = round(source.height * scale);
  if (!updating && (canvasW > MAX_PSD_EDGE || canvasH > MAX_PSD_EDGE)) fail(`배율 ${scale}을(를) 적용한 캔버스가 ${MAX_PSD_EDGE} px보다 큽니다.`);
  let origin;
  const o = options.origin;
  if (markers.length) { origin = { x: markers[0].x * scale, y: markers[0].y * scale }; if (markers.length > 1) warn('ANIM_IMPORT_PIVOT', '맨 위 단계의 회전 중심 표시가 둘 이상입니다. 첫째만 원점으로 씁니다.'); }
  else if (o === 'center') origin = { x: canvasW / 2, y: canvasH / 2 };
  else if (o === 'top-left') origin = { x: 0, y: 0 };
  else if (o === 'top-center') origin = { x: canvasW / 2, y: 0 };
  else if (typeof o === 'object') origin = { x: o.x * scale, y: o.y * scale };
  else origin = { x: canvasW / 2, y: canvasH };
  origin = { x: round(origin.x), y: round(origin.y) };

  const rig = updating ? structuredClone(existing) : {
    schemaVersion: 1, name: (rigName || path.basename(source.file, path.extname(source.file))).slice(0, 80),
    canvas: { width: canvasW, height: canvasH, originX: origin.x, originY: origin.y },
    bones: [{ id: 'root', parent: null, x: origin.x, y: origin.y }], slots: [], attachments: [],
    clips: [{ id: 'idle', name: '대기', durationMs: 1000, loop: true, tracks: [] }], mix: { defaultMs: 150, pairs: [] },
  };
  rig.bones ??= []; rig.slots ??= []; rig.attachments ??= [];
  if (updating && (Math.abs(canvasW - rig.canvas?.width) > 1e-6 || Math.abs(canvasH - rig.canvas?.height) > 1e-6))
    warn('ANIM_IMPORT_CANVAS', `가져온 캔버스 ${canvasW}×${canvasH}가 리그 캔버스 ${rig.canvas?.width}×${rig.canvas?.height}와 다릅니다. 리그 캔버스를 그대로 둡니다.`);
  const changes = { bones: { added: [], kept: [] }, slots: { added: [], kept: [] }, attachments: { added: [], updated: [], unchanged: [], skipped: [], missing: [] } };

  // Bones: every content group, ids from names (made unique per document so a re-import maps the same).
  const rootId = updating ? (rig.bones.find(b => b.id === 'root' && !b.parent)?.id ?? rig.bones.find(b => !b.parent)?.id) : 'root';
  if (!rootId) fail('리그에 루트 본이 없습니다.');
  const world = setupWorld(rig);
  const boneIds = new Set([rootId]), existingBones = new Set(rig.bones.map(b => b.id));
  let boneN = 0;
  const made = [];
  for (const g of groups) {
    if (!g.rects.length && !g.marker) { g.item.role = 'empty-group'; g.item.warnings.push('그림이 없는 그룹이라 본을 만들지 않습니다.'); continue; }
    g.id = unique(toId(g.text, null) ?? `bone-${++boneN}`, boneIds);
    let parent = g.parent;
    while (parent && !parent.id) parent = parent.parent;
    g.parentId = parent ? parent.id : rootId;
    g.bounds = rectsBounds(g.rects, scale);
    const at = g.marker ? { x: g.marker.x * scale, y: g.marker.y * scale } : { x: (g.bounds.x0 + g.bounds.x1) / 2, y: (g.bounds.y0 + g.bounds.y1) / 2 };
    g.world = { x: round(at.x), y: round(at.y) };
    g.item.bone = g.id;
    if (existingBones.has(g.id)) { changes.bones.kept.push(g.id); g.item.status = 'kept'; continue; }
    existingBones.add(g.id);
    made.push(g);
  }
  // New bones only (groups come parents first): kept bones keep their rig work on re-import.
  for (const g of made) {
    const parentWorld = world.get(g.parentId) ?? IDENTITY, inv = invert(parentWorld) ?? IDENTITY;
    const x = inv[0] * g.world.x + inv[2] * g.world.y + inv[4], y = inv[1] * g.world.x + inv[3] * g.world.y + inv[5];
    const bone = { id: g.id, parent: g.parentId, x: round(x), y: round(y) };
    let rotation = 0;
    if (options.boneDirection === 'auto') {
      const aim = boneAim(g, groups.filter(c => c.id && c.parentId === g.id));
      // Local rotation = the aim's screen angle minus the parent's (a parent may be rotated too).
      rotation = round(normalizeDegrees(aim.angle - Math.atan2(parentWorld[1], parentWorld[0]) * 180 / Math.PI));
      if (rotation !== 0) bone.rotation = rotation;
      const sx = Math.hypot(parentWorld[0], parentWorld[1]) || 1;
      if (aim.length > 0) bone.length = Math.round(aim.length / sx * 100) / 100;
      g.item.direction = { angle: round(aim.angle), length: bone.length ?? 0, from: aim.from };
    }
    rig.bones.push(bone);
    world.set(g.id, mul(parentWorld, compose(x, y, rotation, 1, 1)));
    changes.bones.added.push(g.id);
    g.item.status = 'add';
  }
  if (rig.bones.length > LIMITS.bones) fail(`본이 ${LIMITS.bones}개를 넘습니다(${rig.bones.length}개). 그룹을 줄이거나 [slot] 그룹을 쓰세요.`);

  // Slots: (bone, slot name) → one slot; the same name under another bone gets the bone prefix.
  const slotByKey = new Map(), slotIds = new Map(rig.slots.map(s => [s.id, s.bone])), planned = new Map();
  let slotN = 0;
  for (const pic of pictures) {
    let g = pic.group;
    while (g && !g.id) g = g.parent;
    // `slot/name` layers and [slot] groups share a slot; every plain layer is a slot of its own
    // (two layers both called "arm" both stay visible). An existing slot id is reused on re-import.
    const bone = g ? g.id : rootId, key = pic.plain ? `${bone}\n${pic.slotText}\n#${pic.order}` : `${bone}\n${pic.slotText}`;
    let slot = slotByKey.get(key);
    if (!slot) {
      const base = toId(pic.slotText, null) ?? `part-${++slotN}`;
      let id = base;
      // A clash with a slot of another bone takes the bone's name (left/arm → left-arm), else a number.
      if (planned.has(id) && planned.get(id).bone !== bone) id = toId(`${bone}-${base}`, base);
      if (planned.has(id)) { let n = 2; while (planned.has(`${id}-${n}`)) n++; id = `${id}-${n}`; }
      slot = { id, bone: updating && slotIds.has(id) ? slotIds.get(id) : bone, pictures: [], names: new Set() };
      slotByKey.set(key, slot); planned.set(id, slot);
    }
    pic.slot = slot;
    // A plain layer's attachment is named like its slot (`arm` → arm/arm, also for fallback ids).
    pic.attachment = unique(pic.plain ? slot.id : toId(pic.attText, `image-${slot.pictures.length + 1}`), slot.names);
    slot.pictures.push(pic);
  }
  for (const slot of planned.values()) {
    const shown = slot.pictures.filter(p => p.visible);
    if (shown.length > 1) warn('ANIM_IMPORT_SLOT_VISIBLE', `슬롯 '${slot.id}'에 보이는 레이어가 ${shown.length}개입니다. 맨 위 '${shown.at(-1).name}'만 보이고 나머지는 숨긴 첨부가 됩니다.`);
    slot.setup = shown.at(-1) ?? null;
    slot.order = (slot.setup ?? slot.pictures[0]).order;
    for (const p of slot.pictures) { p.item.slot = slot.id; p.item.attachment = p.attachment; p.item.bone = slot.bone; p.item.setup = p === slot.setup; }
  }
  const slots = [...planned.values()].sort((a, b) => a.order - b.order);

  // Pictures → images and attachments.
  const images = new Map(), provLayers = { ...(prov?.layers && typeof prov.layers === 'object' ? prov.layers : {}) }, seen = new Set();
  const imageOf = pic => {
    if (pic.assetId) return pic.assetId;
    const png = encodePng(pic.picture.width, pic.picture.height, pic.picture.rgba), id = imageAssetId(png);
    if (!images.has(id)) images.set(id, png);
    return pic.assetId = id;
  };
  for (const slot of slots) {
    const current = rig.slots.find(s => s.id === slot.id);
    const boneWorld = world.get(slot.bone) ?? IDENTITY;
    for (const pic of slot.pictures) {
      const r = pic.picture, pixels = sha(r.rgba).slice(0, 32), ref = `${slot.id}/${pic.attachment}`;
      seen.add(ref);
      const record = { x: r.x, y: r.y, width: r.width, height: r.height, pixels };
      const at = rig.attachments.findIndex(a => a.slot === slot.id && a.name === pic.attachment);
      if (at < 0) {
        const attachment = { slot: slot.id, name: pic.attachment, imageAssetId: dryRun ? '' : imageOf(pic), width: r.width, height: r.height,
          ...placement(boneWorld, (r.x + r.width / 2) * scale, (r.y + r.height / 2) * scale, scale) };
        rig.attachments.push(attachment);
        changes.attachments.added.push(ref); pic.item.status = 'add';
        provLayers[ref] = record;
        continue;
      }
      const old = rig.attachments[at], before = provLayers[ref];
      if (before && before.pixels === pixels && before.width === r.width && before.height === r.height && before.x === r.x && before.y === r.y) {
        changes.attachments.unchanged.push(ref); pic.item.status = 'same'; continue;
      }
      if (old.type === 'mesh') {
        const moved = before ? before.x !== r.x || before.y !== r.y : false;
        if (old.width !== r.width || old.height !== r.height || moved) {
          changes.attachments.skipped.push(ref); pic.item.status = 'skip';
          warn('ANIM_IMPORT_MESH', `메시 첨부 '${ref}'는 그림 크기나 위치가 바뀌어 바꾸지 않았습니다. 메시를 영역으로 되돌리거나 같은 크기로 그려 주세요.`, pic.key);
          continue;
        }
        const next = { ...old, imageAssetId: dryRun ? old.imageAssetId : imageOf(pic) };
        if (!dryRun && next.imageAssetId === old.imageAssetId) { changes.attachments.unchanged.push(ref); pic.item.status = 'same'; provLayers[ref] = record; continue; }
        rig.attachments[at] = next; changes.attachments.updated.push(ref); pic.item.status = 'update'; provLayers[ref] = record;
        continue;
      }
      // Region: keep the placement (rig work) and move the pivot so the same picture content stays
      // under (x, y): pivot' · size' = pivot · size − d, d = how far the picture's corner moved.
      const next = { ...old, width: r.width, height: r.height };
      if (before) {
        const px = ((old.pivotX ?? 0.5) * old.width - (r.x - before.x)) / r.width, py = ((old.pivotY ?? 0.5) * old.height - (r.y - before.y)) / r.height;
        if (Math.abs(px) <= 4 && Math.abs(py) <= 4) {
          // 1e-12, not 1e-6: pivot × size must land the picture within 1e-6 px of where the PSD has it.
          if (Math.abs(px - 0.5) > 1e-12) next.pivotX = Math.round(px * 1e12) / 1e12; else delete next.pivotX;
          if (Math.abs(py - 0.5) > 1e-12) next.pivotY = Math.round(py * 1e12) / 1e12; else delete next.pivotY;
        } else warn('ANIM_IMPORT_PLACEMENT', `'${ref}'의 그림이 너무 많이 옮겨져 회전 중심 비율을 그대로 둡니다.`, pic.key);
      } else warn('ANIM_IMPORT_PLACEMENT', `'${ref}'는 이전 가져오기 기록이 없어 배치를 그대로 두고 그림만 바꿉니다.`, pic.key);
      if (!dryRun) next.imageAssetId = imageOf(pic);
      if (!dryRun && next.imageAssetId === old.imageAssetId && next.width === old.width && next.height === old.height) { changes.attachments.unchanged.push(ref); pic.item.status = 'same'; provLayers[ref] = record; continue; }
      rig.attachments[at] = next; changes.attachments.updated.push(ref); pic.item.status = 'update'; provLayers[ref] = record;
    }
    if (current) { changes.slots.kept.push(slot.id); continue; }
    const entry = { id: slot.id, bone: slot.bone, attachment: slot.setup ? slot.setup.attachment : null };
    const alpha = slot.setup ? slot.setup.opacity : 1;
    if (alpha < 1) entry.alpha = round(alpha);
    // Draw order: after the nearest earlier imported slot already in the rig (else before the next one).
    const index = slots.indexOf(slot);
    let insert = -1;
    for (let i = index - 1; i >= 0 && insert < 0; i--) { const at = rig.slots.findIndex(s => s.id === slots[i].id); if (at >= 0) insert = at + 1; }
    for (let i = index + 1; i < slots.length && insert < 0; i++) { const at = rig.slots.findIndex(s => s.id === slots[i].id); if (at >= 0) insert = at; }
    if (insert < 0) insert = rig.slots.length;
    rig.slots.splice(insert, 0, entry);
    changes.slots.added.push(slot.id);
  }
  for (const ref of Object.keys(provLayers)) if (!seen.has(ref) && rig.attachments.some(a => `${a.slot}/${a.name}` === ref)) changes.attachments.missing.push(ref);
  if (rig.slots.length > LIMITS.slots) fail(`슬롯이 ${LIMITS.slots}개를 넘습니다(${rig.slots.length}개). eyes/open처럼 같은 슬롯으로 묶으세요.`);
  if (rig.attachments.length > LIMITS.attachments) fail(`첨부가 ${LIMITS.attachments}개를 넘습니다(${rig.attachments.length}개).`);
  if (!pictures.length) fail('가져올 그림 레이어가 없습니다(모두 빠졌거나 비어 있음).');

  // Provenance: editor-only metadata (runtimes ignore `editor`) that lets a re-import keep placements.
  rig.editor = { ...(rig.editor && typeof rig.editor === 'object' ? rig.editor : {}),
    layerImport: { source: path.basename(source.file), format: source.format, width: source.width, height: source.height, scale, layers: provLayers } };
  const hidden = pictures.filter(p => p.slot.setup !== p).length;
  const report = {
    source: { format: source.format, file: path.basename(source.file), width: source.width, height: source.height },
    options: { origin, scale, trim: options.trim, exclude: [...options.exclude], boneDirection: options.boneDirection },
    canvas: rig.canvas, layers, warnings, changes,
    summary: { bones: changes.bones.added.length, slots: slots.length, attachments: pictures.length, hidden, images: dryRun ? null : images.size,
      added: changes.attachments.added.length, updated: changes.attachments.updated.length, unchanged: changes.attachments.unchanged.length },
  };
  return { rig, images: dryRun ? [] : [...images].map(([id, bytes]) => ({ id, bytes })), report };
}

/**
 * The service op: payload {file, id? | rig?, name?, origin?, trim?, scale?, exclude?} against
 * the plugin's `documents` ([{type, id, data}]). Returns {documents, images, report}.
 */
export async function importLayers({ payload, documents = [], dryRun = false, validateRig }) {
  const options = importOptions(payload);
  const rigs = documents.filter(d => d.type === 'rig'), controllers = documents.filter(d => d.type === 'controller');
  const source = await readLayerSource(payload?.file);
  let target = null, id;
  if (payload.rig !== undefined) {
    target = rigs.find(d => d.id === payload.rig);
    if (!target) fail(`갱신할 리그 '${payload.rig}'가 없습니다.`);
    id = target.id;
  } else {
    id = payload.id ?? toId(path.basename(source.file, path.extname(source.file)), 'character');
    if (!ID.test(id) || id.length > 64) fail(`캐릭터 ID '${id}'는 영문 소문자로 시작하고 소문자·숫자·하이픈만 쓸 수 있습니다.`);
    if (rigs.some(d => d.id === id) || controllers.some(d => d.id === id)) fail(`이미 있는 캐릭터 ID입니다: ${id}. 그 캐릭터를 갱신하려면 rig: "${id}"로 가져오세요.`);
  }
  const plan = planImport({ source, options, existing: target ? target.data : null, rigName: payload.name, dryRun });
  // The same core validation as every rig edit (a preview has no image ids yet: not checked).
  if (validateRig && !dryRun) {
    const errors = (validateRig(plan.rig) ?? []).filter(d => d.severity !== 'warning');
    if (errors.length) fail(`가져온 리그가 올바르지 않습니다: ${errors.slice(0, 3).map(d => `${d.pointer} ${d.message}`).join('; ')}`);
  }
  const out = [{ type: 'rig', id, data: plan.rig }];
  if (!target) out.push({ type: 'controller', id, data: { schemaVersion: 1, rig: id, defaultClip: 'idle', layers: [], bindings: [] } });
  return { documents: out, images: plan.images, report: { mode: target ? 'update' : 'create', rigId: id, controllerId: target ? null : id, ...plan.report } };
}

// Layered-art import of the cutout plugin (plugin 1.11.0): a PSD/PSB file, or a folder of PNGs
// with an optional layout.json, becomes a rig whose setup pose looks exactly like the document.
// One implementation for every path: the Qt dialog (host `pluginImport`), MCP `plugin_import` and
// the read-only `plugin_call` op all run importLayers() below. It never writes files: the host
// publishes the returned PNGs (content-hashed) and applies the documents as one undoable edit.
//
// Mapping (README "PSD 가져오기" documents the same rules for artists):
//   * group = bone (id from the group name), nested groups = child bones; the bone origin is the
//     centre of a `[pivot]` / `[bone]` / `#pivot` marker layer inside the group, else (plugin
//     1.20.0, boneDirection "auto") the joint guessed at the edge facing the parent (jointAim),
//     else the centre of the group's content. A top-level marker sets the character origin (root bone).
//   * layer = slot + attachment at its canvas position (pivot 0.5, trimmed to its pixels with a
//     1 px transparent ring); `eyes/open`, `eyes@open` → slot `eyes`, attachment `open`; the
//     layers of a `[slot]` group are attachments of one slot named after the group.
//   * draw order = layer order; hidden layers become attachments that are not shown (expression
//     variants swap them); opacity → slot alpha; `[ignore]` skips a layer or group.
//   * re-import (payload.rig): bones, keys and placements stay; changed pictures are swapped in
//     place (rig.editor.layerImport remembers where each picture came from), new layers are added.
//   * production files (plugin 1.15.0): `scale: "auto"` and pictures downscaled while reading
//     (psd.mjs), clipping groups baked into their base, `[merge]` / `merge` groups flattened into
//     one picture (compose.mjs), `effects: "skip"`, Cyrillic/Greek names transliterated, and a
//     preview that reports (and proposes merges for) plans over the rig limits instead of failing.
//   * hit areas and anchors (plugin 1.17.0): a `[box]` layer becomes a bounding box (slot
//     `<name>-box`, attachment `<name>`) outlining its pixels instead of a picture, a `[point]`
//     layer a point (slot `<name>-point`) at the centre of its pixels; neither moves bone centres.
//   * clipping onto a group (plugin 1.19.1): layers clipped to a group that stays bones / slots are
//     painted into the group's pictures, hidden variants included (compose.mjs paintClippedGroup).
import { createHash } from 'node:crypto';
import { readFile, readdir, stat } from 'node:fs/promises';
import path from 'node:path';
import { decodePng, encodePng } from './atlas.mjs';
import { BAKED_BLENDS, paintClippedGroup, Surface } from './compose.mjs';
import { BLEND_NAMES, MAX_PIXELS, MAX_PSD_EDGE, PsdError, readPsd, resampleRgba, scaledRect } from './psd.mjs';

const MAX_FILE_BYTES = 512 * 1024 * 1024, MAX_FOLDER_FILES = 1024;
const LIMITS = { bones: 256, slots: 256, attachments: 1024 };
const ORIGINS = ['bottom-center', 'center', 'top-left', 'top-center'];
/** `scale: "auto"`: the largest step whose canvas long edge ≤ AUTO_EDGE and pictures ≤ MAX_PIXELS. */
export const SCALE_STEPS = [1, 0.75, 0.5, 0.375, 0.25, 0.125];
const AUTO_EDGE = 2048;
const ID = /^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$/;
const fail = message => { throw new PsdError(message); };
const round = v => Math.round(v * 1e6) / 1e6;
const sha = bytes => createHash('sha256').update(bytes).digest('hex');
export const imageAssetId = bytes => `image-${sha(bytes).slice(0, 24)}`;
const normalBlend = blend => !blend || blend === 'norm' || blend === 'pass';

// Russian + Ukrainian Cyrillic (Russian readings for the shared letters) and Greek → Latin, so
// "рука левая" becomes `ruka-levaya` instead of a numbered fallback. Applied before the accent
// stripping (NFKD would turn й into и + breve).
const TRANSLIT = Object.fromEntries([
  ...'а a б b в v г g д d е e ё yo ж zh з z и i й y к k л l м m н n о o п p р r с s т t у u ф f х kh ц ts ч ch ш sh щ shch ъ _ ы y ь _ э e ю yu я ya є ye і i ї yi ґ g ў u'
    .split(' ').reduce((pairs, v, i, all) => (i % 2 ? pairs : [...pairs, [v, all[i + 1] === '_' ? '' : all[i + 1]]]), []),
  ...'α a β v γ g δ d ε e ζ z η i θ th ι i κ k λ l μ m ν n ξ x ο o π p ρ r σ s ς s τ t υ y φ f χ ch ψ ps ω o ά a έ e ή i ί i ό o ύ y ώ o ϊ i ϋ y ΐ i ΰ y'
    .split(' ').reduce((pairs, v, i, all) => (i % 2 ? pairs : [...pairs, [v, all[i + 1]]]), []),
]);
const transliterate = text => text.normalize('NFC').toLowerCase().replace(/[\u0370-\u03ff\u0400-\u04ff]/g, ch => TRANSLIT[ch] ?? ch);

/** Rig/slot/attachment id from a layer name (Cyrillic and Greek transliterated); other non-Latin names fall back to `fallback`. */
export function toId(text, fallback) {
  let id = transliterate(String(text ?? '')).normalize('NFKD').replace(/[̀-ͯ]/g, '').toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '');
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
  // Records only: planImport decodes each layer it needs, at the import resolution (plugin 1.15.0).
  const psd = readPsd(bytes, { pixels: false });
  let layers = psd.layers, decodeLayer = psd.decodeLayer;
  // A flat document (no layer records) is its merged image: one layer.
  if (!layers.length) {
    const merged = psd.composite();
    layers = [{ kind: 'layer', name: path.basename(file, path.extname(file)), visible: true, opacity: 1, blend: 'norm', clipping: false, mask: false, left: 0, top: 0, width: merged.width, height: merged.height, rgba: merged.rgba }];
    decodeLayer = undefined;
  }
  return { format: psd.format, file, width: psd.width, height: psd.height, layers, warnings: psd.warnings, composite: psd.composite, decodeLayer };
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

/** Joint guess thresholds (1.20.0): see jointAim. */
const JOINT_MIN = 0.15, JOINT_LONG = 1.75, JOINT_INSET = 0.03, JOINT_ALPHA = 128;
/**
 * The joint of a group without a marker (plugin 1.20.0, `boneDirection: "auto"`): hanging parts
 * (hair strands, sleeves, legs, tails, earrings) attach at an edge, so they turn around that edge
 * instead of their middle. c = centre of the group's content bounds, p = `parentAt` (the parent
 * bone's origin: its marker, its own joint or its content centre; the document's content centre
 * for a top-level group). Returns null (the centre origin stays) when |p − c| < 15 % of the
 * larger bounds side: the group sits on its parent (a torso, a face). Else the joint J is
 *   * a tall part (height ≥ 1.75 × width): on its top or bottom end, at the middle of the first
 *     pixel row from that end with alpha ≥ 128 in the group's pictures. The end facing p when p
 *     lies above or below the part; the top end when p lies within its height (a long strand,
 *     sleeve or leg reaching past its parent's origin hangs from its top in the setup pose);
 *   * a wide part (width ≥ 1.75 × height) with p left or right of it: likewise on the end facing
 *     p, at the middle of the first opaque column;
 *   * otherwise where the ray from c toward p (snapped within 4° of an axis) leaves the content
 *     bounds (never past p), moved back along it to the first pixel with alpha ≥ 128 (a slanted
 *     part does not start in empty corner space): a head above the body gets its neck.
 * The bone points from J through c (snapped within 4° like boneAim) and J moves 3 % of the group's
 * extent along that axis inward, so it sits inside the art; length = the farthest bounds corner
 * along the axis. Pictures are sampled in picture px (rig px / k).
 */
function jointAim(g, parentAt, k) {
  const b = g.bounds;
  if (!b || !parentAt) return null;
  const w = b.x1 - b.x0, h = b.y1 - b.y0, cx = (b.x0 + b.x1) / 2, cy = (b.y0 + b.y1) / 2;
  const dx = parentAt.x - cx, dy = parentAt.y - cy, dist = Math.hypot(dx, dy);
  if (dist < 1 || dist < JOINT_MIN * Math.max(w, h)) return null;
  const pics = g.rects.filter(p => p.rgba);
  const opaqueAt = (p, px, py) => px >= 0 && py >= 0 && px < p.width && py < p.height && p.rgba[(py * p.width + px) * 4 + 3] >= JOINT_ALPHA;
  const snap = a => { const s = Math.round(a / 90) * 90; return Math.abs(a - s) <= 4 ? normalizeDegrees(s) : a; };
  let jx = null, jy = null;
  const tall = h >= JOINT_LONG * w, wide = !tall && w >= JOINT_LONG * h && (parentAt.x < b.x0 || parentAt.x > b.x1);
  if (tall || wide) {
    // The end: toward p, or the top when p is within the part's height.
    const side = tall ? (parentAt.y > b.y1 ? 1 : -1) : Math.sign(dx);
    // Picture-px lines across the short side from that end inward: the first with an opaque pixel.
    const along0 = Math.round((tall ? b.y0 : b.x0) / k), across0 = Math.round((tall ? b.x0 : b.y0) / k), n = Math.round((tall ? h : w) / k);
    for (let i = 0; i < n && jx === null; i++) {
      const line = side < 0 ? i : n - 1 - i;
      let lo = Infinity, hi = -Infinity;
      for (const p of pics) {
        const at = along0 + line - (tall ? p.y : p.x), count = tall ? p.width : p.height, off = (tall ? p.x : p.y) - across0;
        if (at < 0 || at >= (tall ? p.height : p.width)) continue;
        for (let j = 0; j < count; j++) if (tall ? opaqueAt(p, j, at) : opaqueAt(p, at, j)) { lo = Math.min(lo, j + off); hi = Math.max(hi, j + off + 1); }
      }
      if (lo >= hi) continue;
      const edge = (along0 + (side < 0 ? line : line + 1)) * k, mid = (across0 + (lo + hi) / 2) * k;
      [jx, jy] = tall ? [mid, edge] : [edge, mid];
    }
  }
  if (jx === null) {
    const a = snap(Math.atan2(-dy, -dx) * 180 / Math.PI) * Math.PI / 180, ux = Math.cos(a), uy = Math.sin(a);
    // c − s·u leaves the bounds at s = exit (at most at p's projection), then back toward c to the art.
    const exit = Math.min(Math.abs(ux) > 1e-9 ? w / 2 / Math.abs(ux) : Infinity, Math.abs(uy) > 1e-9 ? h / 2 / Math.abs(uy) : Infinity, Math.max(0, -(dx * ux + dy * uy)));
    const ex = cx - exit * ux, ey = cy - exit * uy;
    const near = pics.filter(p => p.x * k <= Math.max(ex, cx) + k && (p.x + p.width) * k >= Math.min(ex, cx) - k && p.y * k <= Math.max(ey, cy) + k && (p.y + p.height) * k >= Math.min(ey, cy) - k);
    let s = exit;
    for (let at = exit; at >= 0; at -= k) {
      const x = cx - at * ux, y = cy - at * uy;
      if (near.some(p => opaqueAt(p, Math.floor(x / k - p.x), Math.floor(y / k - p.y)))) { s = at; break; }
    }
    [jx, jy] = [cx - s * ux, cy - s * uy];
  }
  if (Math.hypot(cx - jx, cy - jy) < 1e-6) return null;
  const angle = snap(Math.atan2(cy - jy, cx - jx) * 180 / Math.PI), r = angle * Math.PI / 180, ux = Math.cos(r), uy = Math.sin(r);
  const corners = [[b.x0, b.y0], [b.x1, b.y0], [b.x0, b.y1], [b.x1, b.y1]].map(([x, y]) => (x - jx) * ux + (y - jy) * uy);
  const inset = JOINT_INSET * (Math.max(...corners) - Math.min(...corners));
  jx += inset * ux; jy += inset * uy;
  return { joint: { x: jx, y: jy }, angle, length: Math.max(0, Math.max(...corners) - inset), from: 'joint' };
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
/**
 * The picture of a layer ({left, top, width, height, rgba} in picture px): trimmed + a 1 px
 * transparent ring (clean edges under linear filtering), or the whole layer.
 */
function cutPicture(layer, trim, name) {
  if (!layer?.rgba || !layer.width || !layer.height) return null;
  const b = alphaBounds(layer.width, layer.height, layer.rgba);
  if (!b) return null;
  const ring = trim ? 1 : 0, src = trim ? b : { x: 0, y: 0, width: layer.width, height: layer.height };
  const width = src.width + 2 * ring, height = src.height + 2 * ring, rgba = Buffer.alloc(width * height * 4);
  const from = Buffer.isBuffer(layer.rgba) ? layer.rgba : Buffer.from(layer.rgba.buffer, layer.rgba.byteOffset, layer.rgba.length);
  for (let y = 0; y < src.height; y++) from.copy(rgba, ((y + ring) * width + ring) * 4, ((src.y + y) * layer.width + src.x) * 4, ((src.y + y) * layer.width + src.x + src.width) * 4);
  if (width > MAX_PSD_EDGE || height > MAX_PSD_EDGE) fail(`레이어 '${name}'의 그림이 ${MAX_PSD_EDGE} px보다 큽니다.`);
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
  if (!(scale === 'auto' || (Number.isFinite(scale) && scale >= 0.05 && scale <= 4))) fail('scale은 0.05..4 사이 숫자 또는 "auto"입니다.');
  if (p.trim !== undefined && typeof p.trim !== 'boolean') fail('trim은 true/false입니다.');
  for (const k of ['exclude', 'merge']) if (p[k] !== undefined && !(Array.isArray(p[k]) && p[k].every(v => typeof v === 'string'))) fail(`${k}는 레이어 키 문자열 배열입니다.`);
  for (const k of ['id', 'rig', 'name']) if (p[k] !== undefined && typeof p[k] !== 'string') fail(`${k}는 문자열입니다.`);
  const boneDirection = p.boneDirection ?? 'auto';
  if (!['auto', 'none'].includes(boneDirection)) fail('boneDirection은 auto 또는 none입니다.');
  const effects = p.effects ?? 'keep';
  if (!['keep', 'skip'].includes(effects)) fail('effects는 keep 또는 skip입니다.');
  return { origin, scale, trim: p.trim !== false, exclude: new Set(p.exclude ?? []), merge: new Set(p.merge ?? []), effects, boneDirection };
}

/** Area of every layer rectangle read at `resolution` (what the decoded pictures can take at most). */
function layerArea(nodes, resolution) {
  let area = 0;
  const visit = list => { for (const n of list) if (n.kind === 'group') visit(n.children); else if (n.width && n.height) { const r = scaledRect(n.left, n.top, n.width, n.height, resolution); area += r.width * r.height; } };
  visit(nodes);
  return area;
}
/** `scale: "auto"`: the largest SCALE_STEPS entry whose canvas long edge ≤ 2048 px and pictures ≤ 64 Mpx. */
export function autoScale(source) {
  const long = Math.max(source.width, source.height);
  return SCALE_STEPS.find(s => long * s <= AUTO_EDGE + 1e-9 && layerArea(source.layers, s) <= MAX_PIXELS) ?? SCALE_STEPS.at(-1);
}

// ------------------------------------------------------------------ mapping

/**
 * Plans the import of `source` (readLayerSource) into a new rig (`existing` null) or onto an
 * existing rig. Returns {rig, images: [{id, bytes}] (none when dryRun), report}.
 */
export function planImport({ source, options, existing = null, rigName = '', dryRun = false, traceOutline = null }) {
  const warnings = [];
  const warn = (code, message, layer) => warnings.push({ code, message, ...(layer ? { layer } : {}) });
  for (const w of source.warnings ?? []) warn(w.code, w.message);
  const updating = !!existing;
  const prov = updating && existing.editor?.layerImport && typeof existing.editor.layerImport === 'object' ? existing.editor.layerImport : null;
  // Scale: a re-import keeps the first import's; "auto" fits the canvas and the pixel budget.
  const kept = updating && Number.isFinite(prov?.scale);
  const scale = kept ? prov.scale : options.scale === 'auto' ? autoScale(source) : options.scale;
  const scaleMode = kept ? 'kept' : options.scale === 'auto' ? 'auto' : 'fixed';
  if (kept && options.scale !== 'auto' && prov.scale !== options.scale) warn('ANIM_IMPORT_SCALE', `처음 가져올 때의 배율 ${prov.scale}을(를) 그대로 씁니다.`);
  // Pictures are read at `resolution` = min(1, scale), downscaled while decoding (plugin 1.15.0),
  // and placed at k = scale / resolution rig px per picture px (1 when downscaled). Rigs imported
  // before 1.15.0 have no recorded resolution: their pictures stay full size, placed at the scale.
  const resolution = kept ? (Number.isFinite(prov.resolution) ? prov.resolution : 1) : Math.min(1, scale);
  const k = scale / resolution;
  const area = layerArea(source.layers, resolution);
  if (area > MAX_PIXELS) fail(`배율 ${scale}에서 레이어 그림이 ${(area / 1048576).toFixed(1)}메가픽셀로 너무 많습니다(64메가픽셀 이하). 배율을 낮추거나 scale "auto"로 가져오세요.`);
  // A PSD is decoded layer by layer as the plan needs it; a new character's preview reads only alpha
  // (trim + empty checks). A re-import preview needs colour: unchanged pictures are found by pixel hash.
  const lazy = typeof source.decodeLayer === 'function', alphaOnly = dryRun && lazy && !updating;
  const pixelsOf = node => {
    if (node.kind !== 'layer' || !node.width || !node.height) return null;
    if (lazy && !node.rgba) return source.decodeLayer(node, { resolution, alphaOnly });
    return node.rgba ? resampleRgba(node, resolution) : null;
  };
  /** Centre of a marker layer's pixels in document px (read at full size: markers are tiny). */
  const markerAt = node => {
    const full = node.rgba ? node : lazy && node.width && node.height ? source.decodeLayer(node, { alphaOnly: true }) : null;
    const b = full?.rgba ? alphaBounds(node.width, node.height, full.rgba) : null;
    const r = b ? { x: node.left + b.x, y: node.top + b.y, width: b.width, height: b.height } : { x: node.left, y: node.top, width: node.width, height: node.height };
    return { x: r.x + r.width / 2, y: r.y + r.height / 2 };
  };

  // Pass 1: the report tree + groups (future bones) + pictures, in document order (back → front).
  const groups = [], pictures = [], markers = [], marks = [], groupClips = [];
  const tally = { masks: [], effects: [], empty: 0, emptyGroups: 0, baked: 0, merged: 0, unbaked: new Map() };
  let order = 0;
  const keyOf = (prefix, index) => prefix ? `${prefix}.${index}` : String(index);
  const isMerged = (node, key) => node.kind === 'group' && (parseName(node.name).tags.has('merge') || options.merge.has(key));
  const itemOf = (node, key, include) => {
    const item = { key, name: node.name ?? '', kind: node.kind, visible: node.visible !== false, opacity: round(node.opacity ?? 1), blend: node.blend ?? 'norm', include, warnings: [] };
    if (node.kind === 'layer') item.bounds = { x: node.left, y: node.top, width: node.width, height: node.height };
    return item;
  };
  // Clipping (Photoshop): a clipped node belongs to the nearest unclipped sibling below it; a
  // clipped node with nothing below it is drawn normally. index → base index.
  const clipBases = nodes => {
    const baseOf = new Map();
    let base = -1;
    nodes.forEach((n, i) => { if (!n.clipping) base = i; else if (base >= 0) baseOf.set(i, base); });
    return baseOf;
  };
  /** The clipped siblings painted onto the base at `index`: visible and not left out (hidden ones are skipped like Photoshop). */
  const clipsOf = (nodes, baseOf, index, prefix) => [...baseOf].filter(([, b]) => b === index).map(([i]) => ({ node: nodes[i], key: keyOf(prefix, i) }))
    .filter(({ node, key }) => node.visible !== false && !options.exclude.has(key) && !parseName(node.name).tags.has('ignore'));
  const bakeBlend = node => {
    const blend = node.kind === 'group' && node.blend === 'pass' ? 'norm' : node.blend ?? 'norm';
    if (!BAKED_BLENDS.has(blend)) tally.unbaked.set(blend, (tally.unbaked.get(blend) ?? 0) + 1);
    return blend;
  };
  /** Clipping group → one picture: the base with its clipped layers painted inside its alpha (compose.mjs). */
  const bake = (picture, clips) => {
    if (!picture || !clips.length) return picture;
    tally.baked += clips.length;
    if (alphaOnly) return picture;  // the alpha is the base's: a preview needs nothing else
    const surface = Surface.of(picture);
    for (const { node, key } of clips) {
      const p = node.kind === 'group' ? composeGroup(node, key) : pixelsOf(node);
      if (p) surface.paintLocked(p, bakeBlend(node), node.opacity ?? 1);
    }
    return surface.picture();
  };
  /** Union of the rectangles (at the import resolution) of the layers a merged group draws, or null. */
  const extentOf = (nodes, prefix) => {
    let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
    const walk = (list, at) => { list.forEach((n, i) => {
      const key = keyOf(at, i);
      if (n.visible === false || options.exclude.has(key) || parseName(n.name).tags.has('ignore')) return;
      if (n.kind === 'group') { walk(n.children, key); return; }
      if (!n.width || !n.height) return;
      const r = scaledRect(n.left, n.top, n.width, n.height, resolution);
      x0 = Math.min(x0, r.left); y0 = Math.min(y0, r.top); x1 = Math.max(x1, r.left + r.width); y1 = Math.max(y1, r.top + r.height);
    }); };
    walk(nodes, prefix);
    return x1 > x0 && y1 > y0 ? { left: x0, top: y0, width: x1 - x0, height: y1 - y0 } : null;
  };
  /**
   * A merged group → one picture: its visible children composited back to front with their blend
   * modes, opacities and clipping, isolated (as a "Normal" group; a pass-through group whose
   * children blend with art below the group can differ there). Markers and left-out layers are not drawn.
   */
  const composeGroup = (group, prefix) => {
    const extent = extentOf(group.children, prefix);
    if (!extent) return null;
    const surface = new Surface(extent, alphaOnly), baseOf = clipBases(group.children);
    group.children.forEach((node, index) => {
      const key = keyOf(prefix, index), { tags } = parseName(node.name);
      if (baseOf.has(index) || node.visible === false || options.exclude.has(key) || tags.has('ignore') || tags.has('pivot') || tags.has('bone')) return;
      const picture = bake(node.kind === 'group' ? composeGroup(node, key) : pixelsOf(node), clipsOf(group.children, baseOf, index, prefix));
      if (picture) surface.blendOnto(picture, bakeBlend(node), node.opacity ?? 1);
    });
    return surface.picture();
  };
  /**
   * Layers clipped to a group that stays bones / slots → painted into its pictures (1.19.1). The
   * stack is what the group shows: per slot the topmost picture visible inside the group. Every
   * other picture of the group (hidden variants, a covered second visible layer of a slot) is
   * painted as if it were shown in place of its slot's shown picture (so swapping `eyes/closed`
   * in keeps the shading), or at its own draw position when its slot shows nothing in the group.
   */
  const bakeIntoGroup = ({ clips, members }) => {
    const shown = new Map();
    for (const m of members) if (m.visible) shown.set(m.pic.slot, m);  // back → front: the last wins
    const stack = members.filter(m => shown.get(m.pic.slot) === m);
    const layers = stack.map(m => ({ picture: m.pic.picture, opacity: m.opacity }));
    const variants = members.filter(m => shown.get(m.pic.slot) !== m).map(m => ({ picture: m.pic.picture,
      replace: shown.has(m.pic.slot) ? stack.indexOf(shown.get(m.pic.slot)) : -1, at: stack.filter(o => o.pic.order < m.pic.order).length }));
    const painted = clips.map(({ node, key, blend }) => ({ picture: node.kind === 'group' ? composeGroup(node, key) : pixelsOf(node), blend, opacity: node.opacity ?? 1 })).filter(c => c.picture);
    if (painted.length && (layers.length || variants.length)) paintClippedGroup(layers, variants, painted);
  };
  /** Report items of the layers inside a merged group (drawn into its picture, no slots of their own). */
  const describe = (nodes, include, prefix) => {
    const baseOf = clipBases(nodes);
    return nodes.map((node, index) => {
      const key = keyOf(prefix, index), { tags } = parseName(node.name), item = itemOf(node, key, include && !options.exclude.has(key) && !tags.has('ignore'));
      item.role = tags.has('ignore') ? 'ignored' : !item.include ? 'excluded' : baseOf.has(index) ? 'clipped' : 'in-merge';
      if (baseOf.has(index)) item.clippedTo = keyOf(prefix, baseOf.get(index));
      if (node.kind === 'group') item.children = describe(node.children, item.include, key);
      return item;
    });
  };
  // `baseOf` (clipping bases of this list) is computed once per list, as a default parameter.
  const visit = (nodes, ctx, prefix, baseOf = clipBases(nodes)) => nodes.map((node, index) => {
    const key = keyOf(prefix, index), { text, tags } = parseName(node.name);
    const visible = ctx.visible && node.visible !== false, opacity = ctx.opacity * (node.opacity ?? 1);
    // Visibility / opacity relative to every enclosing group that layers are clipped to (1.19.1):
    // such a group's pictures are baked as the group shows them, even when the group is hidden.
    const frames = ctx.frames.map(f => ({ target: f.target, visible: f.visible && node.visible !== false, opacity: f.opacity * (node.opacity ?? 1) }));
    const item = itemOf(node, key, !options.exclude.has(key) && ctx.include);
    const note = (code, message) => { item.warnings.push(message); warn(code, message, key); };
    if (tags.has('ignore')) { item.role = 'ignored'; item.include = false; if (node.kind === 'group') item.children = visit(node.children, { ...ctx, include: false }, key); return item; }
    if (!item.include) { item.role = 'excluded'; if (node.kind === 'group') item.children = visit(node.children, { ...ctx, include: false }, key); return item; }
    // Clipped onto a layer (or a merged group) below: painted into that picture (plugin 1.15.0);
    // onto a group that stays bones / slots: painted into its pictures (1.19.1, after the slots).
    if (baseOf.has(index)) {
      item.role = 'clipped'; item.clippedTo = keyOf(prefix, baseOf.get(index));
      if (node.kind === 'group') item.children = describe(node.children, true, key);
      return item;
    }
    if (node.mask) { item.warnings.push('레이어 마스크는 무시됩니다.'); tally.masks.push(node.name ?? ''); }
    const merged = isMerged(node, key);
    // A blend mode the rig cannot draw (shadow / glow overlays): drawn normal, or left out with effects "skip".
    if (!normalBlend(node.blend)) {
      tally.effects.push(node.blend);
      if (options.effects === 'skip') {
        item.role = 'effect';
        if (node.kind === 'group') item.children = merged ? describe(node.children, false, key) : visit(node.children, { ...ctx, include: false }, key);
        return item;
      }
      item.warnings.push(`혼합 모드 ${BLEND_NAMES[node.blend] ?? node.blend}는 표준으로 그립니다.`);
    }
    if (node.kind === 'group' && !merged) {
      // Layers clipped to this group: baked into its pictures once the slots are known (innermost
      // group first: a group's composite includes what is clipped to the groups inside it).
      const clips = clipsOf(nodes, baseOf, index, prefix);
      const target = clips.length ? { clips: clips.map(c => ({ ...c, blend: alphaOnly ? null : bakeBlend(c.node) })), members: [] } : null;
      const inner = extra => {
        const out = visit(node.children, { ...ctx, visible, opacity, frames: target ? [...frames, { target, visible: true, opacity: 1 }] : frames, ...extra }, key);
        if (target) { groupClips.push(target); tally.baked += clips.length; }
        return out;
      };
      if (tags.has('slot') && !ctx.slotGroup) {
        item.role = 'slot-group';
        item.children = inner({ slotGroup: { text: text || node.name, key } });
        return item;
      }
      if (ctx.slotGroup) note('ANIM_IMPORT_NESTED_SLOT', `'${node.name}': [slot] 그룹 안의 그룹은 같은 슬롯의 그림으로 가져옵니다([merge]면 한 그림).`);
      if (ctx.slotGroup) { item.role = 'group-in-slot'; item.children = inner({}); return item; }
      if ((node.opacity ?? 1) < 1) note('ANIM_IMPORT_GROUP_OPACITY', `'${node.name}': 그룹 불투명도는 레이어마다 곱해 적용합니다(겹친 부분은 조금 다를 수 있음).`);
      const group = { key, text, name: node.name ?? '', parent: ctx.group, rects: [], marker: null, item };
      groups.push(group);
      item.role = 'bone';
      item.children = inner({ group });
      return item;
    }
    if (!merged && (tags.has('pivot') || tags.has('bone'))) {
      item.role = 'pivot';
      const at = markerAt(node);
      if (ctx.group) { if (ctx.group.marker) note('ANIM_IMPORT_PIVOT', `'${node.name}': 그룹에 회전 중심 표시가 둘 이상입니다. 첫째만 씁니다.`); else ctx.group.marker = at; }
      else markers.push(at);
      return item;
    }
    // `[box]` / `[point]` (plugin 1.17.0): the layer's pixels outline a hit area / mark an anchor; no picture.
    if (!merged && node.kind === 'layer' && (tags.has('box') || tags.has('point'))) {
      const shape = cutPicture(pixelsOf(node), true, node.name);
      if (!shape) { item.role = 'empty'; tally.empty++; return item; }
      item.role = tags.has('box') ? 'box' : 'point';
      marks.push({ item, key, kind: item.role, picture: shape, visible, group: ctx.group, text, order: order++, name: node.name ?? '' });
      return item;
    }
    // The picture: the layer (or the merged group), its clipped layers painted in, trimmed.
    const picture = cutPicture(bake(merged ? composeGroup(node, key) : pixelsOf(node), clipsOf(nodes, baseOf, index, prefix)), options.trim, node.name);
    if (merged) {
      tally.merged++;
      item.merge = tags.has('merge') ? 'tag' : 'key';
      item.children = describe(node.children, true, key);
    }
    if (!picture) { item.role = merged ? 'empty-group' : 'empty'; if (merged) tally.emptyGroups++; else tally.empty++; return item; }
    item.role = merged ? 'merged' : 'image';
    let slotText, attText, plain = false;
    if (ctx.slotGroup) { slotText = ctx.slotGroup.text; attText = text || node.name; }
    else {
      const cut = Math.max(text.lastIndexOf('/'), text.lastIndexOf('@'));
      if (cut > 0 && cut < text.length - 1) { slotText = text.slice(0, cut); attText = text.slice(cut + 1); } else { slotText = attText = text; plain = true; }
    }
    for (let g = ctx.group; g; g = g.parent) g.rects.push(picture);
    const pic = { item, key, picture, visible, opacity, group: ctx.group, slotText, attText, plain, order: order++, name: node.name ?? '' };
    pictures.push(pic);
    for (const f of frames) f.target.members.push({ pic, visible: f.visible, opacity: f.opacity });
    return item;
  });
  const layers = visit(source.layers, { visible: true, opacity: 1, include: true, group: null, slotGroup: null, frames: [] }, '');
  // One summary line per kind, not one per layer (a production file has hundreds of these).
  const list = names => names.slice(0, 3).map(n => `'${n}'`).join(', ') + (names.length > 3 ? ' …' : '');
  const byBlend = blends => [...blends.reduce((m, b) => m.set(b, (m.get(b) ?? 0) + 1), new Map())].map(([b, n]) => `${BLEND_NAMES[b] ?? b} ${n}`).join(' · ');
  if (tally.effects.length) warn('ANIM_IMPORT_BLEND', options.effects === 'skip'
    ? `표준이 아닌 혼합 모드 레이어·그룹 ${tally.effects.length}개(${byBlend(tally.effects)})를 뺐습니다(effects "skip").`
    : `표준이 아닌 혼합 모드 레이어·그룹 ${tally.effects.length}개(${byBlend(tally.effects)})는 표준으로 그립니다. 그림자·빛 효과를 빼려면 「효과 레이어 빼기」(effects "skip"), 살리려면 그 그룹을 [merge]로 합치세요.`);
  if (tally.unbaked.size) warn('ANIM_IMPORT_BLEND', `합칠 때 ${[...tally.unbaked].map(([b, n]) => `${BLEND_NAMES[b] ?? b} ${n}`).join(' · ')}개는 표준으로 계산했습니다(지원: 표준·곱하기·스크린·오버레이·선형 닷지·밝게·어둡게·소프트 라이트).`);
  if (tally.masks.length) warn('ANIM_IMPORT_MASK', `레이어 마스크 ${tally.masks.length}개(${list(tally.masks)})는 무시됩니다. 마스크를 적용(병합)한 뒤 다시 가져오세요.`);

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
  // Joint guesses (1.20.0) look at the parent bone's origin (its marker, joint or content centre);
  // top-level groups at the document's content centre (the root origin is only the character's feet).
  const docBounds = rectsBounds(pictures.map(p => p.picture), k);
  const centreOf = b => b ? { x: (b.x0 + b.x1) / 2, y: (b.y0 + b.y1) / 2 } : null;
  for (const g of groups) {
    if (!g.rects.length && !g.marker) { g.item.role = 'empty-group'; tally.emptyGroups++; continue; }
    g.id = unique(toId(g.text, null) ?? `bone-${++boneN}`, boneIds);
    let parent = g.parent;
    while (parent && !parent.id) parent = parent.parent;
    g.parentId = parent ? parent.id : rootId;
    g.bounds = rectsBounds(g.rects, k);
    g.item.bone = g.id;
    if (existingBones.has(g.id)) {
      // A kept bone stays where the rig has it (a new parent's child aim looks at the real origin).
      const m = world.get(g.id);
      g.world = m ? { x: round(m[4]), y: round(m[5]) } : centreOf(g.bounds);
      changes.bones.kept.push(g.id); g.item.status = 'kept'; continue;
    }
    const parentAt = parent ? parent.world : centreOf(docBounds);
    g.joint = options.boneDirection === 'auto' && !g.marker ? jointAim(g, parentAt, k) : null;
    const at = g.marker ? { x: g.marker.x * scale, y: g.marker.y * scale } : g.joint ? g.joint.joint : centreOf(g.bounds);
    g.world = { x: round(at.x), y: round(at.y) };
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
      const aim = g.joint ?? boneAim(g, groups.filter(c => c.id && c.parentId === g.id));
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
  if (tally.empty || tally.emptyGroups) warn('ANIM_IMPORT_EMPTY', `비어 있는(투명한) 레이어 ${tally.empty}개와 그림이 없는 그룹 ${tally.emptyGroups}개는 건너뜁니다.`);

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
  // Boxes and points: a slot each, `<name>-box` / `<name>-point`, on the bone of their group.
  for (const mark of marks) {
    let g = mark.group;
    while (g && !g.id) g = g.parent;
    const base = toId(mark.text, null);
    let id = base ? `${base}-${mark.kind}` : mark.kind;
    if (planned.has(id)) { let n = 2; while (planned.has(`${id}-${n}`)) n++; id = `${id}-${n}`; }
    const bone = g ? g.id : rootId;
    const slot = { id, bone: updating && slotIds.has(id) ? slotIds.get(id) : bone, pictures: [], names: new Set(), mark };
    mark.attachment = base ?? mark.kind;
    mark.item.slot = id; mark.item.attachment = mark.attachment; mark.item.bone = slot.bone; mark.item.setup = mark.visible;
    planned.set(id, slot);
  }
  for (const slot of planned.values()) {
    if (slot.mark) { slot.setup = slot.mark.visible ? slot.mark : null; slot.order = slot.mark.order; continue; }
    const shown = slot.pictures.filter(p => p.visible);
    if (shown.length > 1) warn('ANIM_IMPORT_SLOT_VISIBLE', `슬롯 '${slot.id}'에 보이는 레이어가 ${shown.length}개입니다. 맨 위 '${shown.at(-1).name}'만 보이고 나머지는 숨긴 첨부가 됩니다.`);
    slot.setup = shown.at(-1) ?? null;
    slot.order = (slot.setup ?? slot.pictures[0]).order;
    for (const p of slot.pictures) { p.item.slot = slot.id; p.item.attachment = p.attachment; p.item.bone = slot.bone; p.item.setup = p === slot.setup; }
  }
  const slots = [...planned.values()].sort((a, b) => a.order - b.order);
  // Layers clipped onto groups (1.19.1): painted into the pictures before they are hashed, so a
  // re-import notices a changed shading layer. A preview reading only alpha has nothing to paint.
  if (!alphaOnly) for (const target of groupClips) bakeIntoGroup(target);

  // Pictures → images and attachments.
  const images = new Map(), provLayers = { ...(prov?.layers && typeof prov.layers === 'object' ? prov.layers : {}) }, seen = new Set();
  const imageOf = pic => {
    if (pic.assetId) return pic.assetId;
    const png = encodePng(pic.picture.width, pic.picture.height, pic.picture.rgba), id = imageAssetId(png);
    if (!images.has(id)) images.set(id, png);
    return pic.assetId = id;
  };
  const PICTURELESS = new Set(['clipping', 'boundingBox', 'point']);
  /** A `[box]` / `[point]` layer → its attachment (added, or the existing one's shape / place updated). */
  const applyMark = (slot, boneWorld) => {
    const m = slot.mark, ref = `${slot.id}/${m.attachment}`, r = m.picture, inv = invert(boneWorld) ?? IDENTITY;
    const toBone = (px, py) => { const x = px * k, y = py * k; return [round(inv[0] * x + inv[2] * y + inv[4]), round(inv[1] * x + inv[3] * y + inv[5])]; };
    let fields;
    if (m.kind === 'box') {
      // The core's alpha outline (autoOutline's tracing) when the caller has it, else the pixels' rectangle.
      const outline = (typeof traceOutline === 'function' ? traceOutline(r) : null) ?? [1, 1, r.width - 1, 1, r.width - 1, r.height - 1, 1, r.height - 1];
      const vertices = [];
      for (let i = 0; i + 1 < outline.length; i += 2) vertices.push(...toBone(r.x + outline[i], r.y + outline[i + 1]));
      fields = { type: 'boundingBox', vertices };
    } else {
      const [x, y] = toBone(r.x + r.width / 2, r.y + r.height / 2);
      fields = { type: 'point', ...(x ? { x } : {}), ...(y ? { y } : {}) };
    }
    const at = rig.attachments.findIndex(a => a.slot === slot.id && a.name === m.attachment);
    if (at < 0) { rig.attachments.push({ slot: slot.id, name: m.attachment, ...fields }); changes.attachments.added.push(ref); m.item.status = 'add'; return; }
    const old = rig.attachments[at];
    if (old.type !== fields.type) {
      changes.attachments.skipped.push(ref); m.item.status = 'skip';
      warn('ANIM_IMPORT_KIND', `'${ref}'는 이미 다른 종류의 첨부라 바꾸지 않았습니다.`, m.key);
      return;
    }
    // Re-import: the outline / place follows the art; weights stay unless the point count changed.
    const next = m.kind === 'box' ? { ...old, vertices: fields.vertices } : { ...old, x: fields.x ?? 0, y: fields.y ?? 0 };
    if (m.kind === 'point') { if (!next.x) delete next.x; if (!next.y) delete next.y; }
    if (m.kind === 'box' && old.weights && old.vertices?.length !== fields.vertices.length) delete next.weights;
    if (JSON.stringify(next) === JSON.stringify(old)) { changes.attachments.unchanged.push(ref); m.item.status = 'same'; return; }
    rig.attachments[at] = next; changes.attachments.updated.push(ref); m.item.status = 'update';
  };
  for (const slot of slots) {
    const current = rig.slots.find(s => s.id === slot.id);
    const boneWorld = world.get(slot.bone) ?? IDENTITY;
    if (slot.mark) applyMark(slot, boneWorld);
    for (const pic of slot.pictures) {
      const r = pic.picture, pixels = sha(r.rgba).slice(0, 32), ref = `${slot.id}/${pic.attachment}`;
      seen.add(ref);
      const record = { x: r.x, y: r.y, width: r.width, height: r.height, pixels };
      const at = rig.attachments.findIndex(a => a.slot === slot.id && a.name === pic.attachment);
      if (at < 0) {
        const attachment = { slot: slot.id, name: pic.attachment, imageAssetId: dryRun ? '' : imageOf(pic), width: r.width, height: r.height,
          ...placement(boneWorld, (r.x + r.width / 2) * k, (r.y + r.height / 2) * k, k) };
        rig.attachments.push(attachment);
        changes.attachments.added.push(ref); pic.item.status = 'add';
        provLayers[ref] = record;
        continue;
      }
      const old = rig.attachments[at], before = provLayers[ref];
      // A box / point / clipping (plugin 1.17.0) of the same name is rig work, never a picture to swap.
      if (PICTURELESS.has(old.type)) {
        changes.attachments.skipped.push(ref); pic.item.status = 'skip';
        warn('ANIM_IMPORT_KIND', `'${ref}'는 그림이 없는 첨부(판정 상자·기준점·클리핑)라 그림으로 바꾸지 않았습니다.`, pic.key);
        continue;
      }
      if (before && before.pixels === pixels && before.width === r.width && before.height === r.height && before.x === r.x && before.y === r.y) {
        changes.attachments.unchanged.push(ref); pic.item.status = 'same'; continue;
      }
      if (old.type === 'mesh') {
        const moved = before ? before.x !== r.x || before.y !== r.y : false;
        if (old.width !== r.width || old.height !== r.height || moved) {
          changes.attachments.skipped.push(ref); pic.item.status = 'skip';
          // A linked mesh (plugin 1.18.0) keeps its parent's shape: only a same-size picture can swap in.
          warn('ANIM_IMPORT_MESH', old.linkedMesh
            ? `링크드 메시 '${ref}'는 그림 크기나 위치가 바뀌어 바꾸지 않았습니다(부모 메시의 모양을 쓰므로 같은 크기의 그림만 바꿉니다).`
            : `메시 첨부 '${ref}'는 그림 크기나 위치가 바뀌어 바꾸지 않았습니다. 메시를 영역으로 되돌리거나 같은 크기로 그려 주세요.`, pic.key);
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
    const alpha = slot.setup && !slot.mark ? slot.setup.opacity : 1;
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
  // Original names (1.20.0): the editor's hierarchy filter finds parts by what the artist called them.
  for (const pic of pictures) { const ref = `${pic.slot.id}/${pic.attachment}`; if (provLayers[ref]) provLayers[ref] = { ...provLayers[ref], name: pic.name }; }
  const provBones = { ...(prov?.bones && typeof prov.bones === 'object' ? prov.bones : {}) };
  for (const g of groups) if (g.id) provBones[g.id] = g.name;
  if (!pictures.length) fail('가져올 그림 레이어가 없습니다(모두 빠졌거나 비어 있음).');
  // Rig limits: an import over them is refused, but a preview reports the problem and proposes
  // which groups to merge (plugin 1.15.0), so a 500-layer file is one click from fitting.
  const counts = { bones: rig.bones.length, slots: rig.slots.length, attachments: rig.attachments.length };
  const limits = Object.fromEntries(Object.entries(LIMITS).map(([what, max]) => [what, { count: counts[what], max }]));
  const over = Object.keys(LIMITS).filter(what => counts[what] > LIMITS[what]);
  limits.exceeded = over.length > 0;
  let autoMerge = null;
  if (over.length) {
    const label = { bones: '본', slots: '슬롯', attachments: '첨부' };
    autoMerge = updating ? { keys: [], fits: false } : proposeMerge(groups, pictures, counts);
    const message = `${over.map(what => `${label[what]}이 ${LIMITS[what]}개를 넘습니다(${counts[what]}개)`).join(', ')}. ` + (autoMerge.keys.length
      ? `그룹을 한 그림으로 합치세요: 미리 보기의 「자동 합치기」(merge ${autoMerge.keys.length}개 그룹${autoMerge.fits ? '' : ', 그래도 넘음'}) 또는 그룹 이름에 [merge].`
      : 'eyes/open처럼 같은 슬롯으로 묶거나 [merge] 그룹·빼기로 레이어를 줄이세요.');
    if (!dryRun) fail(message);
    warn('ANIM_IMPORT_LIMIT', message);
  }

  // Provenance: editor-only metadata (runtimes ignore `editor`) that lets a re-import keep placements.
  rig.editor = { ...(rig.editor && typeof rig.editor === 'object' ? rig.editor : {}),
    layerImport: { source: path.basename(source.file), format: source.format, width: source.width, height: source.height, scale, resolution, layers: provLayers, bones: provBones } };
  const hidden = pictures.filter(p => p.slot.setup !== p).length;
  const report = {
    source: { format: source.format, file: path.basename(source.file), width: source.width, height: source.height, layerPixels: layerArea(source.layers, 1) },
    options: { origin, scale, scaleMode, resolution, trim: options.trim, exclude: [...options.exclude], merge: [...options.merge], effects: options.effects, boneDirection: options.boneDirection },
    canvas: rig.canvas, layers, warnings, changes,
    summary: { bones: changes.bones.added.length, joints: made.filter(g => g.joint).length, slots: slots.length, attachments: pictures.length, hidden, images: dryRun ? null : images.size,
      added: changes.attachments.added.length, updated: changes.attachments.updated.length, unchanged: changes.attachments.unchanged.length,
      ...(marks.length ? { boxes: marks.filter(m => m.kind === 'box').length, points: marks.filter(m => m.kind === 'point').length } : {}),
      baked: tally.baked, merged: tally.merged, effects: tally.effects.length, empty: tally.empty + tally.emptyGroups, pixels: pictures.reduce((s, p) => s + p.picture.width * p.picture.height, 0) },
    limits, ...(autoMerge ? { autoMerge } : {}),
  };
  return { rig, images: dryRun ? [] : [...images].map(([id, bytes]) => ({ id, bytes })), report };
}

/**
 * Groups to merge so a plan over the rig limits fits (`report.autoMerge`, the dialog's 자동 합치기):
 * repeatedly the leaf bone group (every bone group under it already merged) with the most
 * pictures, groups whose hidden variants a merge would drop last. Merging a group removes its
 * bones and turns its slots and attachments into one each; a merged group's children leave the list.
 */
function proposeMerge(groups, pictures, counts) {
  const bones = groups.filter(g => g.id && g.item.status === 'add');
  const within = (g, a) => { for (let p = g; p; p = p.parent) if (p === a) return true; return false; };
  const stats = new Map(bones.map(g => [g, { pictures: 0, slots: new Set(), bones: 0, hidden: 0 }]));
  for (const pic of pictures) for (let g = pic.group; g; g = g.parent) { const s = stats.get(g); if (s) { s.pictures++; s.slots.add(pic.slot.id); if (!pic.visible) s.hidden = 1; } }
  for (const b of bones) for (let g = b; g; g = g.parent) { const s = stats.get(g); if (s) s.bones++; }
  let chosen = [];
  const fits = () => {
    let b = counts.bones, s = counts.slots, a = counts.attachments;
    for (const g of chosen) { const t = stats.get(g); b -= t.bones; s -= Math.max(0, t.slots.size - 1); a -= Math.max(0, t.pictures - 1); }
    return b <= LIMITS.bones && s <= LIMITS.slots && a <= LIMITS.attachments;
  };
  while (!fits()) {
    const covered = g => chosen.some(c => within(g, c));
    const candidates = bones.filter(g => !covered(g) && bones.every(d => d === g || !within(d, g) || covered(d)));
    if (!candidates.length) break;
    candidates.sort((a, b) => stats.get(a).hidden - stats.get(b).hidden || stats.get(b).pictures - stats.get(a).pictures);
    const pick = candidates[0];
    chosen = [...chosen.filter(c => !within(c, pick)), pick];
  }
  return { keys: chosen.map(g => g.key), fits: fits() };
}

/**
 * The service op: payload {file, id? | rig?, name?, origin?, trim?, scale?, exclude?, merge?, effects?} against
 * the plugin's `documents` ([{type, id, data}]). Returns {documents, images, report}.
 */
export async function importLayers({ payload, documents = [], dryRun = false, validateRig, traceOutline = null }) {
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
  const plan = planImport({ source, options, existing: target ? target.data : null, rigName: payload.name, dryRun, traceOutline });
  // The same core validation as every rig edit (a preview has no image ids yet: not checked).
  if (validateRig && !dryRun) {
    const errors = (validateRig(plan.rig) ?? []).filter(d => d.severity !== 'warning');
    if (errors.length) fail(`가져온 리그가 올바르지 않습니다: ${errors.slice(0, 3).map(d => `${d.pointer} ${d.message}`).join('; ')}`);
  }
  const out = [{ type: 'rig', id, data: plan.rig }];
  if (!target) out.push({ type: 'controller', id, data: { schemaVersion: 1, rig: id, defaultClip: 'idle', layers: [], bindings: [] } });
  return { documents: out, images: plan.images, report: { mode: target ? 'update' : 'create', rigId: id, controllerId: target ? null : id, ...plan.report } };
}

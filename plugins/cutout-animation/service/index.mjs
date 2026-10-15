// Node service module of the cutout animation plugin (role "service", platform "node").
// Runs the SAME C++ core as the players (web runtime Wasm) for validation and authoring
// ops, so Studio, MCP and the exported game can never disagree about a rig.
import { open, readFile } from 'node:fs/promises';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import { inflateSync } from 'node:zlib';

const PNG = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
const RIG_SENTINEL = '#rig';
const SCALAR = new Set(['string', 'number', 'boolean']);

/** Width/height from the PNG IHDR chunk, or null when the file is unreadable / not a PNG. */
async function pngSize(file) {
  let handle;
  try {
    handle = await open(file, 'r');
    const header = Buffer.alloc(24);
    const { bytesRead } = await handle.read(header, 0, 24, 0);
    if (bytesRead < 24 || !header.subarray(0, 8).equals(PNG) || header.toString('ascii', 12, 16) !== 'IHDR') return null;
    return { width: header.readUInt32BE(16), height: header.readUInt32BE(20) };
  } catch { return null; } finally { await handle?.close(); }
}

/**
 * Alpha channel of a PNG file (one byte per pixel) for the core's `autoOutline` mask: the core
 * has no PNG decoder (players get decoded images from their host). Gray / gray+alpha / RGB / RGBA
 * at 8 or 16 bits and palettes (1..8 bits, tRNS) — what image editors write; Adam7 interlacing is
 * refused with a message. Read-only, like pngSize.
 */
async function pngAlpha(file) {
  const bytes = await readFile(file);
  if (bytes.length < 33 || !bytes.subarray(0, 8).equals(PNG)) throw new Error(`PNG 파일이 아닙니다: ${file}`);
  let at = 8, header = null, palette = null, trns = null;
  const data = [];
  while (at + 8 <= bytes.length) {
    const length = bytes.readUInt32BE(at), type = bytes.toString('ascii', at + 4, at + 8), body = bytes.subarray(at + 8, at + 8 + length);
    if (type === 'IHDR') header = { width: body.readUInt32BE(0), height: body.readUInt32BE(4), depth: body[8], color: body[9], interlace: body[12] };
    else if (type === 'PLTE') palette = body;
    else if (type === 'tRNS') trns = body;
    else if (type === 'IDAT') data.push(body);
    else if (type === 'IEND') break;
    at += 12 + length;
  }
  if (!header) throw new Error(`PNG 헤더가 없습니다: ${file}`);
  const { width, height, depth, color, interlace } = header;
  const channels = { 0: 1, 2: 3, 3: 1, 4: 2, 6: 4 }[color];
  if (!channels || ![1, 2, 4, 8, 16].includes(depth) || (color !== 0 && color !== 3 && depth < 8)) throw new Error(`지원하지 않는 PNG 형식입니다(색 ${color}, ${depth}비트).`);
  if (interlace) throw new Error('인터레이스 PNG는 자동 윤곽에 쓸 수 없습니다. 인터레이스 없이 다시 저장하세요.');
  if (width * height > 8192 * 8192) throw new Error('PNG가 너무 큽니다.');
  const raw = inflateSync(Buffer.concat(data));
  const bits = channels * depth, bpp = Math.max(1, bits >> 3), stride = Math.ceil(width * bits / 8);
  const rows = Buffer.alloc(stride * height);
  let prev = Buffer.alloc(stride);
  for (let y = 0; y < height; y++) {
    const filter = raw[y * (stride + 1)], line = raw.subarray(y * (stride + 1) + 1, (y + 1) * (stride + 1)), out = rows.subarray(y * stride, (y + 1) * stride);
    for (let i = 0; i < stride; i++) {
      const a = i >= bpp ? out[i - bpp] : 0, b = prev[i], c = i >= bpp ? prev[i - bpp] : 0;
      const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
      const predictor = [0, a, b, (a + b) >> 1, pa <= pb && pa <= pc ? a : pb <= pc ? b : c][filter];
      if (predictor === undefined) throw new Error('손상된 PNG입니다(필터).');
      out[i] = (line[i] + predictor) & 255;
    }
    prev = out;
  }
  const alpha = Buffer.alloc(width * height, 255);
  const sample = (row, x, channel) => depth === 16 ? row[(x * channels + channel) * 2] : depth === 8 ? row[x * channels + channel]
    : (row[(x * depth) >> 3] >> (8 - depth - ((x * depth) & 7))) & ((1 << depth) - 1);
  for (let y = 0; y < height; y++) {
    const row = rows.subarray(y * stride, (y + 1) * stride);
    for (let x = 0; x < width; x++) {
      let v = 255;
      if (color === 6 || color === 4) v = sample(row, x, channels - 1);
      else if (color === 3) { const i = sample(row, x, 0); v = trns && i < trns.length ? trns[i] : 255; }
      else if (trns && depth >= 8) {
        // Color key (tRNS for gray / RGB): that exact color is transparent (compared on the high byte).
        const key = Array.from({ length: channels }, (_, k) => trns[k * 2 + (depth === 16 ? 0 : 1)]);
        v = key.every((k, ch) => sample(row, x, ch) === k) ? 0 : 255;
      }
      alpha[y * width + x] = v;
    }
  }
  return { width, height, alpha };
}
/** `autoOutline` ops (alone or in a batch) naming `imagePath` get the PNG's alpha as `mask`. */
async function fillOutlineMasks(operation) {
  const ops = operation?.op === 'batch' && Array.isArray(operation.operations) ? operation.operations : [operation];
  for (const op of ops) {
    if (op?.op !== 'autoOutline' || op.mask !== undefined || typeof op.imagePath !== 'string') continue;
    const { width, height, alpha } = await pngAlpha(op.imagePath);
    op.mask = { width, height, alpha: alpha.toString('base64') };
    delete op.imagePath;
  }
}

export async function createService({ pluginDir, manifest }) {
  const runtime = manifest.modules.find(m => m.role === 'runtime' && m.platform === 'web-wasm');
  if (!runtime) throw new Error('cutout-animation: web-wasm 런타임 모듈이 없습니다.');
  const { default: factory } = await import(pathToFileURL(path.join(pluginDir, ...runtime.path.split('/'))).href);
  const module = await factory();
  const raw = module.cwrap('raising_extension_request', 'string', ['string']);
  const request = value => {
    const reply = JSON.parse(raw(JSON.stringify(value)));
    if (!reply || typeof reply !== 'object') throw new Error('cutout core가 잘못된 응답을 돌려주었습니다.');
    return reply;
  };
  const version = request({ op: 'version' });
  if (version.ok !== true || version.api !== 1) throw new Error(`cutout core API가 맞지 않습니다: ${JSON.stringify(version)}`);

  async function validate({ documents, assets, elements }) {
    const out = [];
    const push = (file, pointer, code, message, relatedIds = [], severity = 'error') => out.push({ file, pointer, code, severity, message, relatedIds });
    const fromCore = (doc, diagnostics, skipRig = false) => {
      for (const d of diagnostics ?? []) {
        if (skipRig && String(d.pointer).startsWith(RIG_SENTINEL)) continue;
        push(doc.file, String(d.pointer), String(d.code), String(d.message), [doc.id], d.severity === 'warning' ? 'warning' : 'error');
      }
    };
    const rigs = new Map(documents.filter(d => d.type === 'rig').map(d => [d.id, d]));
    const controllers = new Map(documents.filter(d => d.type === 'controller').map(d => [d.id, d]));
    const images = new Map(assets.map(a => [a.id, a]));
    const sizes = new Map();

    for (const rig of rigs.values()) {
      const reply = request({ op: 'validate', rig: rig.data, rigPointer: '/data' });
      if (!reply.ok) { push(rig.file, '/data', 'ANIM_CORE', `리그를 검사할 수 없습니다: ${reply.error}`, [rig.id]); continue; }
      fromCore(rig, reply.diagnostics);
      // Default attachments and every skin's attachments get the same image checks.
      const lists = [['/data/attachments', rig.data.attachments]];
      for (const [s, skin] of (Array.isArray(rig.data.skins) ? rig.data.skins : []).entries()) lists.push([`/data/skins/${s}/attachments`, skin?.attachments]);
      for (const [base, list] of lists) for (const [i, attachment] of (Array.isArray(list) ? list : []).entries()) {
        const id = attachment?.imageAssetId, pointer = `${base}/${i}`;
        if (typeof id !== 'string' || !id) continue; // empty ids are reported by the core
        const asset = images.get(id);
        if (!asset || asset.kind !== 'image') { push(rig.file, `${pointer}/imageAssetId`, 'ANIM_IMAGE_MISSING', asset ? `에셋 '${id}'는 이미지가 아닙니다.` : `이미지 에셋 '${id}'가 없습니다. 에셋 목록에 PNG를 등록하세요.`, [rig.id, id]); continue; }
        if (!asset.absolutePath) continue;
        if (!sizes.has(asset.absolutePath)) sizes.set(asset.absolutePath, await pngSize(asset.absolutePath));
        const size = sizes.get(asset.absolutePath);
        if (size && (size.width !== attachment.width || size.height !== attachment.height))
          push(rig.file, `${pointer}/width`, 'ANIM_IMAGE_SIZE_MISMATCH', `첨부 크기 ${attachment.width}×${attachment.height}가 PNG '${id}'의 실제 크기 ${size.width}×${size.height}와 다릅니다.`, [rig.id, id], 'warning');
      }
    }
    for (const controller of controllers.values()) {
      const rigId = controller.data.rig, rig = rigs.get(rigId);
      if (typeof rigId === 'string' && !rig) push(controller.file, '/data/rig', 'ANIM_RIG_MISSING', `리그 '${rigId}'가 없습니다. plugins/cutout-animation/rig/${rigId}.json을 만드세요.`, [controller.id, rigId]);
      // With the rig present the core also checks clip names; its rig diagnostics are already reported above.
      const reply = request(rig ? { op: 'validate', rig: rig.data, rigPointer: RIG_SENTINEL, controller: controller.data, controllerPointer: '/data' }
        : { op: 'validate', controller: controller.data, controllerPointer: '/data' });
      if (!reply.ok) { push(controller.file, '/data', 'ANIM_CORE', `컨트롤러를 검사할 수 없습니다: ${reply.error}`, [controller.id]); continue; }
      fromCore(controller, (reply.diagnostics ?? []).filter(d => !(rig === undefined && d.code === 'ANIM_RIG_MISSING')), true);
    }
    for (const { file, pointer, element } of elements) {
      const ext = element?.extension ?? {};
      if (ext.type !== 'character') continue;
      if (!controllers.has(ext.ref)) push(file, `${pointer}/extension/ref`, 'PLUGIN_ELEMENT_REF', `애니메이션 연결(controller) '${ext.ref}'가 없습니다.`, [String(ext.ref)]);
      const props = ext.props ?? {};
      const skins = (rigs.get(controllers.get(ext.ref)?.data?.rig)?.data?.skins ?? []).map(s => s?.id);
      for (const [key, value] of Object.entries(props)) {
        if (key === 'scale' && !(typeof value === 'number' && value > 0 && value <= 10)) push(file, `${pointer}/extension/props/scale`, 'ANIM_ELEMENT_PROP', '배율(scale)은 0보다 크고 10 이하인 숫자입니다.');
        else if (key === 'flipX' && typeof value !== 'boolean') push(file, `${pointer}/extension/props/flipX`, 'ANIM_ELEMENT_PROP', '좌우 반전(flipX)은 true/false입니다.');
        // props.skin (plugin 1.2.0): outfit of this placed instance; "" = the controller default.
        else if (key === 'skin' && typeof value !== 'string') push(file, `${pointer}/extension/props/skin`, 'ANIM_ELEMENT_PROP', '스킨(skin)은 리그의 스킨 ID 문자열입니다.');
        else if (key === 'skin' && value !== '' && controllers.has(ext.ref) && !skins.includes(value))
          push(file, `${pointer}/extension/props/skin`, 'ANIM_SKIN_UNKNOWN', `스킨 '${value}'가 리그에 없습니다. 연결의 기본 스킨으로 표시됩니다.`, [String(ext.ref), String(value)], 'warning');
        // posterAssetId is host-reserved (engine CONTRACT §5) and checked by the engine.
        else if (!['scale', 'flipX', 'skin', 'posterAssetId'].includes(key) && SCALAR.has(typeof value)) push(file, `${pointer}/extension/props/${key}`, 'ANIM_ELEMENT_PROP', `알 수 없는 속성 '${key}'는 무시됩니다.`, [], 'warning');
      }
    }
    return out;
  }

  /**
   * Pass-through to the core JSON ops (validate, sample, template, edit, …). Never writes project
   * files; the only read is the PNG an `edit` → `autoOutline` names by `imagePath` (decoded here
   * into the op's `mask`, plugin 1.6.0).
   */
  async function call(op, payload = {}) {
    if (typeof op !== 'string' || !/^[a-zA-Z][a-zA-Z0-9]{0,63}$/.test(op)) throw new Error('잘못된 플러그인 명령입니다.');
    if (payload === null || typeof payload !== 'object' || Array.isArray(payload)) throw new Error('플러그인 명령 인자는 객체여야 합니다.');
    const body = { ...payload, op };
    if (op === 'edit' && body.operation && typeof body.operation === 'object') {
      body.operation = structuredClone(body.operation);
      await fillOutlineMasks(body.operation);
    }
    return request(body);
  }
  /**
   * Player capabilities an exported game needs for these documents beyond `plugin-elements-v1`
   * (packages/plugin-host bundle -> exportGame checks them against the target player). Mesh
   * attachments (plugin 1.4.0) are drawn only by hosts advertising `plugin-mesh-v1`; a rig
   * without meshes keeps exporting to older players.
   */
  function requiredHostFeatures(documents) {
    const lists = d => [d?.data?.attachments, ...(Array.isArray(d?.data?.skins) ? d.data.skins.map(s => s?.attachments) : [])];
    const mesh = (Array.isArray(documents) ? documents : []).some(d => d?.type === 'rig' && lists(d).some(list => Array.isArray(list) && list.some(a => a?.type === 'mesh')));
    return mesh ? ['plugin-mesh-v1'] : [];
  }
  return { validate, call, requiredHostFeatures };
}

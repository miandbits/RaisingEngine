// Node service module of the cutout animation plugin (role "service", platform "node").
// Runs the SAME C++ core as the players (web runtime Wasm) for validation and authoring
// ops, so Studio, MCP and the exported game can never disagree about a rig.
import { open } from 'node:fs/promises';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

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

  /** Pass-through to the core JSON ops (validate, sample, template, edit, …). Never touches project files. */
  function call(op, payload = {}) {
    if (typeof op !== 'string' || !/^[a-zA-Z][a-zA-Z0-9]{0,63}$/.test(op)) throw new Error('잘못된 플러그인 명령입니다.');
    if (payload === null || typeof payload !== 'object' || Array.isArray(payload)) throw new Error('플러그인 명령 인자는 객체여야 합니다.');
    return request({ ...payload, op });
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

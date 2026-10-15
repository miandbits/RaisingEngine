import React, { useEffect, useMemo, useRef, useState, useSyncExternalStore } from 'react';
import type { ExtensionElement as ExtensionSpec, ExtensionHost, ExtensionItem } from '../../../packages/web-player/extensions.js';

// raisingExtensionManualClock: automation keeps dt=0 and drives host.tick(ms) itself, like the native manual clock.
declare global { interface Window { raisingExtensions?: ExtensionHost | null; raisingExtensionManualClock?: boolean } }

// One shared presentation clock for every mounted instance: update once per frame, then frame each.
const MAX_FRAME_MS = 250;
let loop: { host: ExtensionHost; users: number; handle: number; last: number | null } | null = null;
function retainLoop(host: ExtensionHost): () => void {
  if (!loop || loop.host !== host) {
    if (loop) cancelAnimationFrame(loop.handle);
    const current: NonNullable<typeof loop> = { host, users: 0, handle: 0, last: null };
    const tick = (now: number) => {
      // A hidden tab pauses rAF; clamping the resumed delta keeps the clock paused while backgrounded.
      const dt = current.last === null ? 0 : Math.min(MAX_FRAME_MS, Math.max(0, now - current.last));
      current.last = now; host.tick(window.raisingExtensionManualClock ? 0 : dt); current.handle = requestAnimationFrame(tick);
    };
    current.handle = requestAnimationFrame(tick); loop = current;
  }
  const owned = loop; owned.users++;
  return () => { if (--owned.users <= 0 && loop === owned) { cancelAnimationFrame(owned.handle); loop = null; } };
}
const NO_ITEMS: readonly ExtensionItem[] = [];

// Decoded part images shared by every mesh canvas: a mesh redraws each frame from the same image.
const meshImages = new Map<string, HTMLImageElement>();
function meshImage(src: string): HTMLImageElement {
  let image = meshImages.get(src);
  if (!image) { image = new Image(); image.decoding = 'async'; image.src = src; meshImages.set(src, image); }
  return image;
}
/**
 * Atlas mesh (plugin-atlas-v1): its picture is a sub-image of the page. It is copied once, pixel
 * for pixel, into its own canvas, so the triangle clips (grown by a pixel) sample exactly what the
 * separate image had around its edges, never a neighbour in the page.
 */
const regionImages = new Map<string, HTMLCanvasElement>();
function regionImage(page: HTMLImageElement, src: string, region: NonNullable<ExtensionItem['region']>): HTMLCanvasElement | null {
  const key = `${src}#${region.join(',')}`;
  let canvas = regionImages.get(key);
  if (!canvas) {
    const [x, y, w, h] = region;
    if (!page.complete || !page.naturalWidth || w <= 0 || h <= 0) return null;
    canvas = document.createElement('canvas');
    canvas.width = Math.round(w); canvas.height = Math.round(h);
    canvas.getContext('2d')?.drawImage(page, x, y, w, h, 0, 0, canvas.width, canvas.height);
    regionImages.set(key, canvas);
  }
  return canvas;
}
const MAX_MESH_VERTICES = 2048, MAX_MESH_INDICES = 3 * 4096;
/** Bounding box of a well-formed mesh (pairs of the same count, indices in range), or null. */
function meshBox(mesh: ExtensionItem['mesh']): { x: number; y: number; w: number; h: number } | null {
  if (!mesh || !Array.isArray(mesh.positions) || !Array.isArray(mesh.uvs) || !Array.isArray(mesh.indices)) return null;
  const p = mesh.positions, n = p.length / 2;
  if (p.length % 2 || p.length !== mesh.uvs.length || n > MAX_MESH_VERTICES || !mesh.indices.length || mesh.indices.length % 3 || mesh.indices.length > MAX_MESH_INDICES) return null;
  if (!mesh.indices.every(i => Number.isInteger(i) && i >= 0 && i < n) || !p.every(Number.isFinite)) return null;
  let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
  for (let i = 0; i < p.length; i += 2) { x0 = Math.min(x0, p[i]!); x1 = Math.max(x1, p[i]!); y0 = Math.min(y0, p[i + 1]!); y1 = Math.max(y1, p[i + 1]!); }
  // Two pixels of margin keep the grown edge triangles inside the canvas.
  return { x: Math.floor(x0) - 2, y: Math.floor(y0) - 2, w: Math.ceil(x1) - Math.floor(x0) + 4, h: Math.ceil(y1) - Math.floor(y0) + 4 };
}
/**
 * plugin-mesh-v1 item: a <canvas> over the mesh's bounding box. Each triangle is drawn with the
 * affine image transform that maps its uv triangle onto its position triangle, clipped to the
 * triangle grown by a pixel so anti-aliased clip edges of neighbours leave no seams. One
 * canvas per item keeps the back-to-front order with the <img> region items around it.
 */
const MeshItem = React.memo(function MeshItem({ item, src }: { item: ExtensionItem; src: string }) {
  const ref = useRef<HTMLCanvasElement>(null);
  const box = useMemo(() => meshBox(item.mesh), [item.mesh]);
  const [loaded, setLoaded] = useState(0);
  useEffect(() => {
    const canvas = ref.current, mesh = item.mesh;
    if (!canvas || !box || !mesh) return;
    const page = meshImage(src);
    if (!page.complete) { const done = () => setLoaded(n => n + 1); page.addEventListener('load', done, { once: true }); return () => page.removeEventListener('load', done); }
    const image = item.region ? regionImage(page, src, item.region) : page;
    if (!image) return;
    const dpr = window.devicePixelRatio || 1, w = Math.max(1, Math.ceil(box.w * dpr)), h = Math.max(1, Math.ceil(box.h * dpr));
    if (canvas.width !== w) canvas.width = w;
    if (canvas.height !== h) canvas.height = h;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.clearRect(0, 0, w, h);
    // A full device pixel of overlap: two anti-aliased half-covered edges would otherwise show a light seam.
    const iw = image instanceof HTMLImageElement ? image.naturalWidth : image.width, ih = image instanceof HTMLImageElement ? image.naturalHeight : image.height;
    const p = mesh.positions, uv = mesh.uvs, grow = dpr;
    if (!iw || !ih) { canvas.dataset.drawn = 'false'; return; }
    // meshBox validated every index and pair: the lookups below are in range.
    const px = (v: number) => (p[2 * v]! - box.x) * dpr, py = (v: number) => (p[2 * v + 1]! - box.y) * dpr;
    const tu = (v: number) => uv[2 * v]! * iw, tv = (v: number) => uv[2 * v + 1]! * ih;
    for (let t = 0; t < mesh.indices.length; t += 3) {
      const i = mesh.indices[t]!, j = mesh.indices[t + 1]!, k = mesh.indices[t + 2]!;
      const x0 = px(i), y0 = py(i), x1 = px(j), y1 = py(j), x2 = px(k), y2 = py(k);
      const u0 = tu(i), v0 = tv(i), du1 = tu(j) - u0, dv1 = tv(j) - v0, du2 = tu(k) - u0, dv2 = tv(k) - v0;
      const det = du1 * dv2 - du2 * dv1;
      if (Math.abs(det) < 1e-9) continue;  // a degenerate uv triangle maps no image area
      // (u, v) -> (x, y): solve the two edge vectors of the triangle.
      const a = ((x1 - x0) * dv2 - (x2 - x0) * dv1) / det, c = ((x2 - x0) * du1 - (x1 - x0) * du2) / det;
      const b = ((y1 - y0) * dv2 - (y2 - y0) * dv1) / det, d = ((y2 - y0) * du1 - (y1 - y0) * du2) / det;
      const cx = (x0 + x1 + x2) / 3, cy = (y0 + y1 + y2) / 3;
      const out = (x: number, y: number): [number, number] => { const l = Math.hypot(x - cx, y - cy) || 1; return [x + (x - cx) / l * grow, y + (y - cy) / l * grow]; };
      ctx.save();
      ctx.beginPath();
      ctx.moveTo(...out(x0, y0)); ctx.lineTo(...out(x1, y1)); ctx.lineTo(...out(x2, y2));
      ctx.closePath();
      ctx.clip();
      ctx.setTransform(a, b, c, d, x0 - a * u0 - c * v0, y0 - b * u0 - d * v0);
      ctx.drawImage(image, 0, 0);
      ctx.restore();
    }
    canvas.dataset.drawn = 'true';
  }, [item, src, box, loaded]);
  if (!box) return null;  // a malformed mesh draws nothing (never the undeformed fallback rectangle)
  return <canvas ref={ref} className="visual-plugin-item visual-plugin-mesh" aria-hidden="true" data-image-asset-id={item.imageAssetId} data-mesh="true"
    style={{ left: box.x, top: box.y, width: box.w, height: box.h, opacity: item.alpha }}/>;
});
/**
 * A rectangle item. With an atlas region the page is shown through a clipped box of the item's
 * size, offset to the sub-image (CSS sprite; the page's 2 px edge padding keeps filtering at the
 * box edge identical to the separate image). Items unchanged between compact frames keep their
 * object, so memo skips them.
 */
const RegionItem = React.memo(function RegionItem({ item, src }: { item: ExtensionItem; src: string }) {
  const transform = `matrix(${item.matrix.join(',')})`;
  if (!item.region) return <img className="visual-plugin-item" src={src} alt="" aria-hidden="true" draggable={false}
    data-image-asset-id={item.imageAssetId} style={{ width: item.width, height: item.height, opacity: item.alpha, transform }}/>;
  const [x, y, w, h] = item.region;
  return <div className="visual-plugin-item visual-plugin-region" aria-hidden="true" data-image-asset-id={item.imageAssetId} data-region={item.region.join(',')}
    style={{ width: item.width, height: item.height, opacity: item.alpha, transform }}>
    <img src={src} alt="" draggable={false} style={{ transform: `scale(${item.width / w},${item.height / h}) translate(${-x}px,${-y}px)` }}/>
  </div>;
});

/** Screen element of kind `plugin`: absolutely positioned images with the plugin's affine matrices (meshes on canvases). */
export function ExtensionView({ instance, extension, width, height, assetUrl, editing = false }: {
  instance: string; extension: ExtensionSpec | undefined; width: number; height: number; assetUrl: (id: string) => string; editing?: boolean;
}) {
  const host = typeof window === 'undefined' ? null : window.raisingExtensions ?? null;
  const usable = !editing && !!host && !!extension && host.available(extension.plugin);
  const [error, setError] = useState('');
  const key = JSON.stringify([extension, width, height]);
  useEffect(() => {
    if (!usable || !host || !extension) return;
    setError(host.acquire(instance, extension, width, height) ? '' : host.mountError(instance) || '플러그인 요소를 표시할 수 없습니다.');
    return () => host.release(instance);
  }, [usable, host, instance, key]);
  useEffect(() => usable && host ? retainLoop(host) : undefined, [usable, host]);
  const items = useSyncExternalStore(listener => host ? host.subscribe(listener) : () => {}, () => host?.items(instance) ?? NO_ITEMS);
  const plugin = extension?.plugin ?? '?';
  if (!usable || error) return <div className="visual-plugin-placeholder" data-instance={instance} role="note">{!usable ? `플러그인 필요: ${plugin}` : `플러그인 오류: ${plugin}`}{error && <small>{error}</small>}</div>;
  return <div className="visual-plugin-layer" data-instance={instance}>
    {items.map((item, index) => item.mesh ? <MeshItem key={index} item={item} src={assetUrl(item.imageAssetId)}/> : <RegionItem key={index} item={item} src={assetUrl(item.imageAssetId)}/>)}
  </div>;
}

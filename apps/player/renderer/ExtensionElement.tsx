import React, { useEffect, useState, useSyncExternalStore } from 'react';
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

/** Screen element of kind `plugin`: absolutely positioned images with the plugin's affine matrices. */
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
    {items.map((item, index) => <img key={index} className="visual-plugin-item" src={assetUrl(item.imageAssetId)} alt="" aria-hidden="true" draggable={false}
      data-image-asset-id={item.imageAssetId}
      style={{ width: item.width, height: item.height, opacity: item.alpha, transform: `matrix(${item.matrix.join(',')})` }}/>)}
  </div>;
}

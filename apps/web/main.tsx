import React from 'react';
import { createRoot } from 'react-dom/client';
import { Player } from '../player/renderer/main.js';
import { WasmRuntime, type WasmCoreModule } from '../../packages/native-bridge/wasm.js';
import { WebPlayerController } from '../../packages/web-player/controller.js';
import type { GameBundle } from '../../packages/export-service/model.js';
// Generated from the shared save schema, with no runtime eval or separate web contract.
import validateSave from './generated/save-validator.mjs';

const container = document.getElementById('root')!;
async function start() {
  const response = await fetch('./game.json');
  if (!response.ok) throw new Error(`게임 데이터를 열 수 없습니다 (${response.status}).`);
  const bundle = await response.json() as GameBundle;
  if (bundle.formatVersion !== '1' || !bundle.content?.project || !bundle.assets) throw new Error('지원하지 않는 게임 묶음입니다.');
  const wasmModuleUrl = new URL('./wasm/raising-wasm.mjs', document.baseURI).href;
  const { default: createModule } = await import(/* @vite-ignore */ wasmModuleUrl) as { default: () => Promise<WasmCoreModule> };
  const runtime = new WasmRuntime(await createModule());
  window.raisingAssetUrl = id => {
    const relative = Object.hasOwn(bundle.assets, id) ? bundle.assets[id]! : '';
    if (!/^assets\/[a-z0-9-]+\.[a-z0-9]+$/.test(relative)) return '';
    return new URL(relative, document.baseURI).href;
  };
  window.player = new WebPlayerController(bundle.content, runtime, {
    read: () => new Promise<string | null>((resolve, reject) => {
      const input = document.createElement('input'); input.type = 'file'; input.accept = '.json,application/json';
      input.style.display = 'none'; document.body.append(input);
      input.addEventListener('cancel', () => { input.remove(); resolve(null); }, { once: true });
      input.addEventListener('change', () => {
        const file = input.files?.[0]; input.remove();
        if (!file) { resolve(null); return; }
        if (file.size > 16 * 1024 * 1024) { reject(new Error('세이브는 16 MiB 이하여야 합니다.')); return; }
        void file.text().then(resolve, reject);
      }, { once: true });
      input.click();
    }),
    write: async text => {
      const url = URL.createObjectURL(new Blob([text], { type: 'application/json' }));
      const link = document.createElement('a'); link.href = url; link.download = 'raising-save.json';
      document.body.append(link); link.click(); link.remove(); setTimeout(() => URL.revokeObjectURL(url), 1000);
    },
  }, validateSave);
  document.title = bundle.content.project.title;
  createRoot(container).render(<Player />);
}
void start().catch(error => {
  container.replaceChildren();
  const message = document.createElement('p'); message.setAttribute('role', 'alert');
  message.textContent = `게임을 시작하지 못했습니다: ${error instanceof Error ? error.message : String(error)}`;
  container.append(message);
});

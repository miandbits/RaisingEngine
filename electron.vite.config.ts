import { defineConfig } from 'electron-vite';
import { resolve } from 'node:path';

export default defineConfig({
  main: { build: { outDir: 'dist/player/main', externalizeDeps: false,
    rollupOptions: { input: resolve('apps/player/main/index.ts'), output: { format: 'es', entryFileNames: 'index.js' } } } },
  preload: { build: { outDir: 'dist/player/preload', externalizeDeps: false,
    rollupOptions: { input: resolve('apps/player/preload/index.ts'), output: { format: 'cjs', entryFileNames: 'index.cjs' } } } },
  renderer: { root: 'apps/player/renderer', base: './', build: { outDir: resolve('dist/player/renderer'),
    rollupOptions: { input: resolve('apps/player/renderer/index.html') } } },
});

import { defineConfig } from 'electron-vite';
import { resolve } from 'node:path';
export default defineConfig({
  main: { build: { outDir: 'dist/studio/main', externalizeDeps: false, rollupOptions: { input: { index: resolve('legacy/studio/main/index.ts'), 'analysis-worker': resolve('packages/analysis-service/worker.ts'), 'run-worker': resolve('packages/analysis-service/run-worker.ts') }, output: { format: 'es', entryFileNames: '[name].js' } } } },
  preload: { build: { outDir: 'dist/studio/preload', externalizeDeps: false, rollupOptions: { input: resolve('legacy/studio/preload/index.ts'), output: { format: 'cjs', entryFileNames: 'index.cjs' } } } },
  renderer: { root: 'legacy/studio/renderer', base: './', build: { outDir: resolve('dist/studio/renderer'), rollupOptions: { input: resolve('legacy/studio/renderer/index.html') } } },
});

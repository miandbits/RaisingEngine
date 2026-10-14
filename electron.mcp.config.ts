import { defineConfig } from 'electron-vite';
import { resolve } from 'node:path';
export default defineConfig({
  main: { build: { outDir: 'dist/mcp/main', externalizeDeps: false,
    rollupOptions: { input: { index: resolve('apps/tools/mcp/main.ts'), 'analysis-worker': resolve('packages/analysis-service/worker.ts'), 'run-worker': resolve('packages/analysis-service/run-worker.ts') }, output: { format: 'es', entryFileNames: '[name].js' } } } },
});

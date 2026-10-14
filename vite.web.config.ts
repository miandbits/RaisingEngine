import { defineConfig } from 'vite';
import { resolve } from 'node:path';
export default defineConfig({ root: resolve('apps/web'), base: './',
  build: { outDir: resolve('dist/web'), emptyOutDir: true, target: 'es2022' } });

import { readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { browserSmoke } from './browser-smoke.mjs';
const evidence = JSON.parse(await readFile(resolve('.cache/journey-packaged-ui/evidence.json'), 'utf8'));
const cleanup = [];
try {
  await browserSmoke({ after: callback => cleanup.push(callback) }, resolve(evidence.exportRoot, 'web'), resolve('.cache/journey-packaged-ui/packaged-web-inspection.png'));
  console.log('Packaged web smoke passed.');
} finally { for (const callback of cleanup) await callback(); }

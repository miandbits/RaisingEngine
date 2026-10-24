import { readFile, writeFile, mkdir, cp } from 'node:fs/promises';
import { execFileSync } from 'node:child_process';
import Ajv2020 from 'ajv/dist/2020.js';
import standaloneCode from 'ajv/dist/standalone/index.js';
import { build } from 'vite';
import { resolve } from 'node:path';
// WEB_OUT lets tests build a separate copy; the default output stays dist/web.
const out = process.env.WEB_OUT ? resolve(process.env.WEB_OUT) : 'dist/web';
const ajv = new Ajv2020({ strict: true, allErrors: true, code: { source: true, esm: true } });
ajv.addSchema(JSON.parse(await readFile('packages/schema/common.schema.json', 'utf8')));
const validate = ajv.compile(JSON.parse(await readFile('packages/runtime-service/save.schema.json', 'utf8')));
await mkdir('apps/web/generated', { recursive: true });
// Ajv emits ESM exports but CommonJS runtime helpers. Make those static imports for Vite and CSP.
const imports = [];
const source = standaloneCode(ajv, validate).replace(/require\("(ajv\/dist\/runtime\/[a-zA-Z0-9_-]+)"\)\.default/g, (_match, name) => {
  const binding = `ajvRuntime${imports.length}`;
  imports.push(`import ${binding} from ${JSON.stringify(`${name}.js`)};`);
  return `(${binding}.default ?? ${binding})`;
});
if (/\brequire\(/.test(source)) throw new Error('Unsupported CommonJS helper in generated web validator.');
await writeFile('apps/web/generated/save-validator.mjs', `${imports.join('\n')}\n${source}`);
execFileSync(process.execPath, ['node_modules/typescript/bin/tsc', '-p', 'tsconfig.web.json'], { stdio: 'inherit' });
await build({ configFile: 'vite.web.config.ts', ...(process.env.WEB_OUT ? { build: { outDir: out, emptyOutDir: true } } : {}) });
await mkdir(`${out}/wasm`, { recursive: true });
for (const name of ['raising-wasm.mjs', 'raising-wasm.wasm']) await cp(`dist/wasm/${name}`, `${out}/wasm/${name}`);
await writeFile(`${out}/capabilities.json`, JSON.stringify({ formatVersion: '1', runtime: 'cpp-wasm-react',
  visualFeatures: ['readable-layout-v1','save-slots-v1','calendar-v1','choice-feedback-v1','narrative-cards-v1','activity-rules-summary-v1','narrative-actor-binding-v1','data-meters-v1','expedition-conditions-v1','components-v1','anchors-v1','responsive-v1','facilities-v1','generic-rules-v1','action-flows-v1','data-ui-v1','activity-timeline-v1','battle-hud-v1','ui-hierarchy-v1','user-fields-v1','activity-summary-v1','scene-portrait-transform-v1','relationship-change-v1','free-screens-v1','presentation-variants-v1','custom-states-v1','conditional-routing-v1','component-context-v1','roster-activities-v1','character-scoped-rules-v1','story-appointments-v1','actor-story-links-v1','monthly-schedule-v1','inline-result-playback-v1','probabilistic-actions-v1','optional-confirmation-v1','plugin-elements-v1','plugin-mesh-v1','plugin-atlas-v1'],
  imageExtensions: ['.png', '.jpg', '.jpeg', '.webp', '.gif'], audioExtensions: ['.wav', '.mp3', '.ogg'], fontExtensions: ['.ttf','.otf'] }, null, 2));
console.log(`Web player built: ${out} (C++ Wasm runtime).`);

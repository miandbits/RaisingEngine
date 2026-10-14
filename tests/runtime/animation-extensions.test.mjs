import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile,access,cp,appendFile} from 'node:fs/promises';
import path from 'node:path';
import {StudioEditor} from '../../dist/packages/studio-service/editor.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {PlayerPresentation} from '../../dist/packages/player-service/presentation.js';
import {decodeSave,encodeSave} from '../../dist/packages/runtime-service/save.js';
const projectRoot=path.resolve('projects/magical-character-reference');
process.env.RAISING_PLUGIN_STORE=path.resolve('.cache/character-plugin-store');
// Unified plugin system: validation and runtime bundling go through the editor's plugin store.
const open=async store=>{const editor=new StudioEditor({pluginStore:store});await editor.open(projectRoot);return editor;};
const editor=await open(process.env.RAISING_PLUGIN_STORE);
const built=await editor.snapshot(editor.revision);assert.equal(built.ok,true);
const runtime=path.resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME??'dist/native-player-character');
test('native expression save replays in shared service and preserves unavailable plugin state',async()=>{
 const latest=JSON.parse(await readFile('.cache/character-verification/latest.json','utf8'));
 const saved=JSON.parse(await readFile(path.join(latest.work,'slot.json'),'utf8'));
 saved.playerPresentation.extensions['unavailable-animation']={version:7,payload:['keep',42]};
 const loaded=await decodeSave(built.snapshot,JSON.stringify(saved));assert.equal(loaded.ok,true,JSON.stringify(loaded));
 const presentation=new PlayerPresentation();assert.equal(presentation.restore(built.snapshot,loaded.session.state,loaded.playerPresentation),null);
 const encoded=await encodeSave(built.snapshot,loaded.session,presentation.envelope(built.snapshot,loaded.session.state));assert.equal(encoded.ok,true);
 assert.deepEqual(JSON.parse(encoded.text).playerPresentation.extensions,saved.playerPresentation.extensions);
 assert.deepEqual(JSON.parse(encoded.text).state,saved.state);
});
test('export rejects old host, unprepared runtime and damaged DLL; validation rejects missing controller',async()=>{
 await mkdir('.cache/character-contract-tests',{recursive:true});const work=await mkdtemp(path.resolve('.cache/character-contract-tests/run-'));
 const fixture=path.join(work,'runtime');await mkdir(fixture);await writeFile(path.join(fixture,'RaisingPlayer.exe'),'packaging fixture');await writeFile(path.join(fixture,'index.html'),'packaging fixture');
 const capabilities=JSON.parse(await readFile(path.join(runtime,'capabilities.json'),'utf8'));
 await writeFile(path.join(fixture,'capabilities.json'),JSON.stringify({...capabilities,visualFeatures:capabilities.visualFeatures.filter(x=>x!=='plugin-elements-v1')}));
 const extensions=await editor.exportExtensions(editor.revision,built.snapshot,'windows');
 const options={content:built.snapshot,projectRoot,destination:path.join(work,'game'),target:'windows',runtimes:{windows:fixture,web:fixture},extensions};
 await assert.rejects(exportGame(options),/plugin-elements-v1/);await assert.rejects(access(options.destination),{code:'ENOENT'});
 await writeFile(path.join(fixture,'capabilities.json'),JSON.stringify(capabilities));
 // Plugin elements without prepared runtime modules are refused instead of exported silently.
 await assert.rejects(exportGame({...options,extensions:null}),{code:'PLUGIN_RUNTIME_MISSING'});await assert.rejects(access(options.destination),{code:'ENOENT'});
 // The real package also ships a web runtime; preparation picks only the target's files.
 const web=await editor.exportExtensions(editor.revision,built.snapshot,'web');
 assert.deepEqual(web.files.map(f=>f.target),['plugins/cutout-animation/cutout-runtime.mjs','plugins/cutout-animation/cutout-runtime.wasm']);
 // Element references are checked by plugin validation (CONTRACT §3) before any export.
 const broken=await open(process.env.RAISING_PLUGIN_STORE),data=JSON.parse(await readFile(path.join(projectRoot,'project.json'),'utf8'));
 data.visual.screens.find(s=>s.id==='home').elements.find(e=>e.kind==='plugin').extension.ref='missing';
 broken.edit({type:'replace',file:'project.json',data},broken.revision);await broken.validate(broken.revision);
 assert.ok(broken.diagnostics.some(d=>d.code==='PLUGIN_ELEMENT_REF'&&d.severity==='error'),JSON.stringify(broken.diagnostics));
 const damagedStore=path.join(work,'damaged-store');await cp(process.env.RAISING_PLUGIN_STORE,damagedStore,{recursive:true});
 await appendFile(path.join(damagedStore,'installed/cutout-animation/1.0.0/runtime/windows/cutout-runtime.dll'),'damaged');
 const damaged=await open(damagedStore);
 await assert.rejects(damaged.exportExtensions(damaged.revision,built.snapshot,'windows'),{code:'PLUGIN_RUNTIME_MISSING'});
 await exportGame(options);assert.equal(JSON.parse(await readFile(path.join(options.destination,'game.json'),'utf8')).extensions.plugins[0].abi,'raising-extension-1');
});

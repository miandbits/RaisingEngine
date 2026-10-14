import test from 'node:test';
import assert from 'node:assert/strict';
import {mkdir,mkdtemp,readFile,writeFile,access,cp,appendFile} from 'node:fs/promises';
import path from 'node:path';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {exportGame} from '../../dist/packages/export-service/export.js';
import {PlayerPresentation} from '../../dist/packages/player-service/presentation.js';
import {decodeSave,encodeSave} from '../../dist/packages/runtime-service/save.js';
const projectRoot=path.resolve('projects/magical-character-reference');
process.env.RAISING_PLUGIN_STORE=path.resolve('.cache/character-plugin-store');
const built=await buildSnapshot(await loadProject(projectRoot));assert.equal(built.ok,true);
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
test('export rejects old host, missing target module, damaged DLL and missing controller',async()=>{
 await mkdir('.cache/character-contract-tests',{recursive:true});const work=await mkdtemp(path.resolve('.cache/character-contract-tests/run-'));
 const runtime=path.join(work,'runtime');await mkdir(runtime);await writeFile(path.join(runtime,'RaisingPlayer.exe'),'packaging fixture');await writeFile(path.join(runtime,'index.html'),'packaging fixture');
 const capabilities=JSON.parse(await readFile('dist/native-player-character/capabilities.json','utf8'));
 await writeFile(path.join(runtime,'capabilities.json'),JSON.stringify({...capabilities,visualFeatures:capabilities.visualFeatures.filter(x=>x!=='plugin-elements-v1')}));
 const options={content:built.snapshot,projectRoot,destination:path.join(work,'game'),target:'windows',runtimes:{windows:runtime,web:runtime}};
 await assert.rejects(exportGame(options),/plugin-elements-v1/);await assert.rejects(access(options.destination),{code:'ENOENT'});
 await writeFile(path.join(runtime,'capabilities.json'),JSON.stringify(capabilities));
 await assert.rejects(exportGame({...options,target:'web'}),/PLUGIN_RUNTIME_MISSING/);await assert.rejects(access(options.destination),{code:'ENOENT'});
 const invalid=structuredClone(built.snapshot);invalid.project.visual.screens.find(s=>s.id==='home').elements.find(e=>e.kind==='plugin').extension.ref='missing';
 await assert.rejects(exportGame({...options,content:invalid}),/PLUGIN_REFERENCE/);
 const originalStore=process.env.RAISING_PLUGIN_STORE,damagedStore=path.join(work,'damaged-store');await cp(originalStore,damagedStore,{recursive:true});
 await appendFile(path.join(damagedStore,'installed/cutout-animation/1.0.0/runtime/windows/cutout-runtime.dll'),'damaged');
 process.env.RAISING_PLUGIN_STORE=damagedStore;
 try{await assert.rejects(exportGame(options),/PLUGIN_INTEGRITY/);}finally{process.env.RAISING_PLUGIN_STORE=originalStore;}
 await exportGame(options);assert.equal(JSON.parse(await readFile(path.join(options.destination,'game.json'),'utf8')).extensions.plugins[0].abi,'raising-extension-1');
});

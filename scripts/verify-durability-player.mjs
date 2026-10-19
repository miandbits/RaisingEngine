// Consumes the disposable rule-migration fixture produced by test:durability.
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {resolve,join} from 'node:path';
import {spawnSync} from 'node:child_process';
import {loadProject,buildSnapshot} from '../dist/packages/project-service/index.js';
import {decodeSave} from '../dist/packages/runtime-service/save.js';
import {exportGame} from '../dist/packages/export-service/export.js';

const fixture=JSON.parse(await readFile(process.env.QT_MIGRATION_FIXTURE||'.cache/durability/stage4-migration-ui.json','utf8'));
const built=await buildSnapshot(await loadProject(fixture.to));assert.ok(built.ok,JSON.stringify(built.diagnostics));
const migrated=await readFile(fixture.output,'utf8'),decoded=await decodeSave(built.snapshot,migrated);assert.ok(decoded.ok,JSON.stringify(decoded));
const root=join(fixture.evidence,'native-player');await mkdir(root,{recursive:true});
const game=join(root,'game');await exportGame({content:built.snapshot,projectRoot:fixture.to,destination:game,target:'windows',runtimes:{windows:resolve(process.env.RAISING_NATIVE_PLAYER_RUNTIME||'dist/native-player-authoring-preview'),web:resolve('dist/web')}});
const slot=join(root,'save.json');await writeFile(slot,migrated);
const steps=join(root,'steps.json'),report=join(root,'report.json');
await writeFile(steps,JSON.stringify([{key:'F9'},{checkpoint:'migrated',screenshot:join(root,'migrated.png')},{key:'F5'},{key:'F9'},{checkpoint:'restored',screenshot:join(root,'restored.png')}]));
const run=spawnSync(join(game,'RaisingPlayer.exe'),['--bundle',game,'--save',slot,'--hidden','--automation',steps,'--report',report],{windowsHide:true,encoding:'utf8',timeout:60000});
assert.equal(run.status,0,run.stdout+'\n'+run.stderr);
const result=JSON.parse(await readFile(report,'utf8'));assert.ok(result.ok,JSON.stringify(result));assert.deepEqual(result.final.state,decoded.session.state);
const resumed=await decodeSave(built.snapshot,await readFile(slot,'utf8'));assert.ok(resumed.ok);assert.deepEqual(resumed.session,decoded.session);
assert.equal(await readFile(fixture.output,'utf8'),migrated,'reviewed migration is not overwritten by player saves');
console.log(JSON.stringify({ok:true,report,save:slot,restoredCommands:resumed.session.commands.length}));

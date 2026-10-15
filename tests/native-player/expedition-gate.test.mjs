import test from 'node:test';
import assert from 'node:assert/strict';
import {readFile,writeFile,mkdtemp} from 'node:fs/promises';
import {resolve} from 'node:path';
import {spawnSync} from 'node:child_process';
import {buildSnapshot} from '../../dist/packages/project-service/snapshot.js';
import {loadProject} from '../../dist/packages/project-service/loader.js';
import {exportGame} from '../../dist/packages/export-service/export.js';

test('expedition entry explains locked exits; return remains clickable; qualified route moves and wins',{timeout:120000},async()=>{
 const root=await mkdtemp(resolve('.cache/entry-gate-')),game=resolve(root,'game'),evidence=resolve('docs/evidence/goal-routes');
 const built=await buildSnapshot(await loadProject(resolve('examples/starlight-three-months')));assert.equal(built.ok,true);
 await exportGame({content:built.snapshot,projectRoot:resolve('examples/starlight-three-months'),destination:game,target:'windows',runtimes:{windows:resolve('dist/native-player-routes'),web:resolve('dist/web')}});
 const steps=[{click:'start'},{click:'tactics-entry'},{click:'party-depart'},{click:'confirm-purchase'},{checkpoint:'entry',screenshot:resolve(evidence,'entry-conditions.png')},{clickWindow:'move-expedition-clearing'},{checkpoint:'blocked'},{clickWindow:'return-expedition'},{checkpoint:'returned'},{click:'party-home'},{checkpoint:'home'},{key:'F5'},{click:'schedule-link'},{click:'load'},{checkpoint:'restored'}];
 const file=resolve(root,'steps.json'),report=resolve(evidence,'entry-gate-report.json');await writeFile(file,JSON.stringify(steps));
 const result=spawnSync(resolve(game,'RaisingPlayer.exe'),['--bundle',game,'--save',resolve(root,'save.json'),'--automation',file,'--report',report,'--hidden'],{encoding:'utf8',windowsHide:true,timeout:90000});const data=JSON.parse(await readFile(report,'utf8'));assert.equal(result.status,0,JSON.stringify(data.error));
 const ui=id=>data.steps.find(s=>s.command.checkpoint===id).ui,entry=ui('entry');assert.equal(entry.state.systems.expedition.nodeId,'entrance');
 assert.equal(entry.elements.find(e=>e.id==='move-expedition-clearing').enabled,false);assert.equal(entry.elements.find(e=>e.id==='return-expedition').enabled,true);
 const text=Object.values(entry.texts??entry.text??{}).join(' ');assert.match(text,/현재/);assert.match(text,/16/);assert.match(text,/99/);
 assert.deepEqual(ui('blocked').state,entry.state);assert.equal(ui('returned').state.systems.expedition,null);assert.equal(ui('returned').state.phase,'planning');assert.deepEqual(ui('home').state,ui('restored').state);
 const full=JSON.parse(await readFile(resolve(evidence,'play-report.json'),'utf8'));const battle=full.steps.find(s=>s.command.checkpoint==='battle').ui;assert.equal(battle.state.systems.expedition.nodeId,'clearing');assert.equal(battle.state.phase,'battle');assert.equal(full.steps.find(s=>s.command.checkpoint==='victory').ui.state.flags.graduated,true);
 await writeFile(resolve(evidence,'entry-gate-verification.json'),JSON.stringify({ok:true,reasonShown:true,disabledClickPreservedState:true,returnToCamp:true,qualifiedMovementAndVictory:true,saveLoad:true},null,2));
});
